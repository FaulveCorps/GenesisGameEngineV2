#include "ENGINE/SoftwareRenderer.h"
#include "ENGINE/Texture.h"
#include <iostream>
#include <algorithm>
#include <unordered_map>
#include <cstring>

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
    m_sprites.clear();
}

void SoftwareRenderer::EndFrame() {
    // No-op for CPU renderer
}

void SoftwareRenderer::Shutdown() {
    m_meshes.clear();
    m_textures.clear();
    m_initialized = false;
    std::cout << "SoftwareRenderer: shutdown" << std::endl;
}

// Simple internal mesh store for software backend
struct SWMesh {
    MeshDesc desc;
};

static uint64_t s_nextMeshId = 1;
static uint64_t s_nextTextureId = 1;

MeshHandle SoftwareRenderer::CreateMesh(const MeshDesc& desc) {
    MeshHandle h;
    h.id = s_nextMeshId++;
    m_meshes.emplace(h.id, desc);
    std::cout << "SoftwareRenderer: CreateMesh id=" << h.id << " (" << desc.vertices.size() / 3 << " verts, " << desc.indices.size() / 3 << " tris)" << std::endl;
    return h;
}

IGraphicsAPI::TextureHandle SoftwareRenderer::CreateTexture(uint32_t width, uint32_t height, const uint8_t* pixels) {
    IGraphicsAPI::TextureHandle h;
    h.id = s_nextTextureId++;
    SWTexture t;
    t.w = width; t.h = height;
    if (pixels && width > 0 && height > 0) {
        size_t sz = (size_t)width * height * 4;
        t.pixels.resize(sz);
        std::memcpy(t.pixels.data(), pixels, sz);
    }
    m_textures.emplace(h.id, std::move(t));
    std::cout << "SoftwareRenderer: CreateTexture id=" << h.id << " (" << width << "x" << height << ")" << std::endl;
    return h;
}

void SoftwareRenderer::DestroyTexture(const TextureHandle& h) {
    if (!h.IsValid()) return;
    auto it = m_textures.find(h.id);
    if (it != m_textures.end()) m_textures.erase(it);
    std::cout << "SoftwareRenderer: DestroyTexture id=" << h.id << std::endl;
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

void SoftwareRenderer::DrawTexture(Texture* tex, float x, float y, float w, float h, float u0, float v0, float u1, float v1, uint32_t /*color*/) {
    if (!tex) return;
    SoftwareRenderer::SWSprite s;
    s.tex = tex;
    s.x = x; s.y = y; s.w = w; s.h = h; s.u0 = u0; s.v0 = v0; s.u1 = u1; s.v1 = v1; s.color = 0xFFFFFFFF;
    m_sprites.push_back(s);
    std::cout << "SoftwareRenderer: queued sprite tex=" << tex->GetID() << " x=" << x << " y=" << y << " w=" << w << " h=" << h << std::endl;
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

    // Draw any sprites queued this frame
    for (const auto& s : m_sprites) {
        if (!s.tex) continue;
        if (s.w <= 0 || s.h <= 0) continue;
        uint32_t texW = s.tex->Width();
        uint32_t texH = s.tex->Height();
        if (texW == 0 || texH == 0) continue;

        int x0 = static_cast<int>(std::floor(s.x));
        int y0 = static_cast<int>(std::floor(s.y));
        int x1 = static_cast<int>(std::ceil(s.x + s.w));
        int y1 = static_cast<int>(std::ceil(s.y + s.h));

        // Clamp to target
        if (x0 < 0) x0 = 0;
        if (y0 < 0) y0 = 0;
        if (x1 > (int)width) x1 = (int)width;
        if (y1 > (int)height) y1 = (int)height;

        for (int yy = y0; yy < y1; ++yy) {
            for (int xx = x0; xx < x1; ++xx) {
                float fu = (static_cast<float>(xx) + 0.5f - s.x) / s.w;
                float fv = (static_cast<float>(yy) + 0.5f - s.y) / s.h;
                float u = s.u0 + fu * (s.u1 - s.u0);
                float v = s.v0 + fv * (s.v1 - s.v0);
                int sx = std::clamp(static_cast<int>(std::floor(u * (texW - 1) + 0.5f)), 0, static_cast<int>(texW) - 1);
                int sy = std::clamp(static_cast<int>(std::floor(v * (texH - 1) + 0.5f)), 0, static_cast<int>(texH) - 1);
                size_t sIdx = (static_cast<size_t>(sy) * texW + sx) * 4;
                const auto& src = s.tex->Pixels();
                uint8_t sr = src[sIdx + 0];
                uint8_t sg = src[sIdx + 1];
                uint8_t sb = src[sIdx + 2];
                uint8_t sa = src[sIdx + 3];

                size_t idx = (static_cast<size_t>(yy) * width + xx) * 4;

                // Simple alpha blend: out = src * a + dst * (1-a)
                float alpha = sa / 255.0f;
                if (alpha >= 0.999f) {
                    out[idx + 0] = sb;
                    out[idx + 1] = sg;
                    out[idx + 2] = sr;
                    out[idx + 3] = sa;
                } else {
                    out[idx + 0] = static_cast<uint8_t>(sb * alpha + out[idx + 0] * (1.0f - alpha));
                    out[idx + 1] = static_cast<uint8_t>(sg * alpha + out[idx + 1] * (1.0f - alpha));
                    out[idx + 2] = static_cast<uint8_t>(sr * alpha + out[idx + 2] * (1.0f - alpha));
                    out[idx + 3] = static_cast<uint8_t>(sa * alpha + out[idx + 3] * (1.0f - alpha));
                }
            }
        }
    }

    std::cout << "SoftwareRenderer: rendered offscreen " << width << "x" << height << (m_drawnMesh?" (mesh drawn)":"") << (m_sprites.empty()?"":" (sprites drawn)") << std::endl;
    return true;
}
