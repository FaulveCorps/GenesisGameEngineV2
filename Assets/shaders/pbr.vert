#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
out vec3 vNormal;
out vec2 vUV;
uniform mat4 uModel = mat4(1.0);
uniform mat4 uViewProjection = mat4(1.0);
void main() {
    vNormal = mat3(uModel) * aNormal;
    vUV = aUV;
    gl_Position = uViewProjection * uModel * vec4(aPos, 1.0);
}
