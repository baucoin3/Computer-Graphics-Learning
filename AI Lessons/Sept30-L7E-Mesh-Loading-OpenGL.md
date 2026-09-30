# Sept30-L7E — Loading a Mesh into OpenGL

**Learning objective:** Implement a complete mesh loader using tinyobjloader that produces a correctly interleaved VBO, an EBO for indexed drawing, per-vertex tangents for normal mapping, and multi-material submesh support — then render a real .obj model instead of hand-coded geometry.

---

## Part A — Theory

---

### 1. The Loading Pipeline

Every mesh loader does the same five things, in order:

```
Parse file
    ↓
Deduplicate .obj triplets into unique OpenGL vertices
    ↓
Build interleaved vertex buffer
    ↓
Build index buffer
    ↓
Upload to GPU (VBO + EBO + VAO)
```

Understanding each step separately prevents the confusion that comes from trying to do them all at once.

---

### 2. tinyobjloader

tinyobjloader (by Syoyo Fujita) is a single-header C++ .obj loader. Header-only — one `#include` and you're done. No CMake dependency, no library to link. Widely used: LearnOpenGL, the Vulkan Tutorial, and many game prototypes use it.

**GitHub:** `https://github.com/tinyobjloader/tinyobjloader`

Integration: download `tiny_obj_loader.h`, place in your project, then in exactly ONE `.cpp` file:

```cpp
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"
```

In all other files that need it:
```cpp
#include "tiny_obj_loader.h"
```

Do NOT define `TINYOBJLOADER_IMPLEMENTATION` in a header — it will be defined multiple times, causing linker errors.

#### tinyobjloader Data Structures

After loading, tinyobjloader populates three structures:

**`tinyobj::attrib_t`** — the three flat pools from the .obj file:
```cpp
attrib.vertices  // flat float array: x0,y0,z0, x1,y1,z1, ...
attrib.normals   // flat float array: nx0,ny0,nz0, nx1,ny1,nz1, ...
attrib.texcoords // flat float array: u0,v0, u1,v1, ...
```

Accessing position i: `attrib.vertices[3*i+0]`, `[3*i+1]`, `[3*i+2]`
Accessing UV i: `attrib.texcoords[2*i+0]`, `[2*i+1]`
Accessing normal i: `attrib.normals[3*i+0]`, `[3*i+1]`, `[3*i+2]`

**`std::vector<tinyobj::shape_t>`** — the shapes (groups/objects) in the .obj file:
```cpp
shape.name          // string
shape.mesh.indices  // vector<tinyobj::index_t>, one per face corner
shape.mesh.num_face_vertices  // vector<unsigned char>, vertices per face (3=tri, 4=quad)
```

`index_t` is the triplet:
```cpp
struct index_t {
    int vertex_index;   // index into attrib.vertices (already 0-based! tinyobjloader converts)
    int texcoord_index; // index into attrib.texcoords (-1 if not present)
    int normal_index;   // index into attrib.normals (-1 if not present)
};
```

tinyobjloader already converts from 1-based to 0-based. `vertex_index=0` means `attrib.vertices[0]`, not `attrib.vertices[1]`.

**`std::vector<tinyobj::material_t>`** — materials from the .mtl file:
```cpp
material.name
material.diffuse[3]     // Kd: float[3] (R, G, B)
material.specular[3]    // Ks
material.ambient[3]     // Ka
material.emission[3]    // Ke
material.shininess      // Ns
material.diffuse_texname   // map_Kd filename string
material.bump_texname      // map_bump filename string
material.specular_texname  // map_Ks filename string
```

---

### 3. The Deduplication Algorithm

The problem from L7D: .obj has three independent indices per corner, OpenGL needs one. Solution: hash map from (v_idx, vt_idx, n_idx) triplet to unique OpenGL vertex index.

