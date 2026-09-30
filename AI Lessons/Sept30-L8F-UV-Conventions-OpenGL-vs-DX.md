# L8F — UV Conventions: OpenGL vs DirectX, Y-Flip, and Normal Map Green Channel
**Theme A1: UV Mapping + Texture Fundamentals | Stage 2 of 5**

---

## The Problem Without This Concept

You load a PNG, upload it to OpenGL, map it to a quad — the texture appears upside down. This is not a bug you wrote. It is a fundamental coordinate convention mismatch that every graphics programmer hits in their first week with textures.

There is a second, subtler version of this problem: you download a normal map from a texture website, use it in your OpenGL shader, and the bumps appear as dents instead of bumps. Also a convention mismatch — different axis, different symptom, same root cause.

This lesson explains both mismatches completely, gives the correct fixes, and explains why the conventions differ in the first place.

---

## Part A — Theory

### Image File Convention: Y Axis Points Down

All major raster image formats (PNG, JPEG, BMP, TGA, DDS) store pixel rows from **top to bottom**. Row 0 in the file = the visual top of the image. Row (H-1) = the visual bottom. The Y axis points **downward**.

This matches screen conventions in web browsers, operating system UIs, and most image display software (Photoshop, GIMP, Preview). It is also the convention for screen-space rendering APIs.

```
Row 0:    [top-left pixel] [top-right pixel]   ← visual top
Row 1:    ...
...
Row H-1:  [bottom-left]   [bottom-right]        ← visual bottom
```

### OpenGL Texture Convention: Y Axis Points Up

OpenGL defines texture coordinates with (0,0) at the **bottom-left** and (1,1) at the **top-right**. The Y (V) axis points **upward**. This matches the standard mathematical convention for the first quadrant.

When OpenGL uploads a texture, it reads the pixel data starting from row 0 and treats that as the bottom of the texture — what gets placed at V = 0.

If you upload image file data directly to `glTexImage2D` without correction:
- Row 0 of the file (visual top) becomes the OpenGL row 0 (visual bottom of the texture)
- Row H-1 of the file (visual bottom) becomes OpenGL's top row

The texture is flipped vertically.

### The Mismatch: A Concrete Example

Image: a 4-row test image. Contents by row (visual interpretation):

```
Row 0 (file/visual top):    [RED  ][RED  ][RED  ][RED  ]
Row 1:                       [YELL ][YELL ][YELL ][YELL ]
Row 2:                       [GREEN][GREEN][GREEN][GREEN]
Row 3 (file/visual bottom): [BLUE ][BLUE ][BLUE ][BLUE ]
```

**Without fix**: uploaded as-is to OpenGL.

OpenGL stores it as:
```
OpenGL row 0 (V=0, visual bottom): [RED  ][RED  ]...   ← was the file's row 0 = visual TOP
OpenGL row 1 (V=0.25):             [YELL ][YELL ]...
OpenGL row 2 (V=0.5):              [GREEN][GREEN]...
OpenGL row 3 (V=0.75, visual top): [BLUE ][BLUE ]...   ← was the file's row 3 = visual BOTTOM
```

You sample at UV V = 0.75 (75% from bottom, near the top visually):
- OpenGL maps V=0.75 → row 3 in OpenGL storage → [BLUE]
- But the image has BLUE at the visual bottom, not the visual top
- What you expected at 75% from bottom (near visual top) was RED
- You got BLUE — the image is flipped

**With stbi_set_flip_vertically_on_load(1)**: stb_image reverses the row order in memory:

```
Memory row 0 (= OpenGL row 0 = V=0 = bottom): [BLUE ]   ← was file row 3 (visual bottom)
Memory row 1 (V=0.25):                         [GREEN]   ← was file row 2
Memory row 2 (V=0.5):                          [YELL ]   ← was file row 1
Memory row 3 (V=0.75 = near top):             [RED  ]   ← was file row 0 (visual top)
```

Now UV V = 0.75 → memory row 3 → [RED] = visual top. Correct.

### Fix Option 1: Flip at Load Time (Recommended)

```c
stbi_set_flip_vertically_on_load(1);  // call ONCE before any stbi_load

int w, h, channels;
unsigned char* data = stbi_load("texture.png", &w, &h, &channels, 0);
```

This is the standard fix. It is a single line, it affects all subsequent `stbi_load` calls, and it correctly handles the convention for all subsequent texture use in your OpenGL project.

### Fix Option 2: Flip in the Fragment Shader

```glsl
vec2 uv = vec2(vTexCoord.x, 1.0 - vTexCoord.y);
vec4 color = texture(uAlbedo, uv);
```

This works but has disadvantages:
- Adds computation per fragment (minor cost)
- Can cause double-flip errors if also correcting at load time
- Makes it confusing to debug UV issues (the UV in the shader differs from the UV stored in the VBO)
- Does NOT work correctly for normal maps (see below)

### Fix Option 3: Flip UV in the VBO

