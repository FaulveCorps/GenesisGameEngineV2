#pragma once

#include <string>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>

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
};

} // namespace Genesis::Engine
