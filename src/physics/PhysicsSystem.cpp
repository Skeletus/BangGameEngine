#include "PhysicsSystem.h"

#include "PhysicsCharacter.h"
#include "../ecs/Scene.h"
#include "../ecs/Transform.h"
#include "../input/InputSystem.h"
#include "../camera/Camera.h"

#include <BulletCollision/CollisionDispatch/btGhostObject.h>
#include <BulletDynamics/Character/btKinematicCharacterController.h>
#include <btBulletDynamicsCommon.h>

#include <nlohmann/json.hpp>
#include <bx/math.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

using json = nlohmann::json;

namespace
{
    constexpr float kMinStep = 1.0f / 240.0f;
    constexpr float kSprintMultiplier = 1.8f;

    btQuaternion ToBtQuaternion(const float3& euler)
    {
        btQuaternion q;
        q.setEulerZYX(euler.y, euler.x, euler.z);
        return q;
    }
}

PhysicsSystem::PhysicsSystem()
{
}

PhysicsSystem::~PhysicsSystem()
{
    if (m_world)
    {
        for (auto& [entity, runtime] : m_characterRuntime)
        {
            if (runtime.controller)
            {
                m_world->removeAction(runtime.controller.get());
            }
            if (runtime.ghost)
            {
                m_world->removeCollisionObject(runtime.ghost.get());
            }
        }
        m_characterRuntime.clear();

        if (m_groundBody)
        {
            m_world->removeRigidBody(m_groundBody.get());
        }
    }
}

void PhysicsSystem::SetConfigPath(std::filesystem::path path)
{
    m_configPath = std::move(path);
    m_hasLastWriteTime = false;
}

void PhysicsSystem::Initialize()
{
    EnsureWorld();
}

void PhysicsSystem::EnsureWorld()
{
    if (m_world)
    {
        return;
    }
    InitializeWorld();
}

void PhysicsSystem::InitializeWorld()
{
    m_broadphase = std::make_unique<btDbvtBroadphase>();
    m_collisionConfig = std::make_unique<btDefaultCollisionConfiguration>();
    m_dispatcher = std::make_unique<btCollisionDispatcher>(m_collisionConfig.get());
    m_solver = std::make_unique<btSequentialImpulseConstraintSolver>();
    m_world = std::make_unique<btDiscreteDynamicsWorld>(m_dispatcher.get(), m_broadphase.get(), m_solver.get(), m_collisionConfig.get());

    m_world->setGravity(btVector3(0.0f, m_config.gravity, 0.0f));

    m_ghostPairCallback = std::make_unique<btGhostPairCallback>();
    m_world->getBroadphase()->getOverlappingPairCache()->setInternalGhostPairCallback(m_ghostPairCallback.get());

    EnsureGround();
}

void PhysicsSystem::EnsureGround()
{
    if (m_groundBody)
    {
        return;
    }

    m_groundShape = std::make_unique<btStaticPlaneShape>(btVector3(0.0f, 1.0f, 0.0f), 0.0f);
    m_groundMotionState = std::make_unique<btDefaultMotionState>();

    btRigidBody::btRigidBodyConstructionInfo info(0.0f, m_groundMotionState.get(), m_groundShape.get());
    m_groundBody = std::make_unique<btRigidBody>(info);
    m_groundBody->setFriction(1.0f);
    m_groundBody->setRestitution(0.0f);

    m_world->addRigidBody(m_groundBody.get(), btBroadphaseProxy::StaticFilter, btBroadphaseProxy::AllFilter);
    //std::printf("[Physics] Ground plane created at Y=0\n");
}

void PhysicsSystem::OnSceneReloaded(Scene& scene)
{
    EnsureWorld();

    ClearCharacters(scene);

    std::vector<EntityId> toErase;
    for (auto& [entity, character] : scene.GetPhysicsCharacters())
    {
        if (!scene.IsAlive(entity))
        {
            toErase.push_back(entity);
            continue;
        }
        character.entity = entity;
        character.walkSpeed = m_config.walkSpeed;
        character.jumpImpulse = m_config.jumpImpulse;
        character.ghost = nullptr;
        character.controller = nullptr;
        character.dirty = true;
    }

    for (EntityId id : toErase)
    {
        scene.RemovePhysicsCharacter(id);
    }

    const EntityId cj = scene.FindEntityByLogicalId("cj");
    if (cj != kInvalidEntity)
    {
        PhysicsCharacter* character = scene.AddPhysicsCharacter(cj);
        if (character)
        {
            character->entity = cj;
            character->walkSpeed = m_config.walkSpeed;
            character->jumpImpulse = m_config.jumpImpulse;
            character->dirty = true;
            character->ghost = nullptr;
            character->controller = nullptr;
        }
    }

    m_forceCharacterRebuild = true;
}

