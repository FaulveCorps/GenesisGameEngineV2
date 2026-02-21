#include "engine/RenderGraph.h"

#include <deque>
#include <unordered_set>

namespace Genesis::Engine {

RenderGraph::ResourceHandle RenderGraph::CreateResource(const std::string& name, ResourceType type) {
    return RegisterResource(name, type, false);
}

RenderGraph::ResourceHandle RenderGraph::ImportResource(const std::string& name, ResourceType type) {
    return RegisterResource(name, type, true);
}

RenderGraph::ResourceHandle RenderGraph::RegisterResource(const std::string& name, ResourceType type, bool external) {
    if (name.empty()) {
        return ResourceHandle{};
    }

    auto it = m_resourceLookup.find(name);
    if (it != m_resourceLookup.end()) {
        return it->second;
    }

    ResourceDesc desc;
    desc.name = name;
    desc.type = type;
    desc.external = external;
    m_resources.push_back(desc);

    ResourceHandle handle{ static_cast<uint32_t>(m_resources.size()) };
    m_resourceLookup.emplace(name, handle);
    return handle;
}

RenderGraph::PassHandle RenderGraph::AddPass(const std::string& name,
                                             const std::vector<ResourceHandle>& reads,
                                             const std::vector<ResourceHandle>& writes) {
    PassDesc desc;
    desc.name = name;
    desc.reads = reads;
    desc.writes = writes;
    m_passes.push_back(desc);
    m_compiled = false;
    return PassHandle{ static_cast<uint32_t>(m_passes.size()) };
}

void RenderGraph::Clear() {
    m_resources.clear();
    m_resourceLookup.clear();
    m_passes.clear();
    m_executionOrder.clear();
    m_compiled = false;
}

bool RenderGraph::Compile(std::string* error) {
    m_executionOrder.clear();
    m_compiled = false;

    const size_t passCount = m_passes.size();
    const size_t resourceCount = m_resources.size();

    if (passCount == 0) {
        m_compiled = true;
        return true;
    }

    std::vector<int> lastWriter(resourceCount, -1);
    std::vector<std::vector<size_t>> lastReaders(resourceCount);
    std::vector<std::unordered_set<size_t>> adjacency(passCount);
    std::vector<int> indegree(passCount, 0);

    auto setError = [&](const std::string& message) {
        if (error) {
            *error = message;
        }
    };

    auto addEdge = [&](size_t from, size_t to) {
        if (from == to) {
            return;
        }
        auto inserted = adjacency[from].insert(to);
        if (inserted.second) {
            indegree[to] += 1;
        }
    };

    auto resolveResourceIndex = [&](const PassDesc& pass, ResourceHandle handle, const char* usage, size_t* outIndex) {
        if (!handle.IsValid()) {
            setError("RenderGraph: pass '" + pass.name + "' has invalid " + usage + " handle");
            return false;
        }

        const size_t index = IndexFromHandle(handle);
        if (index >= resourceCount) {
            setError("RenderGraph: pass '" + pass.name + "' references unknown " + usage + " resource");
            return false;
        }

        if (outIndex) {
            *outIndex = index;
        }
        return true;
    };

    for (size_t passIndex = 0; passIndex < passCount; ++passIndex) {
        const PassDesc& pass = m_passes[passIndex];

        for (const auto& read : pass.reads) {
            size_t resourceIndex = 0;
            if (!resolveResourceIndex(pass, read, "read", &resourceIndex)) {
                return false;
            }

            const auto& resource = m_resources[resourceIndex];
            const int writer = lastWriter[resourceIndex];
            if (writer < 0) {
                if (!resource.external) {
                    setError("RenderGraph: pass '" + pass.name + "' reads resource '" + resource.name + "' without a writer");
                    return false;
                }
            } else {
                addEdge(static_cast<size_t>(writer), passIndex);
            }

            lastReaders[resourceIndex].push_back(passIndex);
        }

        for (const auto& write : pass.writes) {
            size_t resourceIndex = 0;
            if (!resolveResourceIndex(pass, write, "write", &resourceIndex)) {
                return false;
            }

            const int writer = lastWriter[resourceIndex];
            if (writer >= 0) {
                addEdge(static_cast<size_t>(writer), passIndex);
            }

            for (size_t reader : lastReaders[resourceIndex]) {
                addEdge(reader, passIndex);
            }

            lastReaders[resourceIndex].clear();
            lastWriter[resourceIndex] = static_cast<int>(passIndex);
        }
    }

    std::deque<size_t> ready;
    for (size_t i = 0; i < passCount; ++i) {
        if (indegree[i] == 0) {
            ready.push_back(i);
        }
    }

    while (!ready.empty()) {
        size_t node = ready.front();
        ready.pop_front();
        m_executionOrder.push_back(PassHandle{ static_cast<uint32_t>(node + 1) });

        for (size_t neighbor : adjacency[node]) {
            indegree[neighbor] -= 1;
            if (indegree[neighbor] == 0) {
                ready.push_back(neighbor);
            }
        }
    }

    if (m_executionOrder.size() != passCount) {
        setError("RenderGraph: cycle detected while compiling");
        m_executionOrder.clear();
        return false;
    }

    m_compiled = true;
    return true;
}

const std::vector<RenderGraph::PassHandle>& RenderGraph::GetExecutionOrder() const {
    return m_executionOrder;
}

const RenderGraph::PassDesc* RenderGraph::GetPass(PassHandle handle) const {
    if (!handle.IsValid()) {
        return nullptr;
    }

    const size_t index = IndexFromHandle(handle);
    if (index >= m_passes.size()) {
        return nullptr;
    }

    return &m_passes[index];
}

const RenderGraph::ResourceDesc* RenderGraph::GetResource(ResourceHandle handle) const {
    if (!handle.IsValid()) {
        return nullptr;
    }

    const size_t index = IndexFromHandle(handle);
    if (index >= m_resources.size()) {
        return nullptr;
    }

    return &m_resources[index];
}

RenderGraph::ResourceHandle RenderGraph::FindResource(const std::string& name) const {
    auto it = m_resourceLookup.find(name);
    if (it == m_resourceLookup.end()) {
        return ResourceHandle{};
    }
    return it->second;
}

bool RenderGraph::IsCompiled() const {
    return m_compiled;
}

size_t RenderGraph::GetPassCount() const {
    return m_passes.size();
}

size_t RenderGraph::GetResourceCount() const {
    return m_resources.size();
}

size_t RenderGraph::IndexFromHandle(ResourceHandle handle) {
    return static_cast<size_t>(handle.id - 1);
}

size_t RenderGraph::IndexFromHandle(PassHandle handle) {
    return static_cast<size_t>(handle.id - 1);
}

} // namespace Genesis::Engine
