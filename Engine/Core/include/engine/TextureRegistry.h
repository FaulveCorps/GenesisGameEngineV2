#pragma once

#include <vector>
#include <mutex>

namespace Genesis::Engine {
class Texture;
class IGraphicsAPI;

class TextureRegistry {
public:
    static TextureRegistry& Instance();

    void Register(Texture* t);
    void Unregister(Texture* t);

    void DestroyAllOnRenderer(IGraphicsAPI* renderer);
    void UploadAllToRenderer(IGraphicsAPI* renderer);

private:
    TextureRegistry() = default;
    std::vector<Texture*> m_textures;
    std::mutex m_mutex;
};

} // namespace Genesis::Engine
