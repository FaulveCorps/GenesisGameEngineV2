#pragma once

#include <vector>
#include <cstdint>

namespace Genesis::Engine {

class Mesh {
public:
    Mesh() = default;
    ~Mesh();

    // Fill vertex/normal/index arrays
    void SetData(const std::vector<float>& vertices, const std::vector<float>& normals, const std::vector<uint32_t>& indices);

    // Upload CPU data to GPU (creates VBO/VAO/EBO) when possible.
    void UploadToGPU();

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
};

} // namespace Genesis::Engine
