#pragma once

#include <string>
#include <iostream>
#include <optional>

namespace Genesis::Engine {

class Shader {
public:
    Shader() = default;
    ~Shader();

    // Create & compile shader program from source strings. Returns std::nullopt on failure.
    static std::optional<Shader> FromSource(const std::string& vertexSrc, const std::string& fragmentSrc);

    void Use() const;
    unsigned int GetID() const { return programID_; }

private:
    unsigned int programID_ = 0;
};

} // namespace Genesis::Engine
