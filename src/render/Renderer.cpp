#include "Renderer.h"
#include <bgfx/platform.h>
#include <bx/math.h>

#include <cstdint>
#include <array>
#include <cstring>
#include <cstdio>
#include <string>
#include <filesystem>
#include <cstdarg>

#include "../core/Time.h"
#include "Texture.h"

#ifdef _WIN32
  #include <windows.h>
#endif

// ===== Callback de BGFX para logs/fatales =====
struct BgfxCb : public bgfx::CallbackI
{
    void fatal(const char* filePath, uint16_t line, bgfx::Fatal::Enum code, const char* str) override
    {
        std::fprintf(stderr, "[BGFX FATAL] %s(%u): %s\n",
                     filePath ? filePath : "?", line, str ? str : "");
        std::fflush(stderr);
#ifdef _WIN32
        // No lanzar excepción entre hilos. Imprime y termina limpio.
        ::TerminateProcess(GetCurrentProcess(), (UINT)code);
#else
        std::abort();
#endif
    }

    void traceVargs(const char*, uint16_t, const char* format, va_list argList) override
    {
        std::vfprintf(stderr, format, argList);
        std::fputc('\n', stderr);
    }

    // Métodos requeridos por la interfaz:
    void profilerBegin(const char*, uint32_t, const char*, uint16_t) override {}
    void profilerBeginLiteral(const char*, uint32_t, const char*, uint16_t) override {}
    void profilerEnd() override {}
    uint32_t cacheReadSize(uint64_t) override { return 0; }
    bool     cacheRead(uint64_t, void*, uint32_t) override { return false; }
    void     cacheWrite(uint64_t, const void*, uint32_t) override {}
    void screenShot(const char* filePath, uint32_t w, uint32_t h, uint32_t,
                    const void*, uint32_t, bool) override
    {
        std::fprintf(stderr, "[BGFX] screenShot: %s (%ux%u)\n",
                     filePath ? filePath : "(null)", w, h);
    }
    void captureBegin(uint32_t w, uint32_t h, uint32_t,
                      bgfx::TextureFormat::Enum fmt, bool yflip) override
    {
        std::fprintf(stderr, "[BGFX] captureBegin %ux%u fmt=%d yflip=%d\n",
                     w, h, int(fmt), int(yflip));
    }
    void captureEnd() override { std::fprintf(stderr, "[BGFX] captureEnd\n"); }
    void captureFrame(const void*, uint32_t) override {}
};

static BgfxCb s_bgfxCb;

// ===== Helpers de carpeta .exe / shaders =====
static std::string exeDir()
{
#ifdef _WIN32
    char path[MAX_PATH]{0};
    DWORD n = GetModuleFileNameA(nullptr, path, MAX_PATH);
    if (n == 0 || n == MAX_PATH) return std::string{};
    std::filesystem::path p(path);
    return p.parent_path().string();
#else
    return std::filesystem::current_path().string();
#endif
}

static std::string detectShaderBaseDx11()
{
    if (const char* env = std::getenv("SANDBOXCITY_SHADER_DIR"))
    {
        std::filesystem::path base(env);
        if (std::filesystem::exists(base)) {
            std::printf("[SHADERS] Usando SANDBOXCITY_SHADER_DIR: %s\n", base.string().c_str());
            return base.string();
        } else {
            std::printf("[SHADERS] SANDBOXCITY_SHADER_DIR no existe: %s\n", base.string().c_str());
        }
    }
    {
        std::filesystem::path base = std::filesystem::path(exeDir()) / "shaders" / "dx11";
        if (std::filesystem::exists(base)) {
            std::printf("[SHADERS] Usando carpeta junto al .exe: %s\n", base.string().c_str());
            return base.string();
        }
    }
    {
        std::filesystem::path base = std::filesystem::path(exeDir()) / ".." / ".." / "shaders" / "dx11";
        base = std::filesystem::weakly_canonical(base);
        if (std::filesystem::exists(base)) {
            std::printf("[SHADERS] Usando fallback ../../shaders/dx11: %s\n", base.string().c_str());
            return base.string();
        }
    }
    std::printf("[SHADERS] ADVERTENCIA: usando ruta relativa 'shaders/dx11'\n");
    return "shaders/dx11";
}

