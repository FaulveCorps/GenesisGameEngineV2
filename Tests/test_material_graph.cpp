#include "catch_amalgamated.hpp"
#include "engine/MaterialGraph.h"

#include <filesystem>

using namespace Genesis::Engine;
using Catch::Approx;

TEST_CASE("MaterialGraph save/load roundtrip", "[material][graph]") {
    MaterialGraph graph;
    graph.name = "TestGraph";

    MaterialNode colorNode;
    colorNode.id = 1;
    colorNode.type = MaterialNodeType::ConstantColor;
    colorNode.color = {0.2f, 0.4f, 0.6f, 1.0f};

    MaterialNode outputNode;
    outputNode.id = 2;
    outputNode.type = MaterialNodeType::Output;

    graph.nodes.push_back(colorNode);
    graph.nodes.push_back(outputNode);
    graph.outputNode = outputNode.id;
    graph.links.push_back({colorNode.id, 0, outputNode.id, 0});

    const auto path = std::filesystem::temp_directory_path() / "genesis_material_graph_test.matgraph";
    REQUIRE(SaveMaterialGraph(graph, path.string()));

    MaterialGraph loaded;
    REQUIRE(LoadMaterialGraph(path.string(), loaded));

    REQUIRE(loaded.name == graph.name);
    REQUIRE(loaded.nodes.size() == graph.nodes.size());
    REQUIRE(loaded.links.size() == graph.links.size());
    REQUIRE(loaded.outputNode == graph.outputNode);

    const auto* loadedColor = FindMaterialNode(loaded, colorNode.id);
    REQUIRE(loadedColor != nullptr);
    REQUIRE(loadedColor->color[0] == Approx(colorNode.color[0]));
    REQUIRE(loadedColor->color[1] == Approx(colorNode.color[1]));
    REQUIRE(loadedColor->color[2] == Approx(colorNode.color[2]));
    REQUIRE(loadedColor->color[3] == Approx(colorNode.color[3]));

    std::error_code ec;
    std::filesystem::remove(path, ec);
}
