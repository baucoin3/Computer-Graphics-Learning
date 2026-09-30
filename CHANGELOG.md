# Changelog

## [2026-09-24]

### Added
- **Lesson 4 — Lighting and Shading (Theory)** split across three new markdown files:
  - `AI Lessons/Sept21-L4A-Light-Surface-Physics.md` — Foundation material covering what lighting models approximate, diffuse vs. specular reflection physics, Lambert's cosine law with full derivation and worked examples, ambient/diffuse/specular decomposition, and light attenuation formulas for directional and point lights
  - `AI Lessons/Sept21-L4B-Shading-Models.md` — Comparison of flat shading, Lambert diffuse-only, Gouraud shading, Phong shading, and Blinn-Phong with explanations of where each executes in the pipeline and why Gouraud produces specular artifacts on curved surfaces
  - `AI Lessons/Sept21-L4C-Modern-Shading-PBR.md` — Why Blinn-Phong fails physically, rendering equation conceptually, BRDF definition, Cook-Torrance terms (D/F/G), metallic-roughness PBR parameters, deferred rendering overview, and IBL concepts
- **Lesson 4 Check** — Ten conceptual questions added to `AI Lessons/Sept14-2026-Lessons.md` to verify understanding before implementing lit shaders
- **Lesson 4 reading guide table** in `AI Lessons/Sept14-2026-Lessons.md` mapping each file to its topic coverage
- `cg-work/scripts/delete-project.sh` — Utility script (63 lines) for removing projects from the workspace

### Changed
- `cg-work/CMakeLists.txt` — Updated (purpose unclear from diff)
- `cg-work/projects/01-first-triangle/src/main.cpp` — Added 10 lines of code (purpose unclear from diff)
- `cg-work/projects/02-full-render-pipeline-scratch/CMakeLists.txt` — Added 1 line, appears to be a new or reorganized project variant
- `AI Lessons/full-render-pipeline-questions.md` — Minor 2-line update (content change not shown in truncated diff)

### Removed
- `cg-work/projects/02-first-basic-program/CMakeLists.txt` — Deleted
- `cg-work/projects/02-first-basic-program/src/main.cpp` — Deleted (38 lines removed)

### Clarification Needed
- What specific changes were made to `cg-work/CMakeLists.txt` and why?
- What code was added to `cg-work/projects/01-first-triangle/src/main.cpp` and for what purpose?
- What is the relationship between the deleted `02-first-basic-program` project and the new/modified `02-full-render-pipeline-scratch` project? Is this a rename, replacement, or separate variant?
- What was the 2-line change in `AI Lessons/full-render-pipeline-questions.md`?
- What does the `delete-project.sh` script do exactly, and are there any prerequisites or safety checks it performs?