static std::string detectAssetsBase()
{
    if (const char* env = std::getenv("SANDBOXCITY_ASSETS_DIR"))
    {
        std::filesystem::path base(env);
        if (std::filesystem::exists(base)) {
            std::printf("[ASSETS] Usando SANDBOXCITY_ASSETS_DIR: %s\n", base.string().c_str());
            return base.string();
        } else {
            std::printf("[ASSETS] SANDBOXCITY_ASSETS_DIR no existe: %s\n", base.string().c_str());
        }
    }
    
    // Intenta desde la carpeta del .exe
    {
        std::filesystem::path base = std::filesystem::path(exeDir()) / "assets";
        if (std::filesystem::exists(base)) {
            std::printf("[ASSETS] Usando carpeta junto al .exe: %s\n", base.string().c_str());
            return base.string();
        }
    }
    
    // Intenta ../../../assets (build/bin/RelWithDebInfo -> build -> bin -> .. -> raíz)
    {
        std::filesystem::path base = std::filesystem::path(exeDir()) / ".." / ".." / ".." / "assets";
        base = std::filesystem::weakly_canonical(base);
        if (std::filesystem::exists(base)) {
            std::printf("[ASSETS] Usando fallback ../../../assets: %s\n", base.string().c_str());
            return base.string();
        }
    }
    
    // Último intento: ../../assets
    {
        std::filesystem::path base = std::filesystem::path(exeDir()) / ".." / ".." / "assets";
        base = std::filesystem::weakly_canonical(base);
        if (std::filesystem::exists(base)) {
            std::printf("[ASSETS] Usando fallback ../../assets: %s\n", base.string().c_str());
            return base.string();
        }
    }
    
    std::printf("[ASSETS] ERROR: No se encontró carpeta 'assets' en ninguna ubicación\n");
    std::printf("[ASSETS] Buscó en:\n");
    std::printf("[ASSETS]   - %s/assets\n", exeDir().c_str());
    std::printf("[ASSETS]   - %s/../../../assets\n", exeDir().c_str());
    std::printf("[ASSETS]   - %s/../../assets\n", exeDir().c_str());
    return "assets";
}

static bgfx::TextureHandle makeFallbackChecker()
{
    // 2x2 RGBA (blanco y gris), evita pantalla negra si no hay archivo
    const uint8_t pix[] = {
        255,255,255,255,  64,64,64,255,
         64,64,64,255,   255,255,255,255
    };
    const bgfx::Memory* mem = bgfx::copy(pix, sizeof(pix));
    return bgfx::createTexture2D(2, 2, false, 1, bgfx::TextureFormat::RGBA8, BGFX_TEXTURE_NONE, mem);
}



static bool tryInitBackend(void* nwh, uint32_t w, uint32_t h, bgfx::RendererType::Enum type)
{
    // Sincroniza frame previo si quedó algún worker colgado de ejecuciones anteriores.
    bgfx::renderFrame();

    bgfx::Init init{};
    init.type     = type;                  // Forzamos tipo solicitado
    init.vendorId = BGFX_PCI_ID_NONE;
    init.callback = &s_bgfxCb;
    init.debug    = true;

    bgfx::PlatformData pd{};
    pd.nwh = nwh;
    init.platformData = pd;

    init.resolution.width  = w;
    init.resolution.height = h;
    init.resolution.reset  = BGFX_RESET_VSYNC;

    return bgfx::init(init);
}

// === Vértice con UV ===
struct PosColorUvVertex {
    float    x, y, z;
    uint32_t abgr;
    float    u, v;
};

Renderer::~Renderer()
{
    Shutdown();  // <-- garantiza liberar incluso si te olvidas
}

