#include "engine/Model.h"
#include "engine/Stats.h"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <SDL.h>
#include <iostream>


namespace Genesis::Engine {

bool Model::Load(const std::string& path) {
    m_scene = m_importer.ReadFile(path,
        aiProcess_Triangulate | aiProcess_GenNormals | aiProcess_JoinIdenticalVertices);

    if (!m_scene || m_scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !m_scene->mRootNode) {
        std::cerr << "Assimp failed to load model: " << path << std::endl;
        std::cerr << "Assimp error: " << m_importer.GetErrorString() << std::endl;
        return false;
    }
    std::cout << "Model loaded: " << path << ", meshes: " << m_scene->mNumMeshes << ", materials: " << m_scene->mNumMaterials << std::endl;

    // Parse materials from the Assimp scene into our Material objects
    m_materials.clear();
    if (m_scene->mNumMaterials > 0) {
        m_materials.reserve(m_scene->mNumMaterials);
        for (unsigned int mi = 0; mi < m_scene->mNumMaterials; ++mi) {
            const aiMaterial* am = m_scene->mMaterials[mi];
            Material mat;

            // Base color (PBR base color or fallback to diffuse)
            aiColor4D baseColor(1.0f,1.0f,1.0f,1.0f);
            if (AI_SUCCESS == am->Get(AI_MATKEY_BASE_COLOR, baseColor)) {
                mat.baseColor = { baseColor.r, baseColor.g, baseColor.b, baseColor.a };
            } else {
                aiColor3D diff(1.0f,1.0f,1.0f);
                if (AI_SUCCESS == am->Get(AI_MATKEY_COLOR_DIFFUSE, diff)) {
                    mat.baseColor = { diff.r, diff.g, diff.b, 1.0f };
                }
            }

            // Metallic & roughness
            ai_real metallic = 0.0f;
            if (AI_SUCCESS == am->Get(AI_MATKEY_METALLIC_FACTOR, metallic)) mat.metallic = static_cast<float>(metallic);
            ai_real roughness = 1.0f;
            if (AI_SUCCESS == am->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness)) mat.roughness = static_cast<float>(roughness);

            // Textures (store paths if present)
            aiString texPath;
            if (AI_SUCCESS == am->GetTexture(aiTextureType_BASE_COLOR, 0, &texPath)) mat.baseColorTexture = texPath.C_Str();
            if (AI_SUCCESS == am->GetTexture(aiTextureType_NORMALS, 0, &texPath)) mat.normalTexture = texPath.C_Str();

            m_materials.push_back(std::move(mat));
        }
    }

    // Convert aiMeshes into our Mesh objects (CPU-side arrays)
    m_meshes.clear();
    m_meshes.reserve(m_scene->mNumMeshes);

    for (unsigned int mi = 0; mi < m_scene->mNumMeshes; ++mi) {
        const aiMesh* mesh = m_scene->mMeshes[mi];
        if (!mesh) continue;

        std::vector<float> verts;
        std::vector<float> norms;
        std::vector<uint32_t> idxs;

        verts.reserve(mesh->mNumVertices * 3);
        if (mesh->HasNormals()) norms.reserve(mesh->mNumVertices * 3);

        for (unsigned int v = 0; v < mesh->mNumVertices; ++v) {
            const aiVector3D& pv = mesh->mVertices[v];
            verts.push_back(pv.x);
            verts.push_back(pv.y);
            verts.push_back(pv.z);
            if (mesh->HasNormals()) {
                const aiVector3D& n = mesh->mNormals[v];
                norms.push_back(n.x);
                norms.push_back(n.y);
                norms.push_back(n.z);
            }
        }

        for (unsigned int f = 0; f < mesh->mNumFaces; ++f) {
            const aiFace& face = mesh->mFaces[f];
            if (face.mNumIndices != 3) continue; // should be triangulated
            idxs.push_back(face.mIndices[0]);
            idxs.push_back(face.mIndices[1]);
            idxs.push_back(face.mIndices[2]);
        }

        std::cout << "Mesh " << mi << ": verts=" << verts.size()/3 << ", norms=" << norms.size()/3 << ", tris=" << idxs.size()/3 << std::endl;

        Mesh m;
        m.SetData(verts, norms, idxs);
        std::cout << "Model::Load -> about to UploadToGPU (SDL_GL_GetCurrentContext=" << (void*)SDL_GL_GetCurrentContext() << ")" << std::endl;
        m.UploadToGPU();
        std::cout << "Model::Load -> returned from UploadToGPU" << std::endl;
        m_meshes.push_back(std::move(m));
    }

    return true;
}

void Model::Draw() {
    for (const auto& m : m_meshes) {
        m.Draw();
        Genesis::Engine::Stats::AddDrawCalls((int)m.GetTriangleCount());
    }
}

int Model::GetMaterialIndexForMesh(size_t meshIndex) const {
    if (!m_scene) return -1;
    if (meshIndex >= static_cast<size_t>(m_scene->mNumMeshes)) return -1;
    return static_cast<int>(m_scene->mMeshes[meshIndex]->mMaterialIndex);
}

} // namespace Genesis::Engine
