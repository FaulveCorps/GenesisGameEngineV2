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
    // Anchor/pivot layout (screen space). When enabled, position is treated as
    // offset from the anchor point (0..1), and pivot shifts by width/height.
    bool useAnchors = false;
    float anchorX = 0.0f, anchorY = 0.0f; // 0..1 (screen normalized)
    float pivotX = 0.0f, pivotY = 0.0f;   // 0..1 (0=top-left, 0.5=center)
    std::shared_ptr<Texture> texture;
    std::string texturePath;
    float backgroundColor[4] = {0.2f, 0.2f, 0.2f, 1.0f};
    bool drawBackground = true;
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
