#version 330 core
in vec3 vNormal;
in vec2 vUV;
out vec4 FragColor;
uniform vec4 uBaseColor = vec4(1.0,1.0,1.0,1.0);
uniform float uMetallic = 0.0;
uniform float uRoughness = 1.0;
uniform sampler2D uBaseColorTexture;
uniform int uHasBaseColorTexture = 0;

uniform vec3 uLightDir = vec3(0.5, 0.5, 0.8);
uniform vec3 uLightColor = vec3(1.0, 1.0, 1.0);
uniform float uLightIntensity = 1.0;

void main() {
    vec3 n = normalize(vNormal);
    vec3 lightDir = normalize(uLightDir);
    float NdotL = max(dot(n, lightDir), 0.0);
    
    vec4 baseColor = uBaseColor;
    if (uHasBaseColorTexture != 0) {
        baseColor *= texture(uBaseColorTexture, vUV);
    }

    vec3 diffuse = baseColor.rgb * NdotL * uLightColor * uLightIntensity;
    vec3 viewDir = normalize(vec3(0.0,0.0,1.0));
    vec3 halfDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(n, halfDir), 0.0), mix(16.0, 128.0, 1.0 - uRoughness));
    vec3 specular = vec3(uMetallic) * spec * uLightColor * uLightIntensity;
    FragColor = vec4(diffuse + specular, baseColor.a);
}
