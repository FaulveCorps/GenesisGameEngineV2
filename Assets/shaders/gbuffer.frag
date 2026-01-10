#version 330 core
layout (location = 0) out vec4 gPosition;
layout (location = 1) out vec4 gNormal;
layout (location = 2) out vec4 gAlbedoSpec;

in vec2 TexCoords;
in vec3 FragPos;
in vec3 Normal;

uniform sampler2D uBaseColorTexture;
uniform int uHasBaseColorTexture;
uniform vec4 uBaseColor;
uniform float uMetallic;
uniform float uRoughness;

void main()
{    
    // Store the fragment position vector in the first gbuffer texture
    gPosition = vec4(FragPos, 1.0);
    
    // Also store the per-fragment normals into the gbuffer
    // Store Metallic in Normal.a
    gNormal = vec4(normalize(Normal), uMetallic);
    
    // And the diffuse per-fragment color
    if (uHasBaseColorTexture == 1) {
        gAlbedoSpec.rgb = texture(uBaseColorTexture, TexCoords).rgb * uBaseColor.rgb;
    } else {
        gAlbedoSpec.rgb = uBaseColor.rgb;
    }
    
    // Store Roughness in gAlbedoSpec's alpha component
    gAlbedoSpec.a = uRoughness;
}
