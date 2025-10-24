#pragma once
#include <bgfx/bgfx.h>
#include <string>

#include "../asset/Mesh.h"
#include "Material.h"

class Renderer
{
public:
    Renderer() = default;
    ~Renderer();

    Renderer(const Renderer&) = delete;
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

    // Controles luz (ya los tienes)
    void ResetLightingDefaults();
    void AddLightYawPitch(float dyawRad, float dpitchRad);
    void AdjustAmbient(float delta);
    void AdjustSpecIntensity(float delta);
    void AdjustShininess(float delta);

private:
    // Shaders / programas
    bgfx::ShaderHandle LoadShaderFile(const char* path);
    bool LoadProgramDx11(const char* vsName, const char* fsName);

    // Geometrías built-in
    void CreateCubeGeometry();
    void CreateGroundPlane(float halfSize = 250.0f, float uvTiling = 50.0f);

    // Material helper
    void ApplyMaterial(const Material& mtl);

    // Intentar cargar un OBJ (ruta absoluta o relativa)
    bool TryLoadObj(const std::string& path);

private:
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

    // Textura + uniforms
    bgfx::UniformHandle m_uTexColor  = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_texChecker = BGFX_INVALID_HANDLE;

    bgfx::UniformHandle m_uLightDir   = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_uLightColor = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_uAmbient    = BGFX_INVALID_HANDLE;

    bgfx::UniformHandle m_uNormalMtx  = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_uCameraPos  = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_uSpecParams = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_uSpecColor  = BGFX_INVALID_HANDLE;

    bgfx::UniformHandle m_uBaseTint   = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_uUvScale    = BGFX_INVALID_HANDLE;

    // Luz runtime (ya los tienes)
    float m_lightYaw = 0.f, m_lightPitch = 0.f;
    float m_ambient = 0.5f, m_specIntensity = 0.35f, m_shininess = 32.f;
    float m_lightColor3[3] = {1,1,1};

    // === OBJ cargado ===
    Mesh     m_objMesh;
    Material m_objMat;
    bool     m_objLoaded = false;
};
