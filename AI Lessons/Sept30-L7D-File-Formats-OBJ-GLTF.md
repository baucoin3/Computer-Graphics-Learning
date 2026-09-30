# Sept30-L7D — File Formats: .obj and glTF in Depth

**Learning objective:** Read and manually parse .obj and glTF 2.0 files from first principles, understand why .obj's triple-index system requires deduplication before GPU upload, and know when to use each format.

---

## Part A — Theory

---

### 1. Why Understand File Formats at the Byte Level

You will hit a bug where your loaded mesh has scrambled normals, flipped UVs, missing materials, or half the faces pointing the wrong way. Every one of those bugs is either in the file format specification or in how your code maps from file data to GPU data.

If you treat the file format as a black box, you cannot diagnose these bugs. If you understand the format, you can open the file in a text editor, trace the problem to one specific line, and fix it in minutes.

---

## Section 1: The .obj Format

### 1.1 History and Design Philosophy

The Wavefront OBJ format was created by Wavefront Technologies in the late 1980s for their Advanced Visualizer software. It is plain ASCII text. Every 3D tool on earth can read and write it.

It was NOT designed for real-time rendering. It was designed as an interchange format between modeling tools. Its triple-index system is optimized for storage compactness (share normals across many vertices), not for GPU upload.

This mismatch between .obj's storage model and OpenGL's storage model is the core technical challenge of loading .obj files.

### 1.2 The .obj Grammar

Every non-blank line in an .obj file starts with a keyword. Lines are independent — order matters (definitions come before references), but each line is self-contained.

**Geometry keywords:**

```
v   x y z [w]       # Geometric vertex position. w optional, default 1.0.
                    # Coordinates in local/object space.
                    # Lines are numbered 1, 2, 3, ... (1-based, NOT 0-based)

vn  nx ny nz        # Vertex normal. NOT guaranteed to be unit length.
                    # You must normalize when using. Also 1-indexed.

vt  u [v] [w]       # Texture coordinate. v and w optional, default 0.
                    # v=0 at bottom of image (OpenGL convention).
                    # Also 1-indexed.

vp  u v [w]         # Parameter space vertex (for NURBS/curves). Ignore for real-time.
```

**Face keyword:**

```
f  v1[/vt1][/vn1]  v2[/vt2][/vn2]  v3[/vt3][/vn3]  ...
```

Square brackets denote optional components. All indices are 1-based.

Valid face formats:
```
f 1 2 3                  # position only, no UV, no normal
f 1/2 3/4 5/6            # position + UV, no normal (note: two slashes, one slash each pair)
f 1//2 3//4 5//6         # position + normal, no UV (note: empty slot between slashes)
f 1/2/3 4/5/6 7/8/9      # position + UV + normal (full triplet)
```

Faces can have more than 3 vertices (quads, N-gons). Most real-time loaders triangulate them.

**Negative indices:** .obj allows negative indices meaning "count from the end." `-1` means the most recently defined vertex, `-2` the one before that, etc. Rare in practice but valid:
```
f -3//-1  -2//-1  -1//-1   # last three positions, last normal
```

**Group and material keywords:**

```
mtllib file.mtl      # Reference to material library file. Path relative to .obj.
usemtl matname       # Apply named material to all subsequent faces until next usemtl.
o  ObjectName        # Object name (for multi-object .obj files)
g  GroupName         # Group name (logical grouping, does not affect face data)
s  1                 # Smooth shading group: integer ID or "off". Rarely used now.
#  any text          # Comment line. Ignored.
```

### 1.3 The Three Pools and Why They Are Separate

.obj maintains three completely independent arrays:

**Pool 1: Positions** — all `v` lines, accumulated in order, 1-indexed.
**Pool 2: UV coordinates** — all `vt` lines, accumulated in order, 1-indexed.  
**Pool 3: Normals** — all `vn` lines, accumulated in order, 1-indexed.

Face lines reference all three pools simultaneously but **independently**. Corner `4/7/2` means: use position[4], UV coordinate[7], normal[2].