void Renderer::Init(void* nwh, uint32_t width, uint32_t height)
{
    if (m_initialized) return;

    m_width = width;
    m_height = height;
    m_type = bgfx::RendererType::Count;

    // **Estabilidad**: fuerza D3D11 por defecto (se puede cambiar con la env var SANDBOXCITY_BACKEND)
    bgfx::RendererType::Enum preferred = bgfx::RendererType::Direct3D11;
    if (const char* b = std::getenv("SANDBOXCITY_BACKEND")) {
        if      (std::string(b) == "d3d12") preferred = bgfx::RendererType::Direct3D12;
        else if (std::string(b) == "gl")    preferred = bgfx::RendererType::OpenGL;
        else                                 preferred = bgfx::RendererType::Direct3D11;
    }

    bool ok = tryInitBackend(nwh, m_width, m_height, preferred);
    if (!ok && preferred != bgfx::RendererType::Direct3D11)
        ok = tryInitBackend(nwh, m_width, m_height, bgfx::RendererType::Direct3D11);
    if (!ok) ok = tryInitBackend(nwh, m_width, m_height, bgfx::RendererType::Count);
    if (!ok) ok = tryInitBackend(nwh, m_width, m_height, bgfx::RendererType::Noop);
    if (!ok) throw std::runtime_error("bgfx::init failed on all backends.");

    m_type = bgfx::getRendererType();

    // Clear + viewport + debug
    bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x88AAFFFF, 1.0f, 0);
    bgfx::setViewRect(0, 0, 0, (uint16_t)m_width, (uint16_t)m_height);

    m_debugFlags = BGFX_DEBUG_TEXT;
    bgfx::setDebug(m_debugFlags);

    // View/proj iniciales
    float ident[16]; bx::mtxIdentity(ident);
    std::memcpy(m_view, ident, sizeof(m_view));
    const float aspect = (m_height > 0) ? float(m_width)/float(m_height) : 16.0f/9.0f;
    SetProjection(60.0f, aspect, 0.1f, 1000.0f);

    // Layout con UV
    m_layout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0,   4, bgfx::AttribType::Uint8, true)
        .add(bgfx::Attrib::TexCoord0,2, bgfx::AttribType::Float)
    .end();

    // Geometrías
    CreateCubeGeometry();
    CreateGroundPlane();

    // Shaders (DX11)
    if (!LoadProgramDx11("vs_basic", "fs_basic")) {
        throw std::runtime_error("No se pudo cargar el programa DX11 (vs_basic/fs_basic).");
    }

    // Uniform sampler + textura
    m_uTexColor = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);

    const std::string assets = detectAssetsBase();
    const std::string texPath = (std::filesystem::path(assets) / "textures" / "checker.png").string();
    m_texChecker = tex::LoadTexture2D(texPath.c_str(), /*hasMips=*/false);

    if (!bgfx::isValid(m_texChecker)) {
        std::printf("[TEX] Fallback procedural checker (2x2)\n");
        m_texChecker = makeFallbackChecker();
    }

    m_initialized = true;
}

void Renderer::Shutdown()
{
    if (!m_initialized) return;

    // Destruye recursos de GPU antes de cerrar bgfx
    if (bgfx::isValid(m_prog))       bgfx::destroy(m_prog);
    if (bgfx::isValid(m_vbh))        bgfx::destroy(m_vbh);
    if (bgfx::isValid(m_ibh))        bgfx::destroy(m_ibh);
    if (bgfx::isValid(m_planeVbh))   bgfx::destroy(m_planeVbh);
    if (bgfx::isValid(m_planeIbh))   bgfx::destroy(m_planeIbh);
    if (bgfx::isValid(m_uTexColor))  bgfx::destroy(m_uTexColor);
    if (bgfx::isValid(m_texChecker)) bgfx::destroy(m_texChecker);

    // Asegura que el worker thread termine el frame
    bgfx::renderFrame();
    bgfx::shutdown();

    // Limpia estados
    m_prog        = BGFX_INVALID_HANDLE;
    m_vbh         = BGFX_INVALID_HANDLE;
    m_ibh         = BGFX_INVALID_HANDLE;
    m_planeVbh    = BGFX_INVALID_HANDLE;
    m_planeIbh    = BGFX_INVALID_HANDLE;
    m_uTexColor   = BGFX_INVALID_HANDLE;
    m_texChecker  = BGFX_INVALID_HANDLE;
    m_initialized = false;
}

void Renderer::OnResize(uint32_t width, uint32_t height)
{
    if (!m_initialized) return;
    if (width == 0 || height == 0) return;   // evita reset con ventana minimizada
    if (width == m_width && height == m_height) return;

    m_width = width; m_height = height; m_pendingReset = true;
}

void Renderer::SetCameraDebugInfo(float x, float y, float z)
{
    m_camX = x; m_camY = y; m_camZ = z;
}

