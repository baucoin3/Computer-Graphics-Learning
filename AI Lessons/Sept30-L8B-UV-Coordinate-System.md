# L8B — The UV Coordinate System
**Theme A1: UV Mapping + Texture Fundamentals | Stage 1 of 5**

---

## The Problem Without This Concept

You know that UVs are 2D coordinates that map a surface to a texture. But the coordinate system itself has rules: which direction is "up"? What does (0,0) mean? What happens when a UV is 1.5 or -0.3? These rules differ between OpenGL and DirectX, between image files and GPU conventions, and the mismatches cause bugs that are genuinely confusing if you don't know the system.

This lesson pins down exactly how the UV coordinate system works, how UV values translate to specific texel positions, and what happens outside the [0,1] range.

---

## Part A — Theory

### The UV Space Defined

UV is a 2D coordinate system:

- **U** is the horizontal axis (also called S in legacy OpenGL documentation — `glTexCoord2f(s, t)`)
- **V** is the vertical axis (also called T)
- Both range from **0.0 to 1.0** to address the full texture, regardless of the texture's pixel dimensions

The key design decision: the range [0,1]² is **normalized** and **resolution-independent**. If you have a 512×512 texture and you sample at UV (0.5, 0.5), you get the texel at the center. If you later replace that texture with a 1024×1024 version, the same UV (0.5, 0.5) still gets the center texel. Your UV data does not change when texture resolution changes.

This is correct behavior. If UVs were in pixel coordinates, every UV in every mesh would need to be recalculated whenever texture resolution changed. That would be unworkable.

### OpenGL Origin: Bottom-Left

In OpenGL:
- **(0, 0) is the bottom-left corner** of the texture
- **(1, 1) is the top-right corner**
- U increases to the right
- V increases **upward**

This matches the standard mathematical convention where the Y axis points up.

```
(0,1) ────── (1,1)
  │               │
  │   texture     │
  │               │
(0,0) ────── (1,0)
```

So UV (0.5, 0.75) is a point that is 50% from the left edge and 75% from the **bottom** edge. It is in the upper half of the texture.

This convention is important to internalize because most image files use the **opposite** convention — they store pixels from top to bottom, so row 0 is the visual top. When you load an image from disk and upload it to OpenGL without correction, the texture appears vertically flipped. This is covered in detail in L8F.

### How UV Maps to a Texel Position

A texture has width W pixels and height H pixels. Given UV (u, v):

```
texel_x = u × W          (column index, left to right)
texel_y = v × H          (row index, bottom to top in OpenGL convention)
```

These are floating-point numbers. The GPU uses them to look up the texture, applying the current filter mode to handle the fractional part. Nearest-neighbor rounds to the nearest integer; bilinear interpolates between surrounding texels (L8H covers this).

**Example**: texture is 256×256. UV = (0.3, 0.7).

```
texel_x = 0.3 × 256 = 76.8  → lies between column 76 and column 77
texel_y = 0.7 × 256 = 179.2 → lies between row 179 and row 180
```

With nearest-neighbor: column 77, row 179. With bilinear: interpolate between the 4 surrounding texels at columns {76, 77} × rows {179, 180}.

### Wrap Modes: What Happens Outside [0,1]

UV coordinates outside [0,1] are legal. The GPU applies a **wrap mode** to decide what to do.

**GL_REPEAT** (default): tile the texture infinitely. The effective UV is `fract(uv)` — just the fractional part.

```
u = 2.3 → fract(2.3) = 0.3 → samples at column 0.3 × W
u = -0.4 → fract(-0.4) = 0.6 → samples at column 0.6 × W
```

This lets you tile a brick texture over a large surface by assigning UV values like (0,0) to (4,4) to a quad — the texture repeats 4 times in each direction.

**GL_CLAMP_TO_EDGE**: UV is clamped to [0,1] before lookup.

```
u = 1.5 → clamped to 1.0 → samples the rightmost column
u = -0.2 → clamped to 0.0 → samples the leftmost column
```

Use this when you do not want tiling, and the edge color should extend outward.

**GL_MIRRORED_REPEAT**: tiles but mirrors alternating repetitions.

```
u in [0,1] → normal
u in [1,2] → mirrored (reads from 2-u)
u in [2,3] → normal again
```

This creates seamless tiling for textures whose edges do not match, by flipping adjacent tiles.