bool PhysicsSystem::ReloadConfigIfNeeded(Scene& scene)
{
    if (m_configPath.empty())
    {
        return false;
    }

    std::error_code ec;
    auto currentTime = std::filesystem::last_write_time(m_configPath, ec);
    if (ec)
    {
        return false;
    }

    if (!m_hasLastWriteTime || currentTime != m_lastWriteTime)
    {
        m_lastWriteTime = currentTime;
        m_hasLastWriteTime = true;
        Config newConfig = LoadConfigFromDisk();
        ApplyConfig(scene, newConfig);
        return true;
    }

    return false;
}

PhysicsSystem::Config PhysicsSystem::LoadConfigFromDisk() const
{
    Config cfg = m_config;

    std::ifstream file(m_configPath);
    if (!file.is_open())
    {
        std::cerr << "[Physics] Failed to open config: " << m_configPath << '\n';
        return cfg;
    }

    json data;
    try
    {
        file >> data;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[Physics] Failed to parse config: " << e.what() << '\n';
        return cfg;
    }

    cfg.gravity = data.value("gravity", cfg.gravity);
    cfg.fixedStep = data.value("fixedStep", cfg.fixedStep);
    cfg.stepHeight = data.value("stepHeight", cfg.stepHeight);
    cfg.maxSlopeDeg = data.value("maxSlopeDeg", cfg.maxSlopeDeg);
    cfg.walkSpeed = data.value("walkSpeed", cfg.walkSpeed);
    cfg.jumpImpulse = data.value("jumpImpulse", cfg.jumpImpulse);

    if (auto capsuleIt = data.find("capsule"); capsuleIt != data.end() && capsuleIt->is_object())
    {
        cfg.capsuleHeight = capsuleIt->value("height", cfg.capsuleHeight);
        cfg.capsuleRadius = capsuleIt->value("radius", cfg.capsuleRadius);
    }

    if (!(cfg.fixedStep > 0.0f))
    {
        cfg.fixedStep = 1.0f / 120.0f;
    }

    return cfg;
}

void PhysicsSystem::ApplyConfig(Scene& scene, const Config& newConfig)
{
    EnsureWorld();

    const bool rebuildCharacters = std::fabs(newConfig.capsuleHeight - m_config.capsuleHeight) > 1e-4f
        || std::fabs(newConfig.capsuleRadius - m_config.capsuleRadius) > 1e-4f
        || std::fabs(newConfig.stepHeight - m_config.stepHeight) > 1e-4f
        || std::fabs(newConfig.maxSlopeDeg - m_config.maxSlopeDeg) > 1e-4f;

    m_config = newConfig;

    if (m_world)
    {
        m_world->setGravity(btVector3(0.0f, m_config.gravity, 0.0f));
    }

    for (auto& [entity, runtime] : m_characterRuntime)
    {
        if (runtime.controller)
        {
            runtime.controller->setGravity(btVector3(0.0f, m_config.gravity, 0.0f));
            runtime.controller->setMaxSlope(btScalar(bx::toRad(m_config.maxSlopeDeg)));
            runtime.controller->setJumpSpeed(m_config.jumpImpulse);
            runtime.controller->setFallSpeed(std::fabs(m_config.gravity) * 3.0f);
        }
    }

    for (auto& [entity, character] : scene.GetPhysicsCharacters())
    {
        character.walkSpeed = m_config.walkSpeed;
        character.jumpImpulse = m_config.jumpImpulse;
        character.dirty = true;
    }

    if (rebuildCharacters)
    {
        ClearCharacters(scene);
        m_forceCharacterRebuild = true;
    }
}

void PhysicsSystem::ClearCharacters(Scene& scene)
{
    if (m_world)
    {
        for (auto& [entity, runtime] : m_characterRuntime)
        {
            if (runtime.controller)
            {
                m_world->removeAction(runtime.controller.get());
            }
            if (runtime.ghost)
            {
                m_world->removeCollisionObject(runtime.ghost.get());
            }
        }
    }

    m_characterRuntime.clear();

    for (auto& [entity, character] : scene.GetPhysicsCharacters())
    {
        character.ghost = nullptr;
        character.controller = nullptr;
        character.dirty = true;
    }
}