void Renderer::BeginFrame()
{
    if (!m_initialized) return;

    // Si la ventana está minimizada, evita el reset/draw ese frame
    if (m_width == 0 || m_height == 0) {
        bgfx::frame();
        return;
    }

    if (m_pendingReset) {
        bgfx::reset(m_width, m_height, m_resetFlags);
        bgfx::setViewRect(0, 0, 0, (uint16_t)m_width, (uint16_t)m_height);
        m_pendingReset = false;
    }

    bgfx::setViewTransform(0, m_view, m_proj);
    bgfx::touch(0);

    // HUD
    bgfx::dbgTextClear();
    bgfx::dbgTextPrintf(0, 0, 0x0F, "SandboxCity");
    bgfx::dbgTextPrintf(0, 1, 0x0A, "Renderer: %s", GetBackendName());
    bgfx::dbgTextPrintf(0, 2, 0x0B, "FPS: %.1f", Time::FPS());
    bgfx::dbgTextPrintf(0, 3, 0x0E, "Camera: (%.1f, %.1f, %.1f)", m_camX, m_camY, m_camZ);
    bgfx::dbgTextPrintf(0, 4, 0x0C, "Controls: WASD/Mouse, F1=Wireframe(%s), V=VSync(%s)",
        m_wireframe ? "ON" : "OFF", m_vsync ? "ON" : "OFF");

    if (m_type != bgfx::RendererType::Noop && bgfx::isValid(m_prog)) {
        uint64_t state =  BGFX_STATE_WRITE_RGB
                        | BGFX_STATE_WRITE_A
                        | BGFX_STATE_WRITE_Z
                        | BGFX_STATE_DEPTH_TEST_LESS;

        // === PLANO ===
        {
            float mtx[16];
            bx::mtxSRT(mtx, 1.0f,1.0f,1.0f,  0.0f,0.0f,0.0f,  0.0f,0.0f,0.0f);
            bgfx::setTransform(mtx);
            bgfx::setVertexBuffer(0, m_planeVbh);
            bgfx::setIndexBuffer(m_planeIbh);
            if (bgfx::isValid(m_texChecker))
                bgfx::setTexture(0, m_uTexColor, m_texChecker);
            bgfx::setState(state);
            bgfx::submit(0, m_prog);
        }

        // === CUBO ===
        {
            float mtx[16];
            const float t = (float)Time::ElapsedTime();
            bx::mtxSRT(mtx, 1.0f,1.0f,1.0f,  0.0f,t,0.0f,   0.0f,1.0f,-5.0f);
            bgfx::setTransform(mtx);
            bgfx::setVertexBuffer(0, m_vbh);
            bgfx::setIndexBuffer(m_ibh);
            if (bgfx::isValid(m_texChecker))
                bgfx::setTexture(0, m_uTexColor, m_texChecker);
            bgfx::setState(state);
            bgfx::submit(0, m_prog);
        }
    }
}

void Renderer::EndFrame()
{
    if (m_initialized) bgfx::frame();
}

void Renderer::SetView(const float view[16])
{
    std::memcpy(m_view, view, sizeof(m_view));
}

void Renderer::SetProjection(float fovYDeg, float aspect, float znear, float zfar)
{
    const bool homDepth = bgfx::getCaps()->homogeneousDepth;
    bx::mtxProj(m_proj, fovYDeg, aspect, znear, zfar, homDepth);
}

const char* Renderer::GetBackendName() const
{
    switch (m_type) {
    case bgfx::RendererType::Direct3D11: return "Direct3D 11";
    case bgfx::RendererType::Direct3D12: return "Direct3D 12";
    case bgfx::RendererType::OpenGL:     return "OpenGL";
    case bgfx::RendererType::Noop:       return "Noop";
    default:                              return "Unknown";
    }
}

// === Toggles ===
void Renderer::ToggleWireframe() { SetWireframe(!m_wireframe); }
void Renderer::ToggleVsync()     { SetVsync(!m_vsync); }

void Renderer::SetWireframe(bool on)
{
    m_wireframe = on;
    if (m_wireframe)
        m_debugFlags |= BGFX_DEBUG_WIREFRAME;
    else
        m_debugFlags &= ~BGFX_DEBUG_WIREFRAME;

    bgfx::setDebug(m_debugFlags);
}

void Renderer::SetVsync(bool on)
{
    m_vsync = on;
    m_resetFlags &= ~BGFX_RESET_VSYNC;
    if (m_vsync) m_resetFlags |= BGFX_RESET_VSYNC;
    m_pendingReset = true;
}

