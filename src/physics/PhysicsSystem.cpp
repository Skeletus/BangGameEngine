#include "PhysicsSystem.h"

#include "PhysicsCharacter.h"
#include "BulletDebugDrawer.h"
#include "PhysicsDebugDraw.h"
#include "PhysicsAPI.h"
#include "../ecs/Scene.h"
#include "../ecs/Transform.h"
#include "../input/InputSystem.h"
#include "../camera/Camera.h"

#include <BulletCollision/CollisionDispatch/btGhostObject.h>
#include <BulletDynamics/Character/btKinematicCharacterController.h>
#include <btBulletDynamicsCommon.h>
#include <LinearMath/btDefaultMotionState.h>

#include <nlohmann/json.hpp>
#include <bx/math.h>

#include <chrono>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
#include <unordered_set>

using json = nlohmann::json;

namespace
{
    constexpr float kMinStep = 1.0f / 240.0f;
    constexpr float kSprintMultiplier = 1.8f;
    constexpr uint32_t kDefaultWorldLayer = 1u << 0;
    constexpr uint32_t kDefaultCharacterLayer = 1u << 1;
    constexpr uint32_t kDefaultTriggerLayer = 1u << 2;

    btQuaternion ToBtQuaternion(const float3& euler)
    {
        btQuaternion q;
        q.setEulerZYX(euler.y, euler.x, euler.z);
        return q;
    }

    btVector3 ToBtVector(const float3& v)
    {
        return btVector3(v.x, v.y, v.z);
    }

    float3 ToFloat3(const btVector3& v)
    {
        return float3{static_cast<float>(v.x()), static_cast<float>(v.y()), static_cast<float>(v.z())};
    }

    btTransform MakeBtTransform(const Transform& transform)
    {
        btTransform bt;
        bt.setIdentity();
        bt.setOrigin(btVector3(transform.position.x, transform.position.y, transform.position.z));
        bt.setRotation(ToBtQuaternion(transform.rotationEuler));
        return bt;
    }
}

PhysicsSystem::PhysicsSystem()
{
}

