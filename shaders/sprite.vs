#version 330 core
layout (location = 0) in vec4 vertex; // <vec2 position, vec2 texCoords>

out vec2 TexCoords;
out vec2 FragPos;

uniform mat4 model;
uniform mat4 projection;

void main()
{
    TexCoords = vertex.zw;
    vec4 worldPos = model * vec4(vertex.xy, 0.0, 1.0);
    FragPos = worldPos.xy;
    gl_Position = projection * worldPos;
}