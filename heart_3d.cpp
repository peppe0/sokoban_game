#include "heart_3d.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <iostream>

Heart3D::Heart3D(const std::string& modelPath, glm::vec3 position, glm::vec3 size, glm::vec3 color)
    : Position(position), Size(size), Color(color), Rotation(0.0f), RotationAxis(glm::vec3(0.0f, 1.0f, 0.0f)), vertexCount(0)
{
    loadModel(modelPath);
}

Heart3D::~Heart3D()
{
    glDeleteVertexArrays(1, &this->VAO);
    glDeleteBuffers(1, &this->VBO);
}

void Heart3D::Draw(Shader &shader, glm::mat4 view, glm::mat4 projection)
{
    shader.Use();
    
    // Create model matrix
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, Position);
    model = glm::rotate(model, glm::radians(Rotation), RotationAxis);
    model = glm::scale(model, Size);
    
    shader.SetMatrix4("model", model);
    shader.SetMatrix4("view", view);
    shader.SetMatrix4("projection", projection);
    shader.SetVector3f("boxColor", Color);
    
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, vertexCount);
    glBindVertexArray(0);
}

void Heart3D::loadModel(const std::string& path)
{
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path, 
        aiProcess_Triangulate | 
        aiProcess_FlipUVs | 
        aiProcess_GenNormals |
        aiProcess_CalcTangentSpace);
    
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::cout << "ERROR::ASSIMP::" << importer.GetErrorString() << std::endl;
        return;
    }
    
    std::vector<float> vertices;
    
    // Process all meshes
    for (unsigned int i = 0; i < scene->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[i];
        
        // Process each face
        for (unsigned int j = 0; j < mesh->mNumFaces; j++) {
            aiFace face = mesh->mFaces[j];
            
            // Process each vertex of the face
            for (unsigned int k = 0; k < face.mNumIndices; k++) {
                unsigned int index = face.mIndices[k];
                
                // Position
                vertices.push_back(mesh->mVertices[index].x);
                vertices.push_back(mesh->mVertices[index].y);
                vertices.push_back(mesh->mVertices[index].z);
                
                // Normal
                if (mesh->HasNormals()) {
                    vertices.push_back(mesh->mNormals[index].x);
                    vertices.push_back(mesh->mNormals[index].y);
                    vertices.push_back(mesh->mNormals[index].z);
                } else {
                    vertices.push_back(0.0f);
                    vertices.push_back(1.0f);
                    vertices.push_back(0.0f);
                }
            }
        }
    }
    
    vertexCount = vertices.size() / 6;
    std::cout << "Loaded model with " << vertexCount << " vertices" << std::endl;
    
    // Create OpenGL buffers
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    
    // Position attribute
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    
    // Normal attribute
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}
