#ifndef SPHERE_MESH_H
#define SPHERE_MESH_H

#include <glad/glad.h>

// A UV sphere built entirely in code: no .obj, no .fbx, no Assimp.
//
// Every other mesh in this game is authored in a 3D suite and imported, which
// means the vertex data arrives already cooked and the interesting part - where
// positions, normals and texture coordinates actually come from - is hidden
// inside the file. Here it is all in plain sight: the surface is the
// parametric sphere
//
//     p(phi, theta) = ( sin(phi)cos(theta), cos(phi), sin(phi)sin(theta) )
//
// sampled on a (stacks+1) x (sectors+1) grid, phi running 0..PI from the north
// pole down and theta running 0..2PI around. Two triangles per grid quad give
// the index buffer.
//
// The parametrization hands us the other two attributes for free:
//   - the normal of a unit sphere at p is p itself (already unit length), so no
//     cross products and no averaging are needed;
//   - (theta, phi) normalised to 0..1 is a natural UV map - the classic
//     equirectangular one, the same layout a world map uses.
//
// The mesh is always a *unit* sphere; size and position belong in the model
// matrix, so one VAO serves every orb on screen.
class SphereMesh
{
public:
    // stacks = subdivisions along phi (pole to pole), sectors = around theta.
    SphereMesh(unsigned int stacks = 18, unsigned int sectors = 36);
    ~SphereMesh();

    // No copying: the object owns GPU buffers, and a shallow copy would delete
    // them twice.
    SphereMesh(const SphereMesh&) = delete;
    SphereMesh& operator=(const SphereMesh&) = delete;

    // Binds the VAO and issues one indexed draw call.
    void Draw() const;

    unsigned int VertexCount() const { return vertexCount; }
    unsigned int IndexCount()  const { return indexCount; }

private:
    unsigned int VAO, VBO, EBO;
    unsigned int vertexCount, indexCount;
};

#endif
