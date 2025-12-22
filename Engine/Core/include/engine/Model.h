#pragma once

#include <string>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <vector>
#include "engine/Mesh.h"

namespace Genesis::Engine {

class Model {
public:
    Model() = default;
    ~Model() = default;

    bool Load(const std::string& path);
    void Draw();

private:
    Assimp::Importer m_importer;
    const aiScene* m_scene = nullptr;

    // Extracted CPU-side meshes (drawn via vertex arrays)
    std::vector<Mesh> m_meshes;
};

} // namespace Genesis::Engine
