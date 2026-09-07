#version 330 core
// Vertex shader of the magic bolt: the only mesh in the game that is generated
// in code (see sphere_mesh.cpp) instead of imported from a model file. The
// attribute layout is the standard one - position, normal, texture coordinate -
// so the shader itself does not know or care where the vertices came from.
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoords;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    FragPos = vec3(model * vec4(aPos, 1.0));
    // The bolt is only ever translated, rotated and uniformly scaled, so the
    // normal matrix would be the model matrix itself; the inverse-transpose is
    // kept anyway because it is the generally correct form.
    Normal = mat3(transpose(inverse(model))) * aNormal;
    TexCoords = aTexCoords;
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
