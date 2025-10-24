#pragma once
#include <string>
#include <bgfx/bgfx.h>
#include "../asset/Mesh.h"
#include "../render/Material.h"

namespace asset {

// Carga un .obj (triangula, lee .mtl si existe) y crea buffers BGFX.
// - layout: debe ser Position + Normal + Color0(Uint8, normalized) + TexCoord0
// - fallbackTex: textura a usar si no hay difusa en el material
// - flipV: muchos OBJ esperan V invertida (suele quedar mejor en D3D)
// Devuelve true en éxito.
bool LoadObjToMesh(const std::string& objPath,
                   const bgfx::VertexLayout& layout,
                   bgfx::TextureHandle fallbackTex,
                   Mesh& outMesh,
                   Material& outMat,
                   std::string* outLog = nullptr,
                   bool flipV = true);

} // namespace asset
