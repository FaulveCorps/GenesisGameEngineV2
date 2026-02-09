#include "engine/MaterialGraph.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace Genesis::Engine {
namespace {
    std::string ToLowerCopy(const std::string& value) {
        std::string out = value;
        std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return out;
    }

    std::string TrimLeadingSpace(std::string value) {
        if (!value.empty() && value[0] == ' ') {
            value.erase(0, 1);
        }
        return value;
    }
}

std::string MaterialNodeTypeToString(MaterialNodeType type) {
    switch (type) {
        case MaterialNodeType::Output: return "Output";
        case MaterialNodeType::ConstantColor: return "ConstantColor";
        case MaterialNodeType::ConstantFloat: return "ConstantFloat";
        case MaterialNodeType::Texture2D: return "Texture2D";
        case MaterialNodeType::Multiply: return "Multiply";
        case MaterialNodeType::Add: return "Add";
        default: return "Unknown";
    }
}

bool MaterialNodeTypeFromString(const std::string& value, MaterialNodeType& outType) {
    const std::string lower = ToLowerCopy(value);
    if (lower == "output") { outType = MaterialNodeType::Output; return true; }
    if (lower == "constantcolor") { outType = MaterialNodeType::ConstantColor; return true; }
    if (lower == "constantfloat") { outType = MaterialNodeType::ConstantFloat; return true; }
    if (lower == "texture2d") { outType = MaterialNodeType::Texture2D; return true; }
    if (lower == "multiply") { outType = MaterialNodeType::Multiply; return true; }
    if (lower == "add") { outType = MaterialNodeType::Add; return true; }
    return false;
}

MaterialNode* FindMaterialNode(MaterialGraph& graph, int id) {
    for (auto& node : graph.nodes) {
        if (node.id == id) return &node;
    }
    return nullptr;
}

const MaterialNode* FindMaterialNode(const MaterialGraph& graph, int id) {
    for (const auto& node : graph.nodes) {
        if (node.id == id) return &node;
    }
    return nullptr;
}

bool LoadMaterialGraph(const std::string& path, MaterialGraph& outGraph) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }

    MaterialGraph graph;
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::istringstream iss(line);
        std::string tag;
        iss >> tag;
        if (tag == "NAME") {
            std::string rest;
            std::getline(iss, rest);
            graph.name = TrimLeadingSpace(rest);
        } else if (tag == "NODE") {
            MaterialNode node;
            std::string typeStr;
            iss >> node.id >> typeStr >> node.x >> node.y;
            MaterialNodeType type;
            if (MaterialNodeTypeFromString(typeStr, type)) {
                node.type = type;
            }
            graph.nodes.push_back(std::move(node));
        } else if (tag == "COLOR") {
            int id = 0;
            float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;
            iss >> id >> r >> g >> b >> a;
            if (auto* node = FindMaterialNode(graph, id)) {
                node->color = {r, g, b, a};
            }
        } else if (tag == "VALUE") {
            int id = 0;
            float v = 0.0f;
            iss >> id >> v;
            if (auto* node = FindMaterialNode(graph, id)) {
                node->value = v;
            }
        } else if (tag == "TEXTURE") {
            int id = 0;
            iss >> id;
            std::string rest;
            std::getline(iss, rest);
            if (auto* node = FindMaterialNode(graph, id)) {
                node->texturePath = TrimLeadingSpace(rest);
            }
        } else if (tag == "LINK") {
            MaterialLink link;
            iss >> link.fromNode >> link.fromSlot >> link.toNode >> link.toSlot;
            graph.links.push_back(std::move(link));
        } else if (tag == "OUTPUT") {
            iss >> graph.outputNode;
        }
    }

    if (graph.outputNode < 0) {
        for (const auto& node : graph.nodes) {
            if (node.type == MaterialNodeType::Output) {
                graph.outputNode = node.id;
                break;
            }
        }
    }

    outGraph = std::move(graph);
    return true;
}

bool SaveMaterialGraph(const MaterialGraph& graph, const std::string& path) {
    std::ofstream file(path, std::ios::trunc);
    if (!file.is_open()) {
        return false;
    }

    file << "# MaterialGraph\n";
    file << "NAME " << graph.name << "\n";
    if (graph.outputNode >= 0) {
        file << "OUTPUT " << graph.outputNode << "\n";
    }

    for (const auto& node : graph.nodes) {
        file << "NODE " << node.id << " " << MaterialNodeTypeToString(node.type) << " " << node.x << " " << node.y << "\n";
        if (node.type == MaterialNodeType::ConstantColor) {
            file << "COLOR " << node.id << " " << node.color[0] << " " << node.color[1] << " " << node.color[2] << " " << node.color[3] << "\n";
        } else if (node.type == MaterialNodeType::ConstantFloat) {
            file << "VALUE " << node.id << " " << node.value << "\n";
        } else if (node.type == MaterialNodeType::Texture2D) {
            file << "TEXTURE " << node.id << " " << node.texturePath << "\n";
        }
    }

    for (const auto& link : graph.links) {
        file << "LINK " << link.fromNode << " " << link.fromSlot << " " << link.toNode << " " << link.toSlot << "\n";
    }

    return true;
}

} // namespace Genesis::Engine