Why this design? In the 1980s, storage was expensive. Many faces share the same normal (all faces on the top of a cube share the same upward normal). By storing normals in a separate pool and referencing them independently, the format avoids storing the same normal data repeatedly.

A mesh with 8 positions and 6 normals (one per face of a cube) stores those 8+6=14 values. A flat representation would store all three attributes per vertex per face: 36 vertices × 3 values = 108.

But this optimization creates a problem for GPU upload.

### 1.4 The Core Problem: Triple Indices vs Single Index

**OpenGL requires exactly one index per vertex.** When you call `glDrawElements`, each index points to ONE location in the VBO that contains ALL attributes (position, normal, UV) for that vertex.

**.obj has three independent indices per corner.** The same position index can appear with different normal indices. The same normal index can appear with different UV indices.

This means **you cannot use .obj's position indices directly as OpenGL vertex indices.**

Consider this minimal case:
```
v 0 0 0
v 1 0 0
v 0 1 0
vn 0 0 1
vt 0 0
vt 1 0
vt 0 1

f 1/1/1  2/2/1  3/3/1
f 1/3/1  3/1/1  2/2/1   ← second triangle, different UV for vertex 1
```

Face 1: corner at v=1 uses UV=1.
Face 2: corner at v=1 uses UV=3.

In OpenGL, vertex at position index 1 can only have one UV coordinate. You must create two separate OpenGL vertices — same position, different UV:
```
OpenGL Vertex A: pos(0,0,0)  uv(0,0)  normal(0,0,1)  ← used by face 1 corner 0
OpenGL Vertex B: pos(0,0,0)  uv(0,1)  normal(0,0,1)  ← used by face 2 corner 0
```

The rule is: an OpenGL vertex is uniquely identified by the **triplet** (pos_idx, uv_idx, normal_idx). Two corners with different triplets are different OpenGL vertices even if they share the same position.

---

### Part B — Worked Example: Manual .obj Parse

#### The File

```
# Simple triangle with UV and normal
v  0.0  0.0  0.0
v  2.0  0.0  0.0
v  0.0  3.0  0.0
v  2.0  3.0  0.0

vn  0.0  0.0  1.0

vt  0.00  0.00
vt  1.00  0.00
vt  0.00  1.00
vt  1.00  1.00

# A quad defined as two triangles
f  1/1/1  2/2/1  3/3/1
f  2/2/1  4/4/1  3/3/1
```

#### Step 1: Build the Three Pools

Pool 1 — positions (1-indexed):
```
pos[1] = (0.0, 0.0, 0.0)
pos[2] = (2.0, 0.0, 0.0)
pos[3] = (0.0, 3.0, 0.0)
pos[4] = (2.0, 3.0, 0.0)
```

Pool 2 — UVs (1-indexed):
```
uv[1] = (0.00, 0.00)
uv[2] = (1.00, 0.00)
uv[3] = (0.00, 1.00)
uv[4] = (1.00, 1.00)
```

Pool 3 — normals (1-indexed):
```
n[1] = (0.0, 0.0, 1.0)
```

#### Step 2: Parse Faces and Deduplicate

Process face line `f  1/1/1  2/2/1  3/3/1`:

Corner 0: triplet (pos=1, uv=1, normal=1)
- Not in map. Create OpenGL Vertex 0: pos=(0,0,0), uv=(0,0), n=(0,0,1)
- Map: {(1,1,1) → 0}

Corner 1: triplet (pos=2, uv=2, normal=1)
- Not in map. Create OpenGL Vertex 1: pos=(2,0,0), uv=(1,0), n=(0,0,1)
- Map: {(1,1,1)→0, (2,2,1)→1}

Corner 2: triplet (pos=3, uv=3, normal=1)
- Not in map. Create OpenGL Vertex 2: pos=(0,3,0), uv=(0,1), n=(0,0,1)
- Map: {(1,1,1)→0, (2,2,1)→1, (3,3,1)→2}

Index buffer so far: [0, 1, 2]

