#version 330 core
out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D uScreenTexture;
uniform sampler2D uBloomBlur;
uniform float uExposure;
uniform float uGamma;
uniform bool uBloom;
uniform bool uVignetteEnabled;
uniform float uVignetteIntensity;
uniform float uVignetteRadius;
uniform float uVignetteSoftness;
uniform sampler2D uLUT;
uniform bool uLUTEnabled;
uniform float uLUTIntensity;
uniform float uLUTSize;

vec3 ApplyLUT(vec3 color)
{
    float size = max(uLUTSize, 2.0);
    vec3 c = clamp(color, 0.0, 1.0);

    float slice = c.b * (size - 1.0);
    float slice0 = floor(slice);
    float slice1 = min(slice0 + 1.0, size - 1.0);
    float lerpFactor = slice - slice0;

    float texelSize = 1.0 / (size * size);
    float texelSizeY = 1.0 / size;

    float x0 = (slice0 * size + c.r * (size - 1.0)) * texelSize + texelSize * 0.5;
    float x1 = (slice1 * size + c.r * (size - 1.0)) * texelSize + texelSize * 0.5;
    float y = (c.g * (size - 1.0)) * texelSizeY + texelSizeY * 0.5;

    vec3 c0 = texture(uLUT, vec2(x0, y)).rgb;
    vec3 c1 = texture(uLUT, vec2(x1, y)).rgb;
    return mix(c0, c1, lerpFactor);
}

void main()
{
    vec3 hdrColor = texture(uScreenTexture, TexCoords).rgb;
    vec3 bloomColor = texture(uBloomBlur, TexCoords).rgb;
    if(uBloom)
        hdrColor += bloomColor; // additive blending

    // Tone mapping (Reinhard)
    vec3 result = vec3(1.0) - exp(-hdrColor * uExposure);
    
    // Gamma correction
    result = pow(result, vec3(1.0 / uGamma));

    // Vignette
    if (uVignetteEnabled) {
        float dist = distance(TexCoords, vec2(0.5));
        float edge0 = max(uVignetteRadius - uVignetteSoftness, 0.0);
        float edge1 = max(uVignetteRadius, edge0 + 0.0001);
        float vignette = 1.0 - smoothstep(edge0, edge1, dist);
        float mixAmt = clamp(uVignetteIntensity, 0.0, 1.0);
        result *= mix(1.0, vignette, mixAmt);
    }

    // LUT
    if (uLUTEnabled) {
        vec3 lutColor = ApplyLUT(result);
        result = mix(result, lutColor, clamp(uLUTIntensity, 0.0, 1.0));
    }
    
    FragColor = vec4(result, 1.0);
}
