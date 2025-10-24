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
#include <cmath>

#include "../core/Time.h"
#include "Texture.h"
#include "Material.h"
#include "../asset/Mesh.h"
#include "../asset/ObjLoader.h"

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
        ::TerminateProcess(GetCurrentProcess(), (UINT)code);
#else
        std::abort();
#endif
    }
    void traceVargs(const char*, uint16_t, const char* format, va_list argList) override
    { std::vfprintf(stderr, format, argList); std::fputc('\n', stderr); }

    // stubs requeridos
    void profilerBegin(const char*, uint32_t, const char*, uint16_t) override {}
    void profilerBeginLiteral(const char*, uint32_t, const char*, uint16_t) override {}
    void profilerEnd() override {}
    uint32_t cacheReadSize(uint64_t) override { return 0; }
    bool     cacheRead(uint64_t, void*, uint32_t) override { return false; }
    void     cacheWrite(uint64_t, const void*, uint32_t) override {}
    void screenShot(const char* filePath, uint32_t w, uint32_t h, uint32_t,
                    const void*, uint32_t, bool) override
    { std::fprintf(stderr, "[BGFX] screenShot: %s (%ux%u)\n", filePath?filePath:"(null)", w, h); }
    void captureBegin(uint32_t w, uint32_t h, uint32_t,
                      bgfx::TextureFormat::Enum fmt, bool yflip) override
    { std::fprintf(stderr, "[BGFX] captureBegin %ux%u fmt=%d yflip=%d\n", w, h, int(fmt), int(yflip)); }
    void captureEnd() override { std::fprintf(stderr, "[BGFX] captureEnd\n"); }
    void captureFrame(const void*, uint32_t) override {}
};

static BgfxCb s_bgfxCb;

// ===== Helpers ruta =====
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
    {
        std::filesystem::path base = std::filesystem::path(exeDir()) / "assets";
        if (std::filesystem::exists(base)) {
            std::printf("[ASSETS] Usando carpeta junto al .exe: %s\n", base.string().c_str());
            return base.string();
        }
    }
    {
        std::filesystem::path base = std::filesystem::path(exeDir()) / ".." / ".." / ".." / "assets";
        base = std::filesystem::weakly_canonical(base);
        if (std::filesystem::exists(base)) {
            std::printf("[ASSETS] Usando fallback ../../../assets: %s\n", base.string().c_str());
            return base.string();
        }
    }
    {
        std::filesystem::path base = std::filesystem::path(exeDir()) / ".." / ".." / "assets";
        base = std::filesystem::weakly_canonical(base);
        if (std::filesystem::exists(base)) {
            std::printf("[ASSETS] Usando fallback ../../assets: %s\n", base.string().c_str());
            return base.string();
        }
    }
    std::printf("[ASSETS] ERROR: No se encontró carpeta 'assets'\n");
    return "assets";
}

static bgfx::TextureHandle makeFallbackChecker()
{
    const uint8_t pix[] = {
        255,255,255,255,  64,64,64,255,
         64,64,64,255,   255,255,255,255
    };
    const bgfx::Memory* mem = bgfx::copy(pix, sizeof(pix));
    return bgfx::createTexture2D(2, 2, false, 1, bgfx::TextureFormat::RGBA8, BGFX_TEXTURE_NONE, mem);
}

