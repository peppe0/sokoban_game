#version 330 core
in vec2 TexCoords;
in vec2 FragPos;
out vec4 color;

uniform sampler2D image;
uniform vec3 spriteColor;

// Lighting uniforms
uniform bool enableLighting;
uniform float ambientStrength;
uniform vec2 lightPositions[4];
uniform vec3 lightColors[4];
uniform float lightRadii[4];
uniform int numLights;

void main()
{
    vec4 texColor = vec4(spriteColor, 1.0) * texture(image, TexCoords);
    
    if (!enableLighting) {
        color = texColor;
        return;
    }
    
    // Start with ambient lighting
    vec3 lighting = vec3(ambientStrength);
    
    // Add contribution from each light
    for (int i = 0; i < numLights; i++) {
        vec2 lightDir = lightPositions[i] - FragPos;
        float distance = length(lightDir);
        
        // Attenuation: light fades with distance
        float attenuation = 1.0 - min(distance / lightRadii[i], 1.0);
        attenuation = attenuation * attenuation; // Quadratic falloff for smoother transition
        
        lighting += lightColors[i] * attenuation;
    }
    
    // Clamp to avoid over-brightness
    lighting = min(lighting, vec3(1.5));
    
    color = vec4(lighting, 1.0) * texColor;
}