PhysicsSystem::~PhysicsSystem()
{
    if (m_world)
    {
        ClearTriggers();
        ClearRigidBodies();

        for (auto& [entity, runtime] : m_characterRuntime)
        {
            if (runtime.controller)
            {
                m_world->removeAction(runtime.controller.get());
            }
            if (runtime.ghost)
            {
                UnregisterCollisionObject(runtime.ghost.get());
                m_world->removeCollisionObject(runtime.ghost.get());
            }
        }
        m_characterRuntime.clear();

        if (m_groundBody)
        {
            m_world->removeRigidBody(m_groundBody.get());
        }
    }

    ClearObjectLookup();
    Physics::SetActiveSystem(nullptr);
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

    if (!m_debugDrawer)
    {
        m_debugDrawer = std::make_unique<BulletDebugDrawer>();
    }
    if (m_debugDrawer)
    {
        m_debugDrawer->setDebugMode(m_debugDrawEnabled ? btIDebugDraw::DBG_DrawContactPoints : btIDebugDraw::DBG_NoDebug);
        m_world->setDebugDrawer(m_debugDrawer.get());
    }

    EnsureGround();
    Physics::SetActiveSystem(this);
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
    ClearRigidBodies();
    ClearTriggers();
    ClearObjectLookup();

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
                UnregisterCollisionObject(runtime.ghost.get());
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
                UnregisterCollisionObject(runtimeIt->second.ghost.get());
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

void PhysicsSystem::EnsureRigidBody(Scene& scene, EntityId entity, Collider& collider, RigidBody& body)
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

    auto [it, inserted] = m_rigidBodyRuntime.try_emplace(entity);
    RigidBodyRuntime& runtime = it->second;

    const bool shapeDirty = collider.dirty || !runtime.shape;
    if (shapeDirty)
    {
        runtime.shape = CreateShape(collider.shape, collider.size);
        collider.dirty = false;
        inserted = true;
    }

    const bool needsBody = inserted || !runtime.body || body.dirty;
    const uint32_t desiredLayer = body.layer ? body.layer : kDefaultWorldLayer;
    const uint32_t desiredMask  = body.mask;

    if (needsBody)
    {
        if (runtime.body)
        {
            if (m_world)
            {
                m_world->removeRigidBody(runtime.body.get());
            }
            UnregisterCollisionObject(runtime.body.get());
        }

        btTransform startTransform = MakeBtTransform(*transform);
        auto motionState = std::make_unique<btDefaultMotionState>(startTransform);

        btScalar mass = 0.0f;
        btVector3 inertia(0.0f, 0.0f, 0.0f);
        if (body.type == RigidBodyType::Dynamic)
        {
            mass = std::max(body.mass, 0.01f);
        }

        if (mass > 0.0f && runtime.shape)
        {
            runtime.shape->calculateLocalInertia(mass, inertia);
        }

        btRigidBody::btRigidBodyConstructionInfo info(mass, motionState.get(), runtime.shape.get(), inertia);
        info.m_friction = body.friction;
        info.m_restitution = body.restitution;

        auto rigidBody = std::make_unique<btRigidBody>(info);
        rigidBody->setWorldTransform(startTransform);

        int flags = rigidBody->getCollisionFlags();
        if (body.type == RigidBodyType::Static)
        {
            flags |= btCollisionObject::CF_STATIC_OBJECT;
        }
        else
        {
            flags &= ~btCollisionObject::CF_STATIC_OBJECT;
        }

        if (body.type == RigidBodyType::Kinematic)
        {
            flags |= btCollisionObject::CF_KINEMATIC_OBJECT;
            rigidBody->setMassProps(0.0f, btVector3(0.0f, 0.0f, 0.0f));
            rigidBody->setActivationState(DISABLE_DEACTIVATION);
        }
        else
        {
            flags &= ~btCollisionObject::CF_KINEMATIC_OBJECT;
            rigidBody->setActivationState(ACTIVE_TAG);
        }

        rigidBody->setCollisionFlags(flags);

        runtime.motionState = std::move(motionState);
        runtime.body = std::move(rigidBody);
        runtime.type = body.type;
        runtime.layer = desiredLayer;
        runtime.mask = desiredMask;

        m_world->addRigidBody(runtime.body.get(), static_cast<int>(runtime.layer), static_cast<int>(runtime.mask));
        RegisterCollisionObject(entity, runtime.body.get());

        body.dirty = false;
    }
    else if (runtime.body)
    {
        runtime.type = body.type;
        if (runtime.layer != desiredLayer || runtime.mask != desiredMask)
        {
            if (m_world)
            {
                m_world->removeRigidBody(runtime.body.get());
            }
            UnregisterCollisionObject(runtime.body.get());
            runtime.layer = desiredLayer;
            runtime.mask = desiredMask;
            m_world->addRigidBody(runtime.body.get(), static_cast<int>(runtime.layer), static_cast<int>(runtime.mask));
            RegisterCollisionObject(entity, runtime.body.get());
        }
    }

    if (runtime.body && runtime.body->getMotionState())
    {
        runtime.body->getMotionState()->setWorldTransform(runtime.body->getWorldTransform());
    }
}

void PhysicsSystem::RemoveRigidBody(Scene& scene, EntityId entity)
{
    auto runtimeIt = m_rigidBodyRuntime.find(entity);
    if (runtimeIt != m_rigidBodyRuntime.end())
    {
        if (runtimeIt->second.body)
        {
            if (m_world)
            {
                m_world->removeRigidBody(runtimeIt->second.body.get());
            }
            UnregisterCollisionObject(runtimeIt->second.body.get());
        }
        m_rigidBodyRuntime.erase(runtimeIt);
    }

    if (RigidBody* body = scene.GetRigidBody(entity))
    {
        body->dirty = true;
    }
}

