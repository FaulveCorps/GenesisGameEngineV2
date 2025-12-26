#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include <string>

namespace Genesis::Engine {
class IGraphicsAPI;

class Texture {
public:
    static std::shared_ptr<Texture> CreateFromMemory(uint32_t width, uint32_t height, const std::vector<uint8_t>& pixels);
    static std::shared_ptr<Texture> CreateFromFile(const std::string& path);
    ~Texture();

    void UploadToRenderer(IGraphicsAPI* renderer);
    void DestroyOnRenderer(IGraphicsAPI* renderer);

    uint32_t Width() const { return width_; }
    uint32_t Height() const { return height_; }
    unsigned int GetID() const { return textureID_; }

    // CPU-side pixel data in RGBA order
    const std::vector<uint8_t>& Pixels() const { return pixels_; }

private:
    Texture() = default;

    uint32_t width_ = 0;
    uint32_t height_ = 0;
    std::vector<uint8_t> pixels_;

    // GL texture id (0 == not created)
    unsigned int textureID_ = 0;
};

} // namespace Genesis::Engine
