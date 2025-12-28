#pragma once

#include <vector>
#include <mutex>
#include <unordered_map>
#include <utility>
"Engine/IGraphics.h"

namespace Genesis::Engine {
class Texture;

class TextureRegistry {
public:
    static TextureRegistry& Instance();

    void Register(Texture* t);
    void Unregister(Texture* t);

    void RegisterHandle(Texture* t, IGraphicsAPI* owner, const IGraphicsAPI::TextureHandle& h);
    void UnregisterHandle(Texture* t, IGraphicsAPI* owner, const IGraphicsAPI::TextureHandle& h);

    void DestroyAllOnRenderer(IGraphicsAPI* renderer);
    void UploadAllToRenderer(IGraphicsAPI* renderer);

private:
    TextureRegistry() = default;
    std::vector<Texture*> m_textures;
    // owner -> list of (Texture*, handle)
    std::unordered_map<IGraphicsAPI*, std::vector<std::pair<Texture*, IGraphicsAPI::TextureHandle>>> m_handlesByOwner;
    std::mutex m_mutex;
};

} // namespace Genesis::Engine