static bool tryInitBackend(void* nwh, uint32_t w, uint32_t h, bgfx::RendererType::Enum type)
{
    bgfx::renderFrame();

    bgfx::Init init{};
    init.type     = type;
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

// ===== Vértice con normal =====
struct PosNormColorUvVertex {
    float    x, y, z;
    float    nx, ny, nz;
    uint32_t abgr;
    float    u, v;
};

Renderer::~Renderer() { Shutdown(); }

void Renderer::Init(void* nwh, uint32_t width, uint32_t height)
{
    if (m_initialized) return;

    m_width = width;
    m_height = height;
    m_type = bgfx::RendererType::Count;

    bgfx::RendererType::Enum preferred = bgfx::RendererType::Direct3D11;
    if (const char* b = std::getenv("SANDBOXCITY_BACKEND")) {
        if      (std::string(b) == "d3d12") preferred = bgfx::RendererType::Direct3D12;
        else if (std::string(b) == "gl")    preferred = bgfx::RendererType::OpenGL;
    }

    bool ok = tryInitBackend(nwh, m_width, m_height, preferred);
    if (!ok && preferred != bgfx::RendererType::Direct3D11)
        ok = tryInitBackend(nwh, m_width, m_height, bgfx::RendererType::Direct3D11);
    if (!ok) ok = tryInitBackend(nwh, m_width, m_height, bgfx::RendererType::Count);
    if (!ok) ok = tryInitBackend(nwh, m_width, m_height, bgfx::RendererType::Noop);
    if (!ok) throw std::runtime_error("bgfx::init failed on all backends.");

    m_type = bgfx::getRendererType();

    bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x88AAFFFF, 1.0f, 0);
    bgfx::setViewRect(0, 0, 0, (uint16_t)m_width, (uint16_t)m_height);

    m_debugFlags = BGFX_DEBUG_TEXT;
    bgfx::setDebug(m_debugFlags);

    float ident[16]; bx::mtxIdentity(ident);
    std::memcpy(m_view, ident, sizeof(m_view));
    const float aspect = (m_height > 0) ? float(m_width)/float(m_height) : 16.0f/9.0f;
    SetProjection(60.0f, aspect, 0.1f, 1000.0f);

    // Layout con normal
    m_layout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Normal,   3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0,   4, bgfx::AttribType::Uint8, true)
        .add(bgfx::Attrib::TexCoord0,2, bgfx::AttribType::Float)
    .end();

    CreateCubeGeometry();
    CreateGroundPlane();

    if (!LoadProgramDx11("vs_basic", "fs_basic")) {
        throw std::runtime_error("No se pudo cargar el programa DX11 (vs_basic/fs_basic).");
    }

    // Uniforms
    m_uTexColor   = bgfx::createUniform("s_texColor",   bgfx::UniformType::Sampler);
    m_uLightDir   = bgfx::createUniform("u_lightDir",   bgfx::UniformType::Vec4);
    m_uLightColor = bgfx::createUniform("u_lightColor", bgfx::UniformType::Vec4);
    m_uAmbient    = bgfx::createUniform("u_ambient",    bgfx::UniformType::Vec4);

    m_uNormalMtx  = bgfx::createUniform("u_normalMtx",  bgfx::UniformType::Mat4);
    m_uCameraPos  = bgfx::createUniform("u_cameraPos",  bgfx::UniformType::Vec4);
    m_uSpecParams = bgfx::createUniform("u_specParams", bgfx::UniformType::Vec4);
    m_uSpecColor  = bgfx::createUniform("u_specColor",  bgfx::UniformType::Vec4);
    m_uBaseTint   = bgfx::createUniform("u_baseTint",   bgfx::UniformType::Vec4);
    m_uUvScale    = bgfx::createUniform("u_uvScale",    bgfx::UniformType::Vec4);

    // Textura por defecto
    const std::string assets = detectAssetsBase();
    const std::string texPath = (std::filesystem::path(assets) / "textures" / "checker.png").string();
    m_texChecker = tex::LoadTexture2D(texPath.c_str(), /*hasMips=*/false);
    if (!bgfx::isValid(m_texChecker)) {
        std::printf("[TEX] Fallback procedural checker (2x2)\n");
        m_texChecker = makeFallbackChecker();
    }

    // Estado inicial de iluminación (editable con teclas)
    ResetLightingDefaults();

    // Intenta cargar un OBJ por defecto (si no existe, seguimos con el cubo)
    const std::string defaultObj = (std::filesystem::path(assets) / "models" / "demo.obj").string();
    m_objLoaded = TryLoadObj(defaultObj);

    m_initialized = true;
}