Process face line `f  2/2/1  4/4/1  3/3/1`:

Corner 0: triplet (pos=2, uv=2, normal=1) → already in map → index 1. Reuse!
Corner 1: triplet (pos=4, uv=4, normal=1)
- Not in map. Create OpenGL Vertex 3: pos=(2,3,0), uv=(1,1), n=(0,0,1)
- Map: {..., (4,4,1)→3}
Corner 2: triplet (pos=3, uv=3, normal=1) → already in map → index 2. Reuse!

Index buffer: [0, 1, 2,  1, 3, 2]

#### Final OpenGL Buffers

**Vertex Buffer (interleaved: pos xyz, uv xy, normal xyz):**
```
Vertex 0: 0.0  0.0  0.0  |  0.0  0.0  |  0.0  0.0  1.0
Vertex 1: 2.0  0.0  0.0  |  1.0  0.0  |  0.0  0.0  1.0
Vertex 2: 0.0  3.0  0.0  |  0.0  1.0  |  0.0  0.0  1.0
Vertex 3: 2.0  3.0  0.0  |  1.0  1.0  |  0.0  0.0  1.0
```

**Index Buffer:**
```
0, 1, 2,   1, 3, 2
```

4 OpenGL vertices. 6 indices (2 triangles). Original .obj had 4 positions, 4 UVs, 1 normal. Deduplication preserved vertex 1 and vertex 2 (same triplets appeared in both triangles).

Draw call: `glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0)` — correct.

---

### 1.5 The .mtl Material Library

.obj references a material library with `mtllib`. The .mtl file defines named materials that .obj faces reference with `usemtl`.

.mtl format is the **Phong lighting model** from the 1980s. No PBR.

```
newmtl MaterialName       # Begin new material definition

# Color terms (Phong model)
Ka 0.2 0.2 0.2            # Ambient color (R G B, 0–1)
Kd 0.8 0.6 0.4            # Diffuse color (albedo)
Ks 1.0 1.0 1.0            # Specular color
Ke 0.0 0.0 0.0            # Emissive color
Ns 50.0                   # Specular exponent (shininess, 1–1000)
Ni 1.45                   # Optical density (refraction index)
d  1.0                    # Dissolve: 1.0 = fully opaque, 0.0 = transparent
Tr 0.0                    # Transparency (1.0 - d), alternative notation

# Texture maps
map_Kd  diffuse.png       # Diffuse/albedo texture (path relative to .mtl)
map_Ks  specular.png      # Specular map
map_Ns  roughness.png     # Shininess/roughness map
map_bump  normal.png      # Normal map (or map_Kn in some exporters)
map_d   opacity.png       # Opacity map
map_Ka  ao.png            # Ambient occlusion map

# Illumination model
illum 2
# illum 0 = constant color (no lighting)
# illum 1 = Lambertian diffuse only
# illum 2 = full Phong (diffuse + specular)
# illum 3 = ray-traced mirror reflection (not real-time relevant)
# illum 6 = refraction + transparency
```

**The PBR limitation:** map_Kd maps to albedo, and Ks maps roughly to specular. But there is no `metallic` or `roughness` field. When PBR workflows produce .obj files, exporters often abuse non-standard fields. Tools like Blender may write PBR data as non-standard .mtl extensions that only certain importers understand.

This is the primary technical reason .obj is being replaced by glTF for real-time assets.

---

## Section 2: The glTF 2.0 Format

### 2.1 Design Philosophy

**GL Transmission Format**, version 2.0 (released 2017 by Khronos Group). Designed explicitly for real-time rendering, PBR materials, and efficient GPU upload.

Goals:
- **Vendor-neutral:** defined by the same consortium that defines OpenGL, Vulkan, WebGL
- **PBR-native:** material model is physically-based metallic-roughness, not legacy Phong
- **GPU-upload-ready:** binary data is raw float arrays that can be sent directly to `glBufferData`
- **Complete:** contains geometry, materials, textures, scene hierarchy, animation, skinning in one format

### 2.2 Physical Structure