// === Helpers de shaders/recursos ===
bgfx::ShaderHandle Renderer::LoadShaderFile(const char* path)
{
    std::printf("[DEBUG] Intentando cargar: %s\n", path);
    FILE* f = nullptr;
#ifdef _WIN32
    fopen_s(&f, path, "rb");
#else
    f = fopen(path, "rb");
#endif
    if (!f) {
        std::printf("[ERROR] No se pudo abrir: %s\n", path);
        return BGFX_INVALID_HANDLE;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    const bgfx::Memory* mem = bgfx::alloc((uint32_t)sz);
    fread(mem->data, 1, sz, f);
    fclose(f);

    bgfx::ShaderHandle h = bgfx::createShader(mem);
    std::printf("[DEBUG] Shader %s -> %s\n", path, bgfx::isValid(h) ? "OK" : "FAIL");
    return h;
}

bool Renderer::LoadProgramDx11(const char* vsName, const char* fsName)
{
    std::string base = detectShaderBaseDx11();
    std::filesystem::path vsPath = std::filesystem::path(base) / (std::string(vsName) + ".bin");
    std::filesystem::path fsPath = std::filesystem::path(base) / (std::string(fsName) + ".bin");

    if (!std::filesystem::exists(vsPath) || !std::filesystem::exists(fsPath)) {
        std::printf("[FATAL] Shader(s) no encontrados:\n  %s\n  %s\n",
                    vsPath.string().c_str(), fsPath.string().c_str());
        return false;
    }

    bgfx::ShaderHandle vsh = LoadShaderFile(vsPath.string().c_str());
    bgfx::ShaderHandle fsh = LoadShaderFile(fsPath.string().c_str());
    if (!bgfx::isValid(vsh) || !bgfx::isValid(fsh)) return false;

    m_prog = bgfx::createProgram(vsh, fsh, true);
    return bgfx::isValid(m_prog);
}

// === Geometrías (idénticas a las que ya tienes) ===
void Renderer::CreateCubeGeometry()
{
    const struct PosColorUvVertex { float x,y,z; uint32_t abgr; float u,v; } verts[] = {
        {-1.0f,  1.0f,  1.0f, 0xffff0000, 0.0f, 0.0f},
        { 1.0f,  1.0f,  1.0f, 0xff00ff00, 1.0f, 0.0f},
        {-1.0f, -1.0f,  1.0f, 0xff0000ff, 0.0f, 1.0f},
        { 1.0f, -1.0f,  1.0f, 0xffffff00, 1.0f, 1.0f},
        {-1.0f,  1.0f, -1.0f, 0xffff00ff, 0.0f, 0.0f},
        { 1.0f,  1.0f, -1.0f, 0xff00ffff, 1.0f, 0.0f},
        {-1.0f, -1.0f, -1.0f, 0xffffffff, 0.0f, 1.0f},
        { 1.0f, -1.0f, -1.0f, 0xffff8800, 1.0f, 1.0f},
    };
    const uint16_t indices[] = {
        0,1,2,  1,3,2,
        4,6,5,  5,6,7,
        0,2,4,  4,2,6,
        1,5,3,  5,7,3,
        0,4,1,  1,4,5,
        2,3,6,  3,7,6
    };
    m_vbh = bgfx::createVertexBuffer(bgfx::copy(verts, sizeof(verts)), m_layout);
    m_ibh = bgfx::createIndexBuffer (bgfx::copy(indices, sizeof(indices)));
}

void Renderer::CreateGroundPlane(float halfSize, float uvTiling)
{
    const float hs = halfSize;
    const struct PosColorUvVertex { float x,y,z; uint32_t abgr; float u,v; } verts[] = {
        { -hs, 0.0f,  hs, 0xffcccccc, 0.0f,      uvTiling },
        {  hs, 0.0f,  hs, 0xffcccccc, uvTiling,  uvTiling },
        { -hs, 0.0f, -hs, 0xffcccccc, 0.0f,      0.0f     },
        {  hs, 0.0f, -hs, 0xffcccccc, uvTiling,  0.0f     },
    };
    const uint16_t indices[] = { 0,1,2, 1,3,2 };
    m_planeVbh = bgfx::createVertexBuffer(bgfx::copy(verts, sizeof(verts)), m_layout);
    m_planeIbh = bgfx::createIndexBuffer (bgfx::copy(indices, sizeof(indices)));
}
