#include "engine/Model.h"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <iostream>
#include <GL/gl.h>

namespace Genesis::Engine {

bool Model::Load(const std::string& path) {
    m_scene = m_importer.ReadFile(path,
        aiProcess_Triangulate | aiProcess_GenNormals | aiProcess_JoinIdenticalVertices);

    if (!m_scene || m_scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !m_scene->mRootNode) {
        std::cerr << "Assimp failed to load model: " << path << std::endl;
        return false;
    }
    return true;
}

void Model::Draw() {
    if (!m_scene) return;

    // Simple immediate-mode draw (safe for a small sample/model).
    for (unsigned int m = 0; m < m_scene->mNumMeshes; ++m) {
        const aiMesh* mesh = m_scene->mMeshes[m];
        if (!mesh) continue;

        for (unsigned int f = 0; f < mesh->mNumFaces; ++f) {
            const aiFace& face = mesh->mFaces[f];
            glBegin(GL_TRIANGLES);
            for (unsigned int i = 0; i < face.mNumIndices; ++i) {
                unsigned int idx = face.mIndices[i];
                const aiVector3D& v = mesh->mVertices[idx];
                const aiVector3D& n = (mesh->HasNormals() ? mesh->mNormals[idx] : aiVector3D(0,0,1));
                glNormal3f(n.x, n.y, n.z);
                glVertex3f(v.x, v.y, v.z);
            }
            glEnd();
            // count this triangle draw
            Genesis::Engine::Stats::AddDrawCalls(1);
        }
    }
}

} // namespace Genesis::Engine
