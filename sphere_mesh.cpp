#include "sphere_mesh.h"

#include <cmath>
#include <vector>

namespace {
constexpr float PI = 3.14159265358979323846f;
}

SphereMesh::SphereMesh(unsigned int stacks, unsigned int sectors)
    : VAO(0), VBO(0), EBO(0), vertexCount(0), indexCount(0)
{
    if (stacks < 2)  stacks = 2;
    if (sectors < 3) sectors = 3;

    // Interleaved exactly like the imported models: position(3) normal(3) uv(2).
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    vertices.reserve((stacks + 1) * (sectors + 1) * 8);
    indices.reserve(stacks * sectors * 6);

    // --- Vertices -----------------------------------------------------------
    // The grid is closed on purpose: the seam column (theta = 2PI) repeats the
    // positions of column 0 but carries u = 1 instead of u = 0. Sharing those
    // vertices would make the last quad of every ring sample the whole texture
    // backwards.
    for (unsigned int i = 0; i <= stacks; ++i)
    {
        const float phi = PI * static_cast<float>(i) / static_cast<float>(stacks);
        const float sinPhi = std::sin(phi);
        const float cosPhi = std::cos(phi);

        for (unsigned int j = 0; j <= sectors; ++j)
        {
            const float theta = 2.0f * PI * static_cast<float>(j) / static_cast<float>(sectors);

            // Position on the unit sphere.
            const float x = sinPhi * std::cos(theta);
            const float y = cosPhi;
            const float z = sinPhi * std::sin(theta);

            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);

            // Normal: for a unit sphere centred on the origin it is the position.
            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);

            // UV: theta -> u, phi -> v.
            vertices.push_back(static_cast<float>(j) / static_cast<float>(sectors));
            vertices.push_back(static_cast<float>(i) / static_cast<float>(stacks));
        }
    }

    // --- Indices ------------------------------------------------------------
    // Each grid cell becomes two triangles, wound counter-clockwise as seen
    // from outside so back-face culling keeps the far hemisphere out:
    //
    //     k1 --- k1+1        k1, k1+1, k2
    //      |  \    |         k1+1, k2+1, k2
    //     k2 --- k2+1
    //
    // The two rings touching the poles collapse to a point, so one of their two
    // triangles is degenerate and is skipped.
    for (unsigned int i = 0; i < stacks; ++i)
    {
        unsigned int k1 = i * (sectors + 1);
        unsigned int k2 = k1 + sectors + 1;

        for (unsigned int j = 0; j < sectors; ++j, ++k1, ++k2)
        {
            if (i != 0)             // north pole: no upper triangle
            {
                indices.push_back(k1);
                indices.push_back(k1 + 1);
                indices.push_back(k2);
            }
            if (i != stacks - 1)    // south pole: no lower triangle
            {
                indices.push_back(k1 + 1);
                indices.push_back(k2 + 1);
                indices.push_back(k2);
            }
        }
    }

    vertexCount = static_cast<unsigned int>(vertices.size() / 8);
    indexCount  = static_cast<unsigned int>(indices.size());

    // --- Upload -------------------------------------------------------------
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float),
                 vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int),
                 indices.data(), GL_STATIC_DRAW);

    const GLsizei stride = 8 * sizeof(float);
    glEnableVertexAttribArray(0);   // position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(1);   // normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);   // uv
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));

    // The EBO stays recorded in the VAO; the array buffer binding does not have
    // to, since the attribute pointers already captured it.
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

SphereMesh::~SphereMesh()
{
    if (EBO) glDeleteBuffers(1, &EBO);
    if (VBO) glDeleteBuffers(1, &VBO);
    if (VAO) glDeleteVertexArrays(1, &VAO);
}

void SphereMesh::Draw() const
{
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indexCount), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}