void Renderer::Shutdown()
{
    if (!m_initialized) return;

    if (bgfx::isValid(m_prog))       bgfx::destroy(m_prog);
    if (bgfx::isValid(m_vbh))        bgfx::destroy(m_vbh);
    if (bgfx::isValid(m_ibh))        bgfx::destroy(m_ibh);
    if (bgfx::isValid(m_planeVbh))   bgfx::destroy(m_planeVbh);
    if (bgfx::isValid(m_planeIbh))   bgfx::destroy(m_planeIbh);
    if (bgfx::isValid(m_uTexColor))  bgfx::destroy(m_uTexColor);
    if (bgfx::isValid(m_texChecker)) bgfx::destroy(m_texChecker);

    if (bgfx::isValid(m_uLightDir))   bgfx::destroy(m_uLightDir);
    if (bgfx::isValid(m_uLightColor)) bgfx::destroy(m_uLightColor);
    if (bgfx::isValid(m_uAmbient))    bgfx::destroy(m_uAmbient);

    if (bgfx::isValid(m_uNormalMtx))  bgfx::destroy(m_uNormalMtx);
    if (bgfx::isValid(m_uCameraPos))  bgfx::destroy(m_uCameraPos);
    if (bgfx::isValid(m_uSpecParams)) bgfx::destroy(m_uSpecParams);
    if (bgfx::isValid(m_uSpecColor))  bgfx::destroy(m_uSpecColor);

    if (bgfx::isValid(m_uBaseTint))  bgfx::destroy(m_uBaseTint);
    if (bgfx::isValid(m_uUvScale))   bgfx::destroy(m_uUvScale);

    // OBJ y material
    m_objMesh.destroy();
    for (Material& mat : m_objMaterials) {
        mat.destroy();
    }
    m_objMaterials.clear();
    m_objSubsets.clear();
    m_objLoaded = false;

    bgfx::renderFrame();
    bgfx::shutdown();

    m_prog        = BGFX_INVALID_HANDLE;
    m_vbh         = BGFX_INVALID_HANDLE;
    m_ibh         = BGFX_INVALID_HANDLE;
    m_planeVbh    = BGFX_INVALID_HANDLE;
    m_planeIbh    = BGFX_INVALID_HANDLE;
    m_uTexColor   = BGFX_INVALID_HANDLE;
    m_texChecker  = BGFX_INVALID_HANDLE;
    m_uLightDir   = BGFX_INVALID_HANDLE;
    m_uLightColor = BGFX_INVALID_HANDLE;
    m_uAmbient    = BGFX_INVALID_HANDLE;
    m_uNormalMtx  = BGFX_INVALID_HANDLE;
    m_uCameraPos  = BGFX_INVALID_HANDLE;
    m_uSpecParams = BGFX_INVALID_HANDLE;
    m_uSpecColor  = BGFX_INVALID_HANDLE;
    m_uBaseTint   = BGFX_INVALID_HANDLE;
    m_uUvScale    = BGFX_INVALID_HANDLE;

    m_initialized = false;
}

void Renderer::OnResize(uint32_t width, uint32_t height)
{
    if (!m_initialized) return;
    if (width == 0 || height == 0) return;
    if (width == m_width && height == m_height) return;

    m_width = width; m_height = height; m_pendingReset = true;
}

void Renderer::SetCameraDebugInfo(float x, float y, float z)
{
    m_camX = x; m_camY = y; m_camZ = z;
}

// Helpers
static inline float clampf(float v, float a, float b) {
    return v < a ? a : (v > b ? b : v);
}

void Renderer::ResetLightingDefaults()
{
    m_lightYaw   = bx::toRad(150.0f);
    m_lightPitch = bx::toRad(-60.0f);
    m_ambient        = 0.5f;
    m_specIntensity  = 0.35f;
    m_shininess      = 32.0f;
    m_lightColor3[0] = m_lightColor3[1] = m_lightColor3[2] = 1.0f;
}

void Renderer::AddLightYawPitch(float dyawRad, float dpitchRad)
{
    m_lightYaw += dyawRad;
    m_lightPitch += dpitchRad;
    const float minPitch = bx::toRad(-89.0f);
    const float maxPitch = bx::toRad(-5.0f);
    m_lightPitch = clampf(m_lightPitch, minPitch, maxPitch);
}

void Renderer::AdjustAmbient(float delta)       { m_ambient       = clampf(m_ambient + delta, 0.0f, 1.0f); }
void Renderer::AdjustSpecIntensity(float delta) { m_specIntensity = clampf(m_specIntensity + delta, 0.0f, 1.0f); }
void Renderer::AdjustShininess(float delta)     { m_shininess     = clampf(m_shininess + delta, 2.0f, 256.0f); }

