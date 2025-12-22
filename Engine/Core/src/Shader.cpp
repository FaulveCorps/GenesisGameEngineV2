#include "engine/Shader.h"
#include <GL/gl.h>

namespace Genesis::Engine {

static unsigned int CompileShader(unsigned int type, const std::string& source) {
    unsigned int id = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(id, 1, &src, nullptr);
    glCompileShader(id);

    int result = 0;
    glGetShaderiv(id, GL_COMPILE_STATUS, &result);
    if (result == GL_FALSE) {
        int length = 0;
        glGetShaderiv(id, GL_INFO_LOG_LENGTH, &length);
        std::string message(length, '\0');
        glGetShaderInfoLog(id, length, &length, &message[0]);
        std::cerr << "Shader compilation error: " << message << std::endl;
        glDeleteShader(id);
        return 0;
    }
    return id;
}

std::optional<Shader> Shader::FromSource(const std::string& vertexSrc, const std::string& fragmentSrc) {
    Shader s;
    unsigned int vs = CompileShader(GL_VERTEX_SHADER, vertexSrc);
    if (!vs) return std::nullopt;
    unsigned int fs = CompileShader(GL_FRAGMENT_SHADER, fragmentSrc);
    if (!fs) { glDeleteShader(vs); return std::nullopt; }

    unsigned int program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);

    // Bind attribute locations so we know where aPos/aNormal map (0 and 1)
    glBindAttribLocation(program, 0, "aPos");
    glBindAttribLocation(program, 1, "aNormal");

    glLinkProgram(program);

    int linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        int length = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        std::string message(length, '\0');
        glGetProgramInfoLog(program, length, &length, &message[0]);
        std::cerr << "Shader link error: " << message << std::endl;
        glDeleteProgram(program);
        glDeleteShader(vs);
        glDeleteShader(fs);
        return std::nullopt;
    }

    glDetachShader(program, vs);
    glDetachShader(program, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);

    s.programID_ = program;
    return s;
}

Shader::~Shader() {
    if (programID_) glDeleteProgram(programID_);
}

void Shader::Use() const {
    if (programID_) glUseProgram(programID_);
}

} // namespace Genesis::Engine