Two packaging variants:

**.gltf + .bin + textures:** JSON file + separate binary blob + separate image files. Human-readable JSON. Good for debugging and asset pipelines.

**.glb (binary glTF):** single binary file. Header + JSON chunk + binary chunk + (optional) embedded image chunks. Preferred for shipping — one file, no dependency management.

### 2.3 The JSON Hierarchy

glTF JSON describes the scene at a high level. Binary data is NOT in the JSON — it lives in the binary buffer. The JSON just describes where to find it and how to interpret it.

Top-level keys in a glTF JSON:

```json
{
  "asset": { "version": "2.0", "generator": "Blender 4.x" },
  "scene": 0,
  "scenes": [ { "nodes": [0] } ],
  "nodes": [ ... ],
  "meshes": [ ... ],
  "accessors": [ ... ],
  "bufferViews": [ ... ],
  "buffers": [ ... ],
  "materials": [ ... ],
  "textures": [ ... ],
  "images": [ ... ],
  "samplers": [ ... ]
}
```

### 2.4 Meshes and Primitives

```json
"meshes": [{
  "name": "Cube",
  "primitives": [{
    "attributes": {
      "POSITION":   0,
      "NORMAL":     1,
      "TEXCOORD_0": 2,
      "TANGENT":    3
    },
    "indices": 4,
    "material": 0,
    "mode": 4
  }]
}]
```

`mode: 4` = `GL_TRIANGLES` (same value as the OpenGL constant).

`attributes` maps semantic names to accessor indices. The accessor describes the actual data.

Standard attribute names (from glTF 2.0 spec):
- `POSITION` — vec3, vertex positions
- `NORMAL` — vec3, vertex normals (unit length)
- `TANGENT` — vec4, tangent vector + handedness (w = ±1)
- `TEXCOORD_0`, `TEXCOORD_1` — vec2, UV channels
- `COLOR_0` — vec3 or vec4, vertex color
- `JOINTS_0` — uvec4, bone indices for skinning
- `WEIGHTS_0` — vec4, bone weights for skinning

### 2.5 Accessors

An accessor describes how to interpret typed data from a BufferView:

```json
"accessors": [
  {
    "bufferView": 0,
    "byteOffset": 0,
    "componentType": 5126,
    "count": 24,
    "type": "VEC3",
    "max": [1.0, 1.0, 1.0],
    "min": [-1.0, -1.0, -1.0]
  }
]
```

`componentType` values (OpenGL constants, reused directly):
- `5120` = `GL_BYTE` (int8)
- `5121` = `GL_UNSIGNED_BYTE` (uint8)
- `5122` = `GL_SHORT` (int16)
- `5123` = `GL_UNSIGNED_SHORT` (uint16)
- `5125` = `GL_UNSIGNED_INT` (uint32)
- `5126` = `GL_FLOAT` (float32)

`type` values:
- `"SCALAR"` — single component
- `"VEC2"` — 2 components
- `"VEC3"` — 3 components
- `"VEC4"` — 4 components
- `"MAT2"`, `"MAT3"`, `"MAT4"` — matrix types

`count`: number of elements (not bytes). For the accessor above: 24 VEC3 floats = 24 × 3 × 4 bytes = 288 bytes.

`min/max`: bounding box for POSITION accessor. Required for spatial queries and frustum culling at load time.

### 2.6 BufferViews

A BufferView is a contiguous slice of a buffer:

```json
"bufferViews": [
  {
    "buffer": 0,
    "byteOffset": 0,
    "byteLength": 288,
    "target": 34962
  },
  {
    "buffer": 0,
    "byteOffset": 288,
    "byteLength": 288,
    "target": 34962
  },
  {
    "buffer": 0,
    "byteOffset": 576,
    "byteLength": 192,
    "target": 34962
  },
  {
    "buffer": 0,
    "byteOffset": 768,
    "byteLength": 48,
    "target": 34963
  }
]
```