// === Material v1 ===
void Renderer::ApplyMaterial(const Material& m)
{
    const bgfx::TextureHandle tex = bgfx::isValid(m.albedo) ? m.albedo : m_texChecker;

    bgfx::setTexture(0, m_uTexColor, tex);
    bgfx::setUniform(m_uBaseTint,   m.baseTint);
    bgfx::setUniform(m_uUvScale,    m.uvScale);
    bgfx::setUniform(m_uSpecParams, m.specParams);
    bgfx::setUniform(m_uSpecColor,  m.specColor);
}

// === OBJ loader (intento) ===
bool Renderer::TryLoadObj(const std::string& path)
{
    std::string log;
    Mesh mesh;
    std::vector<Material> materials;
    std::vector<MeshSubset> subsets;

    const bool ok = asset::LoadObjToMesh(path, m_layout, m_texChecker, mesh, materials, subsets, &log, /*flipV=*/true);

    if (!ok) {
        if (!log.empty()) std::printf("[OBJ] %s\n", log.c_str());
        return false;
    }

    // Reemplaza si ya había
    m_objMesh.destroy();
    for (Material& m : m_objMaterials) {
        m.destroy();
    }
    m_objMaterials.clear();
    m_objSubsets.clear();

    m_objMesh = mesh;
    m_objMaterials = std::move(materials);
    m_objSubsets   = std::move(subsets);
    m_objLoaded    = !m_objSubsets.empty();

    std::printf("[OBJ] Cargado OK: %s  (indices: %u)\n", path.c_str(), m_objMesh.indexCount);
    return true;
}

