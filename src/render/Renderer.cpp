#include "Renderer.h"
#include <bgfx/platform.h>
#include <bx/math.h>
#include <stdexcept>
#include <cstdint>
#include <array>
#include <cstring>   // std::memcpy
#include <cstdio>    // fopen/fread
#include <string>
#include "../core/Time.h"
#include <filesystem>
#ifdef _WIN32
  #include <windows.h>
#endif


static bool tryInitBackend(void* nwh, uint32_t w, uint32_t h, bgfx::RendererType::Enum type) {
    // Sincroniza cualquier hilo previo de bgfx (modo single-thread friendly)
    bgfx::renderFrame();

    bgfx::Init init{};
    init.type     = type;
    init.vendorId = BGFX_PCI_ID_NONE;

    // HWND via platformData dentro de init (preferido)
    bgfx::PlatformData pd{};
    pd.nwh = nwh;
    pd.ndt = nullptr;
    pd.context = nullptr;
    pd.backBuffer = nullptr;
    pd.backBufferDS = nullptr;
    init.platformData = pd;

    init.resolution.width  = w;
    init.resolution.height = h;
    init.resolution.reset  = BGFX_RESET_VSYNC;

    return bgfx::init(init);
}

struct PosColorVertex {
    float    x, y, z;
    uint32_t abgr;
};

void Renderer::Init(void* nwh, uint32_t width, uint32_t height) {
    if (m_initialized) return;

    m_width = width;
    m_height = height;
    m_type = bgfx::RendererType::Count;

    // En Windows: D3D11 -> D3D12 -> OpenGL
    const std::array<bgfx::RendererType::Enum, 3> prefs = {
        bgfx::RendererType::Direct3D11,
        bgfx::RendererType::Direct3D12,
        bgfx::RendererType::OpenGL
    };

    bool ok = false;
    for (auto t : prefs) {
        if (tryInitBackend(nwh, m_width, m_height, t)) { m_type = t; ok = true; break; }
    }
    if (!ok && tryInitBackend(nwh, m_width, m_height, bgfx::RendererType::Count)) {
        m_type = bgfx::getRendererType(); ok = true;
    }
    if (!ok && tryInitBackend(nwh, m_width, m_height, bgfx::RendererType::Noop)) {
        m_type = bgfx::RendererType::Noop; ok = true;
    }
    if (!ok) throw std::runtime_error("bgfx::init failed on all backends.");

    bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x88AAFFFF, 1.0f, 0);
    bgfx::setViewRect(0, 0, 0, (uint16_t)m_width, (uint16_t)m_height);
    bgfx::setDebug(BGFX_DEBUG_TEXT);

    // View por defecto (identidad) y proyección inicial
    float ident[16]; bx::mtxIdentity(ident);
    std::memcpy(m_view, ident, sizeof(m_view));
    const float aspect = (m_height > 0) ? float(m_width)/float(m_height) : 16.0f/9.0f;
    SetProjection(60.0f, aspect, 0.1f, 1000.0f);

    // === recursos del cubo ===
    CreateCubeGeometry();
    // Carga el programa desde ./shaders/dx11/*.bin (compilados por CMake)
    if (m_type == bgfx::RendererType::Direct3D11 || m_type == bgfx::RendererType::Direct3D12) {
        if (!LoadProgramDx11("vs_basic", "fs_basic")) {
            throw std::runtime_error("No se pudo cargar el programa DX11 (vs_basic/fs_basic).");
        }
    } else if (m_type == bgfx::RendererType::OpenGL) {
        // Si usas OpenGL, compila también glsl y cambia aquí a LoadProgramGlsl(...)
        throw std::runtime_error("Shaders sólo DX11 por ahora. Compila glsl si vas a usar OpenGL.");
    } else if (m_type == bgfx::RendererType::Noop) {
        // No dibujará, pero el resto corre
    }

    m_initialized = true;
}

void Renderer::Shutdown() {
    if (m_initialized) {
        if (bgfx::isValid(m_prog)) bgfx::destroy(m_prog);
        if (bgfx::isValid(m_vbh))  bgfx::destroy(m_vbh);
        if (bgfx::isValid(m_ibh))  bgfx::destroy(m_ibh);
        bgfx::shutdown();
        m_initialized = false;
    }
}

void Renderer::OnResize(uint32_t width, uint32_t height) {
    if (!m_initialized) return;
    if (width == 0 || height == 0) return;
    if (width == m_width && height == m_height) return;

    m_width = width; m_height = height; m_pendingReset = true;
}

void Renderer::SetCameraDebugInfo(float x, float y, float z) {
    m_camX = x; m_camY = y; m_camZ = z;
}

