# L8E — UV Atlases
**Theme A1: UV Mapping + Texture Fundamentals | Stage 2 of 5**

---

## The Problem Without This Concept

A game scene has 200 objects — crates, rocks, walls, barrels. If each object has its own texture, the GPU must rebind a different texture before each draw call. A texture rebind is a GPU state change that forces the pipeline to flush. At 200 draw calls per frame × 60fps = 12,000 texture binds per second, state-change overhead becomes a real performance problem. The solution is to pack multiple surfaces into a single large texture — a **UV atlas** — so many draw calls share one texture bind.

Beyond performance: lightmaps, sprite animations, and streaming systems all depend on the atlas concept. Understanding atlases is not optional — they appear everywhere in production.

---

## Part A — Theory

### What a UV Atlas Is

A UV atlas (also called a texture atlas or sprite sheet) is a single texture that contains multiple distinct regions, each used by a different surface or mesh. Instead of UV coordinates spanning [0,1]² for the full texture, each mesh's UVs address a **sub-region** of the atlas.

For example:
- Crate mesh: UVs in [0.0, 0.5] × [0.0, 0.5] — bottom-left quarter
- Barrel mesh: UVs in [0.5, 1.0] × [0.0, 0.5] — bottom-right quarter
- Rock mesh: UVs in [0.0, 0.5] × [0.5, 1.0] — top-left quarter
- Wall panel: UVs in [0.5, 1.0] × [0.5, 1.0] — top-right quarter

All four objects share one texture bind. The GPU samples from the correct region for each based on the UV coordinates baked into the mesh.

### Texel Density

**Texel density** is the number of texture texels per unit of world-space surface area. It determines perceived texture sharpness at a given viewing distance.

For a surface of world-space width W meters mapped to UV region of width `(u1 - u0)` in a texture of width T pixels:

```
texel density (horizontal) = (u1 - u0) × T / W    texels per meter
```

Consistent texel density means all visible surfaces have the same texels-per-meter at the same viewing distance. If one object has 512 texels/m and an adjacent object has 64 texels/m, they look obviously mismatched — one sharp, one blurry — at the same camera distance.

**Industry standard texel densities:**
- Hero objects (player character, key props): 512–1024 texels/m
- Background environment: 128–256 texels/m
- Far distance terrain: 32–64 texels/m

These numbers assume a 2048 or 4096 pixel texture. The absolute texel count matters less than the consistency across similarly-distanced objects.

### Sprite Sheets

A sprite sheet is a type of atlas used for 2D sprite animation. Each animation frame is a sub-region of the texture. At runtime, you update the UV offset each frame to show the next frame.

For a 4×4 sprite sheet (16 frames, each occupying 1/4 of the texture width and 1/4 of the height):

```
frame_col = frame_index % 4          // 0–3
frame_row = frame_index / 4          // 0–3

uv_min = vec2(frame_col / 4.0, frame_row / 4.0)
uv_max = uv_min + vec2(0.25, 0.25)

// In vertex shader, transform input UV [0,1] to frame region:
vec2 animUV = aTexCoord * (1.0/4.0) + uv_min;
```

Or equivalently as a uniform:

```glsl
// Set per-frame from CPU:
uniform vec2 uFrameOffset;  // (frame_col/4, frame_row/4)
// In vertex shader:
vTexCoord = aTexCoord * 0.25 + uFrameOffset;
```

### Lightmap Atlases

Unreal Engine 5's Lightmass system bakes indirect lighting (ambient occlusion, soft shadows, color bleeding) into textures. This requires every static surface in the scene to have a UV layout in a second UV channel (UV Channel 1 in UE5) where:

1. **No UV islands overlap** — each surface must have a unique UV region, because the baked light value at each texel is specific to that surface's position and orientation in the scene.
2. **All UVs are in [0,1]²** — unlike the material UV channel (Channel 0), which can tile and repeat, lightmap UVs never tile.
3. **Texel density is budget-constrained** — the global lightmap resolution determines quality vs memory/bake-time tradeoff.

This is different from the material UV (Channel 0) which CAN overlap (two symmetrical surfaces can share the same texture region). The lightmap UV cannot overlap because each position in the scene gets unique lighting.

UE5 calls `GenerateLightmapUVs` automatically during mesh import if Channel 1 is absent. You can see both UV channels in the Static Mesh Editor.

### Mipmap Bleed: The Main Atlas Problem

When a GPU generates mipmaps for a full atlas texture, each mip level halves the entire texture. At mip level k, the atlas has been downsampled 2^k times. At high mip levels, each texel in the downsampled atlas represents a large area of the original image — potentially spanning across the border between two atlas regions.

