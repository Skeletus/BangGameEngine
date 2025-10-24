#pragma once
#include <cstdint>

class Camera {
public:
    Camera();

    // Integración sencilla con juego
    void SetPosition(float x, float y, float z);
    void AddYawPitch(float dyaw, float dpitch); // radianes
    void Move(float dx, float dy, float dz);    // en espacio local

    // Parámetros de proyección
    float GetFovYDeg() const { return m_fovYDeg; }
    float GetNear()   const { return m_nearZ; }
    float GetFar()    const { return m_farZ;  }

    // Matrices (column-major 4x4) listas para bgfx
    void GetView(float outView[16]) const;

    void GetPosition(float& x, float& y, float& z) const {
        x = m_pos[0]; y = m_pos[1]; z = m_pos[2];
    }

private:
    float m_pos[3];            // x,y,z
    float m_yaw;               // rad
    float m_pitch;             // rad
    float m_fovYDeg = 60.0f;   // grados
    float m_nearZ   = 0.1f;
    float m_farZ    = 1000.0f;

    // Helpers
    static void clampPitch(float& p);
};
