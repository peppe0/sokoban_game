#ifndef MODEL3D_H
#define MODEL3D_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include "shader.h"
#include <vector>
#include <string>
#include <map>

#include <assimp/matrix4x4.h>

struct aiScene;
struct aiMesh;
struct aiNode;

// Maximum bones a skinned model may use; must match MAX_BONES in shaders/skinned.vs.
static const int MODEL3D_MAX_BONES = 32;

class Model3D
{
public:
    glm::vec3 Position;
    glm::vec3 Size;
    glm::vec3 Color;
    float Rotation;
    glm::vec3 RotationAxis;
    glm::vec3 RotationEuler;

        Model3D(const std::string& modelPath,
            glm::vec3 position = glm::vec3(0.0f),
            glm::vec3 size = glm::vec3(1.0f),
            glm::vec3 color = glm::vec3(1.0f));
        ~Model3D();

    void Draw(Shader &shader, glm::mat4 view, glm::mat4 projection);
    // Same draw, with the model matrix supplied instead of built from Position/Size/
    // Rotation - which is what lets one model be pinned to another model's bone.
    void DrawWithModel(Shader &shader, const glm::mat4 &model, glm::mat4 view, glm::mat4 projection);
    glm::mat4 ModelMatrix() const;   // what Draw() builds from Position/Size/Rotation
    // A bone's posed transform, in this model's own space (bone-local -> model). False
    // when the model has no bone by that name. Valid after SetPose/SetBindPose.
    bool BoneMatrix(const std::string &boneName, glm::mat4 &out) const;

    // --- Skeletal animation -------------------------------------------------
    // Clips live in their own file (the KayKit rig ships mesh and animations
    // separately); they bind to this model's bones by name.
    void LoadAnimations(const std::string& animationFilePath);
    bool HasAnimation(const std::string& name) const;
    // Poses the skeleton at `timeSeconds` into `clipName`, looping. Call before Draw;
    // one model can therefore be drawn several times per frame at different times.
    void SetPose(const std::string& clipName, float timeSeconds);
    void SetBindPose();
    bool IsSkinned() const { return !BoneIndexByName.empty(); }
    float AnimationDuration(const std::string& name) const;   // seconds, 0 if missing

private:
    unsigned int VAO, VBO;
    unsigned int DiffuseTextureID;
    bool HasTexture;
    bool TriedTexture;
    int vertexCount;
    void loadModel(const std::string& path);
    void processNode(const aiScene* scene, const aiNode* node, const aiMatrix4x4& parentTransform,
                     std::vector<float>& vertices, const std::string& path);
    bool loadDiffuseTexture(const std::string& modelPath, const aiScene* scene, aiMesh* mesh);
    unsigned int loadTextureFromFile(const std::string& filename, const std::string& directory);
    unsigned int loadTextureFromMemory(const struct aiTexture* texture);

    // Skeleton, copied out of the aiScene because assimp frees it with the importer.
    struct Node {
        std::string name;
        glm::mat4 transform;
        std::vector<Node> children;
    };
    struct Channel {                       // one animated bone
        std::vector<std::pair<float, glm::vec3>> positions;
        std::vector<std::pair<float, glm::quat>> rotations;
        std::vector<std::pair<float, glm::vec3>> scales;
    };
    struct Clip {
        float durationTicks = 0.0f;
        float ticksPerSecond = 30.0f;
        std::map<std::string, Channel> channels;
    };

    // A model is drawn one mesh at a time. Each mesh sits under its own node, so the
    // same bone has a *different* offset matrix per mesh (it maps that mesh's space to
    // bone space): sharing one palette across meshes displaces whole body parts.
    struct Part {
        int firstVertex = 0;
        int vertexCount = 0;
        std::vector<glm::mat4> offsets;   // indexed like BoneIndexByName; empty if rigid
    };

    Node                     Skeleton;
    glm::mat4                GlobalInverse = glm::mat4(1.0f);
    std::map<std::string,int> BoneIndexByName;
    std::vector<Part>        Parts;
    std::vector<glm::mat4>   BoneGlobals;      // pose only, offsets applied per part
    std::map<std::string, Clip> Clips;

    void collectBones(const aiScene* scene, aiMesh* mesh, Part& part,
                      std::vector<glm::ivec4>& ids, std::vector<glm::vec4>& weights);
    void copyHierarchy(const aiNode* src, Node& dst);
    void poseHierarchy(const Node& node, const glm::mat4& parentTransform,
                       const Clip* clip, float timeTicks);
};

#endif
