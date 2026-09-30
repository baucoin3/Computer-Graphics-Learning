# L8D — UV Seams
**Theme A1: UV Mapping + Texture Fundamentals | Stage 2 of 5**

---

## The Problem Without This Concept

In L8A we established that a closed 3D surface cannot be mapped to a 2D texture without cuts — the same reason you cannot peel an orange and lay the peel flat without tearing. Those cuts are **UV seams**. Without understanding them, you will encounter unexplained visual cracks in meshes, be confused by why a cube needs 24 vertices instead of 8, and write VBOs with wrong vertex counts for textured geometry.

---

## Part A — Theory

### Why Cuts Are Necessary

Consider a sphere. Its surface is closed and continuous — no edges or holes. Yet a sphere has positive Gaussian curvature everywhere. To lay it flat, you must tear it. A globe map does exactly this: Antarctica is torn and stretched; Greenland is massively distorted near the poles.

For a polygon mesh, the "tear" happens along specific **seam edges**. A seam edge is an edge in the 3D mesh where the UV mapping is discontinuous — the triangles on each side of the edge use different UV coordinates for the same shared 3D vertex position.

In other words: a seam edge is a place where the 3D mesh is continuous (vertices are shared, the surface is smooth or at least connected) but the UV unwrapping treats each side as belonging to separate "patches" in UV space.

### What a Seam Costs: Vertex Duplication

This is the most concrete consequence of seams, and it directly affects your VBO design.

In a VBO with an EBO (indexed rendering), each vertex is uniquely identified by its index. All attributes — position, normal, UV — are tied to that index. There is no way to say "this index uses position A but two different UVs depending on which triangle is referencing it."

At a seam edge, the 3D position on both sides is the same, but the UV coordinates differ. Therefore, you need **two separate vertex entries** in the VBO — same position, same normal, but different UV.

This is not a flaw. It is simply what the data model requires. The vertex is "duplicated" along the seam.

### Counting Vertices: Cube Example

A cube has 8 corners in 3D. Let's count what we actually need for a textured cube.

A cube has 6 faces. Each face is a quad (2 triangles) and needs 4 unique UV coordinates for its corners. The face "owns" its UV region independently of neighboring faces.

Each corner of the cube is shared by 3 faces. Each face assigns a different UV to that corner. The corner vertex position appears 3 times in the VBO — once per face.

Total: 6 faces × 4 vertices per face = **24 vertices** in the VBO.

Without UV (position only, or using the same UV for all faces somehow): only 8 vertices needed.

The seams cost you 24 - 8 = 16 extra vertex entries. For a simple cube this is fine. For complex meshes with many seams, the overhead is typically 5–15% more vertices than the non-UV case.

Compare:
- Non-textured cube: 8 vertices, 36 indices (6 faces × 2 triangles × 3 verts)
- Textured cube: 24 vertices, 36 indices (same indices but referencing different vertex slots)

### UV Islands

When you cut the mesh along seam edges and unfold it, the mesh surface breaks into connected flat patches in UV space. Each patch is called a **UV island**. Islands are separated in UV space by blank (unused) regions.

Rules for UV islands:
1. No island may overlap another island. If two islands overlap in UV space, both surfaces sample from the same texture region, causing one surface to display the wrong texture.
2. Every island must fit within the [0,1]² UV square (for standard single-texture mapping).
3. Islands should be oriented and positioned to minimize wasted UV space and maximize texel coverage for the mesh's visible surfaces.

### Seam Placement Strategy

The seam location matters visually and technically:

**Visually**: seams should be along hard geometric edges (a cube's corners) or in regions not easily visible (the back of a head, inside a character's armpit). A seam on a smooth curved surface creates a visible discontinuity in the texture's pattern.

**Technically for filtering**: at a seam edge, the texture filtering system (bilinear, mipmap) samples from within each UV island independently. At the physical seam boundary in UV space, the filter cannot "reach across" to the neighboring island, even though the surface is continuous in 3D. This can cause a thin visible line at the seam under certain conditions — especially with lower mip levels, which blur across larger texel regions and increase the probability of sampling near the island border.

**Intentional overlapping UVs**: for symmetric meshes (left shoe = right shoe), artists sometimes deliberately overlap UV islands — both halves of the mesh share the same UV region. This effectively halves the texture memory needed (one texture serves both sides). This is valid only when the symmetry is exact and no direction-dependent data (lightmaps, baked shadows, decals) lives on that UV channel.

### Hard Edges and Normal Seams

Seams interact with normals too. On a hard-edge cube, each face has a distinct face normal. At a corner where three faces meet, the three face normals differ. Since each face's corner vertex is a separate VBO entry anyway (for UV), it is natural to also assign per-face normals at these corners.

On a smooth-shaded curved surface, vertices along a seam may need their normals duplicated too, even if the normals are shared. Whenever a vertex needs any different attribute value on two sides of an edge, the vertex must be duplicated. This is true for UV, for normals (smooth vs hard edge), and for any other attribute.

---

## Part B — Numeric Worked Example

### UV Unwrap of a Cube: Cross Layout

Standard cross layout for a cube places the 6 faces in a plus-sign pattern:

```
     [Top]
[Left][Front][Right][Back]
     [Bottom]
```

Each face occupies a 1/4 × 1/4 region of the UV square (if the layout fits in a 4×3 grid of face-sized regions). Let's work with a simpler normalized layout where each face is exactly (0.25 × 0.25) in UV space.