void Renderer::BeginFrame()
{
    if (!m_initialized) return;

    if (m_width == 0 || m_height == 0) { bgfx::frame(); return; }

    if (m_pendingReset) {
        bgfx::reset(m_width, m_height, m_resetFlags);
        bgfx::setViewRect(0, 0, 0, (uint16_t)m_width, (uint16_t)m_height);
        m_pendingReset = false;
    }

    bgfx::setViewTransform(0, m_view, m_proj);
    bgfx::touch(0);

    // Dirección de luz desde yaw/pitch
    const float cy = std::cos(m_lightYaw);
    const float sy = std::sin(m_lightYaw);
    const float cp = std::cos(m_lightPitch);
    const float sp = std::sin(m_lightPitch);
    const float lightDirV[4] = { cy*cp, sp, sy*cp, 0.0f };

    // Uniforms comunes de luz/cámara
    const float lightColor[4] = { m_lightColor3[0], m_lightColor3[1], m_lightColor3[2], 0.0f };
    const float ambient[4]    = { m_ambient, m_ambient, m_ambient, 0.0f };
    const float camPos[4]     = { m_camX, m_camY, m_camZ, 0.0f };

    // HUD
    bgfx::dbgTextClear();
    bgfx::dbgTextPrintf(0, 0, 0x0F, "SandboxCity");
    bgfx::dbgTextPrintf(0, 1, 0x0A, "Renderer: %s", GetBackendName());
    bgfx::dbgTextPrintf(0, 2, 0x0B, "FPS: %.1f", Time::FPS());
    bgfx::dbgTextPrintf(0, 3, 0x0E, "Camera: (%.1f, %.1f, %.1f)", m_camX, m_camY, m_camZ);
    bgfx::dbgTextPrintf(0, 4, 0x0C, "Controls: WASD/Mouse, F1=Wireframe(%s), V=VSync(%s)",
        m_wireframe ? "ON" : "OFF", m_vsync ? "ON" : "OFF");
    bgfx::dbgTextPrintf(0, 5, 0x0A, "Light yaw/pitch: %.1f/%.1f deg | Ambient: %.2f | SpecI: %.2f | Shiny: %.0f",
        bx::toDeg(m_lightYaw), bx::toDeg(m_lightPitch), m_ambient, m_specIntensity, m_shininess);
    bgfx::dbgTextPrintf(0, 6, 0x08, "Arrow keys: rotate light | Z/X ambient -/+ | C/V spec -/+ | B/N shiny -/+ | R reset");

    if (m_type != bgfx::RendererType::Noop && bgfx::isValid(m_prog)) {
        const uint64_t state =  BGFX_STATE_WRITE_RGB
                              | BGFX_STATE_WRITE_A
                              | BGFX_STATE_WRITE_Z
                              | BGFX_STATE_DEPTH_TEST_LESS;

        // Construye materiales por draw (usan los parámetros runtime)
        Material planeMat{};
        planeMat.albedo = m_texChecker;
        planeMat.baseTint[0]=planeMat.baseTint[1]=planeMat.baseTint[2]=1.0f; planeMat.baseTint[3]=1.0f;
        planeMat.uvScale[0]=planeMat.uvScale[1]=1.0f;
        planeMat.specParams[0] = m_shininess;
        planeMat.specParams[1] = m_specIntensity;
        planeMat.specColor[0] = planeMat.specColor[1] = planeMat.specColor[2] = 1.0f; planeMat.specColor[3]=0.0f;

        Material cubeMat = planeMat; // mismo material base por ahora

        // === PLANO ===
        {
            float model[16]; bx::mtxSRT(model, 1,1,1, 0,0,0, 0,0,0);
            float invModel[16], normalMtx[16];
            bx::mtxInverse(invModel, model);
            bx::mtxTranspose(normalMtx, invModel);

            bgfx::setTransform(model);
            bgfx::setVertexBuffer(0, m_planeVbh);
            bgfx::setIndexBuffer(m_planeIbh);

            // Iluminación común
            bgfx::setUniform(m_uLightDir,   lightDirV);
            bgfx::setUniform(m_uLightColor, lightColor);
            bgfx::setUniform(m_uAmbient,    ambient);
            bgfx::setUniform(m_uCameraPos,  camPos);
            bgfx::setUniform(m_uNormalMtx,  normalMtx);

            // Material v1
            ApplyMaterial(planeMat);

            bgfx::setState(state);
            bgfx::submit(0, m_prog);
        }

        // === OBJ o CUBO ===
        {
            float model[16];
            const float t = (float)Time::ElapsedTime();
            bx::mtxSRT(model, 1,1,1, 0,t,0,  0,1,-5);

            float invModel[16], normalMtx[16];
            bx::mtxInverse(invModel, model);
            bx::mtxTranspose(normalMtx, invModel);

            bgfx::setTransform(model);
            bgfx::setUniform(m_uNormalMtx,  normalMtx);

            // Iluminación común
            bgfx::setUniform(m_uLightDir,   lightDirV);
            bgfx::setUniform(m_uLightColor, lightColor);
            bgfx::setUniform(m_uAmbient,    ambient);
            bgfx::setUniform(m_uCameraPos,  camPos);

            if (m_objMesh.valid() && !m_objSubsets.empty()) {
                for (const MeshSubset& subset : m_objSubsets) {
                    if (subset.indexCount == 0) {
                        continue;
                    }

                    bgfx::setVertexBuffer(0, m_objMesh.vbh);
                    bgfx::setIndexBuffer(m_objMesh.ibh, subset.startIndex, subset.indexCount);

                    // Iluminación común
                    bgfx::setUniform(m_uLightDir,   lightDirV);
                    bgfx::setUniform(m_uLightColor, lightColor);
                    bgfx::setUniform(m_uAmbient,    ambient);
                    bgfx::setUniform(m_uCameraPos,  camPos);

                    Material drawMat = cubeMat;
                    if (subset.materialIndex >= 0 && subset.materialIndex < (int)m_objMaterials.size()) {
                        drawMat = m_objMaterials[subset.materialIndex];
                    }

                    drawMat.specParams[0] = m_shininess;
                    drawMat.specParams[1] = m_specIntensity;
                    drawMat.specColor[0] = drawMat.specColor[1] = drawMat.specColor[2] = 1.0f; drawMat.specColor[3]=0.0f;
                    ApplyMaterial(drawMat);
                    bgfx::setState(state);
                    bgfx::submit(0, m_prog);
                }
            } else {
                // Fallback cubo
                bgfx::setVertexBuffer(0, m_vbh);
                bgfx::setIndexBuffer(m_ibh);

                ApplyMaterial(cubeMat);
                
                bgfx::setState(state);
                bgfx::submit(0, m_prog);

            }
            
        }
    }
}

