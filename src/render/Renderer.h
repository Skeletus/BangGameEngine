#pragma once
#include <bgfx/bgfx.h>
#include <string>
#include <vector>
#include "../asset/Mesh.h"
#include "Material.h"

class Scene;
namespace resource { class ResourceManager; }

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

    void BeginFrame(Scene* scene = nullptr);
    void EndFrame();

    void SetView(const float view[16]);
    void SetProjection(float fovYDeg, float aspect, float znear, float zfar);

    void SetCameraDebugInfo(float x, float y, float z);

    const char* GetBackendName() const;

    void SetResourceManager(resource::ResourceManager* manager);

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

    float GetShininess() const { return m_shininess; }
    float GetSpecIntensity() const { return m_specIntensity; }

    void SubmitMeshLit(const Mesh& mesh, const Material& material, const float model[16]);

private:
    // Shaders / programas
    bgfx::ShaderHandle LoadShaderFile(const char* path);
    bool LoadProgramDx11(const char* vsName, const char* fsName);

    // Geometrías built-in
    void CreateCubeGeometry();
    void CreateGroundPlane(float halfSize = 250.0f, float uvTiling = 50.0f);

    // Material helper
    void ApplyMaterial(const Material& mtl);

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
    Mesh m_cubeMesh;

    // Recursos: plano
    Mesh m_planeMesh;

    // Textura + uniforms
    bgfx::UniformHandle m_uTexColor  = BGFX_INVALID_HANDLE;
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

    float m_lightDir4[4]  = {0,0,0,0};
    float m_lightColor4[4]= {1,1,1,0};
    float m_ambient4[4]   = {0,0,0,0};
    float m_camPos4[4]    = {0,0,0,0};
    uint64_t m_defaultState = BGFX_STATE_WRITE_RGB
                            | BGFX_STATE_WRITE_A
                            | BGFX_STATE_WRITE_Z
                            | BGFX_STATE_DEPTH_TEST_LESS;

    resource::ResourceManager* m_resourceManager = nullptr;
};
