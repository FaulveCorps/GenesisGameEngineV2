#include <catch_amalgamated.hpp>
#include <fstream>
#include <sstream>

#include "engine/Model.h"
#include "engine/Material.h"

using namespace Genesis::Engine;
using Catch::Approx;

TEST_CASE("glTF PBR material file contains expected values", "[gltf][material]") {
    std::string assetPath = std::string(PROJECT_SOURCE_DIR) + "/Assets/models/pbr_sample.gltf";
    // Simple, robust parser for this small sample: read file as text and extract numeric values
    std::ifstream ifs(assetPath);
    REQUIRE(ifs.good());
    std::string s((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

    auto findArray = [&](const std::string& key){
        size_t pos = s.find(key);
        REQUIRE(pos != std::string::npos);
        pos = s.find('[', pos);
        REQUIRE(pos != std::string::npos);
        size_t end = s.find(']', pos);
        REQUIRE(end != std::string::npos);
        std::string arr = s.substr(pos+1, end-pos-1);
        std::vector<float> out;
        std::stringstream ss(arr);
        while (ss.good()) {
            std::string tok;
            if (!std::getline(ss, tok, ',')) break;
            try {
                out.push_back(std::stof(tok));
            } catch (...) { }
        }
        return out;
    };

    auto baseColor = findArray("\"baseColorFactor\"");
    REQUIRE(baseColor.size() == 4);
    REQUIRE(baseColor[0] == Approx(0.3f));
    REQUIRE(baseColor[1] == Approx(0.6f));
    REQUIRE(baseColor[2] == Approx(0.9f));
    // metallicFactor and roughnessFactor are scalar values; simple string parse
    auto findScalar = [&](const std::string& key){
        size_t pos = s.find(key);
        REQUIRE(pos != std::string::npos);
        size_t colon = s.find(':', pos);
        REQUIRE(colon != std::string::npos);
        size_t comma = s.find_first_of(",}\n", colon);
        std::string tok = s.substr(colon+1, comma-colon-1);
        return std::stof(tok);
    };

    float metallic = findScalar("\"metallicFactor\"");
    float roughness = findScalar("\"roughnessFactor\"");
    REQUIRE(metallic == Approx(0.5f));
    REQUIRE(roughness == Approx(0.25f));
}
