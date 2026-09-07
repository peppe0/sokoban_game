#include "heart_3d.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "stb_image.h"
#include <iostream>
#include <filesystem>
#include <algorithm>

// assimp stores matrices row-major, glm column-major.
static glm::mat4 toGlm(const aiMatrix4x4& m)
{
    return glm::mat4(m.a1, m.b1, m.c1, m.d1,
                     m.a2, m.b2, m.c2, m.d2,
                     m.a3, m.b3, m.c3, m.d3,
                     m.a4, m.b4, m.c4, m.d4);
}

Model3D::Model3D(const std::string& modelPath, glm::vec3 position, glm::vec3 size, glm::vec3 color)
    : Position(position), Size(size), Color(color), Rotation(0.0f), RotationAxis(glm::vec3(0.0f, 1.0f, 0.0f)), RotationEuler(glm::vec3(0.0f)),
      VAO(0), VBO(0), DiffuseTextureID(0), HasTexture(false), TriedTexture(false), vertexCount(0)
{
    loadModel(modelPath);
}

Model3D::~Model3D()
{
    glDeleteVertexArrays(1, &this->VAO);
    glDeleteBuffers(1, &this->VBO);
    if (this->DiffuseTextureID != 0)
        glDeleteTextures(1, &this->DiffuseTextureID);
}

glm::mat4 Model3D::ModelMatrix() const
{
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, Position);
    model = glm::rotate(model, glm::radians(Rotation), RotationAxis);
    model = glm::rotate(model, glm::radians(RotationEuler.x), glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::rotate(model, glm::radians(RotationEuler.y), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, glm::radians(RotationEuler.z), glm::vec3(0.0f, 0.0f, 1.0f));
    model = glm::scale(model, Size);
    return model;
}

bool Model3D::BoneMatrix(const std::string &boneName, glm::mat4 &out) const
{
    auto it = BoneIndexByName.find(boneName);
    if (it == BoneIndexByName.end() ||
        it->second < 0 || it->second >= static_cast<int>(BoneGlobals.size()))
        return false;
    // BoneGlobals holds the pose without the per-mesh offset matrix, which is exactly
    // what an attachment wants: the offsets map a mesh into bone space, and an object
    // hung off the bone is already there.
    out = BoneGlobals[it->second];
    return true;
}

void Model3D::Draw(Shader &shader, glm::mat4 view, glm::mat4 projection)
{
    DrawWithModel(shader, ModelMatrix(), view, projection);
}

void Model3D::DrawWithModel(Shader &shader, const glm::mat4 &model, glm::mat4 view, glm::mat4 projection)
{
    shader.Use();

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
    if (Parts.empty()) {
        glDrawArrays(GL_TRIANGLES, 0, vertexCount);
    } else {
        for (const Part& part : Parts) {
            // Each part needs its own palette: same pose, but its own offset matrices.
            for (size_t i = 0; i < part.offsets.size(); i++)
                shader.SetMatrix4(("bones[" + std::to_string(i) + "]").c_str(),
                                  BoneGlobals[i] * part.offsets[i]);
            glDrawArrays(GL_TRIANGLES, part.firstVertex, part.vertexCount);
        }
    }
    glBindVertexArray(0);

    if (this->HasTexture)
        glBindTexture(GL_TEXTURE_2D, 0);
}

// Per-vertex skin data for one mesh, indexed by that mesh's vertex index, plus this
// mesh's own bone offset matrices.
void Model3D::collectBones(const aiScene* scene, aiMesh* mesh, Part& part,
                           std::vector<glm::ivec4>& ids, std::vector<glm::vec4>& weights)
{
    ids.assign(mesh->mNumVertices, glm::ivec4(0));
    weights.assign(mesh->mNumVertices, glm::vec4(0.0f));
    part.offsets.assign(MODEL3D_MAX_BONES, glm::mat4(1.0f));

    for (unsigned int b = 0; b < mesh->mNumBones; b++) {
        const aiBone* bone = mesh->mBones[b];
        const std::string name = bone->mName.C_Str();

        // Bone *names* are shared model-wide, so the index is assigned once...
        auto it = BoneIndexByName.find(name);
        int boneIndex;
        if (it == BoneIndexByName.end()) {
            boneIndex = static_cast<int>(BoneIndexByName.size());
            if (boneIndex >= MODEL3D_MAX_BONES) {
                std::cout << "WARNING::MODEL3D: more than " << MODEL3D_MAX_BONES
                          << " bones, '" << name << "' ignored" << std::endl;
                continue;
            }
            BoneIndexByName[name] = boneIndex;
        } else {
            boneIndex = it->second;
        }
        // ...but the offset matrix belongs to this mesh alone.
        part.offsets[boneIndex] = toGlm(bone->mOffsetMatrix);

        for (unsigned int w = 0; w < bone->mNumWeights; w++) {
            unsigned int v = bone->mWeights[w].mVertexId;
            float weight = bone->mWeights[w].mWeight;
            if (v >= mesh->mNumVertices || weight <= 0.0f)
                continue;
            // Keep the four strongest influences, which is all the shader blends.
            for (int slot = 0; slot < 4; ++slot) {
                if (weights[v][slot] == 0.0f) {
                    ids[v][slot] = boneIndex;
                    weights[v][slot] = weight;
                    break;
                }
            }
        }
    }
}

