#version 330 core
layout (location = 0) out vec4 FragColor;
layout (location = 1) out vec4 BrightColor;

in vec2 TexCoords;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedoSpec;
uniform sampler2D shadowMap;
uniform vec2 uShadowMapTexelSize;

uniform vec3 uLightDir;
uniform vec3 uLightColor;
uniform float uLightIntensity;
uniform vec3 viewPos;
uniform mat4 lightSpaceMatrix;
uniform float uBloomThreshold;
uniform float uShadowPcfRadius;

struct PointLight {
    vec3 position;
    vec3 color;
    float intensity;
    float radius;
};
#define NR_POINT_LIGHTS 16
uniform PointLight pointLights[NR_POINT_LIGHTS];
uniform int nrPointLights;

// PBR functions
const float PI = 3.14159265359;

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float ShadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir)
{
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    if (projCoords.z > 1.0) return 0.0;

    float currentDepth = projCoords.z;
    float bias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.005);
    float radius = max(uShadowPcfRadius, 0.5);
    vec2 texelSize = max(uShadowMapTexelSize, vec2(1.0 / 2048.0)) * radius;

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

void main()
{             
    // Retrieve data from gbuffer
    vec3 FragPos = texture(gPosition, TexCoords).rgb;
    vec3 Normal = texture(gNormal, TexCoords).rgb;
    vec3 Albedo = texture(gAlbedoSpec, TexCoords).rgb;
    float Roughness = texture(gAlbedoSpec, TexCoords).a;
    float Metallic = texture(gNormal, TexCoords).a;

    // Calculate lighting (PBR)
    vec3 N = normalize(Normal);
    vec3 V = normalize(viewPos - FragPos);
    vec3 L = normalize(uLightDir); 
    
    vec3 H = normalize(V + L);
    
    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, Albedo, Metallic);

    // Reflectance equation
    vec3 Lo = vec3(0.0);

    // Calculate per-light radiance
    vec3 radiance = uLightColor * uLightIntensity;

    // Cook-Torrance BRDF
    float NDF = DistributionGGX(N, H, Roughness);   
    float G   = GeometrySmith(N, V, L, Roughness);      
    vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);
       
    vec3 numerator    = NDF * G * F; 
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001; 
    vec3 specular = numerator / denominator;
    
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - Metallic;	  

    float NdotL = max(dot(N, L), 0.0);        

    // Shadow
    vec4 fragPosLightSpace = lightSpaceMatrix * vec4(FragPos, 1.0);
    float shadow = ShadowCalculation(fragPosLightSpace, N, L);

    Lo += (kD * Albedo / PI + specular) * radiance * NdotL * (1.0 - shadow);

    // Point Lights
    for(int i = 0; i < nrPointLights; ++i)
    {
        float distance = length(pointLights[i].position - FragPos);
        if(distance < pointLights[i].radius)
        {
            vec3 L = normalize(pointLights[i].position - FragPos);
            vec3 H = normalize(V + L);
            
            // Attenuation (Inverse Square with +1 to avoid singularity)
            float attenuation = 1.0 / (distance * distance + 1.0);
            // Smooth falloff at radius
            float falloff = clamp(1.0 - (distance / pointLights[i].radius), 0.0, 1.0);
            attenuation *= falloff;

            vec3 radiance = pointLights[i].color * pointLights[i].intensity * attenuation;

            // Cook-Torrance BRDF
            float NDF = DistributionGGX(N, H, Roughness);   
            float G   = GeometrySmith(N, V, L, Roughness);      
            vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);
            
            vec3 numerator    = NDF * G * F; 
            float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001; 
            vec3 specular = numerator / denominator;
            
            vec3 kS = F;
            vec3 kD = vec3(1.0) - kS;
            kD *= 1.0 - Metallic;	  

            float NdotL = max(dot(N, L), 0.0);        

            Lo += (kD * Albedo / PI + specular) * radiance * NdotL;
        }
    }
    
    vec3 ambient = vec3(0.03) * Albedo; 
    vec3 color = ambient + Lo;

    FragColor = vec4(color, 1.0);

    // Check for brightness for bloom
    float brightness = dot(FragColor.rgb, vec3(0.2126, 0.7152, 0.0722));
    if(brightness > uBloomThreshold)
        BrightColor = vec4(FragColor.rgb, 1.0);
    else
        BrightColor = vec4(0.0, 0.0, 0.0, 1.0);
}
