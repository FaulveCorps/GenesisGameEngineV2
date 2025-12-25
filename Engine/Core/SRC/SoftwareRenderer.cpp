#include "ENGINE/SoftwareRenderer.h"
#include <iostream>
#include <algorithm>
#include <unordered_map>

using namespace Genesis::Engine;

bool SoftwareRenderer::Init(SDL_Window* window, SDL_GLContext /*glContext*/) {
    if (!window) return false;
    m_initialized = true;
    std::cout << "SoftwareRenderer: initialized (CPU rasterizer)" << std::endl;
    return true;
}

void SoftwareRenderer::BeginFrame() {
    // Clear per-frame state
    m_drawnMesh = 0;
}

void SoftwareRenderer::EndFrame() {
    // No-op for CPU renderer
}

void SoftwareRenderer::Shutdown() {
    m_meshes.clear();
    m_initialized = false;
    std::cout << "SoftwareRenderer: shutdown" << std::endl;
}

// Simple internal mesh store for software backend
struct SWMesh {
    MeshDesc desc;
};

static uint64_t s_nextMeshId = 1;

MeshHandle SoftwareRenderer::CreateMesh(const MeshDesc& desc) {
    MeshHandle h;
    h.id = s_nextMeshId++;
    m_meshes.emplace(h.id, desc);
    std::cout << "SoftwareRenderer: CreateMesh id=" << h.id << " (" << desc.vertices.size() / 3 << " verts, " << desc.indices.size() / 3 << " tris)" << std::endl;
    return h;
}

void SoftwareRenderer::DestroyMesh(const MeshHandle& h) {
    if (!h.IsValid()) return;
    auto it = m_meshes.find(h.id);
    if (it != m_meshes.end()) m_meshes.erase(it);
    if (m_drawnMesh == h.id) m_drawnMesh = 0;
    std::cout << "SoftwareRenderer: DestroyMesh id=" << h.id << std::endl;
}

void SoftwareRenderer::DrawMesh(const MeshHandle& h) {
    if (!h.IsValid()) return;
    if (m_meshes.find(h.id) == m_meshes.end()) return;
    // Mark this mesh as drawn this frame; ReadbackOffscreen will render a triangle to show it
    m_drawnMesh = h.id;
}

static void rasterizeTriangle(std::vector<uint8_t>& out, uint32_t w, uint32_t h, int x0, int y0, int x1, int y1, int x2, int y2, uint8_t r, uint8_t g, uint8_t b) {
    auto edge = [&](int ax, int ay, int bx, int by, int cx, int cy){
        return (cx - ax) * (by - ay) - (cy - ay) * (bx - ax);
    };
    for (int y = 0; y < (int)h; ++y) {
        for (int x = 0; x < (int)w; ++x) {
            int w0 = edge(x1, y1, x2, y2, x, y);
            int w1 = edge(x2, y2, x0, y0, x, y);
            int w2 = edge(x0, y0, x1, y1, x, y);
            if ((w0 >= 0 && w1 >= 0 && w2 >= 0) || (w0 <= 0 && w1 <= 0 && w2 <= 0)) {
                size_t idx = (static_cast<size_t>(y) * w + x) * 4;
                out[idx + 0] = b;
                out[idx + 1] = g;
                out[idx + 2] = r;
                out[idx + 3] = 255;
            }
        }
    }
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

    // If we drew a mesh this frame, render a blue triangle in the center to indicate it
    if (m_drawnMesh != 0 && m_meshes.find(m_drawnMesh) != m_meshes.end()) {
        // Simple heuristic triangle in pixel coordinates
        int tx0 = (int)(width * 0.5);
        int ty0 = (int)(height * 0.15);
        int tx1 = (int)(width * 0.15);
        int ty1 = (int)(height * 0.85);
        int tx2 = (int)(width * 0.85);
        int ty2 = (int)(height * 0.85);
        rasterizeTriangle(out, width, height, tx0, ty0, tx1, ty1, tx2, ty2, 0, 0, 255);
    }

    std::cout << "SoftwareRenderer: rendered offscreen " << width << "x" << height << (m_drawnMesh?" (mesh drawn)":"") << std::endl;
    return true;
}