Invert V in the vertex data: instead of UV (u, v), store (u, 1-v). This is done at mesh creation/export time, not at runtime. Some exporters do this automatically.

### DirectX Convention: Y Axis Points Down

DirectX uses (0,0) at the **top-left** of the texture, matching image file convention. V increases downward. DX/HLSL texture coordinates and image files share the same orientation — no flip needed in a DirectX pipeline.

This is why assets authored for DirectX games (using tools like DXTex, GPU PerfStudio, PIX) have UVs that appear flipped in OpenGL without correction. And it is why online tutorials that use DirectX conventions sometimes show V = 0 at the top.

OpenGL's convention is mathematically standard (matches coordinate geometry). DirectX's convention matches practical image file storage. Both are valid — they just differ.

### The Normal Map Green Channel Problem

Normal maps encode surface normals as texture colors. The normal vector `(nx, ny, nz)` is remapped from [-1,1] to [0,1] for storage: `color = normal * 0.5 + 0.5`.

In the fragment shader, you reverse this: `normal = texture(uNormal, uv).xyz * 2.0 - 1.0`.

The Y component of the normal corresponds to the "up" direction in **tangent space** (the local coordinate frame on the surface). The green channel stores Y.

**OpenGL normal map convention**: positive Y points "up" in tangent space (away from the surface, along the texture's up direction). A texel with G=1.0 encodes ny = +1.0 = pointing up.

**DirectX normal map convention**: positive Y points "down" in tangent space — this is because DX texture V increases downward, so the tangent space Y axis (derived from UV gradient direction) points downward in the DX convention.

This means a DirectX normal map has the green channel representing the OPPOSITE Y direction compared to an OpenGL normal map.

**Visual result of using the wrong convention**: bumps appear concave instead of convex. A brick wall that should have raised bricks appears to have sunken grooves. The lighting is correct in direction but inverted in the normal's Y component.

Most normal maps available online from DCC assets, game texture repositories (Poly Haven, AmbientCG, Quixel), and AAA game exports use the **DirectX convention** (green channel flipped) because most modern game engines target DX12 or Vulkan with DX-convention normals.

**Unity** uses OpenGL convention by default. **Unreal Engine 5** uses DirectX convention.

### Fixing the Green Channel in the Shader

```glsl
vec3 normalSample = texture(uNormal, vTexCoord).xyz;
normalSample = normalSample * 2.0 - 1.0;  // remap to [-1,1]

// If the map is DirectX convention and your engine is OpenGL convention:
normalSample.y = -normalSample.y;          // flip Y
```

Or conditionally based on a uniform:

```glsl
uniform bool uDXNormalMap;

vec3 n = texture(uNormal, vTexCoord).xyz * 2.0 - 1.0;
if (uDXNormalMap) {
    n.y = -n.y;
}
// n is now in OpenGL tangent-space convention
```

Always renormalize after sampling and flipping, because:
1. Texture compression introduces small numeric errors
2. Bilinear/AF filtering of normals produces non-unit-length vectors
3. Y-flip doesn't change length, but renormalize anyway as good practice

```glsl
n = normalize(n);
```

---

## Part B — Numeric Worked Example

### Tracing the Y-Flip Bug

4×4 texture, loaded without stbi flip. Original file (visual layout):

```
File row 0 (visual top):    R  R  R  R
File row 1:                  G  G  G  G
File row 2:                  B  B  B  B
File row 3 (visual bottom): Y  Y  Y  Y   (Y = yellow)
```

After `glTexImage2D` without flip, OpenGL stores:
```
OpenGL row 0 (V=0.0):    R  R  R  R   ← was file row 0 (visual TOP)
OpenGL row 1 (V=0.25):   G  G  G  G
OpenGL row 2 (V=0.5):    B  B  B  B
OpenGL row 3 (V=0.75):   Y  Y  Y  Y   ← was file row 3 (visual BOTTOM)
```

**Sample at UV (0.5, 0.6):**

Texel_y = 0.6 × 4 = 2.4 → floor = 2 → OpenGL row 2 = BLUE

What the user expected: V=0.6 is 60% from the bottom = 40% from the top. In the visual image, 40% from the top is in the GREEN band (row 1 of the file). User expects GREEN, gets BLUE. Wrong.

**After stbi_set_flip_vertically_on_load(1):**

In-memory layout after flip:
```
Memory row 0 (V=0.0):    Y  Y  Y  Y   ← was file row 3 (visual BOTTOM) → now at OpenGL bottom
Memory row 1 (V=0.25):   B  B  B  B
Memory row 2 (V=0.5):    G  G  G  G
Memory row 3 (V=0.75):   R  R  R  R   ← was file row 0 (visual TOP) → now at OpenGL top
```

**Sample at UV (0.5, 0.6):**

Texel_y = 0.6 × 4 = 2.4 → floor = 2 → Memory row 2 = GREEN

V=0.6 is 60% from bottom = 40% from visual top → visual image 40% from top → GREEN band. Correct.

**Show the arithmetic one more time with a specific UV pair:**

UV (0.25, 0.875):
- Texel_x = 0.25 × 4 = 1.0 → column 1
- Texel_y = 0.875 × 4 = 3.5 → row 3 (with nearest: row 3 or 4, clamped to 3)

WITHOUT flip: row 3 = YELLOW (OpenGL stores file rows top-to-bottom → row 3 of file = YELLOW visual bottom → at V=0.875 which should be near visual top).
Expected at V=0.875 (near top): should be RED.
Got: YELLOW. Wrong.

WITH flip: row 3 in memory = RED (flipped, so file row 0 = visual top = now at memory row 3 = OpenGL top). At V=0.875 → memory row 3 → RED. Correct.

---

## Pitfalls

**Pitfall 1: Applying both stbi flip AND 1.0-v in shader**

Mistake: calling `stbi_set_flip_vertically_on_load(1)` AND also writing `uv.y = 1.0 - uv.y` in the fragment shader.

Symptom: the texture is flipped TWICE — which means it ends up correct-orientation again. This seems harmless, but it creates confusion and breaks predictably when:
- You add a normal map and apply the same double-flip to the normal map Y channel
- You load textures from different sources (some need flip, some don't)

Fix: standardize on ONE flip location. Use stbi flip at load time. Remove 1-v from all shaders. Use this consistently across every texture in the project.

**Pitfall 2: Using a DirectX normal map without correcting green channel**

Mistake: downloading a normal map from Poly Haven, AmbientCG, or any DCC tool that exports DX-convention, and using it directly in an OpenGL shader with no Y flip.

Symptom: lighting appears inverted vertically. Bricks that should protrude appear recessed. The effect is subtle under direct frontal lighting but very visible under oblique side-lighting. Specular highlights appear on the wrong side of bumps.

Fix: in the fragment shader: `n.y = -n.y;` after the `* 2.0 - 1.0` remap. OR source a GL-convention normal map (look for maps labeled "OpenGL" on Poly Haven — they provide both versions). OR flip the G channel offline in Photoshop/Krita before importing.

**Pitfall 3: Forgetting to set stbi flip before EVERY new stbi_load call sequence**

Mistake: `stbi_set_flip_vertically_on_load(1)` is a global setting. If you set it to 1, load your textures, then set it to 0 to load a cubemap face (which should NOT be flipped), then load more textures without setting it back to 1, those textures come in flipped wrong.

Symptom: intermittent flipping — some textures correct, others wrong. Hard to diagnose if loading happens in multiple places.

Fix: set the flip state immediately before every `stbi_load` call — even if it seems redundant. Or create a helper function `loadTexture()` that always sets the correct flip state for the kind of texture being loaded.

**Pitfall 4: Forgetting to renormalize sampled normals**

Mistake: sampling a normal map and using the result directly as a normal vector without normalizing.

Symptom: subtly incorrect lighting, especially visible as darkening or washing-out of specular highlights. Bilinear and anisotropic filtering both average neighboring texels — averaging unit vectors produces a vector with length < 1.

Fix: always `normalize()` after sampling and converting a normal map: `vec3 n = normalize(texture(uNormal, uv).xyz * 2.0 - 1.0);`

---

## What to Build

**Exercise 1: Complete texture loading function**

Write a C++ function:

```cpp
GLuint loadTexture(const char* path, bool flipVertically = true);
```

That:
1. Sets `stbi_set_flip_vertically_on_load(flipVertically)`
2. Loads the image with `stbi_load` (4 channels: STBI_rgb_alpha)
3. Generates and binds a GL texture
4. Uploads with `glTexImage2D` using GL_RGBA
5. Calls `glGenerateMipmap`
6. Sets GL_LINEAR_MIPMAP_LINEAR for MIN, GL_LINEAR for MAG
7. Frees the stbi data with `stbi_image_free`
8. On load failure: prints `path` and `stbi_failure_reason()` to stderr and returns 0
9. Returns the texture ID

This is the function you will call for every texture in the A1 project.

**Exercise 2: Normal map convention detector**

Write a GLSL fragment shader that:
- Takes a uniform `sampler2D uNormal`
- Takes a uniform `bool uIsDXNormal`
- Samples the normal map, remaps to [-1,1]
- Applies the green-channel flip if `uIsDXNormal` is true
- Normalizes
- Encodes the resulting normal as a color (x→R, y→G, z→B, remapped to [0,1]) and outputs it

This shader lets you visually compare GL-convention vs DX-convention interpretation of the same normal map by toggling `uIsDXNormal`.

**Exercise 3: Verify the fix**

Load the same normal map twice: once treating it as GL convention (`uIsDXNormal = false`) and once as DX convention (`uIsDXNormal = true`). Apply it to a sphere lit from one side. The G channel visualization should appear greenish (Y component mostly positive) for the correct convention when the light is above. Determine which mode is correct by examining the specular highlight position relative to the bump direction.
