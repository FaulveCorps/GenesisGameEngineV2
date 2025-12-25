#pragma once

#include <vector>
#include <cstdint>
#include "engine/IGraphics.h"

namespace Genesis::Engine {

class Mesh {
public:
    Mesh() = default;
    ~Mesh();

    // Non-copyable (GPU resources must have single owner)
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    // Movable: transfer ownership of GPU resources
    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;

    // Fill vertex/normal/index arrays
    void SetData(const std::vector<float>& vertices, const std::vector<float>& normals, const std::vector<uint32_t>& indices);

    // Upload CPU data to GPU (creates VBO/VAO/EBO) when possible.
    void UploadToGPU();

    // Upload/destroy helpers for renderer switching
    void UploadToRenderer(IGraphicsAPI* renderer);
    void DestroyOnRenderer(IGraphicsAPI* renderer);

    // Draw — uses VAO/VBO path when uploaded, otherwise falls back to client arrays
    void Draw() const;

    size_t GetTriangleCount() const { return (indices_.size() / 3); }

private:
    std::vector<float> vertices_; // x,y,z triples
    std::vector<float> normals_;  // x,y,z triples (optional)
    std::vector<uint32_t> indices_;

    // GL resources (0 == not allocated)
    unsigned int vao_ = 0;
    unsigned int vbo_ = 0;
    unsigned int ebo_ = 0;
    bool uploaded_ = false;

    // Index type used for the element array (GL_UNSIGNED_SHORT or GL_UNSIGNED_INT)
    unsigned int indexType_ = 0;

    // Runtime resource handle for non-GL renderers
    MeshHandle handle_ = {};
    enum class UploadKind { None, GL, Renderer };
    UploadKind uploadKind_ = UploadKind::None;
};

} // namespace Genesis::Engine
