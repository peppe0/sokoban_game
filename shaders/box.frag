#version 330 core
in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

out vec4 FragColor;

uniform vec3 boxColor;
uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 viewPos;
uniform sampler2D diffuseMap;
uniform bool hasTexture;
uniform bool useLighting;

void main()
{
    vec3 albedo = boxColor;
    if (hasTexture) {
        // Tint rather than replace, so a textured mesh can be recoloured (two potions
        // share one bottle model). Every other caller passes white, which is a no-op.
        albedo = texture(diffuseMap, TexCoords).rgb * boxColor;
    }

    if (!useLighting) {
        FragColor = vec4(albedo, 1.0);
        return;
    }

    // Ambient
    float ambientStrength = 0.2;
    vec3 ambient = ambientStrength * lightColor;
    
    // Diffuse
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;
    
    // Specular
    float specularStrength = 0.8;
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = specularStrength * spec * lightColor;
    
    vec3 result = (ambient + diffuse + specular) * albedo;
    FragColor = vec4(result, 1.0);
}