void PhysicsSystem::EnsureTrigger(Scene& scene, EntityId entity, TriggerVolume& trigger)
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

    auto [it, inserted] = m_triggerRuntime.try_emplace(entity);
    TriggerRuntime& runtime = it->second;

    if (trigger.dirty || !runtime.shape)
    {
        runtime.shape = CreateShape(trigger.shape, trigger.size);
        trigger.dirty = false;
        if (runtime.ghost)
        {
            runtime.ghost->setCollisionShape(runtime.shape.get());
        }
    }

    if (!runtime.ghost)
    {
        runtime.ghost = std::make_unique<btPairCachingGhostObject>();
        runtime.ghost->setCollisionShape(runtime.shape.get());
        runtime.ghost->setCollisionFlags(btCollisionObject::CF_NO_CONTACT_RESPONSE | btCollisionObject::CF_STATIC_OBJECT);
    }

    runtime.oneShot = trigger.oneShot;
    const uint32_t desiredLayer = trigger.layer ? trigger.layer : kDefaultTriggerLayer;
    const uint32_t desiredMask  = trigger.mask;

    if (runtime.layer != desiredLayer || runtime.mask != desiredMask)
    {
        if (runtime.active && runtime.ghost)
        {
            m_world->removeCollisionObject(runtime.ghost.get());
            UnregisterCollisionObject(runtime.ghost.get());
        }
        runtime.layer = desiredLayer;
        runtime.mask = desiredMask;
        runtime.active = false;
    }

    if (trigger.active && runtime.ghost)
    {
        runtime.ghost->setWorldTransform(MakeBtTransform(*transform));
        if (!runtime.active)
        {
            m_world->addCollisionObject(runtime.ghost.get(), static_cast<int>(runtime.layer), static_cast<int>(runtime.mask));
            RegisterCollisionObject(entity, runtime.ghost.get());
            runtime.active = true;
            runtime.overlaps.clear();
        }
    }
    else if (runtime.active && runtime.ghost)
    {
        m_world->removeCollisionObject(runtime.ghost.get());
        UnregisterCollisionObject(runtime.ghost.get());
        runtime.active = false;
        runtime.overlaps.clear();
    }
}

void PhysicsSystem::RemoveTrigger(Scene& scene, EntityId entity)
{
    auto runtimeIt = m_triggerRuntime.find(entity);
    if (runtimeIt != m_triggerRuntime.end())
    {
        if (runtimeIt->second.ghost && runtimeIt->second.active)
        {
            m_world->removeCollisionObject(runtimeIt->second.ghost.get());
            UnregisterCollisionObject(runtimeIt->second.ghost.get());
        }
        m_triggerRuntime.erase(runtimeIt);
    }

    if (TriggerVolume* trigger = scene.GetTriggerVolume(entity))
    {
        trigger->dirty = true;
    }
}

void PhysicsSystem::ClearRigidBodies()
{
    if (!m_world)
    {
        m_rigidBodyRuntime.clear();
        return;
    }

    for (auto& [entity, runtime] : m_rigidBodyRuntime)
    {
        if (runtime.body)
        {
            m_world->removeRigidBody(runtime.body.get());
            UnregisterCollisionObject(runtime.body.get());
        }
    }
    m_rigidBodyRuntime.clear();
}

void PhysicsSystem::ClearTriggers()
{
    if (!m_world)
    {
        m_triggerRuntime.clear();
        return;
    }

    for (auto& [entity, runtime] : m_triggerRuntime)
    {
        if (runtime.ghost && runtime.active)
        {
            m_world->removeCollisionObject(runtime.ghost.get());
            UnregisterCollisionObject(runtime.ghost.get());
        }
    }
    m_triggerRuntime.clear();
}

void PhysicsSystem::ClearObjectLookup()
{
    m_objectLookup.clear();
}

void PhysicsSystem::RegisterCollisionObject(EntityId entity, const btCollisionObject* object)
{
    if (!object)
    {
        return;
    }
    m_objectLookup[object] = entity;
}

void PhysicsSystem::UnregisterCollisionObject(const btCollisionObject* object)
{
    if (!object)
    {
        return;
    }
    m_objectLookup.erase(object);
}