void Renderer::BeginFrame() {
    if (!m_initialized) return;

    if (m_pendingReset) {
        bgfx::reset(m_width, m_height, BGFX_RESET_VSYNC);
        bgfx::setViewRect(0, 0, 0, (uint16_t)m_width, (uint16_t)m_height);
        m_pendingReset = false;
    }

    // Aplica matrices
    static int debugFrame = 0;
    if (debugFrame++ % 120 == 0) {  // Cada 2 segundos
        printf("[DEBUG] View matrix: [%.2f, %.2f, %.2f, %.2f]\n", 
            m_view[0], m_view[1], m_view[2], m_view[3]);
        printf("[DEBUG] Proj matrix: [%.2f, %.2f, %.2f, %.2f]\n", 
            m_proj[0], m_proj[1], m_proj[2], m_proj[3]);
    }
    bgfx::setViewTransform(0, m_view, m_proj);
    bgfx::touch(0);

    // Overlay
    bgfx::dbgTextClear();
    bgfx::dbgTextPrintf(0, 0, 0x0F, "SandboxCity");
    bgfx::dbgTextPrintf(0, 1, 0x0A, "Renderer: %s", GetBackendName());
    bgfx::dbgTextPrintf(0, 2, 0x0B, "FPS: %.1f", Time::FPS());
    bgfx::dbgTextPrintf(0, 3, 0x0E, "Camera: (%.1f, %.1f, %.1f)", m_camX, m_camY, m_camZ);
    bgfx::dbgTextPrintf(0, 4, 0x0C, "Controls: WASD=Move, Mouse=Look, ESC=Unlock");

    // === DIBUJO DEL CUBO ===
    if (m_type != bgfx::RendererType::Noop && bgfx::isValid(m_prog)) {
        // *** FUERZA el viewport ***
        bgfx::setViewRect(0, 0, 0, (uint16_t)m_width, (uint16_t)m_height);
        printf("[DEBUG] Viewport: %d x %d\n", m_width, m_height);
        
        // *** FUERZA scissor OFF ***
        bgfx::setViewScissor(0, 0, 0, 0, 0);

        // Modelo: rotación con el tiempo (Y)
        float mtx[16];
        const float t = (float)Time::ElapsedTime();
        bx::mtxSRT(mtx,
            1.0f, 1.0f, 1.0f,  // ← ENORME (era 3.0f)
            0.0f, t, 0.0f,
            0.0f, 0.0f, -5.0f);   // ← Más cerca de la cámara (Z negativo)

        // Buffers + estado
        bgfx::setTransform(mtx);  // <- ¡IMPRESCINDIBLE!
        bgfx::setVertexBuffer(0, m_vbh);
        bgfx::setIndexBuffer(m_ibh);
        // *** ESTADO MÍNIMO - TODO visible ***
        bgfx::setState(
                BGFX_STATE_WRITE_RGB
            | BGFX_STATE_WRITE_A
            | BGFX_STATE_WRITE_Z
            | BGFX_STATE_DEPTH_TEST_LESS
            // sin culling para evitar que una orientación rara te “desaparezca” caras
        );



        // Submit
        bgfx::submit(0, m_prog);

        // *** DEBUG ***
        printf("[RENDER] Cubo dibujado en frame\n");
    }
}

void Renderer::EndFrame() { if (m_initialized) bgfx::frame(); }

void Renderer::SetView(const float view[16]) { std::memcpy(m_view, view, sizeof(m_view)); }

void Renderer::SetProjection(float fovYDeg, float aspect, float znear, float zfar) {
    const bool homDepth = bgfx::getCaps()->homogeneousDepth;
    bx::mtxProj(m_proj, fovYDeg, aspect, znear, zfar, homDepth);
}

// === Helpers ===

static std::string exeDir()
{
#ifdef _WIN32
    char path[MAX_PATH]{0};
    DWORD n = GetModuleFileNameA(nullptr, path, MAX_PATH);
    if (n == 0 || n == MAX_PATH) return std::string{};
    std::filesystem::path p(path);
    return p.parent_path().string(); // carpeta del .exe
#else
    return std::filesystem::current_path().string();
#endif
}

static std::string detectShaderBaseDx11()
{
    // 1) Permite override por variable de entorno
    if (const char* env = std::getenv("SANDBOXCITY_SHADER_DIR"))
    {
        std::filesystem::path base(env);
        if (std::filesystem::exists(base))
        {
            printf("[SHADERS] Usando SANDBOXCITY_SHADER_DIR: %s\n", base.string().c_str());
            return base.string();
        }
        else
        {
            printf("[SHADERS] SANDBOXCITY_SHADER_DIR no existe: %s\n", base.string().c_str());
        }
    }

    // 2) Carpeta al lado del .exe (build/bin/<Config>/shaders/dx11)
    {
        std::filesystem::path base = std::filesystem::path(exeDir()) / "shaders" / "dx11";
        if (std::filesystem::exists(base))
        {
            printf("[SHADERS] Usando carpeta junto al .exe: %s\n", base.string().c_str());
            return base.string();
        }
    }

    // 3) Fallback útil si ejecutas desde la raíz del repo y los .bin están en build/bin/<Config>/…
    {
        std::filesystem::path base = std::filesystem::path(exeDir()) / ".." / ".." / "shaders" / "dx11";
        base = std::filesystem::weakly_canonical(base);
        if (std::filesystem::exists(base))
        {
            printf("[SHADERS] Usando fallback ../../shaders/dx11: %s\n", base.string().c_str());
            return base.string();
        }
    }

    // 4) Último recurso: ruta relativa simple (no recomendado)
    printf("[SHADERS] ADVERTENCIA: usando ruta relativa 'shaders/dx11' (puede fallar)\n");
    return "shaders/dx11";
}