`target` values:
- `34962` = `GL_ARRAY_BUFFER` — vertex attribute data
- `34963` = `GL_ELEMENT_ARRAY_BUFFER` — index data

### 2.7 Buffers

The buffer is the raw binary data:

```json
"buffers": [{
  "uri": "cube.bin",
  "byteLength": 816
}]
```

For .glb files, the buffer's binary data is embedded in the file's binary chunk, and `uri` is omitted (or set to `"data:application/octet-stream;base64,..."` for inline data).

### 2.8 How to Read a POSITION Array

Trace the chain for `attributes.POSITION = 0`:

1. Accessor[0]: bufferView=0, byteOffset=0, componentType=5126 (float), count=24, type=VEC3
2. BufferView[0]: buffer=0, byteOffset=0, byteLength=288
3. Buffer[0]: read "cube.bin"

Go to byte 0 of "cube.bin". Read 288 bytes. Interpret as 24 × float[3]. These are your vertex positions.

In code:
```cpp
float* positions = reinterpret_cast<float*>(binData + bufferView.byteOffset + accessor.byteOffset);
for (int i = 0; i < accessor.count; i++) {
    float x = positions[i * 3 + 0];
    float y = positions[i * 3 + 1];
    float z = positions[i * 3 + 2];
}
```

Note: there are two `byteOffset` values — one on the accessor and one on the bufferView. The total byte offset is `bufferView.byteOffset + accessor.byteOffset`. Accessor byteOffset is always relative to the start of the bufferView.

### 2.9 Interleaved Data and byteStride

Some glTF files store vertex attributes interleaved — position, normal, UV for vertex 0, then position, normal, UV for vertex 1, etc. — all in one BufferView.

In this case, the BufferView has a `byteStride` field:

```json
{
  "buffer": 0,
  "byteOffset": 0,
  "byteLength": 768,
  "byteStride": 32,   ← 8 floats per vertex: pos(3) + normal(3) + uv(2) = 8 × 4 = 32 bytes
  "target": 34962
}
```

Each accessor points into the same BufferView with a different `byteOffset` into the stride block:
- POSITION accessor: byteOffset=0 within stride (starts at byte 0 of each 32-byte block)
- NORMAL accessor: byteOffset=12 within stride (starts at byte 12 = after 3 floats of position)
- TEXCOORD_0 accessor: byteOffset=24 within stride (starts at byte 24 = after 6 floats)

If you ignore `byteStride` and read sequentially, you get garbage — you're reading normal data as position data.

### 2.10 glTF Material: PBR Metallic-Roughness

```json
"materials": [{
  "name": "MetalPlate",
  "pbrMetallicRoughness": {
    "baseColorTexture":         { "index": 0, "texCoord": 0 },
    "baseColorFactor":          [1.0, 1.0, 1.0, 1.0],
    "metallicRoughnessTexture": { "index": 1, "texCoord": 0 },
    "metallicFactor":           1.0,
    "roughnessFactor":          0.4
  },
  "normalTexture":    { "index": 2, "texCoord": 0, "scale": 1.0 },
  "occlusionTexture": { "index": 3, "texCoord": 0, "strength": 1.0 },
  "emissiveTexture":  { "index": 4, "texCoord": 0 },
  "emissiveFactor":   [0.0, 0.0, 0.0],
  "alphaMode":        "OPAQUE",
  "doubleSided":      false
}]
```

`metallicRoughnessTexture` channel packing (glTF spec, section 3.9.3):
- **R channel:** NOT USED (reserved for occlusion in some extensions)
- **G channel:** Roughness value (0.0 = perfectly smooth, 1.0 = fully rough)
- **B channel:** Metallic value (0.0 = dielectric, 1.0 = metal)

This packing is not an accident — it matches the packing used in Unreal Engine's metallic-roughness texture layout. Unreal's UE4/UE5 PBR pipeline uses: R=AO, G=Roughness, B=Metallic — identical to glTF.

`baseColorTexture` is sRGB-encoded. When you sample it in a shader, convert to linear before lighting: use `GL_SRGB8_ALPHA8` as the OpenGL internal format so the GPU hardware converts automatically on sample.

