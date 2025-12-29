#pragma once

#include <string>
#include <iostream>
#include <memory>
#include <array>

namespace Genesis::Engine {
class IGraphicsAPI;

class Shader {
public:
    Shader() = default;
    ~Shader();

    // Create a shader object that stores per-backend source. This does not necessarily compile immediately.
    static std::shared_ptr<Shader> CreateFromSource(const std::string& vertexSrc, const std::string& fragmentSrc);

    // Convenience: compile immediately for GL (returns nullptr on failure)
    static std::shared_ptr<Shader> FromSource(const std::string& vertexSrc, const std::string& fragmentSrc);

    // Upload/recreate resources on the given renderer (called on renderer switch)
    void UploadToRenderer(IGraphicsAPI* renderer);
    void DestroyOnRenderer(IGraphicsAPI* renderer);

    void Use() const;
    unsigned int GetID() const { return programID_; }

    // Quick uniform helpers for prototyping use cases (GL only fallback)
    void SetUniformFloat(const std::string& name, float v) const;
    void SetUniformVec4(const std::string& name, const std::array<float,4>& v) const;

private:
    // Stored source (GL/GLSL for now). In future, add WGSL/HLSL variants.
    std::string vertexSrcGL_;
    std::string fragmentSrcGL_;

    // GL program id (0 == not created)
    unsigned int programID_ = 0;
};

} // namespace Genesis::Engine
