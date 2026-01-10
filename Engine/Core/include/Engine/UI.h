#pragma once

#include <string>
#include <memory>
#include <functional>

namespace Genesis::Engine {

class Texture;

enum class UIType {
    Image,
    Button,
    Text // Placeholder
};

struct UIComponent {
    UIType type = UIType::Image;
    float x = 0.0f, y = 0.0f;
    float width = 100.0f, height = 100.0f;
    std::shared_ptr<Texture> texture;
    float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    std::string text; // For text/button
    std::function<void()> onClick;
    bool isHovered = false;
    bool isPressed = false;
};

class Scene;
class IGraphicsAPI;

class UISystem {
public:
    static void Update(Scene& scene, double dt);
    static void Render(Scene& scene, IGraphicsAPI* renderer);
};

} // namespace Genesis::Engine
