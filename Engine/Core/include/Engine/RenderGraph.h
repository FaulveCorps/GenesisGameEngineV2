#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace Genesis::Engine {

class RenderGraph {
public:
    enum class ResourceType {
        Texture,
        Buffer,
        Unknown
    };

    struct ResourceHandle {
        uint32_t id = 0;
        bool IsValid() const { return id != 0; }
    };

    struct PassHandle {
        uint32_t id = 0;
        bool IsValid() const { return id != 0; }
    };

    struct ResourceDesc {
        std::string name;
        ResourceType type = ResourceType::Unknown;
        bool external = false;
    };

    struct PassDesc {
        std::string name;
        std::vector<ResourceHandle> reads;
        std::vector<ResourceHandle> writes;
    };

    ResourceHandle CreateResource(const std::string& name, ResourceType type);
    ResourceHandle ImportResource(const std::string& name, ResourceType type);

    PassHandle AddPass(const std::string& name,
                       const std::vector<ResourceHandle>& reads,
                       const std::vector<ResourceHandle>& writes);

    void Clear();
    bool Compile(std::string* error = nullptr);

    const std::vector<PassHandle>& GetExecutionOrder() const;
    const PassDesc* GetPass(PassHandle handle) const;
    const ResourceDesc* GetResource(ResourceHandle handle) const;

    ResourceHandle FindResource(const std::string& name) const;
    bool IsCompiled() const;
    size_t GetPassCount() const;
    size_t GetResourceCount() const;

private:
    ResourceHandle RegisterResource(const std::string& name, ResourceType type, bool external);
    static size_t IndexFromHandle(ResourceHandle handle);
    static size_t IndexFromHandle(PassHandle handle);

    std::vector<ResourceDesc> m_resources;
    std::unordered_map<std::string, ResourceHandle> m_resourceLookup;

    std::vector<PassDesc> m_passes;
    std::vector<PassHandle> m_executionOrder;
    bool m_compiled = false;
};

} // namespace Genesis::Engine
