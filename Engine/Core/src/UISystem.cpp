#include "engine/UI.h"
#include "engine/Scene.h"
#include "engine/IGraphics.h"
#include "engine/IInput.h"
#include "engine/Engine.h"
#include "engine/SubsystemManager.h"
#include "engine/Texture.h"
#include <SDL.h> // For SDL_BUTTON_LEFT
#include "imgui.h"

namespace Genesis::Engine {

void UISystem::Update(Scene& scene, double /*dt*/) {
    auto input = GetInputSubsystem();
    if (!input) return;

    int mouseX, mouseY;
    input->GetMousePosition(mouseX, mouseY);
    bool mouseDown = input->IsMouseButtonDown(SDL_BUTTON_LEFT);

    auto view = scene.Registry().view<UIComponent>();
    view.each([&](auto& ui) {
        if (ui.type == UIType::Button) {
            bool hover = (mouseX >= ui.x && mouseX <= ui.x + ui.width &&
                          mouseY >= ui.y && mouseY <= ui.y + ui.height);
            
            if (hover) {
                if (!ui.isHovered) {
                    // OnEnter
                }
                ui.isHovered = true;

                if (mouseDown) {
                    ui.isPressed = true;
                } else if (ui.isPressed) {
                    // Clicked (released while hovered)
                    if (ui.onClick) ui.onClick();
                    ui.isPressed = false;
                }
            } else {
                ui.isHovered = false;
                ui.isPressed = false;
            }
        }
    });
}

void UISystem::Render(Scene& scene, IGraphicsAPI* renderer) {
    if (!renderer) return;

    auto view = scene.Registry().view<UIComponent>();
    view.each([&](auto& ui) {
        if (ui.type == UIType::Text && !ui.text.empty()) {
            ImGui::GetBackgroundDrawList()->AddText(
                ImVec2(ui.x, ui.y), 
                ImGui::GetColorU32(ImVec4(ui.color[0], ui.color[1], ui.color[2], ui.color[3])), 
                ui.text.c_str()
            );
        }
        else if (ui.texture) {
            float r = ui.color[0];
            float g = ui.color[1];
            float b = ui.color[2];
            float a = ui.color[3];

            if (ui.type == UIType::Button) {
                if (ui.isPressed) {
                    r *= 0.7f; g *= 0.7f; b *= 0.7f;
                } else if (ui.isHovered) {
                    r *= 0.9f; g *= 0.9f; b *= 0.9f;
                }
            }

            uint32_t ur = (uint32_t)(r * 255.0f);
            uint32_t ug = (uint32_t)(g * 255.0f);
            uint32_t ub = (uint32_t)(b * 255.0f);
            uint32_t ua = (uint32_t)(a * 255.0f);
            uint32_t color = (ua << 24) | (ur << 16) | (ug << 8) | ub;

            // Draw texture
            renderer->DrawTexture(ui.texture.get(), ui.x, ui.y, ui.width, ui.height, 0.0f, 0.0f, 1.0f, 1.0f, color);
        }
    });
}

} // namespace Genesis::Engine
