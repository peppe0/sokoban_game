#include "heart_3d.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "stb_image.h"
#include <iostream>
#include <filesystem>

Heart3D::Heart3D(const std::string& modelPath, glm::vec3 position, glm::vec3 size, glm::vec3 color)
    : Position(position), Size(size), Color(color), Rotation(0.0f), RotationAxis(glm::vec3(0.0f, 1.0f, 0.0f)),
      VAO(0), VBO(0), DiffuseTextureID(0), HasTexture(false), vertexCount(0)
{
    loadModel(modelPath);
}

Heart3D::~Heart3D()
{
    glDeleteVertexArrays(1, &this->VAO);
    glDeleteBuffers(1, &this->VBO);
    if (this->DiffuseTextureID != 0)
        glDeleteTextures(1, &this->DiffuseTextureID);
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
    shader.SetInteger("hasTexture", this->HasTexture ? 1 : 0);

    if (this->HasTexture)
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, this->DiffuseTextureID);
        shader.SetInteger("diffuseMap", 0);
    }
    
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, vertexCount);
    glBindVertexArray(0);

    if (this->HasTexture)
        glBindTexture(GL_TEXTURE_2D, 0);
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

        if (!this->HasTexture) {
            this->HasTexture = loadDiffuseTexture(path, scene, mesh);
        }
        
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

                // TexCoords
                if (mesh->HasTextureCoords(0)) {
                    vertices.push_back(mesh->mTextureCoords[0][index].x);
                    vertices.push_back(mesh->mTextureCoords[0][index].y);
                } else {
                    vertices.push_back(0.0f);
                    vertices.push_back(0.0f);
                }
            }
        }
    }
    
    vertexCount = vertices.size() / 8;
    std::cout << "Loaded model with " << vertexCount << " vertices" << std::endl;
    
    // Create OpenGL buffers
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    
    // Position attribute
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    
    // Normal attribute
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));

    // UV attribute
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

bool Heart3D::loadDiffuseTexture(const std::string& modelPath, const aiScene* scene, aiMesh* mesh)
{
    if (mesh->mMaterialIndex >= scene->mNumMaterials)
        return false;

    aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
    if (material->GetTextureCount(aiTextureType_DIFFUSE) == 0)
        return false;

    aiString texturePath;
    if (material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) != AI_SUCCESS)
        return false;

    std::filesystem::path modelDir = std::filesystem::path(modelPath).parent_path();
    std::string textureFile = texturePath.C_Str();
    this->DiffuseTextureID = loadTextureFromFile(textureFile, modelDir.string());
    return this->DiffuseTextureID != 0;
}

unsigned int Heart3D::loadTextureFromFile(const std::string& filename, const std::string& directory)
{
    std::filesystem::path fullPath = std::filesystem::path(directory) / filename;

    int width = 0;
    int height = 0;
    int nrChannels = 0;
    stbi_set_flip_vertically_on_load(false);
    unsigned char* data = stbi_load(fullPath.string().c_str(), &width, &height, &nrChannels, 0);

    if (!data)
    {
        std::cout << "ERROR::TEXTURE_3D: Failed to load texture " << fullPath.string() << std::endl;
        return 0;
    }

    GLenum format = GL_RGB;
    if (nrChannels == 1)
        format = GL_RED;
    else if (nrChannels == 3)
        format = GL_RGB;
    else if (nrChannels == 4)
        format = GL_RGBA;

    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);
    glBindTexture(GL_TEXTURE_2D, 0);

    return textureID;
}