Set wrap modes separately for U and V:

```c
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);    // U axis
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); // V axis
```

### UV Tiling by Scaling

To tile a texture N times across a surface, multiply the U or V coordinate by N before sampling. This is done in the fragment shader:

```glsl
vec2 tiledUV = vTexCoord * vec2(4.0, 2.0); // tile 4x horizontal, 2x vertical
vec4 color = texture(uAlbedo, tiledUV);
```

Or in the vertex shader before passing to the fragment shader:

```glsl
vTexCoord = aTexCoord * uTilingFactor;
```

The wrap mode must be `GL_REPEAT` for tiling to work correctly.

### Texel Aspect Ratio vs UV Aspect Ratio

The UV square [0,1]² always maps to the full texture, regardless of the texture's pixel dimensions. A 512×256 texture (2:1 aspect ratio) maps UV (0,0)→(1,1) to its full area. If you apply this texture to a square quad with UVs (0,0)→(1,1), the image is squashed horizontally to fit a square quad from a 2:1 image — it will appear half as wide as it should.

To avoid squashing: either use UVs that match the image's aspect ratio, or choose a texture with the correct aspect ratio for your surface.

Example: surface is 2m wide, 1m tall. Texture is 512×256 (2:1). UVs should be (0,0)→(1,1) — the texture aspect ratio matches the surface aspect ratio. No squashing.

If the surface was 1m×1m (square) but the texture was 512×256, you'd need to choose: either squash the texture or use UVs (0,0)→(0.5,1) to use only the left half of the texture.

---

## Part B — Numeric Worked Example

### Texture and Triangle Setup

Texture: 8×8 texels (grayscale, for simplicity).

A triangle with three vertices and their UV coordinates:
- Vertex A: UV = (0.0, 0.0) → bottom-left corner
- Vertex B: UV = (1.0, 0.0) → bottom-right corner
- Vertex C: UV = (0.5, 1.0) → top-center

**Step 1: What texel does each vertex map to?**

Texture is 8×8, so W = 8, H = 8.

Vertex A: texel_x = 0.0 × 8 = 0.0, texel_y = 0.0 × 8 = 0.0 → texel (0, 0) = bottom-left corner texel.

Vertex B: texel_x = 1.0 × 8 = 8.0, texel_y = 0.0 × 8 = 0.0

Note: texel_x = 8.0 is outside the valid range [0, 7]. OpenGL handles this: with GL_REPEAT, `fract(1.0) = 0.0` so it maps to texel 0. With GL_CLAMP_TO_EDGE, it clamps to the rightmost texel (column 7). This is a subtlety at exactly UV = 1.0 — in practice this is handled correctly by hardware but it explains why you should use UV = (1.0 - 0.5/W) as the maximum safe value on a texture to avoid this boundary.

Vertex C: texel_x = 0.5 × 8 = 4.0, texel_y = 1.0 × 8 = 8.0 → same boundary issue on V.

**Step 2: A fragment is rasterized at interpolated UV = (0.375, 0.5). What texel?**

texel_x = 0.375 × 8 = 3.0 exactly → lies at the left edge of column 3.
texel_y = 0.5 × 8 = 4.0 exactly → lies at the bottom edge of row 4.

With nearest-neighbor: column 3, row 4.
With bilinear: (3.0, 4.0) is at an exact integer boundary. The four surrounding texels are at (2,3), (3,3), (2,4), (3,4). The fractional parts are both 0.0 — so the bilinear weights are: full weight on texel (2, 3) (the one at floor(3.0), floor(4.0)). Wait — let me be precise.

bilinear uses floor: i = floor(3.0) = 3, j = floor(4.0) = 4, fx = 3.0 - 3 = 0.0, fy = 4.0 - 4 = 0.0.

The four texels are T[j][i]=T[4][3], T[j][i+1]=T[4][4], T[j+1][i]=T[5][3], T[j+1][i+1]=T[5][4].

Weights: (1-fx)(1-fy) = 1.0 for T[4][3], others are 0. Result = T[4][3] exactly. An exactly integer texel coordinate gives 100% weight to one texel — no interpolation needed.

**Step 3: UV = (0.625, 0.375). What texel?**

texel_x = 0.625 × 8 = 5.0 → exact integer, column 5.
texel_y = 0.375 × 8 = 3.0 → exact integer, row 3.
Result: T[3][5]. Single texel, no blending.

