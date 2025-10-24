#include "Camera.h"
#include <bx/math.h>
#include <cmath>
#include <algorithm>

namespace {
    // forward desde yaw/pitch con Y-up (radianes)
    inline void yawPitchToForward(float yaw, float pitch, float out[3]) {
        const float cy = std::cos(yaw);
        const float sy = std::sin(yaw);
        const float cp = std::cos(pitch);
        const float sp = std::sin(pitch);
        out[0] = cy * cp; // x
        out[1] = sp;      // y
        out[2] = sy * cp; // z
    }
}

Camera::Camera() {
    m_pos[0] = 0.0f;
    m_pos[1] = 2.0f;
    m_pos[2] = -7.0f;          // detrás del origen

    m_yaw   = bx::kPiHalf;     // mirar a +Z
    m_pitch = 0.0f;
}


void Camera::SetPosition(float x, float y, float z) {
    m_pos[0] = x; m_pos[1] = y; m_pos[2] = z;
}

void Camera::AddYawPitch(float dyaw, float dpitch) {
    m_yaw   += dyaw;
    m_pitch += dpitch;
    clampPitch(m_pitch);
}

void Camera::Move(float dx, float dy, float dz) {
    float fwd[3]; yawPitchToForward(m_yaw, m_pitch, fwd);

    // right = normalize( cross(fwd, up) ), up=(0,1,0)
    float right[3] = { fwd[2], 0.0f, -fwd[0] };
    const float lenXZ = std::sqrt(std::max(right[0]*right[0] + right[2]*right[2], 1e-20f));
    const float invLen = 1.0f / lenXZ;
    right[0] *= invLen; right[2] *= invLen;

    const float up[3] = {0.0f, 1.0f, 0.0f};

    // Traslación local: dx * right + dy * up + dz * fwd
    m_pos[0] += dx * right[0] + dy * up[0] + dz * fwd[0];
    m_pos[1] += dx * right[1] + dy * up[1] + dz * fwd[1];
    m_pos[2] += dx * right[2] + dy * up[2] + dz * fwd[2];
}

void Camera::GetView(float outView[16]) const {
    float fwd[3]; yawPitchToForward(m_yaw, m_pitch, fwd);

    const bx::Vec3 eye = { m_pos[0], m_pos[1], m_pos[2] };
    const bx::Vec3 at  = { m_pos[0] + fwd[0], m_pos[1] + fwd[1], m_pos[2] + fwd[2] };
    const bx::Vec3 up  = { 0.0f, 1.0f, 0.0f };

    // Usa la sobrecarga de bx::Vec3 (evita bx::load)
    bx::mtxLookAt(outView, eye, at, up);
}

void Camera::clampPitch(float& p) {
    const float limit = bx::toRad(89.0f);
    if (p >  limit) p =  limit;
    if (p < -limit) p = -limit;
}
