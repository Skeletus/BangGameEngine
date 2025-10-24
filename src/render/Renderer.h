#pragma once
#include <bgfx/bgfx.h>
#include <string>

class Renderer
{
public:
    Renderer() = default;
    ~Renderer();                        // <-- asegura cierre

    Renderer(const Renderer&) = delete; // no copiar
    Renderer& operator=(const Renderer&) = delete;

    void Init(void* nwh, uint32_t width, uint32_t height);
    void Shutdown();

    void OnResize(uint32_t width, uint32_t height);

    void BeginFrame();
    void EndFrame();

    void SetView(const float view[16]);
    void SetProjection(float fovYDeg, float aspect, float znear, float zfar);

    void SetCameraDebugInfo(float x, float y, float z);

    const char* GetBackendName() const;

    // Debug toggles
    void ToggleWireframe();
    void ToggleVsync();
    void SetWireframe(bool on);
    void SetVsync(bool on);

private:
    // Shaders / programas
    bgfx::ShaderHandle LoadShaderFile(const char* path);
    bool LoadProgramDx11(const char* vsName, const char* fsName);

    // Geometrías
    void CreateCubeGeometry();
    void CreateGroundPlane(float halfSize = 250.0f, float uvTiling = 50.0f);

    // Estado / backend
    uint32_t    m_width  = 0;
    uint32_t    m_height = 0;
    uint32_t    m_resetFlags = BGFX_RESET_VSYNC;
    uint32_t    m_debugFlags = BGFX_DEBUG_TEXT;
    bool        m_pendingReset = false;

    bool        m_initialized = false;
    bool        m_wireframe   = false;
    bool        m_vsync       = true;

    bgfx::RendererType::Enum m_type = bgfx::RendererType::Count;

    float       m_view[16]{};
    float       m_proj[16]{};
    float       m_camX=0, m_camY=0, m_camZ=0;

    // Layout/Program
    bgfx::VertexLayout m_layout{};
    bgfx::ProgramHandle m_prog = BGFX_INVALID_HANDLE;

    // Recursos: cubo
    bgfx::VertexBufferHandle m_vbh = BGFX_INVALID_HANDLE;
    bgfx::IndexBufferHandle  m_ibh = BGFX_INVALID_HANDLE;

    // Recursos: plano
    bgfx::VertexBufferHandle m_planeVbh = BGFX_INVALID_HANDLE;
    bgfx::IndexBufferHandle  m_planeIbh = BGFX_INVALID_HANDLE;

    // Textura + sampler
    bgfx::UniformHandle m_uTexColor = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_texChecker = BGFX_INVALID_HANDLE;
};
