#pragma once
#include <string>

namespace Genesis::Engine {
    class Scene;
    
    class SceneLoader {
    public:
        static bool LoadScene(Scene& scene, const std::string& filePath);
    };
}