bgfx::ShaderHandle Renderer::LoadShaderFile(const char* path) {
    printf("[DEBUG] Intentando cargar: %s\n", path);
    
    FILE* f = nullptr;
#ifdef _WIN32
    fopen_s(&f, path, "rb");
#else
    f = fopen(path, "rb");
#endif
    if (!f) {
        printf("[ERROR] No se pudo abrir: %s\n", path);
        return BGFX_INVALID_HANDLE;
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    printf("[DEBUG] Tamaño del archivo: %ld bytes\n", sz);

    const bgfx::Memory* mem = bgfx::alloc((uint32_t)sz + 1);
    if (fread(mem->data, 1, sz, f) != (size_t)sz) {
        fclose(f);
        printf("[ERROR] Fallo al leer el archivo\n");
        return BGFX_INVALID_HANDLE;
    }
    fclose(f);
    mem->data[sz] = '\0';

    bgfx::ShaderHandle handle = bgfx::createShader(mem);
    printf("[DEBUG] Handle válido: %s\n", bgfx::isValid(handle) ? "SI" : "NO");
    
    return handle;
}

bool Renderer::LoadProgramDx11(const char* vsName, const char* fsName) {
    std::string base = detectShaderBaseDx11();
    std::filesystem::path vsPath = std::filesystem::path(base) / (std::string(vsName) + ".bin");
    std::filesystem::path fsPath = std::filesystem::path(base) / (std::string(fsName) + ".bin");

    bgfx::ShaderHandle vsh = LoadShaderFile(vsPath.string().c_str());
    bgfx::ShaderHandle fsh = LoadShaderFile(fsPath.string().c_str());

    printf("[DEBUG] vsh válido: %s\n", bgfx::isValid(vsh) ? "SI" : "NO");
    printf("[DEBUG] fsh válido: %s\n", bgfx::isValid(fsh) ? "SI" : "NO");

    if (!bgfx::isValid(vsh) || !bgfx::isValid(fsh)) {
        printf("[ERROR] Uno de los shaders no es válido\n");
        return false;
    }

    m_prog = bgfx::createProgram(vsh, fsh, true);
    printf("[DEBUG] Programa válido: %s\n", bgfx::isValid(m_prog) ? "SI" : "NO");
    return bgfx::isValid(m_prog);
}


void Renderer::CreateCubeGeometry() {
    // Layout: posición (float3) + color (rgba8)
    m_layout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0,   4, bgfx::AttribType::Uint8, true) // normalized
    .end();

    // 8 vértices del cubo (colores BRILLANTES)
    const PosColorVertex verts[] = {
        {-1.0f,  1.0f,  1.0f, 0xffff0000}, // 0 - ROJO
        { 1.0f,  1.0f,  1.0f, 0xff00ff00}, // 1 - VERDE
        {-1.0f, -1.0f,  1.0f, 0xff0000ff}, // 2 - AZUL
        { 1.0f, -1.0f,  1.0f, 0xffffff00}, // 3 - CIAN
        {-1.0f,  1.0f, -1.0f, 0xffff00ff}, // 4 - MAGENTA
        { 1.0f,  1.0f, -1.0f, 0xff00ffff}, // 5 - AMARILLO
        {-1.0f, -1.0f, -1.0f, 0xffffffff}, // 6 - BLANCO
        { 1.0f, -1.0f, -1.0f, 0xffff8800}, // 7 - NARANJA
    };

    // 36 índices (12 triángulos, 6 caras * 2)
    const uint16_t indices[] = {
        0,1,2,  1,3,2, // +Z
        4,6,5,  5,6,7, // -Z
        0,2,4,  4,2,6, // -X
        1,5,3,  5,7,3, // +X
        0,4,1,  1,4,5, // +Y
        2,3,6,  3,7,6  // -Y
    };

    const bgfx::Memory* vmem = bgfx::copy(verts, sizeof(verts));
    const bgfx::Memory* imem = bgfx::copy(indices, sizeof(indices));
    m_vbh = bgfx::createVertexBuffer(vmem, m_layout);
    m_ibh = bgfx::createIndexBuffer(imem);
}