Face UV regions (u_min, u_max, v_min, v_max):
- Top:    [0.25, 0.50] × [0.75, 1.00]
- Left:   [0.00, 0.25] × [0.50, 0.75]
- Front:  [0.25, 0.50] × [0.50, 0.75]
- Right:  [0.50, 0.75] × [0.50, 0.75]
- Back:   [0.75, 1.00] × [0.50, 0.75]
- Bottom: [0.25, 0.50] × [0.25, 0.50]

Now consider the cube edge shared between the **Top face** and the **Front face** — call it edge AB where A = top-front-left corner and B = top-front-right corner.

In 3D:
- A: position (-1, 1, 1)
- B: position ( 1, 1, 1)

From the Top face's perspective, the edge AB is the bottom edge of the Top UV island. In the Top face UV region [0.25, 0.50] × [0.75, 1.00]:
- A gets UV: (0.25, 0.75)
- B gets UV: (0.50, 0.75)

From the Front face's perspective, the edge AB is the top edge of the Front UV island. In the Front face UV region [0.25, 0.50] × [0.50, 0.75]:
- A gets UV: (0.25, 0.75) — same value here by coincidence of this layout
- B gets UV: (0.50, 0.75) — same value here too

In this particular layout, the top-front edge happens to share UV values because the cross layout places those islands adjacent. But this is an artifact of the specific layout. In general, the two sides of a seam have DIFFERENT UV values, which is why vertex duplication is required.

Now let us choose an edge that clearly differs. Edge between **Right face** and **Top face** — call it edge CD where:
- C: position (1, 1, 1)   (top-front-right corner)
- D: position (1, 1, -1)  (top-back-right corner)

From the Right face UV region [0.50, 0.75] × [0.50, 0.75], edge CD is the top edge:
- C gets UV: (0.50, 0.75)
- D gets UV: (0.75, 0.75)

From the Top face UV region [0.25, 0.50] × [0.75, 1.00], edge CD is the right edge:
- C gets UV: (0.50, 0.75)
- D gets UV: (0.50, 1.00)

C is the same (0.50, 0.75) in both. D is (0.75, 0.75) from Right's view and (0.50, 1.00) from Top's view. These are DIFFERENT.

Therefore vertex D (position (1, 1, -1)) must appear TWICE in the VBO:

```
VBO entry for D from Right face's perspective:
  position: (1.0, 1.0, -1.0)
  UV:       (0.75, 0.75)

VBO entry for D from Top face's perspective:
  position: (1.0, 1.0, -1.0)  ← identical position
  UV:       (0.50, 1.00)      ← different UV
```

Both entries have the same position but different UVs. The EBO uses different index values for each. Right face triangles reference the first entry; Top face triangles reference the second.

---

## Pitfalls

**Pitfall 1: Forgetting vertex duplication at seams when hand-building a cube**

Mistake: building a textured cube with only 8 vertices, assuming you can index them with different UVs per face.

Symptom: some faces display the wrong texture region. The texture appears "mixed up" — a face shows the UV coordinates intended for a different face. Or worse, the UV coordinates are averaged between the two faces' intended values by the rasterizer (if vertices are shared, barycentric interpolation blends the UVs from the two connected triangles' vertices).

Fix: use 24 vertices for a textured cube (4 per face). Each face is completely self-contained in the VBO, with its own UV coordinates for each corner.

**Pitfall 2: Overlapping UV islands without intent**

Mistake: during UV unwrapping, two islands accidentally overlap because the auto-unwrap algorithm packed them on top of each other.

Symptom: two different surfaces on the mesh display identical texture content — they both show the same region of the image. Easy to miss on symmetric geometry (the left and right sides of a symmetric object may look correct precisely because they show the same texture).

Fix: enable "highlight overlaps" in the UV editor (available in Blender, Maya, 3ds Max). Any overlapping UV shells are highlighted in red or orange. Separate the overlapping islands into non-overlapping UV space.

**Pitfall 3: Seam on a smooth-shaded curved surface**

Mistake: placing a UV seam along a smoothly curved surface (the center of a character's forehead or the top of a cylinder's side).

Symptom: a visible thin line or color discontinuity at the seam, especially visible when mip levels kick in at distance. The adjacent UV islands have a gap in UV space even though the 3D surface is continuous. At low mip levels, the texel blending reaches the island border and cannot cross it, creating a sharp edge in the blurred texture.

Fix: place seams on hard geometric edges or in hidden regions. For cylinders, the seam traditionally runs along the back of the cylinder where it is least visible. For characters, seams go behind the ears, in the armpit, along the inside of the leg.

---

## What to Build

**Exercise 1: UV island layout for a cylinder**

A cylinder has 3 parts: the curved side surface and two flat end caps (top and bottom circles).

1. Sketch (describe in prose) the UV island shape for the curved side: what shape does it become when unrolled? What are its UV dimensions relative to the full [0,1]² space?
2. What shape are the two end cap islands? How many seam edges are needed for each cap?
3. Where should the single seam on the curved side be placed for minimum visual impact?

**Exercise 2: Vertex count for a UV-mapped sphere**

A sphere mesh built with 16 longitude divisions and 8 latitude divisions has:
- 16 × 8 = 128 quad faces → 256 triangles
- In 3D, it has (16 × (8-1)) + 2 pole vertices = 112 + 2 = 114 unique 3D positions

For UV mapping:
- The seam runs along one longitude line (the "back seam")
- All vertices along the seam need to be duplicated (one copy for UV u=0, one for UV u=1)
- The pole vertices need one copy per longitude segment (the pole UV fans)

Estimate the total vertex count needed in the UV-mapped VBO. Explain which vertices are duplicated and why. Show your counting arithmetic.