EntityId PhysicsSystem::FindEntityByCollisionObject(const btCollisionObject* object) const
{
    if (!object)
    {
        return kInvalidEntity;
    }
    auto it = m_objectLookup.find(object);
    if (it == m_objectLookup.end())
    {
        return kInvalidEntity;
    }
    return it->second;
}

std::unique_ptr<btCollisionShape> PhysicsSystem::CreateShape(ColliderShape shape, const float3& size) const
{
    switch (shape)
    {
    case ColliderShape::Box:
    {
        const float hx = std::max(size.x, 0.01f);
        const float hy = std::max(size.y, 0.01f);
        const float hz = std::max(size.z, 0.01f);
        return std::make_unique<btBoxShape>(btVector3(hx, hy, hz));
    }
    case ColliderShape::Capsule:
    {
        const float radius = std::max(size.x, 0.01f);
        const float halfHeight = std::max(size.y, 0.0f);
        return std::make_unique<btCapsuleShape>(radius, halfHeight * 2.0f);
    }
    default:
        break;
    }
    return std::make_unique<btBoxShape>(btVector3(0.5f, 0.5f, 0.5f));
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
        // ❌ QUITA TODO EL ESCALADO
        // const float scale = transform->scale.y;
        // const float scaledRadius = m_config.capsuleRadius * scale;
        // const float scaledHeight = m_config.capsuleHeight * scale;
        // const float scaledStepHeight = m_config.stepHeight * scale;
        // const float scaledJumpImpulse = m_config.jumpImpulse * scale;

        // ✅ USA LOS VALORES DIRECTOS DEL CONFIG
        runtime.shape = std::make_unique<btCapsuleShape>(m_config.capsuleRadius, m_config.capsuleHeight);

        // ✅ Calcular y guardar el offset visual
        const float capsuleTotalHeight = m_config.capsuleHeight + m_config.capsuleRadius * 2.0f;
        runtime.visualOffsetY = -(capsuleTotalHeight * 0.5f);

        auto ghost = std::make_unique<btPairCachingGhostObject>();
        btTransform startTransform;
        startTransform.setIdentity();
        startTransform.setOrigin(btVector3(transform->position.x, transform->position.y, transform->position.z));
        startTransform.setRotation(ToBtQuaternion(transform->rotationEuler));
        ghost->setWorldTransform(startTransform);
        ghost->setCollisionShape(runtime.shape.get());
        ghost->setCollisionFlags(btCollisionObject::CF_CHARACTER_OBJECT);
        ghost->setActivationState(DISABLE_DEACTIVATION);

        auto controller = std::make_unique<btKinematicCharacterController>(
            ghost.get(), 
            static_cast<btConvexShape*>(runtime.shape.get()), 
            m_config.stepHeight,  // ✅ Sin escalar
            btVector3(0.0f, 1.0f, 0.0f)
        );
        
        controller->setMaxSlope(btScalar(bx::toRad(m_config.maxSlopeDeg)));
        controller->setGravity(btVector3(0.0f, m_config.gravity, 0.0f));
        controller->setJumpSpeed(m_config.jumpImpulse);  // ✅ Sin escalar
        controller->setFallSpeed(std::fabs(m_config.gravity) * 3.0f);
        controller->setUseGhostSweepTest(true);

        m_world->addCollisionObject(ghost.get(),
            static_cast<int>(kDefaultCharacterLayer),
            static_cast<int>(0xffffffffu));

        m_world->addAction(controller.get());

        runtime.ghost = std::move(ghost);
        runtime.controller = std::move(controller);

        RegisterCollisionObject(entity, runtime.ghost.get());

        std::printf("[Physics] Character created at (%.2f, %.2f, %.2f) radius=%.2f height=%.2f\n",
            transform->position.x, transform->position.y, transform->position.z,
            m_config.capsuleRadius, m_config.capsuleHeight);
    }

    character.entity = entity;
    character.ghost = runtime.ghost.get();
    character.controller = runtime.controller.get();

    if (runtime.ghost)
    {
        RegisterCollisionObject(entity, runtime.ghost.get());
    }
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

    if (m_debugDrawer)
    {
        m_debugDrawer->BeginFrame();
    }

    auto start = std::chrono::high_resolution_clock::now();
    m_lastStepSubsteps = m_world->stepSimulation(static_cast<btScalar>(dt), 4, fixedStep);
    auto end = std::chrono::high_resolution_clock::now();

    m_lastStepDurationMs = std::chrono::duration<double, std::milli>(end - start).count();
    m_lastStepDt = dt;

    if (m_debugDrawEnabled && m_debugDrawer)
    {
        m_world->debugDrawWorld();
        CollectDebugLines();
        //std::printf("[PhysicsDebug] lines=%zu\n", m_debugDrawer->GetLines().size());
    }
}

