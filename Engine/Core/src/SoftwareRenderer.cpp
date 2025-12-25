#include "engine/SoftwareRenderer.h"
#include <iostream>
#include <algorithm>

using namespace Genesis::Engine;

bool SoftwareRenderer::Init(SDL_Window* window, SDL_GLContext /*glContext*/) {
    if (!window) return false;
    m_initialized = true;
    std::cout << "SoftwareRenderer: initialized (CPU rasterizer)" << std::endl;
    return true;
}

void SoftwareRenderer::BeginFrame() {
    // No-op for CPU renderer; real engines would set up command lists here
}

void SoftwareRenderer::EndFrame() {
    // No-op for CPU renderer
}

void SoftwareRenderer::Shutdown() {
    m_initialized = false;
    std::cout << "SoftwareRenderer: shutdown" << std::endl;
}

bool SoftwareRenderer::ReadbackOffscreen(uint32_t width, uint32_t height, std::vector<uint8_t>& out) {
    if (!m_initialized) {
        std::cerr << "SoftwareRenderer: not initialized" << std::endl;
        return false;
    }
    if (width == 0 || height == 0) return false;

    size_t sz = static_cast<size_t>(width) * height * 4;
    out.assign(sz, 0);

    // Clear to black, alpha=255
    for (size_t i = 0; i < sz; i += 4) {
        out[i + 0] = 0;   // B
        out[i + 1] = 0;   // G
        out[i + 2] = 0;   // R
        out[i + 3] = 255; // A
    }

    // Draw a centered red rectangle covering the middle half of the image
    uint32_t x0 = width / 4;
    uint32_t x1 = (width * 3) / 4;
    uint32_t y0 = height / 4;
    uint32_t y1 = (height * 3) / 4;

    for (uint32_t y = y0; y < y1; ++y) {
        for (uint32_t x = x0; x < x1; ++x) {
            size_t idx = (static_cast<size_t>(y) * width + x) * 4;
            out[idx + 0] = 0;   // B
            out[idx + 1] = 0;   // G
            out[idx + 2] = 255; // R
            out[idx + 3] = 255; // A
        }
    }

    std::cout << "SoftwareRenderer: rendered offscreen " << width << "x" << height << std::endl;
    return true;
}
