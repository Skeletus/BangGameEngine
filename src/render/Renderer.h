#pragma once
#include <cstdint>
#include <bgfx/bgfx.h>

class Renderer {
public:
    Renderer() = default;
    ~Renderer() = default;

    void Init(void* nwh, uint32_t width, uint32_t height);
    void Shutdown();

    void OnResize(uint32_t width, uint32_t height);

    void BeginFrame();
    void EndFrame();

    // View/Projection
    void SetView(const float view[16]);
    void SetProjection(float fovYDeg, float aspect, float znear, float zfar);

    bgfx::RendererType::Enum GetType() const { return m_type; }
    const char* GetBackendName() const { return bgfx::getRendererName(m_type); }

    void SetCameraDebugInfo(float x, float y, float z);

private:
    // helpers internos
    bool LoadProgramDx11(const char* vsName, const char* fsName);
    static bgfx::ShaderHandle LoadShaderFile(const char* path);
    void CreateCubeGeometry();

private:
    bool     m_initialized = false;
    uint32_t m_width  = 0;
    uint32_t m_height = 0;
    bool     m_pendingReset = false;

    float m_camX = 0.0f, m_camY = 0.0f, m_camZ = 0.0f;

    bgfx::RendererType::Enum m_type = bgfx::RendererType::Count;

    float m_view[16];
    float m_proj[16];

    // === recursos para el cubo ===
    bgfx::VertexLayout       m_layout{};
    bgfx::VertexBufferHandle m_vbh = BGFX_INVALID_HANDLE;
    bgfx::IndexBufferHandle  m_ibh = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle      m_prog = BGFX_INVALID_HANDLE;
};
