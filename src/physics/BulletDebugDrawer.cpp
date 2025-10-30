#include "BulletDebugDrawer.h"

#include <LinearMath/btAabbUtil2.h>
#include <LinearMath/btQuaternion.h>
#include <LinearMath/btTransform.h>
#include <LinearMath/btVector3.h>
#include <btBulletCollisionCommon.h>
#include <btBulletDynamicsCommon.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <limits>

namespace
{
    constexpr uint32_t kContactColorAbgr = 0xff0000ffu; // Red in ABGR
}

BulletDebugDrawer::BulletDebugDrawer()
{
}

void BulletDebugDrawer::BeginFrame()
{
    m_lines.clear();
}

void BulletDebugDrawer::DrawCollisionObject(const btCollisionObject& object, uint32_t color)
{
    const btCollisionShape* shape = object.getCollisionShape();
    if (!shape)
    {
        return;
    }
    DrawShape(object.getWorldTransform(), *shape, color);
}

void BulletDebugDrawer::drawLine(const btVector3& from, const btVector3& to, const btVector3& color)
{
    SubmitLine(from, to, ToAbgr(color));
}

void BulletDebugDrawer::drawContactPoint(const btVector3& pointOnB,
                                         const btVector3& normalOnB,
                                         btScalar /*distance*/,
                                         int /*lifeTime*/,
                                         const btVector3& /*color*/)
{
    btVector3 normal = normalOnB;
    if (normal.length2() < SIMD_EPSILON)
    {
        normal = btVector3(0.0f, 1.0f, 0.0f);
    }
    normal.normalize();
    const btScalar scale = btScalar(0.25f);
    SubmitLine(pointOnB, pointOnB + normal * scale, kContactColorAbgr);
}

void BulletDebugDrawer::reportErrorWarning(const char* warningString)
{
    if (warningString)
    {
        std::fprintf(stderr, "[PhysicsDebug] %s\n", warningString);
    }
}

void BulletDebugDrawer::draw3dText(const btVector3&, const char*)
{
    // Not used.
}

void BulletDebugDrawer::setDebugMode(int debugMode)
{
    m_debugMode = debugMode;
}

int BulletDebugDrawer::getDebugMode() const
{
    return m_debugMode;
}

void BulletDebugDrawer::SubmitLine(const btVector3& from, const btVector3& to, uint32_t abgr)
{
    PhysicsDebugLine line{};
    line.from[0] = static_cast<float>(from.x());
    line.from[1] = static_cast<float>(from.y());
    line.from[2] = static_cast<float>(from.z());
    line.to[0]   = static_cast<float>(to.x());
    line.to[1]   = static_cast<float>(to.y());
    line.to[2]   = static_cast<float>(to.z());
    line.abgr    = abgr;
    m_lines.push_back(line);
}

uint32_t BulletDebugDrawer::ToAbgr(const btVector3& color)
{
    auto clamp01 = [](btScalar value)
    {
        if (value < btScalar(0.0f)) return btScalar(0.0f);
        if (value > btScalar(1.0f)) return btScalar(1.0f);
        return value;
    };

    const btScalar r = clamp01(color.x());
    const btScalar g = clamp01(color.y());
    const btScalar b = clamp01(color.z());

    const uint32_t ri = static_cast<uint32_t>(r * btScalar(255.0f) + btScalar(0.5f));
    const uint32_t gi = static_cast<uint32_t>(g * btScalar(255.0f) + btScalar(0.5f));
    const uint32_t bi = static_cast<uint32_t>(b * btScalar(255.0f) + btScalar(0.5f));

    return 0xff000000u | (bi << 16) | (gi << 8) | ri;
}

void BulletDebugDrawer::DrawShape(const btTransform& transform, const btCollisionShape& shape, uint32_t color)
{
    switch (shape.getShapeType())
    {
    case STATIC_PLANE_PROXYTYPE:
        DrawStaticPlane(transform, static_cast<const btStaticPlaneShape&>(shape), color);
        break;
    case BOX_SHAPE_PROXYTYPE:
        DrawBox(transform, static_cast<const btBoxShape&>(shape), color);
        break;
    case CAPSULE_SHAPE_PROXYTYPE:
    // ✅ Solo deja este, quita los otros dos
        DrawCapsule(transform, static_cast<const btCapsuleShape&>(shape), color);
        break;
    case COMPOUND_SHAPE_PROXYTYPE:
        DrawCompound(transform, static_cast<const btCompoundShape&>(shape), color);
        break;
    default:
    {
        btVector3 aabbMin, aabbMax;
        shape.getAabb(transform, aabbMin, aabbMax);
        btVector3 extent = (aabbMax - aabbMin) * btScalar(0.5f);
        btVector3 center = (aabbMax + aabbMin) * btScalar(0.5f);
        btBoxShape temp(extent);
        btTransform boxTransform;
        boxTransform.setIdentity();
        boxTransform.setOrigin(center);
        DrawBox(boxTransform, temp, color);
        break;
    }
    }
}

