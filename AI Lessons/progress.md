# Known Baseline

Authoritative record of topics covered and current comfort level.
Read this file before calibrating any new lesson plan.

Last updated: 2026-09-30

---

## Coordinate Spaces + TRS Transforms
- Completed: Sept 2026 | Source: L1B, L1C
- Comfort: Conceptual. Understand model/world/camera/clip/NDC space chain, why each transform exists, how model→world→view→projection composes. Understand homogeneous coordinates and the w divide. Cannot derive full rotation/projection matrix from memory without reference.

## Full Render Pipeline
- Completed: Sept 2026 | Source: L1F, Project 01 (first triangle), Project 02
- Comfort: Conceptual + applied. Know the stage sequence (vertex shader → rasterizer → fragment shader → output merger) and what each stage does. Have coded VAO/VBO/EBO setup, `glDrawElements`, and compiled shaders with error checking. Can read and write basic pipeline code. Cannot reproduce rasterization math from memory.

## Phong / Blinn-Phong Shading
- Completed: Sept 2026 | Source: L4A–L4C, Project 02/03
- Comfort: Conceptual + applied. Understand ambient/diffuse/specular terms, why each exists, what Blinn's half-vector optimization does. Have written and debugged lighting in fragment shaders. Know why per-fragment beats per-vertex. Cannot derive the full energy analysis or Fresnel approximation from memory.

## FPS Camera (Yaw/Pitch + View Matrix)
- Completed: Sept 2026 | Source: L6A–L6D, Project 03
- Comfort: Conceptual + applied. Understand how yaw/pitch angles construct a forward vector, how `glm::lookAt` builds the view matrix from eye/center/up, and how per-frame mouse delta drives rotation. Have coded FPS-style camera. Cannot reconstruct the lookAt derivation from memory without prompting.

## VAO / VBO / EBO + Indexed Draw Calls
- Completed: Sept 2026 | Source: L2, Project 01–03
- Comfort: Applied. Have set up interleaved VBOs, bound attribute pointers, and used EBOs for indexed geometry across multiple projects. Know why indexed rendering exists (vertex reuse). Understand the bind/upload/draw sequence. Would need a reference for stride/offset math when adding new attributes.

## Player Movement + AABB Collision
- Completed: Sept 2026 | Source: L7A
- Comfort: Conceptual. Understand AABB overlap test and axis-aligned push-out response. Have seen the code. Not yet applied it in a full project — comfort is more theoretical than hands-on.
