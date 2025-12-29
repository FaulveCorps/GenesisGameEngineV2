#include <catch_amalgamated.hpp>

#include "engine/Material.h"

using namespace Genesis::Engine;
using Catch::Approx;

TEST_CASE("Material defaults and basic properties", "[material]") {
    Material m;
    REQUIRE(m.baseColor[0] == Approx(1.0f));
    REQUIRE(m.baseColor[1] == Approx(1.0f));
    REQUIRE(m.baseColor[2] == Approx(1.0f));
    REQUIRE(m.baseColor[3] == Approx(1.0f));
    REQUIRE(m.metallic == Approx(0.0f));
    REQUIRE(m.roughness == Approx(1.0f));
    REQUIRE(m.baseColorTexture.empty());
}