When the GPU samples near the border of an atlas region at a high mip level, it samples from texels that now contain blended color from the ADJACENT atlas region. The crate color bleeds into the barrel region. This creates visible colored fringes or incorrect colors at the edges of surfaces, especially at distance.

**Fix**: pad each atlas region with a **border** of additional texels that duplicate the outermost edge color of the region. The border must be at least `2^(max_mip_level_used)` texels wide to prevent bleed.

For a 1024×1024 atlas with 10 mip levels, the border should be 2^10 = 1024 texels — which is the entire atlas. This is clearly infeasible for a deep mipmap chain. In practice:
- Limit the mip chain for atlases (set GL_TEXTURE_MAX_LEVEL)
- Use a 2–4 texel border and accept some bleed at extreme distances
- Use GL_TEXTURE_2D_ARRAY instead of packing into one atlas

**UV inset**: for each atlas region, inset the UV coordinates by 0.5 texel to ensure sample centers land inside the valid region rather than exactly on the border. For a region in a 512-wide atlas where the border starts at column 128:

```
safe_u = 128.5 / 512 = 0.2510 instead of 128/512 = 0.2500
```

This prevents bilinear filtering from sampling just outside the region boundary.

### GL_TEXTURE_2D_ARRAY: The Modern Alternative

Instead of packing surfaces into one texture, modern OpenGL offers **texture arrays** — a stack of same-dimension 2D textures, addressed by a third coordinate (the layer index).

```c
glGenTextures(1, &texArray);
glBindTexture(GL_TEXTURE_2D_ARRAY, texArray);
glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, 256, 256, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
// Upload each layer:
for (int i = 0; i < 16; i++) {
    glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, i, 256, 256, 1, GL_RGBA, GL_UNSIGNED_BYTE, imageData[i]);
}
```

In GLSL:

```glsl
uniform sampler2DArray uTexArray;
uniform float uLayer;  // which layer to sample

vec4 color = texture(uTexArray, vec3(vTexCoord, uLayer));
```

Advantages over atlas packing:
- No mipmap bleed between layers — each layer mipmaps independently
- No wasted UV space from imperfect packing
- Simpler UV coordinates (each mesh uses full [0,1]² UVs)
- No need for texel-density normalization across differently-sized surfaces

