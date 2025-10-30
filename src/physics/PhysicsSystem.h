#pragma once

#include "PhysicsCharacter.h"
#include "PhysicsDebugDraw.h"

#include "../core/EventBus.h"
#include "../ecs/PhysicsComponents.h"

#include <filesystem>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class Scene;
class Camera;
class InputSystem;
class Transform;

class btDbvtBroadphase;
class btDefaultCollisionConfiguration;
class btCollisionDispatcher;
class btSequentialImpulseConstraintSolver;
class btDiscreteDynamicsWorld;
class btBroadphaseInterface;
class btGhostPairCallback;
class btCollisionShape;
class btDefaultMotionState;
class btRigidBody;
class btPairCachingGhostObject;
class btKinematicCharacterController;
class BulletDebugDrawer;
class btMotionState;
struct PhysicsRaycastHit;

class PhysicsSystem
{
public:
    PhysicsSystem();
    ~PhysicsSystem();

    void SetConfigPath(std::filesystem::path path);
    void Initialize();
    void OnSceneReloaded(Scene& scene);
    bool ReloadConfigIfNeeded(Scene& scene);
    void Update(Scene& scene, const Camera& camera, const InputSystem& input, double dt);
    void LogStats() const;

    struct TriggerEvent
    {
        enum class Type
        {
            Enter,
            Stay,
            Exit,
        };

        Type     type = Type::Enter;
        EntityId trigger = kInvalidEntity;
        EntityId other   = kInvalidEntity;
    };

    EventBus& GetEventBus() { return m_eventBus; }

    bool Raycast(const float3& origin,
                 const float3& direction,
                 float maxDistance,
                 uint32_t layerMask,
                 struct PhysicsRaycastHit& outHit) const;

    std::vector<struct PhysicsRaycastHit> RaycastAll(const float3& origin,
                                                     const float3& direction,
                                                     float maxDistance,
                                                     uint32_t layerMask) const;

    double GetFixedStep() const { return m_config.fixedStep; }

    void ToggleDebugOverlay();
    void SetDebugOverlayEnabled(bool enabled);
    bool IsDebugOverlayEnabled() const { return m_debugDrawEnabled; }
    const PhysicsDebugLineBuffer& GetDebugLines() const;

private:
    struct Config
    {
        float gravity = -9.81f;
        float fixedStep = 1.0f / 120.0f;
        float stepHeight = 0.35f;
        float maxSlopeDeg = 50.0f;
        float capsuleHeight = 1.7f;
        float capsuleRadius = 0.35f;
        float walkSpeed = 3.5f;
        float jumpImpulse = 5.0f;
    };

    struct CharacterRuntime
    {
        std::unique_ptr<btCollisionShape> shape;
        std::unique_ptr<btPairCachingGhostObject> ghost;
        std::unique_ptr<btKinematicCharacterController> controller;
        float visualOffsetY = 0.0f;
    };

    void EnsureWorld();
    void InitializeWorld();
    void EnsureGround();
    void ApplyConfig(Scene& scene, const Config& newConfig);
    Config LoadConfigFromDisk() const;
    void ClearCharacters(Scene& scene);
    void EnsureCharacter(Scene& scene, EntityId entity, PhysicsCharacter& character);
    void RemoveCharacter(Scene& scene, EntityId entity);
    void EnsureRigidBody(Scene& scene, EntityId entity, Collider& collider, RigidBody& body);
    void RemoveRigidBody(Scene& scene, EntityId entity);
    void EnsureTrigger(Scene& scene, EntityId entity, TriggerVolume& trigger);
    void RemoveTrigger(Scene& scene, EntityId entity);
    void HandleCharacterInput(Scene& scene, const Camera& camera, const InputSystem& input, double dt);
    void StepSimulation(double dt);
    void SyncCharactersFromPhysics(Scene& scene);
    void SyncRigidBodiesFromPhysics(Scene& scene);
    void SyncKinematicBodiesToPhysics(Scene& scene);
    void SyncTriggersToPhysics(Scene& scene);
    void ProcessTriggerEvents(Scene& scene);
    void CollectDebugLines();
    void ClearRigidBodies();
    void ClearTriggers();
    void ClearObjectLookup();
    void RegisterCollisionObject(EntityId entity, const btCollisionObject* object);
    void UnregisterCollisionObject(const btCollisionObject* object);
    EntityId FindEntityByCollisionObject(const btCollisionObject* object) const;
    std::unique_ptr<btCollisionShape> CreateShape(ColliderShape shape, const float3& size) const;

private:
    std::filesystem::path          m_configPath;
    std::filesystem::file_time_type m_lastWriteTime{};
    bool                           m_hasLastWriteTime = false;

    Config m_config{};

    std::unique_ptr<btDbvtBroadphase>                 m_broadphase;
    std::unique_ptr<btDefaultCollisionConfiguration>  m_collisionConfig;
    std::unique_ptr<btCollisionDispatcher>            m_dispatcher;
    std::unique_ptr<btSequentialImpulseConstraintSolver> m_solver;
    std::unique_ptr<btDiscreteDynamicsWorld>          m_world;
    std::unique_ptr<btGhostPairCallback>              m_ghostPairCallback;

    std::unique_ptr<btCollisionShape>   m_groundShape;
    std::unique_ptr<btDefaultMotionState> m_groundMotionState;
    std::unique_ptr<btRigidBody>        m_groundBody;

    std::unordered_map<EntityId, CharacterRuntime> m_characterRuntime;
    struct RigidBodyRuntime
    {
        std::unique_ptr<btCollisionShape> shape;
        std::unique_ptr<btMotionState>    motionState;
        std::unique_ptr<btRigidBody>      body;
        RigidBodyType                     type = RigidBodyType::Static;
        uint32_t                          layer = 0u;
        uint32_t                          mask  = 0xffffffffu;
    };

    struct TriggerRuntime
    {
        std::unique_ptr<btCollisionShape>        shape;
        std::unique_ptr<btPairCachingGhostObject> ghost;
        std::unordered_set<EntityId>             overlaps;
        uint32_t                                 layer = 0u;
        uint32_t                                 mask  = 0xffffffffu;
        bool                                     oneShot = false;
        bool                                     active  = true;
    };

    std::unordered_map<EntityId, RigidBodyRuntime> m_rigidBodyRuntime;
    std::unordered_map<EntityId, TriggerRuntime>   m_triggerRuntime;
    std::unordered_map<const btCollisionObject*, EntityId> m_objectLookup;

    double m_lastStepDurationMs = 0.0;
    double m_lastStepDt = 0.0;
    int    m_lastStepSubsteps = 0;

    bool m_forceCharacterRebuild = false;

    std::unique_ptr<BulletDebugDrawer> m_debugDrawer;
    mutable PhysicsDebugLineBuffer     m_emptyDebugLines;
    bool                               m_debugDrawEnabled = false;

    EventBus m_eventBus;
};

