#pragma once

#include "PhysicsCharacter.h"

#include <filesystem>
#include <memory>
#include <unordered_map>

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

    double GetFixedStep() const { return m_config.fixedStep; }

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
    };

    void EnsureWorld();
    void InitializeWorld();
    void EnsureGround();
    void ApplyConfig(Scene& scene, const Config& newConfig);
    Config LoadConfigFromDisk() const;
    void ClearCharacters(Scene& scene);
    void EnsureCharacter(Scene& scene, EntityId entity, PhysicsCharacter& character);
    void RemoveCharacter(Scene& scene, EntityId entity);
    void HandleCharacterInput(Scene& scene, const Camera& camera, const InputSystem& input, double dt);
    void StepSimulation(double dt);
    void SyncCharactersFromPhysics(Scene& scene);

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

    double m_lastStepDurationMs = 0.0;
    double m_lastStepDt = 0.0;
    int    m_lastStepSubsteps = 0;

    bool m_forceCharacterRebuild = false;
};

