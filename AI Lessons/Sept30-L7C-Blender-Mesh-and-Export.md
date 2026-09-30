# Sept30-L7C — Blender Mesh Internals and Export

**Learning objective:** Understand what Blender stores internally in its BMesh data structure, how UV unwrapping works as a mathematical parameterization, what the modifier stack does, and — critically — what data survives vs is lost when exporting to .obj or glTF.

---

## Part A — Theory

---

### 1. Why This Matters Before Writing a Loader

You are going to write code that loads meshes exported from Blender. When that mesh looks wrong — wrong normals, missing UVs, wrong scale, rotated geometry — the cause is almost always in the export step, not in your loader.

Understanding what Blender stores and what happens at export boundary means you can diagnose these problems in minutes instead of hours.

---

### 2. Blender's Internal Representation: BMesh

Blender (since version 2.63) stores mesh data in **BMesh** — a half-edge data structure. It is not a flat array. It is a fully navigable graph of elements connected by pointers.

BMesh has four element types:

#### BMVert (vertex)
- Position: (x, y, z) in local object space
- Computed normal: averaged normal from surrounding faces
- Custom data layers: float layers can be attached (see BMLoop for UVs)
- Pointers: one outgoing half-edge (to traverse the mesh)

#### BMEdge (edge)
- Two BMVert pointers: the edge's endpoints
- Doubly-linked list of BMLoop references: all loops (face-corners) using this edge
- This is how Blender finds "all faces sharing this edge" in O(1)

#### BMLoop (half-edge / face corner)
One vertex's participation in one specific face. This is the key element.

BMLoop stores:
- `v`: which BMVert
- `e`: which BMEdge (the edge from this vertex to the next vertex CCW)
- `f`: which BMFace
- `next`: next BMLoop around the same face (CCW)
- `prev`: previous BMLoop around the same face
- **Custom data layers** — this is where UV coordinates and vertex colors live

The BMLoop is the storage location for **UV coordinates**, not BMVert. One BMVert can have different UV coordinates in different loops (different faces it participates in). This is how UV seams work: the same position has two loops at a seam edge, each with different UV values.

#### BMFace (face)
- `l_first`: pointer to any one of its BMLoops (you can iterate all corners from here)
- `no`: face normal (vec3)
- `mat_nr`: material index (unsigned short)
- N-gon support: faces can have any number of loops (3 = triangle, 4 = quad, 5+ = N-gon)

---

### 3. Custom Data Layers: How UVs Actually Work

In Blender, mesh data is organized into **custom data layers**. These are typed arrays that can be attached to any element type (vertices, edges, faces, or loops).

Standard loop layers:
- `CD_MLOOPUV` — UV coordinates (one per UV channel, named "UVMap", "UVMap.001", etc.)
- `CD_MLOOPCOL` — vertex color per loop
- `CD_CUSTOMLOOPNORMAL` — custom split normals (overrides computed normals)

Standard vertex layers:
- `CD_BWEIGHT` — bevel weight
- `CD_CREASE` — edge crease (for subdivision)

Why UVs live on loops, not vertices: consider a UV seam. Two faces share an edge. Along that edge, both faces have vertices at the same positions. But on one side of the seam, face A's UV islands ends at u=1.0, while face B's island starts at u=0.0. The positions are identical — but the UV coordinates are different.

If UV were on BMVert, one vertex could only have one UV value. On BMLoop, each face-corner has its own UV value. The two loops at the seam edge carry different UVs. This is the correct representation.

---

### 4. UV Unwrapping: The Mathematical Problem

UV unwrapping is finding a **2D parameterization of a 3D surface**: a function

```
f: Surface → [0, 1]²
```

that maps every point on the mesh surface to a 2D coordinate (u, v) in a unit square.

#### Why it's a hard problem

For all but the simplest surfaces (planes, cylinders, spheres), there is no perfect parameterization. The same mathematical impossibility that prevents drawing an accurate flat world map also prevents perfect UV unwrapping. Every flat map of the Earth distorts either area, angles, or both. Every UV unwrap distorts either area or angles (or both).