void Model3D::processNode(const aiScene* scene, const aiNode* node, const aiMatrix4x4& parentTransform,
                          std::vector<float>& vertices, const std::string& path)
{
    const aiMatrix4x4 transform = parentTransform * node->mTransformation;
    // Normals need the inverse-transpose so non-uniform node scaling doesn't skew them.
    aiMatrix3x3 normalMatrix(transform);
    normalMatrix.Inverse();
    normalMatrix.Transpose();

    for (unsigned int i = 0; i < node->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];

        // Only probe once per model: multi-mesh models share one material, and a miss
        // would otherwise report the same failure for every mesh.
        if (!this->TriedTexture) {
            this->TriedTexture = true;
            this->HasTexture = loadDiffuseTexture(path, scene, mesh);
        }

        // A skinned mesh must stay in its own space: the bone matrices already carry the
        // placement, so baking the node transform in would apply it a second time. Rigid
        // meshes still need it baked, otherwise multi-node models fall apart.
        const bool skinned = mesh->HasBones();
        Part part;
        part.firstVertex = static_cast<int>(vertices.size() / 16);
        std::vector<glm::ivec4> boneIds;
        std::vector<glm::vec4> boneWeights;
        if (skinned)
            collectBones(scene, mesh, part, boneIds, boneWeights);

        for (unsigned int j = 0; j < mesh->mNumFaces; j++) {
            aiFace face = mesh->mFaces[j];

            for (unsigned int k = 0; k < face.mNumIndices; k++) {
                unsigned int index = face.mIndices[k];

                // Position
                aiVector3D position = skinned ? mesh->mVertices[index]
                                              : transform * mesh->mVertices[index];
                vertices.push_back(position.x);
                vertices.push_back(position.y);
                vertices.push_back(position.z);

                // Normal
                if (mesh->HasNormals()) {
                    aiVector3D normal = skinned ? mesh->mNormals[index]
                                                : normalMatrix * mesh->mNormals[index];
                    normal.NormalizeSafe();
                    vertices.push_back(normal.x);
                    vertices.push_back(normal.y);
                    vertices.push_back(normal.z);
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

                // Bone ids + weights (zero weights make the shader fall back to rigid)
                for (int s = 0; s < 4; ++s)
                    vertices.push_back(skinned ? static_cast<float>(boneIds[index][s]) : 0.0f);
                for (int s = 0; s < 4; ++s)
                    vertices.push_back(skinned ? boneWeights[index][s] : 0.0f);
            }
        }

        part.vertexCount = static_cast<int>(vertices.size() / 16) - part.firstVertex;
        if (part.vertexCount > 0)
            Parts.push_back(std::move(part));
    }

    for (unsigned int i = 0; i < node->mNumChildren; i++)
        processNode(scene, node->mChildren[i], transform, vertices, path);
}

void Model3D::loadModel(const std::string& path)
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

    // Walk the node graph so each mesh is baked with its node transform. Single-node
    // models (all the .obj assets) have identity transforms and are unaffected, but
    // multi-node rigs (e.g. the .fbx skeleton) fall apart without this.
    processNode(scene, scene->mRootNode, aiMatrix4x4(), vertices, path);

    const int floatsPerVertex = 16;   // pos3 + normal3 + uv2 + boneIds4 + weights4
    vertexCount = vertices.size() / floatsPerVertex;
    std::cout << "Loaded model with " << vertexCount << " vertices";
    if (IsSkinned()) {
        GlobalInverse = glm::inverse(toGlm(scene->mRootNode->mTransformation));
        copyHierarchy(scene->mRootNode, Skeleton);
        BoneGlobals.assign(MODEL3D_MAX_BONES, glm::mat4(1.0f));
        std::cout << " and " << BoneIndexByName.size() << " bones in " << Parts.size() << " parts";
    }
    std::cout << std::endl;

    // Create OpenGL buffers
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    const GLsizei stride = floatsPerVertex * sizeof(float);

    // Position attribute
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);

    // Normal attribute
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));

    // UV attribute
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));

    // Bone ids / weights (read only by the skinned shader)
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride, (void*)(8 * sizeof(float)));
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, stride, (void*)(12 * sizeof(float)));

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void Model3D::copyHierarchy(const aiNode* src, Node& dst)
{
    dst.name = src->mName.C_Str();
    dst.transform = toGlm(src->mTransformation);
    dst.children.resize(src->mNumChildren);
    for (unsigned int i = 0; i < src->mNumChildren; i++)
        copyHierarchy(src->mChildren[i], dst.children[i]);
}