Use atlases when: surfaces have truly variable sizes that benefit from packing optimization, or when supporting older hardware/API versions. Use texture arrays when: all surfaces can be normalized to the same resolution, or targeting modern APIs (Vulkan's `VK_IMAGE_VIEW_TYPE_2D_ARRAY`).

---

## Part B — Numeric Worked Example

### Atlas: Texel Density Analysis

Atlas texture: 1024×1024. Three surfaces:

**Surface 1 — Wall panel**:
- UV region: [0.0, 0.5] × [0.0, 1.0] (left half, full height)
- Atlas texels: 512 wide × 1024 tall = 524,288 texels
- World size: 2m wide × 4m tall

Texel density:
- Horizontal: (0.5 × 1024) / 2 = 512 / 2 = **256 texels/m**
- Vertical: (1.0 × 1024) / 4 = 1024 / 4 = **256 texels/m**

Square. Consistent aspect ratio. Good.

**Surface 2 — Crate lid**:
- UV region: [0.5, 0.75] × [0.5, 1.0] (top-right quadrant)
- Atlas texels: 256 wide × 512 tall = 131,072 texels
- World size: 0.5m × 0.5m

Texel density:
- Horizontal: (0.25 × 1024) / 0.5 = 256 / 0.5 = **512 texels/m**
- Vertical: (0.5 × 1024) / 0.5 = 512 / 0.5 = **1024 texels/m**

Not square — the crate lid's UV region has a 1:2 aspect ratio (256×512) but the surface is square (0.5×0.5m). The texture is stretched vertically 2× on the crate lid. Either the UV region should be square (256×256) or the world-space geometry should be 0.5×1m.

**Surface 3 — Floor tile**:
- UV region: [0.5, 1.0] × [0.0, 0.5] (bottom-right quarter)
- Atlas texels: 512 wide × 512 tall = 262,144 texels
- World size: 1m × 1m

Texel density:
- Horizontal: (0.5 × 1024) / 1 = 512 / 1 = **512 texels/m**
- Vertical: (0.5 × 1024) / 1 = 512 / 1 = **512 texels/m**

Square. Consistent. 512 texels/m vs wall panel's 256 texels/m — the floor tile has 2× higher texel density than the wall. From the same viewing distance, the floor will look noticeably sharper than the wall. For a floor that is typically viewed at grazing angles (which reduces apparent sharpness anyway), this may be intentional.

**Density comparison**:
- Wall: 256 texels/m
- Crate lid: 512 H / 1024 V texels/m (inconsistent within itself, and 2× wall horizontally)
- Floor: 512 texels/m

The crate lid has a bug (non-square UV for a square surface). The floor is denser than the wall. In a real project this would be flagged in texel density review.

### Sprite Sheet Frame Address Calculation

Sprite sheet: 4 columns × 4 rows = 16 frames. Each frame is 64×64 pixels in a 256×256 texture.

Frame 11 (0-indexed). Compute UV:

```
frame_col = 11 % 4 = 3     (11 = 2×4 + 3, remainder 3)
frame_row = 11 / 4 = 2     (integer division)

uv_min.x = 3 / 4.0 = 0.75
uv_min.y = 2 / 4.0 = 0.50

uv_max.x = uv_min.x + 0.25 = 1.00
uv_max.y = uv_min.y + 0.25 = 0.75
```

Frame 11 occupies UV region [0.75, 1.00] × [0.50, 0.75] — the rightmost column, third row from bottom.

For a sprite vertex at input UV (0.5, 1.0) (top-center of the sprite):

```
final_uv.x = 0.5 × 0.25 + 0.75 = 0.125 + 0.75 = 0.875
final_uv.y = 1.0 × 0.25 + 0.50 = 0.250 + 0.50 = 0.750
```

This samples the top-center texel of frame 11 in the atlas.

---

## Pitfalls

**Pitfall 1: Atlas border bleed from mipmapping**

Mistake: packing surfaces tightly in an atlas (no padding between regions) and enabling full mipmaps.

Symptom: colored fringes around surfaces at distance. A crate surrounded by a floor texture shows floor-colored edges when viewed far away. The artifact intensifies at lower mip levels (greater distances).

Fix: add at least 2–4 texel padding between regions. Set `GL_TEXTURE_MAX_LEVEL` to limit how deep the mipmap chain goes for the atlas texture. Alternatively, inset UVs by half a texel per side.

**Pitfall 2: Exact UV boundary hit under bilinear filtering**

Mistake: UV coordinate exactly at the boundary between two atlas regions (e.g., u = 0.5 exactly).

Symptom: bilinear filtering at that boundary samples from both regions — 50% from each. The boundary texels on the left region plus the boundary texels on the right region average together, producing an incorrect blended color along the seam line.

Fix: inset UVs by 0.5 / atlas_width from the region boundary. For a 1024-wide atlas: inset by 0.5/1024 = 0.000488. Round to 0.001 for safety. The exact boundary UV should never be reached by any surface vertex.

**Pitfall 3: Inconsistent texel density across atlas regions**

Mistake: allocating UV space based on visual importance without computing actual texel density for each surface.

Symptom: surfaces at the same viewing distance appear to have mismatched sharpness. The wall looks noticeably blurrier or sharper than adjacent objects.

Fix: compute texel density for every atlas region using the formula above. Standardize to a target (e.g., 256 texels/m for mid-range props) and allocate UV space accordingly. Tools like Substance Painter have a texel density checker built in.

---

## What to Build

**Exercise 1: Atlas layout design**

Design a 512×512 atlas for a simple scene with these surfaces:
- 4 wall panel variants, each 1m × 2m in world space
- 1 floor tile, 1m × 1m
- 1 ceiling panel, 2m × 2m

All should have 256 texels/m. Describe in prose:
1. How many texels does each surface's UV region need?
2. Lay out the regions in the 512×512 atlas (describe x_min, y_min, x_max, y_max for each)
3. Does everything fit? If not, what is the minimum atlas size that works?

Show all arithmetic.

**Exercise 2: Sprite sheet vertex shader**

Write the complete GLSL vertex shader for a sprite that:
- Receives a mesh UV in [0,1]² (aTexCoord)
- Receives a uniform `int uFrame` (0–15, for a 4×4 sprite sheet)
- Computes the correct atlas sub-region UV and passes it to the fragment shader

The output `vTexCoord` should address the correct frame in the 4×4 sprite sheet.

**Exercise 3: Texture array upload**

Write the C++ code to upload 4 textures into a `GL_TEXTURE_2D_ARRAY` where each texture is 256×256 RGBA. Assume you have `unsigned char* images[4]`, each containing the raw pixel data. Include:
- glGenTextures, glBindTexture
- glTexImage3D for allocation
- glTexSubImage3D loop for each layer
- glTexParameteri calls for filtering
- glGenerateMipmap
