#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;

out vec3 vPos;
out vec3 vNormal;
out vec2 vUV;
out vec4 vFragPosLightSpace;

uniform mat4 uModel = mat4(1.0);
uniform mat4 uViewProjection = mat4(1.0);
uniform mat4 lightSpaceMatrix;

void main() {
    vPos = vec3(uModel * vec4(aPos, 1.0));
    vNormal = mat3(uModel) * aNormal;
    vUV = aUV;
    vFragPosLightSpace = lightSpaceMatrix * vec4(vPos, 1.0);
    gl_Position = uViewProjection * uModel * vec4(aPos, 1.0);
}
