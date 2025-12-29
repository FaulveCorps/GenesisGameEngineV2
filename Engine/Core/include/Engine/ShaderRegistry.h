#pragma once

#include <vector>
#include <mutex>

namespace Genesis::Engine {
class Shader;
class IGraphicsAPI;

class ShaderRegistry {
public:
    static ShaderRegistry& Instance();

    void Register(Shader* s);
    void Unregister(Shader* s);

    void DestroyAllOnRenderer(IGraphicsAPI* renderer);
    void UploadAllToRenderer(IGraphicsAPI* renderer);

private:
    ShaderRegistry() = default;
    std::vector<Shader*> m_shaders;
    std::mutex m_mutex;
};

} // namespace Genesis::Engine