void PhysicsSystem::RemoveCharacter(Scene& scene, EntityId entity)
{
    auto runtimeIt = m_characterRuntime.find(entity);
    if (runtimeIt != m_characterRuntime.end())
    {
        if (m_world)
        {
            if (runtimeIt->second.controller)
            {
                m_world->removeAction(runtimeIt->second.controller.get());
            }
            if (runtimeIt->second.ghost)
            {
                m_world->removeCollisionObject(runtimeIt->second.ghost.get());
            }
        }
        m_characterRuntime.erase(runtimeIt);
    }

    if (PhysicsCharacter* character = scene.GetPhysicsCharacter(entity))
    {
        character->ghost = nullptr;
        character->controller = nullptr;
        character->dirty = true;
    }
}

void PhysicsSystem::EnsureCharacter(Scene& scene, EntityId entity, PhysicsCharacter& character)
{
    if (!m_world)
    {
        return;
    }

    Transform* transform = scene.GetTransform(entity);
    if (!transform)
    {
        return;
    }

    auto [it, inserted] = m_characterRuntime.try_emplace(entity);
    CharacterRuntime& runtime = it->second;

    if (inserted || !runtime.controller)
    {
        runtime.shape = std::make_unique<btCapsuleShape>(m_config.capsuleRadius, m_config.capsuleHeight);

        auto ghost = std::make_unique<btPairCachingGhostObject>();
        btTransform startTransform;
        startTransform.setIdentity();
        startTransform.setOrigin(btVector3(transform->position.x, transform->position.y, transform->position.z));
        startTransform.setRotation(ToBtQuaternion(transform->rotationEuler));
        ghost->setWorldTransform(startTransform);
        ghost->setCollisionShape(runtime.shape.get());
        ghost->setCollisionFlags(btCollisionObject::CF_CHARACTER_OBJECT);
        ghost->setActivationState(DISABLE_DEACTIVATION);

        auto controller = std::make_unique<btKinematicCharacterController>(ghost.get(), static_cast<btConvexShape*>(runtime.shape.get()), m_config.stepHeight, btVector3(0.0f, 1.0f, 0.0f));
        controller->setMaxSlope(btScalar(bx::toRad(m_config.maxSlopeDeg)));
        controller->setGravity(btVector3(0.0f, m_config.gravity, 0.0f));
        controller->setJumpSpeed(m_config.jumpImpulse);
        controller->setFallSpeed(std::fabs(m_config.gravity) * 3.0f);
        controller->setUseGhostSweepTest(true);

        m_world->addCollisionObject(ghost.get(), 
            btBroadphaseProxy::CharacterFilter, 
            btBroadphaseProxy::AllFilter);  // Colisiona con TODO

        m_world->addAction(controller.get());

        runtime.ghost = std::move(ghost);
        runtime.controller = std::move(controller);

        std::printf("[Physics] Character created at (%.2f, %.2f, %.2f)\n", 
            transform->position.x, transform->position.y, transform->position.z);
    }

    character.entity = entity;
    character.ghost = runtime.ghost.get();
    character.controller = runtime.controller.get();
}

void PhysicsSystem::HandleCharacterInput(Scene& scene, const Camera& camera, const InputSystem& input, double dt)
{
    if (dt <= 0.0)
    {
        return;
    }

    const float moveForward = input.HasAxis("MoveForward") ? input.GetAxis("MoveForward") : 0.0f;
    const float moveRight = input.HasAxis("MoveRight") ? input.GetAxis("MoveRight") : 0.0f;
    const auto  jump = input.GetAction("Jump");
    const auto  sprint = input.GetAction("Sprint");

    const float yaw = camera.GetYaw();
    const float forwardX = std::cos(yaw);
    const float forwardZ = std::sin(yaw);
    const float rightX = forwardZ;
    const float rightZ = -forwardX;

    const float desiredSpeedMultiplier = sprint.held ? kSprintMultiplier : 1.0f;

    for (auto& [entity, runtime] : m_characterRuntime)
    {
        PhysicsCharacter* character = scene.GetPhysicsCharacter(entity);
        if (!character || !runtime.controller)
        {
            continue;
        }

        btVector3 desiredDirection(
            forwardX * moveForward + rightX * moveRight,
            0.0f,
            forwardZ * moveForward + rightZ * moveRight);

        if (desiredDirection.length2() > 1e-5f)
        {
            desiredDirection.normalize();
            const float speed = character->walkSpeed * desiredSpeedMultiplier;
            runtime.controller->setWalkDirection(desiredDirection * speed * static_cast<btScalar>(dt));
        }
        else
        {
            runtime.controller->setWalkDirection(btVector3(0.0f, 0.0f, 0.0f));
        }

        if (jump.pressed)
        {
            bool onGround = runtime.controller->onGround();
            bool canJump = runtime.controller->canJump();
            //std::printf("[Physics] Jump pressed - onGround=%d canJump=%d\n", onGround, canJump);
            
            if (onGround)
            {
                runtime.controller->jump();
            }
        }
    }
}

