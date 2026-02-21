#include "catch_amalgamated.hpp"
#include "engine/RenderGraph.h"

using namespace Genesis::Engine;

namespace {
size_t FindPassIndex(const RenderGraph& graph,
                     const std::vector<RenderGraph::PassHandle>& order,
                     const std::string& name) {
    for (size_t i = 0; i < order.size(); ++i) {
        const auto* pass = graph.GetPass(order[i]);
        if (pass && pass->name == name) {
            return i;
        }
    }
    return static_cast<size_t>(-1);
}
}

TEST_CASE("RenderGraph compiles a simple chain", "[rendergraph]") {
    RenderGraph graph;
    auto gbuffer = graph.CreateResource("GBuffer", RenderGraph::ResourceType::Texture);

    graph.AddPass("GBufferPass", {}, { gbuffer });
    graph.AddPass("LightingPass", { gbuffer }, {});

    std::string error;
    REQUIRE(graph.Compile(&error));

    const auto& order = graph.GetExecutionOrder();
    REQUIRE(order.size() == 2);

    size_t gbufferIndex = FindPassIndex(graph, order, "GBufferPass");
    size_t lightingIndex = FindPassIndex(graph, order, "LightingPass");
    REQUIRE(gbufferIndex < lightingIndex);
}

TEST_CASE("RenderGraph rejects read-before-write", "[rendergraph]") {
    RenderGraph graph;
    auto depth = graph.CreateResource("Depth", RenderGraph::ResourceType::Texture);

    graph.AddPass("DepthRead", { depth }, {});

    std::string error;
    REQUIRE_FALSE(graph.Compile(&error));
    REQUIRE(error.find("reads resource") != std::string::npos);
}

TEST_CASE("RenderGraph allows external resource reads", "[rendergraph]") {
    RenderGraph graph;
    auto backbuffer = graph.ImportResource("Backbuffer", RenderGraph::ResourceType::Texture);

    graph.AddPass("Blit", { backbuffer }, {});

    std::string error;
    REQUIRE(graph.Compile(&error));
}

TEST_CASE("RenderGraph orders writes after readers", "[rendergraph]") {
    RenderGraph graph;
    auto gbuffer = graph.CreateResource("GBuffer", RenderGraph::ResourceType::Texture);

    graph.AddPass("GBufferPass", {}, { gbuffer });
    graph.AddPass("Resolve", { gbuffer }, {});
    graph.AddPass("TAA", { gbuffer }, {});
    graph.AddPass("PostProcess", {}, { gbuffer });

    std::string error;
    REQUIRE(graph.Compile(&error));

    const auto& order = graph.GetExecutionOrder();
    size_t resolveIndex = FindPassIndex(graph, order, "Resolve");
    size_t taaIndex = FindPassIndex(graph, order, "TAA");
    size_t postIndex = FindPassIndex(graph, order, "PostProcess");

    REQUIRE(postIndex > resolveIndex);
    REQUIRE(postIndex > taaIndex);
}
