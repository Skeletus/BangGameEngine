#pragma once
#include <bgfx/bgfx.h>

struct Material {
    // Albedo / UV
    float baseTint[4] = {1, 1, 1, 1};   // multiplicador de albedo
    float uvScale[4]  = {1, 1, 0, 0};   // tiling simple
    bgfx::TextureHandle albedo = BGFX_INVALID_HANDLE;
    bool ownsTexture = false;           // si true, destroy() liberará la textura

    // Especular (lo que falta)
    // specParams.x = shininess (p.ej. 32, 64, 128)
    // specParams.y = intensity  (0..1)
    // z,w libres por ahora
    float specParams[4] = {32.0f, 0.35f, 0.0f, 0.0f};
    float specColor [4] = {1.0f, 1.0f, 1.0f, 0.0f};

    void reset() {
        baseTint[0]=baseTint[1]=baseTint[2]=1.0f; baseTint[3]=1.0f;
        uvScale[0]=1.0f; uvScale[1]=1.0f; uvScale[2]=uvScale[3]=0.0f;

        specParams[0] = 32.0f;  // shininess
        specParams[1] = 0.35f;  // intensity
        specParams[2] = 0.0f;   // libre
        specParams[3] = 0.0f;   // libre

        specColor[0] = specColor[1] = specColor[2] = 1.0f;
        specColor[3] = 0.0f;

        albedo = BGFX_INVALID_HANDLE;
        ownsTexture = false;
    }

    void destroy() {
        if (ownsTexture && bgfx::isValid(albedo)) {
            bgfx::destroy(albedo);
        }
        albedo = BGFX_INVALID_HANDLE;
        ownsTexture = false;
    }
};