The two types of distortion:
- **Angular distortion (shear):** angles of the original surface are not preserved. A square patch on the surface maps to a parallelogram in UV space. Causes textures to look "slanted."
- **Area distortion (stretch):** area of surface patches is not preserved. A large area on the surface maps to a small UV region. Causes textures to appear zoomed-in (too small) on the stretched area.

UV islands can be packed to maximize UV space utilization (Blender's "Pack Islands" tool, or RizomUV in production).

#### Seams

The way you allow a "perfect" local parameterization is by cutting the surface. **Seams** are cuts — like cutting orange peel before flattening it. Along a seam, two loops at the same geometric edge carry different UV coordinates.

Seam placement is an art. Good seams:
- Are hidden in crevices or along natural boundaries (belt lines, joint lines)
- Minimize stretching of the islands
- Enable the texture artist to paint without visible seam artifacts

In Blender: Edge Select Mode → select edges → Mark Seam (Ctrl+E → Mark Seam). UV > Unwrap runs the parameterization algorithm.

#### Unwrapping Algorithms in Blender

**Angle Based Flattening (ABF):**  
Optimizes to minimize angular distortion. Solves a least-squares system over all angles in the mesh. High quality, slower. Good default for organic meshes.

**Least Squares Conformal Maps (LSCM):**  
Conformal (angle-preserving) mapping. Solves a sparse linear system. Fast. Similar quality to ABF for most meshes.

**Smart UV Project:**  
Groups faces by approximate face-normal direction. Projects each group onto a plane and packs. No artist seam placement needed. Good for architectural meshes, bad for characters.

**Cube/Cylinder/Sphere Projection:**  
Analytic projections from Blender geometry. Cube: 6 planar projections. Cylinder: unrolls cylindrical coordinate mapping. Simple, fast, produces predictable seam lines.

---

### 5. The Modifier Stack

Blender is non-destructive. The mesh you see in the viewport is not necessarily what is stored. The **modifier stack** applies operations in sequence on top of the base mesh data.

Modifiers execute in order, top to bottom. The base mesh is never modified — modifiers generate derived meshes.

Key modifiers and their export behavior:

**Subdivision Surface (Catmull-Clark):**  
Subdivides mesh for smooth appearance. At viewport, you can set preview level (fast) vs render level (high). On export with "Apply Modifiers" enabled, the **evaluated** mesh at the render subdivision level is exported. A cube with Subdivision level 3 = 6 × 4³ = 384 quads = 768 triangles after triangulation. This can surprise you if you forget.

**Mirror:**  
Duplicates the mesh across an axis, optionally merging center vertices. On export, the mirrored geometry is included.

**Boolean:**  
Subtracts or adds another object's volume. Evaluated at export. The result may contain N-gons and T-junctions.

**Solidify:**  
Adds thickness to a surface. Creates new faces for the inner surface and connecting edge faces.

**Array:**  
Duplicates the mesh N times with an offset/rotation/scale. On export, all N instances become geometry.

**Armature (deformation):**  
Applies bone weights to deform vertices. At rest pose, export produces the rest-pose mesh. Animated poses require a different export path (FBX with animation, or glTF with animations).

**To export the base mesh without modifiers:** in .obj export dialog, uncheck "Apply Modifiers." This is rarely what you want for game assets — you want the evaluated mesh.

---

### 6. Normals in Blender: Three Levels

#### Level 1: Computed Face Normals
Computed from geometry via cross product. Always correct. Updated automatically when you edit the mesh. Never stored persistently — derived on the fly.

#### Level 2: Vertex Normals (Smooth Shading)
Blender computes vertex normals by averaging surrounding face normals. "Auto Smooth" controls the threshold: if the angle between two adjacent face normals is below the threshold (default 30°), their shared edge gets smooth vertex normals. If above, the edge gets hard — vertex is split at export.

In Blender 4.x: the "Shade Smooth" operator sets all edges smooth; "Shade Auto Smooth" uses the angle threshold; "Mark Sharp" on individual edges forces them hard regardless of angle.

#### Level 3: Custom Split Normals
Artist-defined normals stored per loop (face-corner). These override computed normals entirely. Used in hard-surface modeling to control lighting independent of topology — a technique called **normal transfer** or **normal editing**.

Example use case: a car door mesh with sharp creases along styling lines, but the surrounding surface appears smooth. The topology has the crease geometry, but the normals are transferred from a smoothed version to control highlight behavior.

These are stored as the `CD_CUSTOMLOOPNORMAL` custom data layer in BMesh. On export to .obj, they appear as `vn` values that differ from what you'd compute from the geometry. On export to glTF, they appear as the NORMAL attribute data.

---

### 7. Triangulation on Export

BMesh supports N-gons. .obj and most real-time formats expect triangles (or quads for formats that support them).

When you check "Triangulate Faces" in the .obj export dialog, Blender runs ear-clipping triangulation on every face with more than 3 vertices.

**Quads:** split along the shortest diagonal (minimizes the aspect ratio of resulting triangles). Blender 3.x uses "Beauty" triangulation by default (shortest diagonal).

**N-gons:** ear-clipping. One vertex at a time, find an "ear" (a triangle where all vertices are within the polygon), clip it, repeat. Non-optimal for large N-gons — may produce thin triangles.

**For real-time assets:** always triangulate before export, or use quads-only topology and let the exporter triangulate. N-gons in source geometry should be resolved in Blender before export (Mesh → Faces → Triangulate, or Alt+J to convert to quads first).

---

### 8. The Coordinate System Trap

This is the single most common confusion when moving from Blender to OpenGL.

**Blender native coordinate system:**
- Z axis = Up
- Y axis = Forward (into screen in front orthographic view)
- X axis = Right
- Right-handed

**OpenGL coordinate system (standard right-handed):**
- Y axis = Up
- -Z axis = Forward (into screen from camera, looking down -Z)
- X axis = Right
- Right-handed

A mesh resting on the "floor" in Blender (Z = 0, mesh extends upward in +Z) will appear lying on its back in OpenGL (Y = 0, mesh extends in +Z away from viewer).

#### The Fix: Axis Conversion at Export

In Blender's .obj export dialog:
- **Forward:** -Z
- **Up:** Y

This applies a -90° rotation around X to all geometry before writing. The resulting .obj file has Y as up, matching OpenGL convention.

**glTF:** the glTF 2.0 specification mandates Y-up. Blender's glTF exporter always converts. When loading glTF, you do NOT need an additional rotation.

**FBX:** FBX has its own coordinate system metadata and most importers handle it automatically.

If you load a Blender .obj that was exported without axis conversion (Forward: Y, Up: Z — Blender defaults), your model will appear rotated 90° around X. Fix in OpenGL by pre-multiplying the model matrix:
```cpp
glm::mat4 model = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
```
This is a workaround. Prefer exporting with correct axis conversion.

---

### 9. Worked Example: What a Cube Export Produces

Setup: Blender default cube. 8 positions, 6 quad faces. Flat shading. No modifiers. Default UV map (cube projection applied).

Export: .obj, Apply Modifiers, Include Normals, Include UVs, Triangulate, Forward: -Z, Up: Y.

**Post-export .obj structure (abbreviated):**

```
# Blender 4.x OBJ File
mtllib cube.mtl

o Cube

# Vertex positions — 8 unique positions
v  1.000000  1.000000 -1.000000
v  1.000000 -1.000000 -1.000000
v  1.000000  1.000000  1.000000
v  1.000000 -1.000000  1.000000
v -1.000000  1.000000 -1.000000
v -1.000000 -1.000000 -1.000000
v -1.000000  1.000000  1.000000
v -1.000000 -1.000000  1.000000

# Vertex normals — 6, one per face direction
vn  0.0000  1.0000  0.0000
vn  0.0000  0.0000  1.0000
vn -1.0000  0.0000  0.0000
vn  0.0000 -1.0000  0.0000
vn  1.0000  0.0000  0.0000
vn  0.0000  0.0000 -1.0000

# UV coordinates — varies, cube projection
vt 0.000000 0.000000
vt 1.000000 0.000000
vt 1.000000 1.000000
vt 0.000000 1.000000
(... more UV entries for each face ...)

usemtl Material
s off

# Faces — 12 triangles (6 quads × 2)
f 1/1/1 5/2/1 7/3/1
f 1/1/1 7/3/1 3/4/1
f 4/1/2 3/2/2 7/3/2
f 4/1/2 7/3/2 8/4/2
f 8/1/3 7/2/3 5/3/3
f 8/1/3 5/3/3 6/4/3
(... 6 more face lines ...)
```

**Observation 1:** 8 position vertices (`v` lines), 6 normals (`vn` lines, one per face direction). Only 6 normals because flat shading = all corners of a face share one normal.

**Observation 2:** Face syntax `1/1/1` = position index 1, UV index 1, normal index 1. All three indices are 1-based.

**Observation 3:** Face `f 1/1/1 5/2/1 7/3/1` — all three corners share normal index 1 (top face normal). Each corner has a different position index AND a different UV index.

**Now switch to Smooth Shading and re-export:**

`vn` count increases dramatically. Each corner of each face now has a different normal (the interpolated vertex normal, not the face normal). A cube with 12 triangles × 3 corners = 36 corners, many sharing the same computed vertex normal — but the `vn` count can still be up to 24 (vertex-normal combinations per vertex-split cube).

OpenGL vertex count for flat-shaded cube: 24 (4 per face × 6 faces — vertex split at each sharp edge).
OpenGL vertex count for smooth-shaded cube: could be 8 (if no UV seams force splits) — but with UV seams from cube projection, likely 24 as well.

---

### 10. What Survives .obj Export

| Blender Data | Survives .obj Export? | Notes |
|---|---|---|
| Vertex positions | Yes | In `v` lines |
| Computed normals | Yes (if exported) | In `vn` lines |
| Custom split normals | Yes | Baked into `vn` |
| UV coordinates | Yes (if exported) | First UV channel only |
| Material names | Yes | Via `mtllib` + `usemtl` |
| Diffuse texture paths | Yes | In .mtl `map_Kd` |
| Normal map paths | Yes | In .mtl `map_bump` |
| Specular maps | Yes | In .mtl `map_Ks` |
| Vertex colors | **No** | Not supported in .obj |
| Multiple UV channels | **No** | Only first channel |
| Armature/bone weights | **No** | Baked rest pose only |
| Shape keys | **No** | Only active key |
| Modifier stack (unapplied) | **No** | Evaluated mesh only |
| Object hierarchy | **No** | Each object separate |
| Animations | **No** | Static mesh only |
| PBR roughness/metallic | **No** | .mtl predates PBR |
| Emission HDR values | Partial | Ke term, no intensity |

**What survives glTF 2.0 export (contrast):**

| Blender Data | Survives glTF Export? | Notes |
|---|---|---|
| Vertex positions, normals, UVs | Yes | All standard |
| Multiple UV channels | Yes | TEXCOORD_0, TEXCOORD_1 |
| Vertex colors | Yes | COLOR_0 |
| PBR metallic-roughness | Yes | Native to glTF material model |
| Tangents | Yes | TANGENT attribute (VEC4 with handedness) |
| Armature/bone weights | Yes | JOINTS_0, WEIGHTS_0 |
| Animations | Yes | Keyframe + skinning |
| Scene hierarchy | Yes | Full node tree |
| Morph targets | Yes | Blendshapes |
| Embedded textures | Yes | In .glb binary blob |
| Custom split normals | Yes | Baked into NORMAL attribute |

This is why glTF is the industry direction for real-time assets. .obj is still used for simple static geometry interchange.

---

### 11. The Scale Issue: Apply Transforms Before Export

In Blender, objects have a **transform** that is separate from their geometry: location, rotation, and scale applied at the object level (not in edit mode).

If you scale an object to (2, 1, 0.5) in object mode without "Applying" it, the object data in BMesh still has the original vertex positions. The scale is a transform on the object container.

When you export to .obj with "Apply Modifiers" but NOT applying the object transform, the exported positions do NOT include the scale. The mesh in OpenGL will appear at its original unscaled size.

Worse: the normals are also wrong. A non-uniform scale (2, 1, 0.5) distorts normals — they no longer point perpendicular to the surface. The inverse-transpose correction (normal matrix) must be applied.

**Fix:** before export, select all, Ctrl+A → Apply → All Transforms. Verify: Object Properties panel, Transform section → Location, Rotation, Scale should all read identity (0, 0, 0 / 0°, 0°, 0° / 1.000, 1.000, 1.000).

---

### 12. Pitfalls

**Pitfall 1: Smooth Normals Look Correct in Blender, Wrong in OpenGL**

Exact mistake: exporting without "Include Normals" checked. OpenGL loader computes normals from geometry — but geometry is triangulated, and triangulation changes edge angles slightly. Computed normals differ from Blender's split normals.

Exact symptom: mesh appears with slightly different lighting in OpenGL than Blender. Edge highlights in the wrong place. Especially visible on hard-surface models with stylized normal flow.

Exact fix: always export with "Include Normals" checked. In tinyobjloader, use `attrib.normals` from the parsed data rather than computing normals from positions.

---

**Pitfall 2: Axis Mismatch — Model Appears Rotated**

Exact mistake: exporting .obj without changing axis convention from Blender defaults (Forward: Y, Up: Z).

Exact symptom: model appears lying on its side (rotated -90° around X axis) in OpenGL. A character stands upright in Blender, lies on their back in OpenGL.

Exact fix: in Blender .obj export, set Forward: -Z, Up: Y. Or apply a -90° X rotation in your model matrix as a temporary workaround. Prefer fixing the export.

---

**Pitfall 3: N-gon Triangulation Artifacts**

Exact mistake: mesh contains N-gons (5+ sided faces). Not triangulated before export. Loader receives quads or N-gons and cannot handle them (tinyobjloader triangulates, but the result may surprise).

Exact symptom: triangulation is done by ear-clipping, which may produce very thin triangles. These thin triangles produce sharp shading discontinuities visible in smooth-shaded mode. Sometimes certain faces appear inverted.

Exact fix: in Blender, select all, Mesh → Faces → Triangulate (Ctrl+T). Inspect result for bad triangles. Fix by manually splitting problem faces. Export triangulated mesh.

---

**Pitfall 4: Subdivision Surface Not Applied at Correct Level**

Exact mistake: Subdivision Surface modifier set to Viewport=1, Render=3. Exporting with "Apply Modifiers" uses the render level (3). Resulting mesh is 64× the base poly count.

Exact symptom: loader is slow, VBO is unexpectedly large, mesh appears smooth but performance is bad. The subdivision detail you wanted for rendering is baked into geometry instead of being done by a normal map.

Exact fix: before exporting game assets, either: (1) set Subdivision level to 0 and bake normals to a texture (normal map workflow), or (2) set Viewport and Render levels both to the desired level explicitly.

---

**Pitfall 5: UV Seams Cause Doubled Vertex Count**

Not a bug — correct behavior to understand.

At every UV seam, vertices are split. A mesh with many seams (e.g., a character with seams along every clothing boundary) has significantly more OpenGL vertices than Blender vertices.

Exact symptom: OpenGL vertex count is 3–5× the Blender vertex count. Not wrong, just surprising.

Exact reason: each seam edge has two loops (one per adjacent face), each with different UV coordinates. These become two OpenGL vertices at the same position.

Exact implication: the more UV seams you have, the larger your VBO. Minimize seams while still avoiding stretch, placing them in areas not visible in final renders.

---

### 13. What to Build

**Exercise 1:** Take any mesh in Blender (cube, cylinder, or your own). Export it twice: once with "Include Normals" unchecked, once with it checked. In your OpenGL program, compute normals from geometry for the first file, and use the file's normals for the second. Render both. Can you see a difference? On what geometry shapes would the difference be most visible?

**Exercise 2:** Create a simple mesh in Blender. In UV Editing workspace, mark specific seams and unwrap. Export to .obj. Open the file and count: how many `v` lines? How many `vn` lines? How many unique (v, vt, vn) triplets appear in the face lines? This is your OpenGL vertex count.

**Exercise 3:** Add a Subdivision Surface modifier (level 2) to a cube. Export once with Apply Modifiers ON. Export again with Apply Modifiers OFF. Compare face counts in both files. Explain the difference.

**Exercise 4:** Scale a cube to (2, 1, 0.5) in Blender's object mode (S → X → 2, etc.). Export without applying transforms. Load in OpenGL. Then apply transforms (Ctrl+A → Scale) and re-export. Load again. Compare the visual sizes. Add a Phong lighting shader — the non-applied-scale version will have slightly wrong normals on a point light test.