void PhysicsSystem::SyncCharactersFromPhysics(Scene& scene)
{
    // Offset para compensar la diferencia entre el tamaño del modelo (0.05) y el cápsula (1.0)
    constexpr float kVisualOffsetY = -1.9f; // Mitad del radio del cápsula aproximadamente
    
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

        // ✅ Aplicar offset visual para que el modelo pequeño esté centrado en el cápsula
        transform->position.x = static_cast<float>(origin.x());
        transform->position.y = static_cast<float>(origin.y()) + runtime.visualOffsetY;
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

void PhysicsSystem::SyncRigidBodiesFromPhysics(Scene& scene)
{
    for (auto& [entity, runtime] : m_rigidBodyRuntime)
    {
        RigidBody* body = scene.GetRigidBody(entity);
        if (!body || !runtime.body)
        {
            continue;
        }

        if (body->type != RigidBodyType::Dynamic)
        {
            continue;
        }

        Transform* transform = scene.GetTransform(entity);
        if (!transform)
        {
            continue;
        }

        const btTransform& worldTransform = runtime.body->getWorldTransform();
        const btVector3 origin = worldTransform.getOrigin();
        const btQuaternion rotation = worldTransform.getRotation();

        transform->position = ToFloat3(origin);

        btScalar yaw, pitch, roll;
        btMatrix3x3(rotation).getEulerZYX(yaw, pitch, roll);
        transform->rotationEuler.x = static_cast<float>(pitch);
        transform->rotationEuler.y = static_cast<float>(yaw);
        transform->rotationEuler.z = static_cast<float>(roll);
        transform->MarkDirty();
    }
}

void PhysicsSystem::SyncKinematicBodiesToPhysics(Scene& scene)
{
    for (auto& [entity, runtime] : m_rigidBodyRuntime)
    {
        RigidBody* body = scene.GetRigidBody(entity);
        Transform* transform = scene.GetTransform(entity);
        if (!body || !transform || !runtime.body)
        {
            continue;
        }

        const bool isDynamic = body->type == RigidBodyType::Dynamic;
        if (!isDynamic && !transform->dirty && !body->dirty)
        {
            continue;
        }

        if (isDynamic && !body->dirty && !transform->dirty)
        {
            continue;
        }

        const btTransform bt = MakeBtTransform(*transform);
        runtime.body->setWorldTransform(bt);
        if (runtime.body->getMotionState())
        {
            runtime.body->getMotionState()->setWorldTransform(bt);
        }

        if (isDynamic && (body->dirty || transform->dirty))
        {
            runtime.body->setLinearVelocity(btVector3(0.0f, 0.0f, 0.0f));
            runtime.body->setAngularVelocity(btVector3(0.0f, 0.0f, 0.0f));
        }

        body->dirty = false;
    }
}

void PhysicsSystem::SyncTriggersToPhysics(Scene& scene)
{
    for (auto& [entity, runtime] : m_triggerRuntime)
    {
        TriggerVolume* trigger = scene.GetTriggerVolume(entity);
        Transform* transform = scene.GetTransform(entity);
        if (!trigger || !transform || !runtime.ghost)
        {
            continue;
        }

        if (!trigger->active || !runtime.active)
        {
            continue;
        }

        if (!trigger->dirty && !transform->dirty)
        {
            continue;
        }

        runtime.ghost->setWorldTransform(MakeBtTransform(*transform));
        trigger->dirty = false;
    }
}

void PhysicsSystem::ProcessTriggerEvents(Scene& scene)
{
    for (auto& [entity, runtime] : m_triggerRuntime)
    {
        TriggerVolume* trigger = scene.GetTriggerVolume(entity);
        if (!trigger || !runtime.ghost || !runtime.active)
        {
            continue;
        }

        std::unordered_set<EntityId> current;
        const int overlapCount = runtime.ghost->getNumOverlappingObjects();
        for (int i = 0; i < overlapCount; ++i)
        {
            const btCollisionObject* object = runtime.ghost->getOverlappingObject(i);
            EntityId other = FindEntityByCollisionObject(object);
            if (other == kInvalidEntity || other == entity)
            {
                continue;
            }
            current.insert(other);
        }

        for (EntityId other : current)
        {
            if (runtime.overlaps.find(other) == runtime.overlaps.end())
            {
                m_eventBus.Publish(TriggerEvent{TriggerEvent::Type::Enter, entity, other});
            }
            else
            {
                m_eventBus.Publish(TriggerEvent{TriggerEvent::Type::Stay, entity, other});
            }
        }

        for (EntityId previous : runtime.overlaps)
        {
            if (current.find(previous) == current.end())
            {
                m_eventBus.Publish(TriggerEvent{TriggerEvent::Type::Exit, entity, previous});
            }
        }

        runtime.overlaps = std::move(current);

        if (runtime.oneShot && !runtime.overlaps.empty())
        {
            trigger->active = false;
            runtime.active = false;
            if (runtime.ghost)
            {
                m_world->removeCollisionObject(runtime.ghost.get());
                UnregisterCollisionObject(runtime.ghost.get());
            }
            runtime.overlaps.clear();
        }
    }
}

bool PhysicsSystem::Raycast(const float3& origin,
                            const float3& direction,
                            float maxDistance,
                            uint32_t layerMask,
                            PhysicsRaycastHit& outHit) const
{
    if (!m_world || maxDistance <= 0.0f || layerMask == 0u)
    {
        return false;
    }

    const btVector3 from = ToBtVector(origin);
    const btVector3 to = from + ToBtVector(direction) * btScalar(maxDistance);

    btCollisionWorld::ClosestRayResultCallback callback(from, to);
    callback.m_collisionFilterMask = static_cast<int>(layerMask);
    callback.m_collisionFilterGroup = -1;

    m_world->rayTest(from, to, callback);

    if (!callback.hasHit())
    {
        return false;
    }

    outHit.entity = FindEntityByCollisionObject(callback.m_collisionObject);
    outHit.point = ToFloat3(callback.m_hitPointWorld);
    outHit.normal = ToFloat3(callback.m_hitNormalWorld);
    outHit.distance = static_cast<float>(callback.m_closestHitFraction * maxDistance);
    return true;
}

std::vector<PhysicsRaycastHit> PhysicsSystem::RaycastAll(const float3& origin,
                                                         const float3& direction,
                                                         float maxDistance,
                                                         uint32_t layerMask) const
{
    std::vector<PhysicsRaycastHit> hits;
    if (!m_world || maxDistance <= 0.0f || layerMask == 0u)
    {
        return hits;
    }

    const btVector3 from = ToBtVector(origin);
    const btVector3 to = from + ToBtVector(direction) * btScalar(maxDistance);

    btCollisionWorld::AllHitsRayResultCallback callback(from, to);
    callback.m_collisionFilterMask = static_cast<int>(layerMask);
    callback.m_collisionFilterGroup = -1;

    m_world->rayTest(from, to, callback);

    if (!callback.hasHit())
    {
        return hits;
    }

    const int count = callback.m_collisionObjects.size();
    hits.reserve(count);
    for (int i = 0; i < count; ++i)
    {
        PhysicsRaycastHit hit{};
        hit.entity = FindEntityByCollisionObject(callback.m_collisionObjects[i]);
        hit.point = ToFloat3(callback.m_hitPointWorld[i]);
        hit.normal = ToFloat3(callback.m_hitNormalWorld[i]);
        hit.distance = static_cast<float>(callback.m_hitFractions[i] * maxDistance);
        hits.push_back(hit);
    }

    return hits;
}

void PhysicsSystem::CollectDebugLines()
{
    if (!m_world || !m_debugDrawer)
    {
        return;
    }

    constexpr uint32_t kStaticColor   = 0xff7f7f7fu;
    constexpr uint32_t kDynamicColor  = 0xff00ffffu;
    constexpr uint32_t kTriggerColor  = 0xffff00ffu;

    const btCollisionObjectArray& objects = m_world->getCollisionObjectArray();
    for (int i = 0; i < objects.size(); ++i)
    {
        const btCollisionObject* object = objects[i];
        if (!object)
        {
            continue;
        }

        uint32_t color = object->isStaticObject() ? kStaticColor : kDynamicColor;
        if (object->getCollisionFlags() & btCollisionObject::CF_NO_CONTACT_RESPONSE)
        {
            color = kTriggerColor;
        }
        m_debugDrawer->DrawCollisionObject(*object, color);
    }
}

void PhysicsSystem::ToggleDebugOverlay()
{
    SetDebugOverlayEnabled(!m_debugDrawEnabled);
}

void PhysicsSystem::SetDebugOverlayEnabled(bool enabled)
{
    if (m_debugDrawEnabled == enabled)
    {
        return;
    }

    m_debugDrawEnabled = enabled;
    if (m_debugDrawer)
    {
        m_debugDrawer->setDebugMode(enabled ? btIDebugDraw::DBG_DrawContactPoints : btIDebugDraw::DBG_NoDebug);
    }
    std::printf("[PhysicsDebug] overlay %s\n", enabled ? "ON" : "OFF");
}

const PhysicsDebugLineBuffer& PhysicsSystem::GetDebugLines() const
{
    if (m_debugDrawEnabled && m_debugDrawer)
    {
        return m_debugDrawer->GetLines();
    }

    m_emptyDebugLines.clear();
    return m_emptyDebugLines;
}

void PhysicsSystem::Update(Scene& scene, const Camera& camera, const InputSystem& input, double dt)
{
    EnsureWorld();
    if (!m_world)
    {
        return;
    }

    if (m_forceCharacterRebuild)
    {
        ClearCharacters(scene);
        m_forceCharacterRebuild = false;
    }

    std::vector<EntityId> bodiesToRemove;
    for (const auto& [entity, runtime] : m_rigidBodyRuntime)
    {
        if (!scene.IsAlive(entity) || !scene.GetRigidBody(entity) || !scene.GetCollider(entity))
        {
            bodiesToRemove.push_back(entity);
        }
    }
    for (EntityId id : bodiesToRemove)
    {
        RemoveRigidBody(scene, id);
    }

    std::vector<EntityId> triggersToRemove;
    for (const auto& [entity, runtime] : m_triggerRuntime)
    {
        if (!scene.IsAlive(entity) || !scene.GetTriggerVolume(entity))
        {
            triggersToRemove.push_back(entity);
        }
    }
    for (EntityId id : triggersToRemove)
    {
        RemoveTrigger(scene, id);
    }

    for (auto& [entity, body] : scene.GetRigidBodies())
    {
        if (!scene.IsAlive(entity))
        {
            continue;
        }
        Collider* collider = scene.GetCollider(entity);
        if (!collider)
        {
            continue;
        }
        EnsureRigidBody(scene, entity, *collider, body);
    }

    for (auto& [entity, trigger] : scene.GetTriggerVolumes())
    {
        if (!scene.IsAlive(entity))
        {
            continue;
        }
        EnsureTrigger(scene, entity, trigger);
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

    SyncKinematicBodiesToPhysics(scene);
    SyncTriggersToPhysics(scene);
    HandleCharacterInput(scene, camera, input, dt);
    StepSimulation(dt);
    SyncRigidBodiesFromPhysics(scene);
    SyncCharactersFromPhysics(scene);
    ProcessTriggerEvents(scene);
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