void Model3D::LoadAnimations(const std::string& animationFilePath)
{
    if (!IsSkinned()) {
        std::cout << "WARNING::MODEL3D: " << animationFilePath
                  << " ignored, the model has no bones" << std::endl;
        return;
    }

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(animationFilePath, 0);
    if (!scene || scene->mNumAnimations == 0) {
        std::cout << "ERROR::MODEL3D: no animations in " << animationFilePath << std::endl;
        return;
    }

    for (unsigned int a = 0; a < scene->mNumAnimations; a++) {
        const aiAnimation* src = scene->mAnimations[a];
        Clip clip;
        clip.durationTicks = static_cast<float>(src->mDuration);
        clip.ticksPerSecond = (src->mTicksPerSecond > 0.0) ? static_cast<float>(src->mTicksPerSecond) : 30.0f;

        for (unsigned int c = 0; c < src->mNumChannels; c++) {
            const aiNodeAnim* ch = src->mChannels[c];
            Channel channel;
            for (unsigned int k = 0; k < ch->mNumPositionKeys; k++)
                channel.positions.push_back({static_cast<float>(ch->mPositionKeys[k].mTime),
                    glm::vec3(ch->mPositionKeys[k].mValue.x, ch->mPositionKeys[k].mValue.y, ch->mPositionKeys[k].mValue.z)});
            for (unsigned int k = 0; k < ch->mNumRotationKeys; k++)
                channel.rotations.push_back({static_cast<float>(ch->mRotationKeys[k].mTime),
                    glm::quat(ch->mRotationKeys[k].mValue.w, ch->mRotationKeys[k].mValue.x,
                              ch->mRotationKeys[k].mValue.y, ch->mRotationKeys[k].mValue.z)});
            for (unsigned int k = 0; k < ch->mNumScalingKeys; k++)
                channel.scales.push_back({static_cast<float>(ch->mScalingKeys[k].mTime),
                    glm::vec3(ch->mScalingKeys[k].mValue.x, ch->mScalingKeys[k].mValue.y, ch->mScalingKeys[k].mValue.z)});
            clip.channels[ch->mNodeName.C_Str()] = std::move(channel);
        }
        Clips[src->mName.C_Str()] = std::move(clip);
    }
    std::cout << "Loaded " << scene->mNumAnimations << " clips from " << animationFilePath << std::endl;
}

bool Model3D::HasAnimation(const std::string& name) const
{
    return Clips.find(name) != Clips.end();
}

float Model3D::AnimationDuration(const std::string& name) const
{
    auto it = Clips.find(name);
    if (it == Clips.end() || it->second.ticksPerSecond <= 0.0f)
        return 0.0f;
    return it->second.durationTicks / it->second.ticksPerSecond;
}

// Keyframe lookup: find the pair straddling `t` and blend between them.
template <typename T, typename Blend>
static T sampleKeys(const std::vector<std::pair<float, T>>& keys, float t, Blend blend)
{
    if (keys.empty()) return T();
    if (keys.size() == 1 || t <= keys.front().first) return keys.front().second;
    if (t >= keys.back().first) return keys.back().second;

    size_t i = 0;
    while (i + 1 < keys.size() && keys[i + 1].first < t) ++i;
    const float span = keys[i + 1].first - keys[i].first;
    const float f = (span > 0.0f) ? (t - keys[i].first) / span : 0.0f;
    return blend(keys[i].second, keys[i + 1].second, f);
}

void Model3D::poseHierarchy(const Node& node, const glm::mat4& parentTransform,
                            const Clip* clip, float timeTicks)
{
    glm::mat4 nodeTransform = node.transform;

    if (clip) {
        auto ch = clip->channels.find(node.name);
        if (ch != clip->channels.end()) {
            const glm::vec3 t = sampleKeys(ch->second.positions, timeTicks,
                [](const glm::vec3& a, const glm::vec3& b, float f) { return glm::mix(a, b, f); });
            const glm::quat r = sampleKeys(ch->second.rotations, timeTicks,
                [](const glm::quat& a, const glm::quat& b, float f) { return glm::normalize(glm::slerp(a, b, f)); });
            const glm::vec3 s = sampleKeys(ch->second.scales, timeTicks,
                [](const glm::vec3& a, const glm::vec3& b, float f) { return glm::mix(a, b, f); });

            nodeTransform = glm::translate(glm::mat4(1.0f), t) * glm::mat4_cast(r) * glm::scale(glm::mat4(1.0f), s);
        }
    }

    const glm::mat4 global = parentTransform * nodeTransform;

    // Store the pose only; each part folds in its own offset matrix at draw time.
    auto bone = BoneIndexByName.find(node.name);
    if (bone != BoneIndexByName.end())
        BoneGlobals[bone->second] = GlobalInverse * global;

    for (const Node& child : node.children)
        poseHierarchy(child, global, clip, timeTicks);
}

