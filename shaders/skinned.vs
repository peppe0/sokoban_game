#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec4 aBoneIds;
layout (location = 4) in vec4 aWeights;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoords;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

const int MAX_BONES = 32;   // keep in sync with MODEL3D_MAX_BONES in heart_3d.h
uniform mat4 bones[MAX_BONES];

void main()
{
    float total = aWeights.x + aWeights.y + aWeights.z + aWeights.w;

    vec4 localPos;
    vec3 localNormal;
    if (total > 0.0001) {
        // Linear blend skinning. Weights are renormalised because only the four
        // strongest influences per vertex are kept when the model is loaded.
        mat4 skin = mat4(0.0);
        for (int i = 0; i < 4; ++i) {
            int id = int(aBoneIds[i] + 0.5);
            if (aWeights[i] > 0.0 && id >= 0 && id < MAX_BONES)
                skin += bones[id] * (aWeights[i] / total);
        }
        localPos = skin * vec4(aPos, 1.0);
        localNormal = mat3(skin) * aNormal;
    } else {
        // Rigid mesh drawn with this shader: pass through untouched.
        localPos = vec4(aPos, 1.0);
        localNormal = aNormal;
    }

    FragPos = vec3(model * localPos);
    Normal = mat3(transpose(inverse(model))) * localNormal;
    TexCoords = aTexCoords;
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