```cpp
struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
    glm::vec3 tangent;    // computed later
    glm::vec3 bitangent;  // computed later
};

struct TripletHash {
    size_t operator()(const std::tuple<int,int,int>& t) const {
        // FNV-inspired mixing — just needs to be fast and low-collision
        size_t h = std::get<0>(t) * 73856093;
        h ^= std::get<1>(t) * 19349663;
        h ^= std::get<2>(t) * 83492791;
        return h;
    }
};

std::vector<Vertex>        vertices;
std::vector<unsigned int>  indices;
std::unordered_map<std::tuple<int,int,int>, unsigned int, TripletHash> uniqueMap;

for (const auto& idx : shape.mesh.indices) {
    auto triplet = std::make_tuple(idx.vertex_index, idx.texcoord_index, idx.normal_index);

    auto it = uniqueMap.find(triplet);
    if (it != uniqueMap.end()) {
        // Already seen this combination — reuse the index
        indices.push_back(it->second);
    } else {
        // New unique combination — create a new Vertex
        Vertex v{};

        int vi = idx.vertex_index;
        v.position = glm::vec3(
            attrib.vertices[3*vi+0],
            attrib.vertices[3*vi+1],
            attrib.vertices[3*vi+2]
        );

        if (idx.normal_index >= 0) {
            int ni = idx.normal_index;
            v.normal = glm::normalize(glm::vec3(
                attrib.normals[3*ni+0],
                attrib.normals[3*ni+1],
                attrib.normals[3*ni+2]
            ));
        }

        if (idx.texcoord_index >= 0) {
            int ti = idx.texcoord_index;
            v.uv = glm::vec2(
                attrib.texcoords[2*ti+0],
                attrib.texcoords[2*ti+1]
            );
        }

        unsigned int newIndex = static_cast<unsigned int>(vertices.size());
        vertices.push_back(v);
        indices.push_back(newIndex);
        uniqueMap[triplet] = newIndex;
    }
}
```

After this loop: `vertices` contains deduplicated vertices, `indices` is the EBO data.

---

### 4. Interleaved VBO Layout

Data for each vertex is stored sequentially in one buffer — all attributes for vertex 0, then all attributes for vertex 1, etc.

```
[v0.pos.x, v0.pos.y, v0.pos.z, v0.norm.x, v0.norm.y, v0.norm.z, v0.uv.x, v0.uv.y,
 v0.tan.x, v0.tan.y, v0.tan.z, v0.bitan.x, v0.bitan.y, v0.bitan.z,
 v1.pos.x, v1.pos.y, v1.pos.z, ...]
```

With tangents and bitangents: 14 floats per vertex = 56 bytes per vertex.

Without tangents (position + normal + UV only): 8 floats = 32 bytes per vertex.

**Stride:** total bytes per vertex = 14 × 4 = 56 bytes (with TBN).

**Offsets:**
- Position: offset 0
- Normal: offset 12 (after 3 floats × 4 bytes)
- UV: offset 24 (after 6 floats × 4 bytes)
- Tangent: offset 32 (after 8 floats × 4 bytes)
- Bitangent: offset 44 (after 11 floats × 4 bytes)

Upload and describe:
```cpp
glBindVertexArray(VAO);

glBindBuffer(GL_ARRAY_BUFFER, VBO);
glBufferData(GL_ARRAY_BUFFER,
    vertices.size() * sizeof(Vertex),
    vertices.data(),
    GL_STATIC_DRAW);

glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
glBufferData(GL_ELEMENT_ARRAY_BUFFER,
    indices.size() * sizeof(unsigned int),
    indices.data(),
    GL_STATIC_DRAW);

// Position — location 0
glEnableVertexAttribArray(0);
glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
    (void*)offsetof(Vertex, position));

// Normal — location 1
glEnableVertexAttribArray(1);
glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
    (void*)offsetof(Vertex, normal));

// UV — location 2
glEnableVertexAttribArray(2);
glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
    (void*)offsetof(Vertex, uv));

// Tangent — location 3
glEnableVertexAttribArray(3);
glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
    (void*)offsetof(Vertex, tangent));

// Bitangent — location 4
glEnableVertexAttribArray(4);
glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
    (void*)offsetof(Vertex, bitangent));

glBindVertexArray(0);
```

Using `offsetof(Vertex, field)` instead of hardcoded byte counts is safer — if you reorder struct fields, the offsets automatically update.

---

### 5. Tangent Generation

Tangents and bitangents define the TBN matrix used in normal mapping (lesson L8C). Even if you are not using normal maps yet, generate them now — they live in the VBO and are zero-cost to include.

#### What Tangents Represent

In a triangle, the tangent **T** points in the direction of increasing U (in UV space), and the bitangent **B** points in the direction of increasing V. Together with the normal **N**, they form a 3×3 orthonormal basis — the TBN matrix.

