#pragma once

#include <string>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <vector>
#include "Engine/Mesh.h"
#include "Engine/Material.h"

namespace Genesis::Engine {

class Model {
public:
    Model() = default;
    ~Model() = default;

    bool Load(const std::string& path);
    void Draw(const float* transform = nullptr);

    // Material accessors (parsed from scene materials)
    const std::vector<Material>& Materials() const { return m_materials; }
    int GetMaterialIndexForMesh(size_t meshIndex) const;

    void AddMesh(Mesh&& mesh) { m_meshes.push_back(std::move(mesh)); }

private:
    Assimp::Importer m_importer;
    const aiScene* m_scene = nullptr;

    // Extracted CPU-side meshes (drawn via vertex arrays)
    std::vector<Mesh> m_meshes;

    // Parsed materials from the source file (mapped from aiMaterial)
    std::vector<Material> m_materials;
};

} // namespace Genesis::Engine