void Renderer::EndFrame()
{
    if (m_initialized) bgfx::frame();
}

void Renderer::SetView(const float view[16]) { std::memcpy(m_view, view, sizeof(m_view)); }

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

void Renderer::ToggleWireframe() { SetWireframe(!m_wireframe); }
void Renderer::ToggleVsync()     { SetVsync(!m_vsync); }

void Renderer::SetWireframe(bool on)
{
    m_wireframe = on;
    if (m_wireframe) m_debugFlags |= BGFX_DEBUG_WIREFRAME;
    else             m_debugFlags &= ~BGFX_DEBUG_WIREFRAME;
    bgfx::setDebug(m_debugFlags);
}

void Renderer::SetVsync(bool on)
{
    m_vsync = on;
    m_resetFlags &= ~BGFX_RESET_VSYNC;
    if (m_vsync) m_resetFlags |= BGFX_RESET_VSYNC;
    m_pendingReset = true;
}

// === Carga shaders ===
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

// === Geometrías ===
void Renderer::CreateCubeGeometry()
{
    const float s = 1.0f;
    const PosNormColorUvVertex verts[] =
    {
        // x, y, z,      nx, ny, nz,                  color,      u, v
        { -s,  s,  s,  -0.577f, 0.577f, 0.577f, 0xffffffff, 0.0f, 0.0f },
        {  s,  s,  s,   0.577f, 0.577f, 0.577f, 0xffffffff, 1.0f, 0.0f },
        { -s, -s,  s,  -0.577f,-0.577f, 0.577f, 0xffffffff, 0.0f, 1.0f },
        {  s, -s,  s,   0.577f,-0.577f, 0.577f, 0xffffffff, 1.0f, 1.0f },
        { -s,  s, -s,  -0.577f, 0.577f,-0.577f, 0xffffffff, 0.0f, 0.0f },
        {  s,  s, -s,   0.577f, 0.577f,-0.577f, 0xffffffff, 1.0f, 0.0f },
        { -s, -s, -s,  -0.577f,-0.577f,-0.577f, 0xffffffff, 0.0f, 1.0f },
        {  s, -s, -s,   0.577f,-0.577f,-0.577f, 0xffffffff, 1.0f, 1.0f },
    };

    const uint16_t indices[] =
    {
        0,1,2,  1,3,2, // +Z
        4,6,5,  5,6,7, // -Z
        0,2,4,  4,2,6, // -X
        1,5,3,  5,7,3, // +X
        0,4,1,  1,4,5, // +Y
        2,3,6,  3,7,6  // -Y
    };

    m_vbh = bgfx::createVertexBuffer(bgfx::copy(verts, sizeof(verts)), m_layout);
    m_ibh = bgfx::createIndexBuffer (bgfx::copy(indices, sizeof(indices)));
}

void Renderer::CreateGroundPlane(float halfSize, float uvTiling)
{
    const float hs = halfSize;

    const PosNormColorUvVertex verts[] =
    {
        { -hs, 0.0f,  hs,   0.0f, 1.0f, 0.0f, 0xffffffff, 0.0f,      uvTiling },
        {  hs, 0.0f,  hs,   0.0f, 1.0f, 0.0f, 0xffffffff, uvTiling,  uvTiling },
        { -hs, 0.0f, -hs,   0.0f, 1.0f, 0.0f, 0xffffffff, 0.0f,      0.0f     },
        {  hs, 0.0f, -hs,   0.0f, 1.0f, 0.0f, 0xffffffff, uvTiling,  0.0f     },
    };
    const uint16_t indices[] = { 0,1,2, 1,3,2 };

    m_planeVbh = bgfx::createVertexBuffer(bgfx::copy(verts, sizeof(verts)), m_layout);
    m_planeIbh = bgfx::createIndexBuffer (bgfx::copy(indices, sizeof(indices)));
}
