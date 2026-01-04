#version 330 core
out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D uScreenTexture;
uniform sampler2D uBloomBlur;
uniform float uExposure;
uniform float uGamma;
uniform bool uBloom;

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
    
    FragColor = vec4(result, 1.0);
}