void Model3D::SetPose(const std::string& clipName, float timeSeconds)
{
    if (!IsSkinned())
        return;

    auto it = Clips.find(clipName);
    if (it == Clips.end()) {
        SetBindPose();
        return;
    }

    const Clip& clip = it->second;
    float timeTicks = 0.0f;
    if (clip.durationTicks > 0.0f)
        timeTicks = std::fmod(timeSeconds * clip.ticksPerSecond, clip.durationTicks);

    poseHierarchy(Skeleton, glm::mat4(1.0f), &clip, timeTicks);
}

void Model3D::SetBindPose()
{
    if (!IsSkinned())
        return;
    poseHierarchy(Skeleton, glm::mat4(1.0f), nullptr, 0.0f);
}

bool Model3D::loadDiffuseTexture(const std::string& modelPath, const aiScene* scene, aiMesh* mesh)
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

    // .glb packs its textures inside the file; assimp hands those back as "*0" rather
    // than a path on disk, so there is nothing to open.
    if (const aiTexture* embedded = scene->GetEmbeddedTexture(textureFile.c_str())) {
        this->DiffuseTextureID = loadTextureFromMemory(embedded);
        if (this->DiffuseTextureID != 0)
            return true;
        std::cout << "ERROR::TEXTURE_3D: could not decode the texture embedded in "
                  << modelPath << std::endl;
        return false;
    }

    // Try the path as written, then the bare file name beside the model, then the shared
    // textures folder. Asset packs bake in the exporting machine's absolute path (often a
    // Windows one, whose '\' separators std::filesystem does not split here) just as often
    // as they use a plain file name, and neither resolves against this project's layout.
    size_t cut = textureFile.find_last_of("/\\");
    std::string fileName = (cut == std::string::npos) ? textureFile : textureFile.substr(cut + 1);

    for (const std::string& candidate : {textureFile, fileName}) {
        for (const std::string& dir : {modelDir.string(), std::string("textures")}) {
            this->DiffuseTextureID = loadTextureFromFile(candidate, dir);
            if (this->DiffuseTextureID != 0)
                return true;
        }
    }

    std::cout << "ERROR::TEXTURE_3D: no diffuse texture found for " << modelPath
              << " (material asks for '" << textureFile << "'; also tried '"
              << fileName << "' in " << modelDir.string() << "/ and textures/)" << std::endl;
    return false;
}

// Uploads a texture that lives inside the model file rather than on disk.
unsigned int Model3D::loadTextureFromMemory(const aiTexture* texture)
{
    int width = 0, height = 0, channels = 0;
    unsigned char* data = nullptr;
    bool ownsData = false;

    if (texture->mHeight == 0) {
        // Compressed payload (png/jpg): mWidth is the byte count.
        data = stbi_load_from_memory(reinterpret_cast<const unsigned char*>(texture->pcData),
                                     static_cast<int>(texture->mWidth), &width, &height, &channels, 0);
        ownsData = true;
    } else {
        // Raw texels, stored by assimp as BGRA.
        width = static_cast<int>(texture->mWidth);
        height = static_cast<int>(texture->mHeight);
        channels = 4;
        data = new unsigned char[static_cast<size_t>(width) * height * 4];
        ownsData = true;
        for (int i = 0; i < width * height; i++) {
            data[i * 4 + 0] = texture->pcData[i].r;
            data[i * 4 + 1] = texture->pcData[i].g;
            data[i * 4 + 2] = texture->pcData[i].b;
            data[i * 4 + 3] = texture->pcData[i].a;
        }
    }
    if (!data)
        return 0;

    GLenum format = (channels == 1) ? GL_RED : (channels == 4 ? GL_RGBA : GL_RGB);

    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    if (ownsData) {
        if (texture->mHeight == 0) stbi_image_free(data);
        else delete[] data;
    }
    return textureID;
}

unsigned int Model3D::loadTextureFromFile(const std::string& filename, const std::string& directory)
{
    std::filesystem::path fullPath = std::filesystem::path(directory) / filename;

    int width = 0;
    int height = 0;
    int nrChannels = 0;
    stbi_set_flip_vertically_on_load(false);
    unsigned char* data = stbi_load(fullPath.string().c_str(), &width, &height, &nrChannels, 0);

    if (!data)
    {
        // Callers may probe several candidate paths, so a single miss is not an error;
        // reporting is left to whoever runs out of candidates.
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
