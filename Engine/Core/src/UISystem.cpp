#include "engine/UI.h"
#include "engine/Scene.h"
#include "engine/IGraphics.h"
#include "engine/IInput.h"
#include "engine/Engine.h"
#include "engine/SubsystemManager.h"
#include "engine/Texture.h"
#include <SDL.h> // For SDL_BUTTON_LEFT
#include "imgui.h"
#include <algorithm>

namespace Genesis::Engine {

static void ResolveUIRect(const UIComponent& ui, float screenW, float screenH, float elemW, float elemH,
                          float& outX, float& outY, float& outW, float& outH) {
    outW = elemW;
    outH = elemH;

    if (ui.useAnchors && screenW > 0.0f && screenH > 0.0f) {
        float anchorX = std::clamp(ui.anchorX, 0.0f, 1.0f);
        float anchorY = std::clamp(ui.anchorY, 0.0f, 1.0f);
        float pivotX = std::clamp(ui.pivotX, 0.0f, 1.0f);
        float pivotY = std::clamp(ui.pivotY, 0.0f, 1.0f);
        outX = anchorX * screenW + ui.x - pivotX * outW;
        outY = anchorY * screenH + ui.y - pivotY * outH;
    } else {
        outX = ui.x;
        outY = ui.y;
    }
}

void UISystem::Update(Scene& scene, double /*dt*/) {
    auto input = GetInputSubsystem();
    if (!input) return;

    int mouseX, mouseY;
    input->GetMousePosition(mouseX, mouseY);
    bool mouseDown = input->IsMouseButtonDown(SDL_BUTTON_LEFT);

    ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    float screenW = displaySize.x;
    float screenH = displaySize.y;

    auto view = scene.Registry().view<UIComponent>();
    view.each([&](auto& ui) {
        if (ui.type == UIType::Button) {
            float rx = 0.0f, ry = 0.0f, rw = ui.width, rh = ui.height;
            ResolveUIRect(ui, screenW, screenH, ui.width, ui.height, rx, ry, rw, rh);
            bool hover = (mouseX >= rx && mouseX <= rx + rw &&
                          mouseY >= ry && mouseY <= ry + rh);
            
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

    ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    float screenW = displaySize.x;
    float screenH = displaySize.y;

    auto view = scene.Registry().view<UIComponent>();
    view.each([&](auto& ui) {
        if (ui.type == UIType::Text && !ui.text.empty()) {
            ImVec2 textSize = ImGui::CalcTextSize(ui.text.c_str());
            float rx = 0.0f, ry = 0.0f, rw = textSize.x, rh = textSize.y;
            ResolveUIRect(ui, screenW, screenH, rw, rh, rx, ry, rw, rh);
            ImGui::GetBackgroundDrawList()->AddText(
                ImVec2(rx, ry),
                ImGui::GetColorU32(ImVec4(ui.color[0], ui.color[1], ui.color[2], ui.color[3])),
                ui.text.c_str()
            );
            return;
        }

        if (ui.type == UIType::Button) {
            float rx = 0.0f, ry = 0.0f, rw = ui.width, rh = ui.height;
            ResolveUIRect(ui, screenW, screenH, ui.width, ui.height, rx, ry, rw, rh);

            if (ui.drawBackground) {
                float br = ui.backgroundColor[0];
                float bg = ui.backgroundColor[1];
                float bb = ui.backgroundColor[2];
                float ba = ui.backgroundColor[3];
                if (ui.isPressed) {
                    br *= 0.7f; bg *= 0.7f; bb *= 0.7f;
                } else if (ui.isHovered) {
                    br *= 0.9f; bg *= 0.9f; bb *= 0.9f;
                }
                ImGui::GetBackgroundDrawList()->AddRectFilled(
                    ImVec2(rx, ry), ImVec2(rx + rw, ry + rh),
                    ImGui::GetColorU32(ImVec4(br, bg, bb, ba))
                );
            }

            if (ui.texture) {
                float r = ui.color[0];
                float g = ui.color[1];
                float b = ui.color[2];
                float a = ui.color[3];
                if (ui.isPressed) {
                    r *= 0.7f; g *= 0.7f; b *= 0.7f;
                } else if (ui.isHovered) {
                    r *= 0.9f; g *= 0.9f; b *= 0.9f;
                }
                uint32_t ur = (uint32_t)(r * 255.0f);
                uint32_t ug = (uint32_t)(g * 255.0f);
                uint32_t ub = (uint32_t)(b * 255.0f);
                uint32_t ua = (uint32_t)(a * 255.0f);
                uint32_t color = (ua << 24) | (ur << 16) | (ug << 8) | ub;
                renderer->DrawTexture(ui.texture.get(), rx, ry, rw, rh, 0.0f, 0.0f, 1.0f, 1.0f, color);
            }

            if (!ui.text.empty()) {
                ImVec2 textSize = ImGui::CalcTextSize(ui.text.c_str());
                float tx = rx + (rw - textSize.x) * 0.5f;
                float ty = ry + (rh - textSize.y) * 0.5f;
                ImGui::GetBackgroundDrawList()->AddText(
                    ImVec2(tx, ty),
                    ImGui::GetColorU32(ImVec4(ui.color[0], ui.color[1], ui.color[2], ui.color[3])),
                    ui.text.c_str()
                );
            }
            return;
        }

        if (ui.texture) {
            float rx = 0.0f, ry = 0.0f, rw = ui.width, rh = ui.height;
            ResolveUIRect(ui, screenW, screenH, ui.width, ui.height, rx, ry, rw, rh);
            float r = ui.color[0];
            float g = ui.color[1];
            float b = ui.color[2];
            float a = ui.color[3];

            uint32_t ur = (uint32_t)(r * 255.0f);
            uint32_t ug = (uint32_t)(g * 255.0f);
            uint32_t ub = (uint32_t)(b * 255.0f);
            uint32_t ua = (uint32_t)(a * 255.0f);
            uint32_t color = (ua << 24) | (ur << 16) | (ug << 8) | ub;

            renderer->DrawTexture(ui.texture.get(), rx, ry, rw, rh, 0.0f, 0.0f, 1.0f, 1.0f, color);
        }
    });
}

} // namespace Genesis::Engine