void PhysicsSystem::StepSimulation(double dt)
{
    if (!m_world)
    {
        return;
    }

    const btScalar fixedStep = btScalar(std::max(m_config.fixedStep, kMinStep));

    auto start = std::chrono::high_resolution_clock::now();
    m_lastStepSubsteps = m_world->stepSimulation(static_cast<btScalar>(dt), 4, fixedStep);
    auto end = std::chrono::high_resolution_clock::now();

    m_lastStepDurationMs = std::chrono::duration<double, std::milli>(end - start).count();
    m_lastStepDt = dt;
}

void PhysicsSystem::SyncCharactersFromPhysics(Scene& scene)
{
    for (auto& [entity, runtime] : m_characterRuntime)
    {
        PhysicsCharacter* character = scene.GetPhysicsCharacter(entity);
        if (!character || !runtime.ghost)
        {
            continue;
        }

        Transform* transform = scene.GetTransform(entity);
        if (!transform)
        {
            continue;
        }

        const btTransform& worldTransform = runtime.ghost->getWorldTransform();
        const btVector3 origin = worldTransform.getOrigin();
        const btQuaternion rotation = worldTransform.getRotation();

        transform->position.x = static_cast<float>(origin.x());
        transform->position.y = static_cast<float>(origin.y());
        transform->position.z = static_cast<float>(origin.z());

        btScalar yaw, pitch, roll;
        btMatrix3x3(rotation).getEulerZYX(yaw, pitch, roll);
        transform->rotationEuler.x = static_cast<float>(pitch);
        transform->rotationEuler.y = static_cast<float>(yaw);
        transform->rotationEuler.z = static_cast<float>(roll);
        transform->MarkDirty();

        character->dirty = false;
    }
}

void PhysicsSystem::Update(Scene& scene, const Camera& camera, const InputSystem& input, double dt)
{
    EnsureWorld();

    if (m_forceCharacterRebuild)
    {
        ClearCharacters(scene);
        m_forceCharacterRebuild = false;
    }

    auto& characters = scene.GetPhysicsCharacters();

    std::vector<EntityId> toRemove;
    for (const auto& [entity, runtime] : m_characterRuntime)
    {
        if (!scene.IsAlive(entity) || characters.find(entity) == characters.end())
        {
            toRemove.push_back(entity);
        }
    }
    for (EntityId id : toRemove)
    {
        RemoveCharacter(scene, id);
    }

    for (auto& [entity, character] : characters)
    {
        if (!scene.IsAlive(entity))
        {
            continue;
        }
        EnsureCharacter(scene, entity, character);

        Transform* transform = scene.GetTransform(entity);
        auto runtimeIt = m_characterRuntime.find(entity);
        if (!transform || runtimeIt == m_characterRuntime.end())
        {
            continue;
        }

        CharacterRuntime& runtime = runtimeIt->second;
        if (character.dirty || transform->dirty)
        {
            btTransform worldTransform;
            worldTransform.setIdentity();
            worldTransform.setOrigin(btVector3(transform->position.x, transform->position.y, transform->position.z));
            worldTransform.setRotation(ToBtQuaternion(transform->rotationEuler));

            if (runtime.ghost)
            {
                runtime.ghost->setWorldTransform(worldTransform);
            }
            if (runtime.controller)
            {
                runtime.controller->warp(btVector3(transform->position.x, transform->position.y, transform->position.z));
            }
            character.dirty = false;
        }
    }

    HandleCharacterInput(scene, camera, input, dt);
    StepSimulation(dt);
    SyncCharactersFromPhysics(scene);
}

void PhysicsSystem::LogStats() const
{
    const int bodies = m_world ? m_world->getNumCollisionObjects() : 0;
    const size_t characters = m_characterRuntime.size();
    std::printf("[Physics] bodies=%d characters=%zu stepTime=%.4fms substeps=%d fixedStep=%.4f actualDt=%.4f\n",
                bodies,
                characters,
                m_lastStepDurationMs,
                m_lastStepSubsteps,
                m_config.fixedStep,
                m_lastStepDt);
}