`normalTexture` and `metallicRoughnessTexture` are **data textures** — they store float data encoded as uint8. Do NOT use `GL_SRGB8_ALPHA8` for these. Use `GL_RGBA8`. Applying sRGB correction to normal map data produces wildly wrong normals.

### 2.11 Tangents in glTF

glTF includes tangent data as the `TANGENT` attribute. Format: VEC4.

- `tangent.xyz` — the tangent vector in model space (unit length)
- `tangent.w` — handedness: +1.0 or -1.0

The bitangent is computed in the shader as:
```glsl
vec3 bitangent = cross(normal, tangent.xyz) * tangent.w;
```

The `w` component is needed because bitangent direction can differ between left-handed and right-handed coordinate systems, and some meshes (especially mirrored geometry) require the bitangent to be flipped. The cross product alone cannot encode this flip — hence the sign stored in `w`.

With glTF, you do NOT need to compute tangents at load time — they're in the file.
With .obj, you compute them yourself (covered in L7E).

### 2.12 Textures, Images, Samplers

```json
"textures": [{ "source": 0, "sampler": 0 }],
"images":   [{ "uri": "diffuse.png" }],
"samplers": [{
  "magFilter": 9729,
  "minFilter": 9987,
  "wrapS": 10497,
  "wrapT": 10497
}]
```

Sampler values are OpenGL constants:
- `9729` = `GL_LINEAR` (mag filter)
- `9987` = `GL_LINEAR_MIPMAP_LINEAR` (min filter with mipmaps)
- `10497` = `GL_REPEAT`
- `33071` = `GL_CLAMP_TO_EDGE`
- `33648` = `GL_MIRRORED_REPEAT`

In .glb, images can be embedded as binary chunks with a MIME type of `"image/png"` or `"image/jpeg"`. No external file dependency.

---

### 2.13 .obj vs glTF: Decision Matrix

| Criterion | .obj | glTF 2.0 |
|---|---|---|
| Format | ASCII text | JSON + binary |
| Human-readable | Yes (at cost of file size) | JSON header only |
| PBR materials | No (Phong .mtl) | Yes (metallic-roughness) |
| Embedded textures | No | Yes (.glb) |
| Tangents | Not stored | Yes (VEC4 with handedness) |
| Multiple UV channels | First only | TEXCOORD_0 through N |
| Vertex colors | No | Yes |
| Skinning / animation | No | Yes |
| Morph targets | No | Yes |
| Scene hierarchy | Limited (o/g) | Full node tree |
| Coordinate system | Tool-dependent | Y-up guaranteed |
| File size | Large | Compact (.glb) |
| GPU upload overhead | High (deduplication required) | Low (binary-direct) |
| Tool support | Universal | Nearly universal (2023+) |
| Debuggability | Easy (text editor) | Medium (.gltf text, .glb binary) |

**When to use .obj:**
- Simple test geometry (a cube, a quad)
- One-off debugging meshes
- Interfacing with a legacy tool that only exports .obj
- Your loader is simple and you don't need PBR materials

**When to use glTF:**
- Any production game asset
- Assets with PBR materials
- Characters with animations
- Multiple UV channels (lightmaps, detail maps)
- Embedded textures for single-file portability