void BulletDebugDrawer::DrawStaticPlane(const btTransform& transform, const btStaticPlaneShape& plane, uint32_t color)
{
    const btVector3 normal = plane.getPlaneNormal();
    const btScalar constant = plane.getPlaneConstant();

    btVector3 origin = normal * constant;
    btVector3 u, v;
    btPlaneSpace1(normal, u, v);

    const btScalar extent = btScalar(25.0f);
    std::array<btVector3, 4> corners = {
        origin + ( u + v) * extent,
        origin + ( u - v) * extent,
        origin + (-u - v) * extent,
        origin + (-u + v) * extent,
    };

    btVector3 worldCorners[4];
    for (int i = 0; i < 4; ++i)
    {
        worldCorners[i] = transform * corners[i];
    }

    for (int i = 0; i < 4; ++i)
    {
        SubmitLine(worldCorners[i], worldCorners[(i + 1) % 4], color);
    }

    const int gridLines = 4;
    for (int i = 1; i <= gridLines; ++i)
    {
        const btScalar t = btScalar(i) / btScalar(gridLines + 1);
        btVector3 a = worldCorners[0].lerp(worldCorners[3], t);
        btVector3 b = worldCorners[1].lerp(worldCorners[2], t);
        btVector3 c = worldCorners[0].lerp(worldCorners[1], t);
        btVector3 d = worldCorners[3].lerp(worldCorners[2], t);
        SubmitLine(a, b, color);
        SubmitLine(c, d, color);
    }
}

void BulletDebugDrawer::DrawBox(const btTransform& transform, const btBoxShape& box, uint32_t color)
{
    const btVector3 he = box.getHalfExtentsWithMargin();
    std::array<btVector3, 8> corners = {
        btVector3(-he.x(), -he.y(), -he.z()),
        btVector3( he.x(), -he.y(), -he.z()),
        btVector3( he.x(),  he.y(), -he.z()),
        btVector3(-he.x(),  he.y(), -he.z()),
        btVector3(-he.x(), -he.y(),  he.z()),
        btVector3( he.x(), -he.y(),  he.z()),
        btVector3( he.x(),  he.y(),  he.z()),
        btVector3(-he.x(),  he.y(),  he.z())
    };

    std::array<int, 24> indices = {
        0,1, 1,2, 2,3, 3,0,
        4,5, 5,6, 6,7, 7,4,
        0,4, 1,5, 2,6, 3,7
    };

    std::array<btVector3, 8> worldCorners;
    for (int i = 0; i < 8; ++i)
    {
        worldCorners[i] = transform * corners[i];
    }

    for (size_t i = 0; i < indices.size(); i += 2)
    {
        SubmitLine(worldCorners[indices[i]], worldCorners[indices[i + 1]], color);
    }
}

void BulletDebugDrawer::DrawCapsule(const btTransform& transform, const btCapsuleShape& capsule, uint32_t color)
{
    const btScalar radius = capsule.getRadius();
    const btScalar halfHeight = capsule.getHalfHeight();
    const int upAxis = capsule.getUpAxis();

    const btMatrix3x3 basis = transform.getBasis();
    btVector3 axisY = basis.getColumn(upAxis);
    btVector3 axisX = basis.getColumn((upAxis + 1) % 3);
    btVector3 axisZ = basis.getColumn((upAxis + 2) % 3);

    axisY.normalize();
    axisX.normalize();
    axisZ.normalize();

    const btVector3 center = transform.getOrigin();
    const btVector3 topCenter = center + axisY * halfHeight;
    const btVector3 bottomCenter = center - axisY * halfHeight;

    const int segments = 24;
    for (int i = 0; i < segments; ++i)
    {
        const btScalar theta0 = (btScalar(i) / btScalar(segments)) * SIMD_2_PI;
        const btScalar theta1 = (btScalar(i + 1) / btScalar(segments)) * SIMD_2_PI;

        const btVector3 dir0 = axisX * btCos(theta0) + axisZ * btSin(theta0);
        const btVector3 dir1 = axisX * btCos(theta1) + axisZ * btSin(theta1);

        const btVector3 top0 = topCenter + dir0 * radius;
        const btVector3 top1 = topCenter + dir1 * radius;
        const btVector3 bottom0 = bottomCenter + dir0 * radius;
        const btVector3 bottom1 = bottomCenter + dir1 * radius;

        SubmitLine(top0, top1, color);
        SubmitLine(bottom0, bottom1, color);
        SubmitLine(top0, bottom0, color);
    }

    const int hemiSegments = 12;
    for (int i = 0; i < hemiSegments; ++i)
    {
        const btScalar phi0 = (btScalar(i) / btScalar(hemiSegments)) * SIMD_HALF_PI;
        const btScalar phi1 = (btScalar(i + 1) / btScalar(hemiSegments)) * SIMD_HALF_PI;

        const btScalar sin0 = btSin(phi0);
        const btScalar cos0 = btCos(phi0);
        const btScalar sin1 = btSin(phi1);
        const btScalar cos1 = btCos(phi1);

        btVector3 offset0 = axisX * (cos0 * radius);
        btVector3 offset1 = axisX * (cos1 * radius);
        btVector3 up0 = axisY * (sin0 * radius);
        btVector3 up1 = axisY * (sin1 * radius);

        SubmitLine(topCenter + offset0 + up0, topCenter + offset1 + up1, color);
        SubmitLine(bottomCenter - offset0 - up0, bottomCenter - offset1 - up1, color);

        offset0 = axisZ * (cos0 * radius);
        offset1 = axisZ * (cos1 * radius);

        SubmitLine(topCenter + offset0 + up0, topCenter + offset1 + up1, color);
        SubmitLine(bottomCenter - offset0 - up0, bottomCenter - offset1 - up1, color);
    }
}

void BulletDebugDrawer::DrawCompound(const btTransform& transform, const btCompoundShape& compound, uint32_t color)
{
    const int childCount = compound.getNumChildShapes();
    for (int i = 0; i < childCount; ++i)
    {
        const btCollisionShape* child = compound.getChildShape(i);
        if (!child)
        {
            continue;
        }
        btTransform childTransform = transform * compound.getChildTransform(i);
        DrawShape(childTransform, *child, color);
    }
}