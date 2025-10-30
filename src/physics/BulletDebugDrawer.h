#pragma once

#include "PhysicsDebugDraw.h"

#include <LinearMath/btIDebugDraw.h>
#include <memory>

class btCollisionObject;
class btCollisionShape;
class btTransform;
class btStaticPlaneShape;
class btBoxShape;
class btCapsuleShape;
class btCompoundShape;

class BulletDebugDrawer : public btIDebugDraw
{
public:
    BulletDebugDrawer();

    void BeginFrame();

    const PhysicsDebugLineBuffer& GetLines() const { return m_lines; }

    void DrawCollisionObject(const btCollisionObject& object, uint32_t color);

    // btIDebugDraw interface
    void drawLine(const btVector3& from, const btVector3& to, const btVector3& color) override;
    void drawContactPoint(const btVector3& PointOnB,
                          const btVector3& normalOnB,
                          btScalar distance,
                          int lifeTime,
                          const btVector3& color) override;
    void reportErrorWarning(const char* warningString) override;
    void draw3dText(const btVector3& location, const char* textString) override;
    void setDebugMode(int debugMode) override;
    int  getDebugMode() const override;

private:
    void SubmitLine(const btVector3& from, const btVector3& to, uint32_t abgr);
    static uint32_t ToAbgr(const btVector3& color);

    void DrawShape(const btTransform& transform, const btCollisionShape& shape, uint32_t color);
    void DrawStaticPlane(const btTransform& transform, const btStaticPlaneShape& plane, uint32_t color);
    void DrawBox(const btTransform& transform, const btBoxShape& box, uint32_t color);
    void DrawCapsule(const btTransform& transform, const btCapsuleShape& capsule, uint32_t color);
    void DrawCompound(const btTransform& transform, const btCompoundShape& compound, uint32_t color);

private:
    PhysicsDebugLineBuffer m_lines;
    int m_debugMode = 0;
};