#pragma once

#include <array>
#include <string>
#include <vector>

namespace Genesis::Engine {

enum class MaterialNodeType {
    Output,
    ConstantColor,
    ConstantFloat,
    Texture2D,
    Multiply,
    Add
};

struct MaterialNode {
    int id = 0;
    MaterialNodeType type = MaterialNodeType::ConstantColor;
    float x = 0.0f;
    float y = 0.0f;

    std::array<float, 4> color{1.0f, 1.0f, 1.0f, 1.0f};
    float value = 1.0f;
    std::string texturePath;
};

struct MaterialLink {
    int fromNode = 0;
    int fromSlot = 0;
    int toNode = 0;
    int toSlot = 0;
};

struct MaterialGraph {
    std::string name = "MaterialGraph";
    std::vector<MaterialNode> nodes;
    std::vector<MaterialLink> links;
    int outputNode = -1;
};

std::string MaterialNodeTypeToString(MaterialNodeType type);
bool MaterialNodeTypeFromString(const std::string& value, MaterialNodeType& outType);

MaterialNode* FindMaterialNode(MaterialGraph& graph, int id);
const MaterialNode* FindMaterialNode(const MaterialGraph& graph, int id);

bool LoadMaterialGraph(const std::string& path, MaterialGraph& outGraph);
bool SaveMaterialGraph(const MaterialGraph& graph, const std::string& path);

} // namespace Genesis::Engine
