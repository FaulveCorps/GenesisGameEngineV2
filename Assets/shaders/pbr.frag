#version 330 core
layout (location = 0) out vec4 FragColor;
layout (location = 1) out vec4 BrightColor;

in vec3 vPos;
in vec3 vNormal;
in vec2 vUV;
in vec4 vFragPosLightSpace;

uniform vec4 uBaseColor = vec4(1.0,1.0,1.0,1.0);
uniform float uMetallic = 0.0;
uniform float uRoughness = 1.0;
uniform sampler2D uBaseColorTexture;
uniform int uHasBaseColorTexture = 0;

uniform vec3 uLightDir = vec3(0.5, 0.5, 0.8);
uniform vec3 uLightColor = vec3(1.0, 1.0, 1.0);
uniform float uLightIntensity = 1.0;
uniform float uBloomThreshold;

uniform sampler2D shadowMap;
uniform vec2 uShadowMapTexelSize;

float ShadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir)
{
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    if (projCoords.z > 1.0)
        return 0.0;

    float currentDepth = projCoords.z;
    float bias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.005);
    vec2 texelSize = max(uShadowMapTexelSize, vec2(1.0 / 2048.0));

    float shadow = 0.0;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += (currentDepth - bias > pcfDepth) ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;
    return shadow;
}

void main() {
    vec3 n = normalize(vNormal);
    vec3 lightDir = normalize(uLightDir);
    float NdotL = max(dot(n, lightDir), 0.0);
    
    vec4 baseColor = uBaseColor;
    if (uHasBaseColorTexture != 0) {
        baseColor *= texture(uBaseColorTexture, vUV);
    }

    float shadow = ShadowCalculation(vFragPosLightSpace, n, lightDir);

    vec3 diffuse = baseColor.rgb * NdotL * uLightColor * uLightIntensity;
    vec3 viewDir = normalize(vec3(0.0,0.0,1.0)); // TODO: Pass camera pos
    vec3 halfDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(n, halfDir), 0.0), mix(16.0, 128.0, 1.0 - uRoughness));
    vec3 specular = vec3(uMetallic) * spec * uLightColor * uLightIntensity;
    
    vec3 ambient = baseColor.rgb * 0.1;
    vec3 lighting = ambient + (1.0 - shadow) * (diffuse + specular);
    
    FragColor = vec4(lighting, baseColor.a);

    // Check whether fragment output is higher than threshold, if so output as brightness color
    float brightness = dot(FragColor.rgb, vec3(0.2126, 0.7152, 0.0722));
    if(brightness > uBloomThreshold)
        BrightColor = vec4(FragColor.rgb, 1.0);
    else
        BrightColor = vec4(0.0, 0.0, 0.0, 1.0);
}
