#include "ObjLoader.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <filesystem>
#include <cmath>
#include <utility>

#include <tiny_obj_loader.h>

#include "../render/Texture.h"      // tex::LoadTexture2D
#include <bgfx/bgfx.h>

namespace asset {

struct VertexPNUV8 {
    float    x, y, z;
    float    nx, ny, nz;
    uint32_t abgr;   // 0xAABBGGRR (Uint8 normalized en layout)
    float    u, v;
};

static inline uint32_t packColorRGBA8(float r, float g, float b, float a)
{
    auto to8 = [](float v)->uint32_t {
        v = (v < 0.0f) ? 0.0f : (v > 1.0f ? 1.0f : v);
        return (uint32_t)std::lround(v * 255.0f);
    };
    uint32_t R = to8(r), G = to8(g), B = to8(b), A = to8(a);
    // BGFX usa ABGR en Color0 (Uint8 normalized)
    return (A<<24) | (B<<16) | (G<<8) | (R);
}

static inline void computeFaceNormal(const float p0[3], const float p1[3], const float p2[3], float out[3])
{
    const float ux = p1[0]-p0[0], uy = p1[1]-p0[1], uz = p1[2]-p0[2];
    const float vx = p2[0]-p0[0], vy = p2[1]-p0[1], vz = p2[2]-p0[2];
    out[0] = uy*vz - uz*vy;
    out[1] = uz*vx - ux*vz;
    out[2] = ux*vy - uy*vx;
    const float len = std::sqrt(out[0]*out[0] + out[1]*out[1] + out[2]*out[2]);
    if (len > 1e-20f) { out[0]/=len; out[1]/=len; out[2]/=len; }
    else { out[0]=0; out[1]=1; out[2]=0; }
}

static std::string joinPath(const std::string& a, const std::string& b)
{
    std::filesystem::path pa(a), pb(b);
    return (pa / pb).string();
}

bool LoadObjToMesh(const std::string& objPath,
                   const bgfx::VertexLayout& layout,
                   bgfx::TextureHandle fallbackTex,
                   Mesh& outMesh,
                   std::vector<Material>& outMaterials,
                   std::string* outLog,
                   bool flipV,
                   uint32_t* outVertexCount,
                   std::function<bgfx::TextureHandle(const std::string&)> textureLoader)
{
    if (outLog) outLog->clear();
    if (outVertexCount) *outVertexCount = 0;

    outMesh.destroy();
    outMesh.materials.clear();
    for (Material& m : outMaterials) {
        m.destroy();
    }
    outMaterials.clear();
    outMesh.submeshes.clear();

    tinyobj::ObjReaderConfig cfg;
    cfg.triangulate = true;

    // Base dir para buscar .mtl y texturas
    std::filesystem::path p(objPath);
    const std::string baseDir = p.parent_path().string();
    cfg.mtl_search_path = baseDir.empty() ? std::string() : (baseDir + std::string(1, std::filesystem::path::preferred_separator));

    tinyobj::ObjReader reader;
    if (!reader.ParseFromFile(objPath, cfg)) {
        if (outLog) *outLog = reader.Error();
        return false;
    }
    if (!reader.Warning().empty() && outLog) {
        *outLog += reader.Warning();
    }

    const auto& attrib  = reader.GetAttrib();
    const auto& shapes  = reader.GetShapes();
    const auto& materials = reader.GetMaterials();

    std::vector<VertexPNUV8> vertices;
    vertices.reserve(2048);

    std::unordered_map<int, std::vector<uint16_t>> perMaterialIndices;
    std::unordered_map<int, size_t> materialOrderLut;
    std::vector<int> materialOrder;
    size_t totalIndexCount = 0;

    const bool useCustomTextureLoader = static_cast<bool>(textureLoader);

    for (const auto& shape : shapes) {
        size_t indexOffset = 0;
        for (size_t f = 0; f < (size_t)shape.mesh.num_face_vertices.size(); ++f) {
            const size_t fv = (size_t)shape.mesh.num_face_vertices[f]; // debería ser 3 (triangulado)
            if (fv != 3) { indexOffset += fv; continue; }

            int matIdx = -1;
            if (f < shape.mesh.material_ids.size()) {
                matIdx = shape.mesh.material_ids[f];
            }
            if (materialOrderLut.find(matIdx) == materialOrderLut.end()) {
                materialOrderLut[matIdx] = materialOrder.size();
                materialOrder.push_back(matIdx);
            }

            // Posiciones de la cara (para normal si falta)
            float ppos[3][3];

            tinyobj::index_t idx0 = shape.mesh.indices[indexOffset + 0];
            tinyobj::index_t idx1 = shape.mesh.indices[indexOffset + 1];
            tinyobj::index_t idx2 = shape.mesh.indices[indexOffset + 2];

            auto fetchPos = [&](const tinyobj::index_t& idx, float out[3]) {
                const int vi = idx.vertex_index;
                out[0] = attrib.vertices[3*vi + 0];
                out[1] = attrib.vertices[3*vi + 1];
                out[2] = attrib.vertices[3*vi + 2];
            };
            fetchPos(idx0, ppos[0]);
            fetchPos(idx1, ppos[1]);
            fetchPos(idx2, ppos[2]);

            float faceN[3] = {0,1,0};
            const bool hasNormals = !attrib.normals.empty();
            if (!hasNormals ||
                idx0.normal_index < 0 || idx1.normal_index < 0 || idx2.normal_index < 0)
            {
                computeFaceNormal(ppos[0], ppos[1], ppos[2], faceN);
            }

            auto emitVertex = [&](const tinyobj::index_t& idx, const float fallbackN[3]) -> uint16_t {
                VertexPNUV8 v{};
                // Pos
                v.x = attrib.vertices[3*idx.vertex_index + 0];
                v.y = attrib.vertices[3*idx.vertex_index + 1];
                v.z = attrib.vertices[3*idx.vertex_index + 2];
                // Normal
                if (!attrib.normals.empty() && idx.normal_index >= 0) {
                    v.nx = attrib.normals[3*idx.normal_index + 0];
                    v.ny = attrib.normals[3*idx.normal_index + 1];
                    v.nz = attrib.normals[3*idx.normal_index + 2];
                } else {
                    v.nx = fallbackN[0]; v.ny = fallbackN[1]; v.nz = fallbackN[2];
                }
                // UV
                if (!attrib.texcoords.empty() && idx.texcoord_index >= 0) {
                    v.u = attrib.texcoords[2*idx.texcoord_index + 0];
                    v.v = attrib.texcoords[2*idx.texcoord_index + 1];
                    if (flipV) v.v = 1.0f - v.v;
                } else {
                    v.u = v.v = 0.0f;
                }
                // Color (blanco)
                v.abgr = packColorRGBA8(1.0f,1.0f,1.0f,1.0f);

                // push
                const uint16_t newIndex = (uint16_t)vertices.size();
                vertices.push_back(v);
                return newIndex;
            };

            uint16_t i0 = emitVertex(idx0, faceN);
            uint16_t i1 = emitVertex(idx1, faceN);
            uint16_t i2 = emitVertex(idx2, faceN);

            auto& list = perMaterialIndices[matIdx];
            list.push_back(i0);
            list.push_back(i1);
            list.push_back(i2);
            totalIndexCount += 3;

            indexOffset += fv;
        }
    }

    if (vertices.empty() || totalIndexCount == 0) {
        if (outLog) *outLog = "OBJ sin geometría válida.";
        return false;
    }

    std::vector<uint16_t> indices;
    indices.reserve(totalIndexCount);
    std::vector<Submesh> submeshes;
    submeshes.reserve(materialOrder.size());

    for (int matId : materialOrder) {
        auto it = perMaterialIndices.find(matId);
        if (it == perMaterialIndices.end() || it->second.empty()) {
            continue;
        }

        Submesh subset;
        subset.startIndex = (uint32_t)indices.size();
        subset.indexCount = (uint32_t)it->second.size();
        subset.materialIndex = (int)outMaterials.size();

        Material mat;
        mat.reset();
        mat.albedo = fallbackTex;
        mat.ownsTexture = !useCustomTextureLoader;

        if (matId >= 0 && matId < (int)materials.size()) {
            const tinyobj::material_t& srcMat = materials[matId];
            mat.baseTint[0] = srcMat.diffuse[0];
            mat.baseTint[1] = srcMat.diffuse[1];
            mat.baseTint[2] = srcMat.diffuse[2];
            mat.baseTint[3] = 1.0f;

            if (!srcMat.diffuse_texname.empty()) {
                std::string texPath = joinPath(baseDir, srcMat.diffuse_texname);
                if (useCustomTextureLoader) {
                    bgfx::TextureHandle th = textureLoader(texPath);
                    if (bgfx::isValid(th)) {
                        mat.albedo = th;
                        mat.ownsTexture = false;
                    }
                } else {
                    bgfx::TextureHandle th = tex::LoadTexture2D(texPath.c_str(), /*hasMips*/false);
                    if (bgfx::isValid(th)) {
                        mat.albedo = th;
                        mat.ownsTexture = true;
                    }
                }
            }
        }

        outMaterials.push_back(mat);
        submeshes.push_back(subset);

        indices.insert(indices.end(), it->second.begin(), it->second.end());
    }

    if (indices.empty()) {
        if (outLog) *outLog = "OBJ sin índices por material.";
        return false;
    }

    // Crea buffers BGFX
    const bgfx::Memory* vmem = bgfx::copy(vertices.data(), (uint32_t)(vertices.size() * sizeof(VertexPNUV8)));
    const bgfx::Memory* imem = bgfx::copy(indices.data (), (uint32_t)(indices.size()  * sizeof(uint16_t)));

    outMesh.vbh = bgfx::createVertexBuffer(vmem, layout);
    outMesh.ibh = bgfx::createIndexBuffer (imem);
    outMesh.indexCount = (uint32_t)indices.size();
    outMesh.vertexCount = (uint32_t)vertices.size();
    outMesh.submeshes = std::move(submeshes);

    if (outVertexCount) {
        *outVertexCount = (uint32_t)vertices.size();
    }

    if (!bgfx::isValid(outMesh.vbh) || !bgfx::isValid(outMesh.ibh)) {
        outMesh.destroy();
        if (outLog) *outLog = "Fallo al crear buffers de malla.";
        return false;
    }

    return true;
}

} // namespace asset