**Industry direction:** glTF 2.0 is explicitly endorsed by Khronos, Epic (Unreal Engine's glTF import), Unity, Autodesk, Microsoft (3D Viewer), and Google. It is the designated successor to .obj for real-time interchange. The **glTF tools extension** for VS Code lets you inspect .gltf/glb files visually — use it.

---

### 3. Pitfalls

**Pitfall 1: Using .obj Position Index Directly as OpenGL Vertex Index**

Exact mistake: `indices.push_back(face.v_idx - 1)` without checking for unique (v, vt, vn) triplets.

Exact symptom: UVs appear visually scrambled across the mesh. Normals look wrong on smooth-shaded meshes. The geometry itself looks correct (positions are right) but shading/texturing is broken.

Exact fix: hash map over (v_idx, vt_idx, normal_idx) triplet → OpenGL vertex index. Only create a new OpenGL vertex when the triplet is new. Code example in L7E.

---

**Pitfall 2: .obj Indices Are 1-Based, Not 0-Based**

Exact mistake: `positions[face.v_idx]` — off by one. This accesses the SECOND position for corner 0 (face.v_idx=1 accesses index 1, which is the second element).

Exact symptom: first triangle of mesh is completely wrong — has one vertex at a garbage position or at the position of a different vertex. The rest of the mesh may look approximately correct.

Exact fix: always subtract 1 when reading .obj indices. `positions[face.v_idx - 1]`. tinyobjloader does this for you — but if parsing manually, you must remember.

---

**Pitfall 3: glTF sRGB Albedo Treated as Linear**

Exact mistake: `glTexImage2D(..., GL_RGBA8, ...)` for the baseColorTexture.

Exact symptom: colors appear washed out, too bright, with insufficient contrast. Lighting looks incorrect — gamma-encoded values (0.5 appears as 0.214 in linear) are used as raw linear light values. Highly visible on skin, bright fabric, and mid-grey surfaces.

Exact fix: `glTexImage2D(..., GL_SRGB8_ALPHA8, ...)` for albedo/baseColor textures. The GPU converts from sRGB to linear on sample in hardware, with correct conversion. Use `GL_RGBA8` for normal maps, roughness/metallic maps, and any other data textures.

---

**Pitfall 4: Missing byteStride Handling in glTF**

Exact mistake: reading accessor data without checking BufferView.byteStride. Assuming all VEC3 floats are packed 12 bytes apart.

Exact symptom: positions, normals, or UVs are completely wrong — values are mixtures of different attributes. Mesh appears as a cloud of random geometry.

Exact fix: check `bufferView.byteStride`. If zero (or absent), data is tightly packed — stride = componentSize × componentCount. If nonzero, use it: `float* element = basePtr + i * byteStride`.

---

**Pitfall 5: .mtl File Not Found**

Exact mistake: .obj is loaded from a directory, but .mtl file is not co-located or loader's working directory differs.

Exact symptom: loader reports no error (many loaders silently skip missing .mtl) but mesh loads with default material. All faces are white/grey with no texture. Normals and geometry are correct.

Exact fix: ensure .mtl and all texture files are in the same directory as .obj. Pass a `base_path` to your loader when loading from a non-current directory. Check loader documentation — tinyobjloader takes an explicit material directory parameter.

---

### 4. What to Build

**Exercise 1 — Manual parse:** Write a C++ function with no library dependencies that reads a minimal .obj file (just `v`, `vn`, `vt`, `f` lines) from `std::ifstream` and prints: position count, UV count, normal count, raw face corner count, and unique OpenGL vertex count after deduplication. Test with the worked example file above — expected output: 4 positions, 4 UVs, 1 normal, 6 face corners, 4 unique OpenGL vertices.

**Exercise 2 — .obj UV flip test:** Load a mesh with `stb_image`. Render with UVs from the .obj file. If the texture appears vertically flipped, add `stbi_set_flip_vertically_on_load(true)` and reload. Document: in your test setup, was the flip needed? Understand why (OpenGL origin = bottom-left vs image file origin = top-left).

**Exercise 3 — glTF inspection:** Install the "glTF Tools" extension for VS Code. Open a .gltf file exported from Blender. Navigate: meshes → primitives → attributes → POSITION → accessor → bufferView. Confirm the accessor `count` matches the vertex count you'd expect. Click "Preview glTF" to see the 3D model rendered.

**Exercise 4 — Format comparison:** Export the same mesh from Blender as both .obj and .glb. Compare file sizes. Open the .obj in a text editor — estimate how many lines. Open the .glb in a hex editor — identify the JSON header and binary chunk boundary (look for the magic number `0x46546C67` = "glTF" in ASCII at offset 0). Document the size difference and explain why .glb is smaller.