This basis converts a normal sampled from a tangent-space normal map (in the map's local XYZ space) into world space for lighting.

#### Derivation: Tangent from UV Deltas

For a triangle with vertices (P0, UV0), (P1, UV1), (P2, UV2):

```
edge1 = P1 - P0       delta_uv1 = UV1 - UV0
edge2 = P2 - P0       delta_uv2 = UV2 - UV0
```

We want to find T and B such that:
```
edge1 = delta_uv1.u * T  +  delta_uv1.v * B
edge2 = delta_uv2.u * T  +  delta_uv2.v * B
```

This is a 3×2 system solved for T and B simultaneously. Write it as a matrix equation:
```
[ edge1 ]   [ delta_uv1.u  delta_uv1.v ] [ T ]
[ edge2 ] = [ delta_uv2.u  delta_uv2.v ] [ B ]
```

Invert the 2×2 UV matrix:
```
[ T ]       1                  [  delta_uv2.v  -delta_uv1.v ] [ edge1 ]
[ B ] = ───────────────────── × [ -delta_uv2.u   delta_uv1.u ] [ edge2 ]
        det(UV matrix)
```

where `det = delta_uv1.u * delta_uv2.v - delta_uv1.v * delta_uv2.u`

Component-wise for T:
```
f = 1.0f / det

T.x = f * ( delta_uv2.v * edge1.x  -  delta_uv1.v * edge2.x )
T.y = f * ( delta_uv2.v * edge1.y  -  delta_uv1.v * edge2.y )
T.z = f * ( delta_uv2.v * edge1.z  -  delta_uv1.v * edge2.z )

B.x = f * ( -delta_uv2.u * edge1.x  +  delta_uv1.u * edge2.x )
B.y = f * ( -delta_uv2.u * edge1.y  +  delta_uv1.u * edge2.y )
B.z = f * ( -delta_uv2.u * edge1.z  +  delta_uv1.u * edge2.z )
```

This is the Lengyel method (Eric Lengyel, "Mathematics for 3D Game Programming and Computer Graphics," 2002). The same formula used by most game engines.

#### Averaging Per-Vertex Tangents

The formula above produces one tangent per triangle. A smooth mesh needs one tangent per vertex, averaged from surrounding triangles — same process as averaging normals for smooth shading.

Algorithm:

**Pass 1:** for each triangle, compute T and B using Lengyel's formula. **Accumulate** (add) the triangle's T and B into each of the triangle's three vertices.

**Pass 2:** for each vertex, normalize the accumulated T and B. Then orthogonalize: re-orthogonalize T against N using Gram-Schmidt to remove any component of T that is parallel to N. This is necessary because normal averaging can destroy orthogonality.

```cpp
// After Pass 1 (accumulation):
for (auto& v : vertices) {
    // Gram-Schmidt orthogonalization: remove N-parallel component from T
    v.tangent = glm::normalize(v.tangent - glm::dot(v.tangent, v.normal) * v.normal);
    
    // Recompute bitangent from N × T to ensure orthogonality
    // The sign (handedness) is determined by comparing with the accumulated B
    float handedness = (glm::dot(glm::cross(v.normal, v.tangent), v.bitangent) < 0.0f) ? -1.0f : 1.0f;
    v.bitangent = glm::cross(v.normal, v.tangent) * handedness;
}
```

The handedness calculation is important for mirrored geometry. A mirrored mesh (e.g., a character's right arm mirrored from the left) has its UV islands also mirrored, flipping the bitangent direction. Without the handedness correction, normal maps look inverted on mirrored surfaces.

---

## Part B — Worked Example: Full Load of a Quad Mesh

Input mesh: a flat quad at Y=0, UV mapped to [0,1]×[0,1].

.obj file:
```
v  0.0  0.0  0.0
v  1.0  0.0  0.0
v  0.0  0.0  1.0
v  1.0  0.0  1.0

vn  0.0  1.0  0.0

vt  0.0  0.0
vt  1.0  0.0
vt  0.0  1.0
vt  1.0  1.0

f  1/1/1  2/2/1  3/3/1
f  2/2/1  4/4/1  3/3/1
```

(Note: Y-up, Y=0 plane, normal pointing up = (0,1,0).)

#### tinyobjloader parsing

After `tinyobj::LoadObj()`:
```
attrib.vertices  = [0,0,0, 1,0,0, 0,0,1, 1,0,1]   (4 positions × 3 floats)
attrib.normals   = [0,1,0]                           (1 normal × 3 floats)
attrib.texcoords = [0,0, 1,0, 0,1, 1,1]             (4 UVs × 2 floats)

shapes[0].mesh.indices = [
  {0,0,0}, {1,1,0}, {2,2,0},   ← face 0 (already 0-based)
  {1,1,0}, {3,3,0}, {2,2,0}    ← face 1
]
```

#### Deduplication

Process indices in order:

| Corner | Triplet (v,vt,vn) | In map? | Action | OpenGL idx |
|---|---|---|---|---|
| 0 | (0,0,0) | No | Create vertex 0 | 0 |
| 1 | (1,1,0) | No | Create vertex 1 | 1 |
| 2 | (2,2,0) | No | Create vertex 2 | 2 |
| 3 | (1,1,0) | Yes | Reuse | 1 |
| 4 | (3,3,0) | No | Create vertex 3 | 3 |
| 5 | (2,2,0) | Yes | Reuse | 2 |

Index buffer: [0, 1, 2,  1, 3, 2]

Vertex buffer (before tangents):
```
v0: pos(0,0,0)  n(0,1,0)  uv(0,0)
v1: pos(1,0,0)  n(0,1,0)  uv(1,0)
v2: pos(0,0,1)  n(0,1,0)  uv(0,1)
v3: pos(1,0,1)  n(0,1,0)  uv(1,1)
```

#### Tangent computation for triangle 0 (vertices 0, 1, 2)

```
P0=(0,0,0)  UV0=(0,0)
P1=(1,0,0)  UV1=(1,0)
P2=(0,0,1)  UV2=(0,1)

edge1 = P1 - P0 = (1,0,0)
edge2 = P2 - P0 = (0,0,1)

delta_uv1 = UV1 - UV0 = (1,0)
delta_uv2 = UV2 - UV0 = (0,1)

det = delta_uv1.u * delta_uv2.v - delta_uv1.v * delta_uv2.u
    = 1 * 1 - 0 * 0 = 1.0

f = 1.0 / 1.0 = 1.0

T.x = f * (delta_uv2.v * edge1.x - delta_uv1.v * edge2.x)
    = 1 * (1 * 1  -  0 * 0) = 1
T.y = f * (delta_uv2.v * edge1.y - delta_uv1.v * edge2.y)
    = 1 * (1 * 0  -  0 * 0) = 0
T.z = f * (delta_uv2.v * edge1.z - delta_uv1.v * edge2.z)
    = 1 * (1 * 0  -  0 * 1) = 0

T = (1, 0, 0)  ← points in +X, which is the direction of increasing U

B.x = f * (-delta_uv2.u * edge1.x + delta_uv1.u * edge2.x)
    = 1 * (0 * (-1) * 1 + 1 * 0) = 0
B.y = f * (-delta_uv2.u * edge1.y + delta_uv1.u * edge2.y)
    = 1 * (0 + 0) = 0
B.z = f * (-delta_uv2.u * edge1.z + delta_uv1.u * edge2.z)
    = 1 * (-0 * 0 + 1 * 1) = 1

B = (0, 0, 1)  ← points in +Z, which is the direction of increasing V
```

Verification: N=(0,1,0), T=(1,0,0), B=(0,0,1). Are they orthogonal?
- N · T = 0 ✓
- N · B = 0 ✓
- T · B = 0 ✓
- T × B = (0*0-1*0, 1*1-0*0, 0*0-0*1) = (0, 1, 0) = N ✓ (right-handed TBN frame)

Tangents for triangle 1 produce the same T and B (same UV layout, same positions).

Accumulated tangent for each vertex = sum from triangles it participates in, then normalize:
- v0: (1,0,0) from tri0 → normalized (1,0,0)
- v1: (1,0,0) from tri0 + (1,0,0) from tri1 = (2,0,0) → normalized (1,0,0)
- v2: (1,0,0) from tri0 + (1,0,0) from tri1 = (2,0,0) → normalized (1,0,0)
- v3: (1,0,0) from tri1 → normalized (1,0,0)

Final vertex buffer:
```
v0: pos(0,0,0)  n(0,1,0)  uv(0,0)  T(1,0,0)  B(0,0,1)
v1: pos(1,0,0)  n(0,1,0)  uv(1,0)  T(1,0,0)  B(0,0,1)
v2: pos(0,0,1)  n(0,1,0)  uv(0,1)  T(1,0,0)  B(0,0,1)
v3: pos(1,0,1)  n(0,1,0)  uv(1,1)  T(1,0,0)  B(0,0,1)
```

---

## Part C — Full Implementation

### Complete Mesh.h / Mesh.cpp

**Mesh.h:**
```cpp
#pragma once

#include <vector>
#include <string>
#include <glm/glm.hpp>

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
    glm::vec3 tangent;
    glm::vec3 bitangent;
};

// One renderable submesh — one material per submesh
struct SubMesh {
    unsigned int indexOffset;  // byte offset into EBO
    unsigned int indexCount;   // number of indices
    int materialIndex;         // index into mesh's material list, -1 if none
};

class Mesh {
public:
    explicit Mesh(const std::string& path);
    ~Mesh();

    // Render all submeshes (caller binds shader and textures per submesh)
    void draw() const;

    // Access submesh info for per-material rendering
    const std::vector<SubMesh>& getSubMeshes() const { return submeshes; }
    unsigned int getVAO() const { return VAO; }
    size_t getTotalIndices() const { return totalIndices; }

    bool loaded = false;

private:
    unsigned int VAO = 0, VBO = 0, EBO = 0;
    size_t totalIndices = 0;
    std::vector<SubMesh> submeshes;

    void uploadToGPU(const std::vector<Vertex>& vertices,
                     const std::vector<unsigned int>& indices);
};
```

**Mesh.cpp:**
```cpp
#include "Mesh.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <unordered_map>
#include <tuple>
#include <stdexcept>

// Custom hash for (int, int, int) tuple
struct TripletHash {
    size_t operator()(const std::tuple<int,int,int>& t) const {
        size_t h = std::get<0>(t) * 73856093ULL;
        h ^= std::get<1>(t) * 19349663ULL;
        h ^= std::get<2>(t) * 83492791ULL;
        return h;
    }
};

static void computeTangents(std::vector<Vertex>& vertices,
                             const std::vector<unsigned int>& indices) {
    // Initialize tangent/bitangent accumulators to zero
    for (auto& v : vertices) {
        v.tangent   = glm::vec3(0.0f);
        v.bitangent = glm::vec3(0.0f);
    }

    // Pass 1: accumulate tangent and bitangent per triangle
    for (size_t i = 0; i < indices.size(); i += 3) {
        Vertex& v0 = vertices[indices[i + 0]];
        Vertex& v1 = vertices[indices[i + 1]];
        Vertex& v2 = vertices[indices[i + 2]];

        glm::vec3 edge1 = v1.position - v0.position;
        glm::vec3 edge2 = v2.position - v0.position;
        glm::vec2 duv1  = v1.uv - v0.uv;
        glm::vec2 duv2  = v2.uv - v0.uv;

        float det = duv1.x * duv2.y - duv1.y * duv2.x;
        if (std::abs(det) < 1e-8f) continue;  // degenerate UV triangle — skip
        float f = 1.0f / det;

        glm::vec3 T{
            f * (duv2.y * edge1.x - duv1.y * edge2.x),
            f * (duv2.y * edge1.y - duv1.y * edge2.y),
            f * (duv2.y * edge1.z - duv1.y * edge2.z)
        };
        glm::vec3 B{
            f * (-duv2.x * edge1.x + duv1.x * edge2.x),
            f * (-duv2.x * edge1.y + duv1.x * edge2.y),
            f * (-duv2.x * edge1.z + duv1.x * edge2.z)
        };

        v0.tangent += T;  v0.bitangent += B;
        v1.tangent += T;  v1.bitangent += B;
        v2.tangent += T;  v2.bitangent += B;
    }

    // Pass 2: normalize and orthogonalize each vertex's TBN
    for (auto& v : vertices) {
        if (glm::length(v.tangent) < 1e-6f) {
            // No UV data or degenerate — synthesize a tangent perpendicular to normal
            glm::vec3 up = std::abs(v.normal.y) < 0.99f ? glm::vec3(0,1,0) : glm::vec3(1,0,0);
            v.tangent = glm::normalize(glm::cross(v.normal, up));
            v.bitangent = glm::cross(v.normal, v.tangent);
            continue;
        }

        // Gram-Schmidt: orthogonalize T against N
        v.tangent = glm::normalize(v.tangent - glm::dot(v.tangent, v.normal) * v.normal);

        // Determine handedness and recompute B
        float handedness = (glm::dot(glm::cross(v.normal, v.tangent), v.bitangent) < 0.0f)
                           ? -1.0f : 1.0f;
        v.bitangent = glm::cross(v.normal, v.tangent) * handedness;
    }
}

Mesh::Mesh(const std::string& path) {
    // Extract directory from path for .mtl resolution
    std::string dir = path.substr(0, path.find_last_of("/\\") + 1);

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    bool ok = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err,
                                path.c_str(), dir.c_str());
    if (!warn.empty()) std::cerr << "[tinyobj warn] " << warn << "\n";
    if (!err.empty())  std::cerr << "[tinyobj error] " << err << "\n";
    if (!ok) {
        std::cerr << "Failed to load: " << path << "\n";
        return;
    }

    std::vector<Vertex>       vertices;
    std::vector<unsigned int> allIndices;

    // Process each shape as a separate submesh
    for (const auto& shape : shapes) {
        SubMesh sm{};
        sm.indexOffset  = static_cast<unsigned int>(allIndices.size() * sizeof(unsigned int));
        sm.indexCount   = 0;
        sm.materialIndex = -1;

        std::unordered_map<std::tuple<int,int,int>, unsigned int, TripletHash> uniqueMap;

        // Use a local index list per submesh; these are offsets into the global vertex array
        std::vector<unsigned int> submeshIndices;

        size_t indexOffset = 0;
        for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++) {
            int fv = shape.mesh.num_face_vertices[f];
            if (fv < 3) { indexOffset += fv; continue; }  // malformed face

            // Record first material seen for this submesh
            if (sm.materialIndex == -1 && !shape.mesh.material_ids.empty()) {
                sm.materialIndex = shape.mesh.material_ids[f];
            }

            // Triangulate N-gons with fan triangulation
            // Fan: (0,1,2), (0,2,3), (0,3,4), ...
            for (int tri = 0; tri < fv - 2; tri++) {
                int corners[3] = { 0, tri + 1, tri + 2 };
                for (int c = 0; c < 3; c++) {
                    tinyobj::index_t idx = shape.mesh.indices[indexOffset + corners[c]];

                    auto triplet = std::make_tuple(
                        idx.vertex_index,
                        idx.texcoord_index,
                        idx.normal_index
                    );

                    auto it = uniqueMap.find(triplet);
                    if (it != uniqueMap.end()) {
                        submeshIndices.push_back(it->second);
                    } else {
                        Vertex v{};

                        int vi = idx.vertex_index;
                        v.position = glm::vec3(
                            attrib.vertices[3*vi+0],
                            attrib.vertices[3*vi+1],
                            attrib.vertices[3*vi+2]
                        );

                        if (idx.normal_index >= 0) {
                            int ni = idx.normal_index;
                            v.normal = glm::normalize(glm::vec3(
                                attrib.normals[3*ni+0],
                                attrib.normals[3*ni+1],
                                attrib.normals[3*ni+2]
                            ));
                        }

                        if (idx.texcoord_index >= 0) {
                            int ti = idx.texcoord_index;
                            v.uv = glm::vec2(
                                attrib.texcoords[2*ti+0],
                                attrib.texcoords[2*ti+1]
                            );
                        }

                        unsigned int newIdx = static_cast<unsigned int>(vertices.size());
                        vertices.push_back(v);
                        submeshIndices.push_back(newIdx);
                        uniqueMap[triplet] = newIdx;
                    }
                }
            }
            indexOffset += fv;
        }

        sm.indexCount = static_cast<unsigned int>(submeshIndices.size());
        for (auto i : submeshIndices) allIndices.push_back(i);
        submeshes.push_back(sm);
    }

    totalIndices = allIndices.size();

    // Compute tangents for the full mesh
    computeTangents(vertices, allIndices);

    uploadToGPU(vertices, allIndices);
    loaded = true;

    std::cout << "Loaded: " << path << " — "
              << vertices.size() << " vertices, "
              << allIndices.size() / 3 << " triangles, "
              << submeshes.size() << " submeshes\n";
}

void Mesh::uploadToGPU(const std::vector<Vertex>& vertices,
                        const std::vector<unsigned int>& indices) {
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER,
        vertices.size() * sizeof(Vertex),
        vertices.data(),
        GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
        indices.size() * sizeof(unsigned int),
        indices.data(),
        GL_STATIC_DRAW);

    // Position — location 0
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
        (void*)offsetof(Vertex, position));
    // Normal — location 1
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
        (void*)offsetof(Vertex, normal));
    // UV — location 2
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
        (void*)offsetof(Vertex, uv));
    // Tangent — location 3
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
        (void*)offsetof(Vertex, tangent));
    // Bitangent — location 4
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
        (void*)offsetof(Vertex, bitangent));

    glBindVertexArray(0);
}

void Mesh::draw() const {
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(totalIndices), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

Mesh::~Mesh() {
    if (VAO) glDeleteVertexArrays(1, &VAO);
    if (VBO) glDeleteBuffers(1, &VBO);
    if (EBO) glDeleteBuffers(1, &EBO);
}
```

---

### 6. Multi-Material Submesh Rendering

When a mesh has multiple materials, you draw each submesh with its own material bound:

```cpp
void renderMesh(const Mesh& mesh, GLuint shader,
                const std::vector<GLuint>& diffuseTextures) {
    glUseProgram(shader);
    glBindVertexArray(mesh.getVAO());

    for (const auto& sm : mesh.getSubMeshes()) {
        // Bind the texture for this submesh's material
        if (sm.materialIndex >= 0 && sm.materialIndex < (int)diffuseTextures.size()) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, diffuseTextures[sm.materialIndex]);
            glUniform1i(glGetUniformLocation(shader, "diffuseMap"), 0);
        }

        glDrawElements(
            GL_TRIANGLES,
            sm.indexCount,
            GL_UNSIGNED_INT,
            (void*)(sm.indexOffset)   // byte offset into the bound EBO
        );
    }

    glBindVertexArray(0);
}
```

The key: `sm.indexOffset` is a byte offset into the EBO. All submeshes share the same VAO/VBO/EBO — only the draw range differs.

---

### 7. Handling Meshes Without Normals

If the .obj has no `vn` lines (e.g., a very old or minimal file), `idx.normal_index` is -1 for all indices.

In this case, compute flat normals from geometry after loading:

```cpp
// After deduplication, if vertex normals are all zero:
bool hasNormals = false;
for (const auto& v : vertices) {
    if (glm::length(v.normal) > 0.01f) { hasNormals = true; break; }
}

if (!hasNormals) {
    // Compute flat face normals and assign to each face's vertices
    for (size_t i = 0; i < indices.size(); i += 3) {
        glm::vec3 p0 = vertices[indices[i]].position;
        glm::vec3 p1 = vertices[indices[i+1]].position;
        glm::vec3 p2 = vertices[indices[i+2]].position;
        glm::vec3 n  = glm::normalize(glm::cross(p1 - p0, p2 - p0));
        vertices[indices[i]].normal   = n;
        vertices[indices[i+1]].normal = n;
        vertices[indices[i+2]].normal = n;
    }
}
```

For smooth normals without a source, accumulate face normals per vertex and normalize — same averaging process as tangent computation.

---

### 8. Vertex Shader for Loaded Mesh

Update your vertex shader to accept all five attributes:

```glsl
#version 330 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
layout(location = 3) in vec3 aTangent;
layout(location = 4) in vec3 aBitangent;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 fragPos;
out vec3 fragNormal;
out vec2 fragUV;
out mat3 TBN;

void main() {
    vec4 worldPos = model * vec4(aPosition, 1.0);
    fragPos    = vec3(worldPos);
    gl_Position = projection * view * worldPos;
    fragUV     = aUV;

    mat3 normalMatrix = mat3(transpose(inverse(model)));
    vec3 N = normalize(normalMatrix * aNormal);
    vec3 T = normalize(normalMatrix * aTangent);
    vec3 B = normalize(normalMatrix * aBitangent);

    // Re-orthogonalize T with respect to N (prevents precision drift)
    T = normalize(T - dot(T, N) * N);
    TBN = mat3(T, B, N);

    fragNormal = N;
}
```

---

### 9. Pitfalls

**Pitfall 1: `TINYOBJLOADER_IMPLEMENTATION` Defined in a Header**

Exact mistake: placing `#define TINYOBJLOADER_IMPLEMENTATION` in a header that is included by multiple .cpp files.

Exact symptom: linker error — multiple definition of `tinyobj::LoadObj`, `tinyobj::...` etc. One error per symbol for every .cpp that includes the header.

Exact fix: define `TINYOBJLOADER_IMPLEMENTATION` in exactly ONE .cpp file (your Mesh.cpp), before the include. In all other .cpp files, include the header without the define.

---

**Pitfall 2: Degenerate UV Triangle Causes NaN Tangents**

Exact mistake: mesh has triangles where two UV coordinates are identical (UV seam or degenerate UV island). `det = duv1.x * duv2.y - duv1.y * duv2.x = 0`. Dividing by det produces `inf` or `nan`.

Exact symptom: normal mapping (L8C) produces black or solid-color highlights on parts of the mesh. The NaN tangent propagates through TBN matrix multiply in the shader.

Exact fix: check `if (std::abs(det) < 1e-8f) continue;` before dividing. For vertices that receive no tangent contribution (all surrounding triangles had degenerate UV), synthesize a tangent (see fallback in `computeTangents` above).

---

**Pitfall 3: Wrong Submesh Index Offset Type**

Exact mistake: using int index offset instead of void* cast for `glDrawElements`:
```cpp
glDrawElements(GL_TRIANGLES, sm.indexCount, GL_UNSIGNED_INT, sm.indexOffset);
// WRONG — sm.indexOffset is unsigned int, treated as pointer value
```

Exact symptom: all submeshes draw from the start of the EBO. Or: segfault/undefined behavior. Or: only the last submesh appears.

Exact fix: cast to void*: `(void*)(uintptr_t)(sm.indexOffset)`. Alternatively, store the offset as `void*` directly. The fourth argument to `glDrawElements` is a byte offset into the currently bound EBO, typed as `const void*`.

---

**Pitfall 4: .obj Loaded from Wrong Working Directory**

Exact mistake: `tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, "models/cube.obj")` without passing the material directory as the fifth argument. tinyobjloader looks for .mtl relative to process working directory, not the .obj file's directory.

Exact symptom: `warn` contains "Material file [ cube.mtl ] not found". All faces load with material index -1. No textures, no material data.

Exact fix: pass `dir.c_str()` as the fifth argument (material base path):
```cpp
std::string dir = path.substr(0, path.find_last_of("/\\") + 1);
tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.c_str(), dir.c_str());
```

---

**Pitfall 5: Bitangent Handedness Wrong on Mirrored Geometry**

Exact mistake: always computing bitangent as `cross(normal, tangent)` without the handedness check.

Exact symptom: with normal mapping applied (L8C), mirrored parts of a mesh (e.g., both arms of a character, or a symmetric vehicle) have inverted normal map on one side. One side looks correctly lit; the mirrored side has lighting flipped.

Exact fix: handedness check using the accumulated bitangent from Pass 1:
```cpp
float handedness = (glm::dot(glm::cross(v.normal, v.tangent), v.bitangent) < 0.0f)
                   ? -1.0f : 1.0f;
v.bitangent = glm::cross(v.normal, v.tangent) * handedness;
```
The `v.bitangent` at this point is the accumulated (pre-normalization) version from Pass 1, which encodes which direction the UV increases. The handedness check compares our computed cross(N, T) against this accumulated direction. Flip if they disagree.

---

**Pitfall 6: Model Matrix Not Applied to Normals**

Exact mistake: transforming normals with the model matrix directly: `normalize(mat3(model) * aNormal)`.

Exact symptom: normals visually wrong when mesh has non-uniform scale — specular highlights in wrong location. Looks correct on uniformly scaled meshes.

Exact fix: use the normal matrix: `mat3(transpose(inverse(model)))`. Compute CPU-side and pass as a `uniform mat3` to avoid recomputing per vertex.

---

### 10. What to Build

**Exercise 1 — Replace hand-coded geometry:** Take your existing project 03 (full pipeline with camera and Phong shading). Remove the hard-coded cube VBO setup. Replace it with a `Mesh` object loaded from a .obj file. Export a simple mesh from Blender — apply transforms, triangulate, include normals and UVs, Y-up axis. Confirm it renders correctly with your existing Phong shader. Print vertex and triangle counts.

**Exercise 2 — Tangent verification:** Add a debug visualization mode: in the fragment shader, when a `uniform int debugMode = 1`, output `vec3(v.tangent * 0.5 + 0.5)` as color (tangent visualization). Red = +X tangent, green = +Y, blue = +Z. On a flat horizontal plane, you should see a solid red (tangent pointing +X along U direction). On a curved mesh, tangents should follow the UV flow. If you see noisy/random colors, your tangent generation has a bug.

**Exercise 3 — Multi-material mesh:** Create a mesh in Blender with two materials: one red, one blue, on different faces. Export to .obj. Load it with the submesh system. For each submesh, set a `uniform vec3 baseColor` in the shader — red for material 0, blue for material 1. Confirm the correct faces are each color.

**Exercise 4 — Normals from file vs computed:** Load the same mesh twice: once using `idx.normal_index` from the file, once ignoring file normals and computing flat normals from positions. Render both side-by-side (translate them apart). On a smooth curved surface (sphere or cylinder), the difference should be obvious: file normals = smooth shading, computed flat normals = faceted shading.
