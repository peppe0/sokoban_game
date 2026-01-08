#ifndef HEART3D_H
#define HEART3D_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "shader.h"
#include <vector>
#include <string>

class Heart3D
{
public:
    glm::vec3 Position;
    glm::vec3 Size;
    glm::vec3 Color;
    float Rotation;
    glm::vec3 RotationAxis;

    Heart3D(const std::string& modelPath,
            glm::vec3 position = glm::vec3(0.0f), 
            glm::vec3 size = glm::vec3(1.0f), 
            glm::vec3 color = glm::vec3(1.0f));
    ~Heart3D();

    void Draw(Shader &shader, glm::mat4 view, glm::mat4 projection);

private:
    unsigned int VAO, VBO;
    int vertexCount;
    void loadModel(const std::string& path);
};

#endif