**Step 4: UV = (0.4, 0.6). What texel (bilinear)?**

texel_x = 0.4 × 8 = 3.2, texel_y = 0.6 × 8 = 4.8.
i = floor(3.2) = 3, j = floor(4.8) = 4.
fx = 3.2 - 3 = 0.2, fy = 4.8 - 4 = 0.8.

Four texels: T[4][3], T[4][4], T[5][3], T[5][4].
Weights:
- T[4][3]: (1-0.2)×(1-0.8) = 0.8 × 0.2 = 0.16
- T[4][4]: 0.2 × (1-0.8) = 0.2 × 0.2 = 0.04
- T[5][3]: (1-0.2) × 0.8 = 0.8 × 0.8 = 0.64
- T[5][4]: 0.2 × 0.8 = 0.2 × 0.8 = 0.16

Sum: 0.16 + 0.04 + 0.64 + 0.16 = 1.00. Correct.

This fragment is mostly influenced by T[5][3] (64% weight) because the UV is close to the top-left of this texel quad.

---

## Pitfalls

**Pitfall 1: Assuming (0,0) is the top-left corner**

Mistake: treating UV (0, 0) as the visual top-left of the texture (the DirectX / image-file convention).

Symptom: texture appears vertically flipped. A texture that should show a face shows it upside down. The top of the face appears at the bottom of the rendered surface.

Fix: either flip V at load time with `stbi_set_flip_vertically_on_load(1)` (covered in L8F), or remember that in OpenGL, V = 0 is the BOTTOM of the texture. When hand-assigning UVs to a quad that fills the full texture, the bottom-left vertex should have UV (0,0), the top-left vertex should have UV (0,1).

**Pitfall 2: UV values exactly at 1.0 with GL_CLAMP_TO_EDGE**

Mistake: assigning UV = 1.0 to the corner vertex of a surface, expecting it to sample the last texel cleanly.

Symptom: at the extreme edge of the surface, bilinear filtering samples just outside the texture boundary and clamps, which can produce a slight color fringe from the edge texel being over-sampled.

Fix: for atlas textures where edge bleeding is a concern, inset UVs by `0.5 / texture_width` to ensure sample centers land inside valid texel regions. For single-texture surfaces with GL_CLAMP_TO_EDGE, UV = 1.0 is fine in practice.

**Pitfall 3: Confusing UV scaling with geometry scaling**

Mistake: scaling the 3D object and expecting the texture to scale with it.

Symptom: a crate that was 1m³ with correct UV tiling is scaled to 2m³, but the UV data is still the same. The texture on the larger crate covers twice the world-space area per texel cycle — the bricks look twice as large.

Fix: UV coordinates do not automatically adjust when geometry is scaled. To maintain consistent texel density, either scale the UV by the same factor (multiply the UV attribute or the in-shader UV), or rescale the geometry in modeling space and re-export the mesh.

---

## What to Build

**Exercise 1: UV assignment for aspect-correct tiling**

A quad has world-space vertices at (0,0,0), (3,0,0), (3,1,0), (0,1,0). It is 3 meters wide and 1 meter tall. You want to tile a brick texture (which is 512×256, a 2:1 aspect ratio image) so that individual bricks appear square on the wall. Bricks in the image are approximately 1/8 of the image height each, making them roughly 64 pixels wide and 32 pixels tall in the image. In world space you want bricks to be 0.25m wide and 0.125m tall.

1. How many times should the texture tile horizontally?
2. How many times vertically?
3. Write the 4 UV coordinates for the quad vertices.

Show all arithmetic.

**Exercise 2: UV mirror shader**

Write a GLSL fragment shader snippet that mirrors the texture horizontally on the right half of the surface. Specifically: if U > 0.5, sample from U = 1.0 - U instead. If U ≤ 0.5, sample normally. Write the complete `void main()` body including the conditional UV modification and the texture sample.

**Exercise 3: Texel from UV**

Texture is 64×64. Compute the texel coordinates (as floats, then floored to integers) for the following UV values:
1. UV = (0.0,  0.0)
2. UV = (1.0,  1.0)
3. UV = (0.5,  0.5)
4. UV = (0.125, 0.75)
5. UV = (0.333, 0.666)

Show `texel_x = u × 64` and `texel_y = v × 64` for each. Which of these lands exactly on an integer texel center?
