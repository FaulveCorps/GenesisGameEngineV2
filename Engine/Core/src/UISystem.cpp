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

static void ResolveTextPosition(const UIComponent& ui, float rx, float ry, float rw, float rh,
                                const ImVec2& textSize, bool defaultCenter,
                                float& outX, float& outY) {
    const float padX = std::max(0.0f, ui.paddingX);
    const float padY = std::max(0.0f, ui.paddingY);
    const float contentX = rx + padX;
    const float contentY = ry + padY;
    const float contentW = std::max(0.0f, rw - padX * 2.0f);
    const float contentH = std::max(0.0f, rh - padY * 2.0f);

    if (!ui.useTextAlign) {
        if (defaultCenter) {
            outX = contentX + (contentW - textSize.x) * 0.5f;
            outY = contentY + (contentH - textSize.y) * 0.5f;
        } else {
            outX = contentX;
            outY = contentY;
        }
        return;
    }

    switch (ui.textAlignH) {
        case UIAlignH::Left:   outX = contentX; break;
        case UIAlignH::Center: outX = contentX + (contentW - textSize.x) * 0.5f; break;
        case UIAlignH::Right:  outX = contentX + (contentW - textSize.x); break;
        default:               outX = contentX; break;
    }

    switch (ui.textAlignV) {
        case UIAlignV::Top:    outY = contentY; break;
        case UIAlignV::Center: outY = contentY + (contentH - textSize.y) * 0.5f; break;
        case UIAlignV::Bottom: outY = contentY + (contentH - textSize.y); break;
        default:               outY = contentY; break;
    }
}

void UISystem::Update(Scene& scene, double /*dt*/) {
    if (!ImGui::GetCurrentContext()) return;
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
    if (!ImGui::GetCurrentContext()) return;

    ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    float screenW = displaySize.x;
    float screenH = displaySize.y;

    auto view = scene.Registry().view<UIComponent>();
    view.each([&](auto& ui) {
        if (ui.type == UIType::Text && !ui.text.empty()) {
            const float scale = (ui.textScale > 0.0f) ? ui.textScale : 1.0f;
            float baseFontSize = ImGui::GetFontSize();
            float fontSize = baseFontSize * scale;

            float elemW = (ui.useTextAlign || ui.wrapText) ? (ui.width > 0.0f ? ui.width : 0.0f) : 0.0f;
            float elemH = (ui.useTextAlign || ui.wrapText) ? (ui.height > 0.0f ? ui.height : 0.0f) : 0.0f;

            const float padX = std::max(0.0f, ui.paddingX);
            const float padY = std::max(0.0f, ui.paddingY);
            float contentW = (elemW > 0.0f) ? std::max(0.0f, elemW - padX * 2.0f) : 0.0f;
            float contentH = (elemH > 0.0f) ? std::max(0.0f, elemH - padY * 2.0f) : 0.0f;
            float wrapWidth = ui.wrapText ? contentW : 0.0f;
            float wrapWidthCalc = (wrapWidth > 0.0f && scale > 0.0f) ? (wrapWidth / scale) : 0.0f;

            ImVec2 textSize = ImGui::CalcTextSize(ui.text.c_str(), nullptr, false, wrapWidthCalc);
            textSize.x *= scale;
            textSize.y *= scale;

            if (elemW <= 0.0f) elemW = textSize.x + padX * 2.0f;
            if (elemH <= 0.0f) elemH = textSize.y + padY * 2.0f;

            float rx = 0.0f, ry = 0.0f, rw = elemW, rh = elemH;
            ResolveUIRect(ui, screenW, screenH, elemW, elemH, rx, ry, rw, rh);
            float tx = 0.0f;
            float ty = 0.0f;
            ResolveTextPosition(ui, rx, ry, rw, rh, textSize, false, tx, ty);
            if (ui.drawBorder && ui.borderThickness > 0.0f) {
                ImGui::GetBackgroundDrawList()->AddRect(
                    ImVec2(rx, ry), ImVec2(rx + rw, ry + rh),
                    ImGui::GetColorU32(ImVec4(ui.borderColor[0], ui.borderColor[1], ui.borderColor[2], ui.borderColor[3])),
                    0.0f, 0, ui.borderThickness
                );
            }
            ImGui::GetBackgroundDrawList()->AddText(
                ImGui::GetFont(),
                fontSize,
                ImVec2(tx, ty),
                ImGui::GetColorU32(ImVec4(ui.color[0], ui.color[1], ui.color[2], ui.color[3])),
                ui.text.c_str(),
                nullptr,
                wrapWidth
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

            if (ui.drawBorder && ui.borderThickness > 0.0f) {
                ImGui::GetBackgroundDrawList()->AddRect(
                    ImVec2(rx, ry), ImVec2(rx + rw, ry + rh),
                    ImGui::GetColorU32(ImVec4(ui.borderColor[0], ui.borderColor[1], ui.borderColor[2], ui.borderColor[3])),
                    0.0f, 0, ui.borderThickness
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
                const float scale = (ui.textScale > 0.0f) ? ui.textScale : 1.0f;
                float baseFontSize = ImGui::GetFontSize();
                float fontSize = baseFontSize * scale;

                const float padX = std::max(0.0f, ui.paddingX);
                const float padY = std::max(0.0f, ui.paddingY);
                float contentW = std::max(0.0f, rw - padX * 2.0f);
                float contentH = std::max(0.0f, rh - padY * 2.0f);
                float wrapWidth = ui.wrapText ? contentW : 0.0f;
                float wrapWidthCalc = (wrapWidth > 0.0f && scale > 0.0f) ? (wrapWidth / scale) : 0.0f;

                ImVec2 textSize = ImGui::CalcTextSize(ui.text.c_str(), nullptr, false, wrapWidthCalc);
                textSize.x *= scale;
                textSize.y *= scale;
                float tx = 0.0f;
                float ty = 0.0f;
                ResolveTextPosition(ui, rx, ry, rw, rh, textSize, true, tx, ty);
                ImGui::GetBackgroundDrawList()->AddText(
                    ImGui::GetFont(),
                    fontSize,
                    ImVec2(tx, ty),
                    ImGui::GetColorU32(ImVec4(ui.color[0], ui.color[1], ui.color[2], ui.color[3])),
                    ui.text.c_str(),
                    nullptr,
                    wrapWidth
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

            if (ui.drawBorder && ui.borderThickness > 0.0f) {
                ImGui::GetBackgroundDrawList()->AddRect(
                    ImVec2(rx, ry), ImVec2(rx + rw, ry + rh),
                    ImGui::GetColorU32(ImVec4(ui.borderColor[0], ui.borderColor[1], ui.borderColor[2], ui.borderColor[3])),
                    0.0f, 0, ui.borderThickness
                );
            }
        }
    });
}

} // namespace Genesis::Engine
