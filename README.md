# OpenGL Skeletal Animation Pipeline

## 1. Introduction

This project plays FBX skeletal animation in C++ and OpenGL.

It started as my university major project. I rebuilt it step by step to make it faster on both the CPU and the GPU, and measured the CPU and GPU time at every step.

## 2. Summary

**Final comparison (11/24, same day, same conditions)**

100 characters, ms, average of 9 samples: 3 runs x 3 seconds, warmup excluded

|             | Bone  | Submit | Frame | GPU  |
|-------------|-------|--------|-------|------|
| baseline-v2 | 0.038 | 4.16   | 7.47  | 7.16 |
| final       | 0.032 | 0.061  | 1.06  | 0.97 |
| change      |       | -98.5% | -86%  | -86% |

Bone is per character in baseline-v2 and per frame in final.

Memory: peak during load went from 819 MB to 501 MB (Debug, v0-university → final).

https://github.com/user-attachments/assets/67202495-d9de-4bb5-aaff-011619fcecda



## 3. Measuring method

**Conditions**

- Release mode
- vsync off
- window size 600x400
- fixed camera
- 100 characters
- 3 runs, average of 3 seconds per run
- laptop on AC power
- warmup excluded
- other programs closed
- GPU Model : NVIDIA GeForce RTX 3060 Laptop GPU
- Driver version : 32.0.15.9174

**Labels**

- Bone: time to update the joint matrices (per character until opt-9, per frame from opt-10)
- Submit: CPU time for one frame, not counting `glfwSwapBuffers`
- Frame: whole frame including `glfwSwapBuffers`, so it also includes waiting for the GPU
- GPU: GPU time for one frame, measured with `GL_TIME_ELAPSED` queries

The CPU and GPU profilers were written for this project.

## 4. Result sheet by level

Results (100 characters, ms, average of 9 samples: 3 runs x 3 seconds, warmup excluded)

| Step | Change | Bone | Submit | Frame | GPU |
|---|---|---|---|---|---|
| baseline-v2 | Naive runtime version | 0.040 | 4.38 | 7.64 | 7.25 |
| opt1-node-name-ref | Take node names by reference | 0.036 | 3.94 | 7.38 | 6.94 |
| opt2-cache-uniform-location | Cache uniform locations | 0.036 | 3.88 | 7.29 | 6.96 |
| opt-3-cache-string-hashmap | Precompute name-to-index lookups | 0.023 | 2.59 | 7.31 | 6.99 |
| opt-4-convert-vmath-to-glm-FBXModel-h | Replace vmath with GLM | 0.018 | 2.05 | 7.32 | 6.93 |
| opt-5-replace-recursive-bone-update | Replace recursion with a linear loop | 0.018 | 2.05 | 7.14 | 6.86 |
| opt-6-using-EBO-to-reduce-GPU-time | Indexed drawing with EBO | 0.017 | 1.97 | 4.66 | 4.34 |
| refactor-using-mesh-offset | Per-mesh skinning (finger fix) | 0.020 | 2.37 | 7.44 | 7.11 |
| opt-7-orphanize-uniform-buffer | UBO orphaning | 0.020 | 2.48 | 4.12 | 3.88 |
| feat-add-blending | Animation blending | 0.020 | 2.57 | 4.24 | 3.93 |
| opt-8-normal matrix without inverse | Drop per-vertex inverse in normal matrix | 0.023 | 2.85 | 3.87 | 3.63 |
| opt-9-front-to-back-draw-order | Draw characters front to back | 0.019 | 2.36 | 2.44 | 1.72 |
| opt-10-Instancing | One bone update per frame + instanced draw | 0.033 | 0.066 | 1.27 | 1.16 |
| opt-11-use-mipmap-for-texture-minification | Use mipmaps for texture minification | 0.032 | 0.061 | 1.06 | 0.97 |

opt-2 made little difference. At that point about 90% of the CPU time was the bone update, so the cost of looking up uniform locations was too small to show.

opt-5 made no measurable difference. The nodes were already stored in depth-first order when loading, so the recursion was reading memory in the same order as the loop.

Raw logs are in [`measurement/`](measurement/).

## 5. Frustrations and Wins

### Stretched fingers

Some fingers looked stretched. I had stored one inverse bind matrix per joint name and used it for every mesh. But three of the six meshes were skinned in a different bind pose, so the same joint had a different inverse bind matrix in each mesh. I changed it so that each mesh keeps its own joint list and inverse bind matrices (one skin per mesh), and the fingers were fixed.

After this fix, GPU time went up from 4.34 ms to 7.11 ms (+64%). The cause was rewriting the same UBO 600 times per frame while the GPU was still reading it, so each upload had to wait. Orphaning the buffer before each upload (opt-7) brought it down to 3.88 ms.

### GPU bottleneck experiment

After the CPU work, the frame time was decided by the GPU (3.86 ms), but I didn't know which part of the GPU work was expensive. So I removed one part at a time and measured the GPU time again.

My first two guesses were wrong. Removing the per-vertex inverse in the normal matrix saved only 0.24 ms (I kept it as opt-8), and removing texture sampling saved only 0.36 ms. Removing the lighting while still passing the vertex outputs (exp7) saved about 2 ms, so the lighting was the biggest cost.

Phong lighting is cheap per pixel, so the problem had to be how many times it ran. I was drawing the back row first, so hidden pixels were lit and then overwritten. Drawing the characters front to back lets the depth test skip those pixels before the fragment shader runs. It was a one-line change to the loop direction, and GPU time went from 3.63 ms to 1.72 ms (-53%).

exp1-4, 6 and 7 remove features, so I used them only to find the cause, not as fixes.

| Experiment | Change |
|---|---|
| exp1 | resolution x4 (1200x800) |
| exp2 | MSAA off |
| exp3 | flat fragment color |
| exp4 | no skinning in vertex shader |
| exp5 | normal matrix without inverse |
| exp6 | exp5 + no texture sampling |
| exp7 | exp5 + no lighting, varyings still used |
| exp8 | exp5 + front-to-back draw order |

### Instancing

All 100 characters share the same pose, but `draw()` updated the bones and uploaded the same matrices 100 times per frame. I now update the bones once per frame, put the model matrices in an SSBO, and draw each mesh with one `glDrawElementsInstanced` call (600 draw calls -> 6). Submit went from 2.36 ms to 0.066 ms.

I expected only the CPU side to improve, but GPU time also dropped from 1.72 ms to 1.16 ms. My guess is that uploading the skinning UBO 6 times instead of 600 times per frame removed some buffer switching and waiting on the GPU side.

### Mipmap setting

The textures had mipmaps, but `MIN_FILTER` was `GL_LINEAR`, so the mipmaps were never used. The characters are small on screen, so each pixel was reading texels far apart in a large texture, which is bad for the texture cache. Switching to `GL_LINEAR_MIPMAP_LINEAR` cut GPU time from 1.22 ms to 0.97 ms (-21%).

In exp6, removing texture sampling saved only 0.36 ms, so I had decided textures were not the problem. Looking at it again from the filtering side showed that part of that cost was real and easy to remove.

## 6. Memory

Process memory (VS Diagnostic Tools, MB). v0 was measured on 11/01.

|                     | Peak during load | Steady |
|---------------------|------------------|--------|
| Debug v0-university | 819              | -      |
| Debug baseline-v2   | 559              | 318    |
| Debug final         | 501              | 267    |
| Release baseline-v2 | 536              | 296    |
| Release final       | 493              | 258    |

Debug peak memory went from 819 MB (v0) to 501 MB (final). Most of the remaining memory is probably Assimp while loading, and the driver and textures while running (my guess).

VRAM is not included.

## 7. Premises and Remaining tasks

**Premises**

- From opt-8: each model matrix has a uniform scale (no stretching along one axis)
- From opt-10: all characters play the same animation at the same time (same pose)

**Remaining tasks**

- Key cursor caching in `pickKey` (the bone time changes with the playback position because of the linear search)
- Move the joint matrices from a UBO to an SSBO (removes the 52-joint limit)
- Read texture paths from the FBX materials
- Release the Assimp importers right after loading to lower the peak memory
- A common render interface for `Model` and `FBXModel`

## 8. Controls

| Key | Action |
|---|---|
| W / S | Walk forward / backward |
| A / D, mouse drag | Turn |
| Mouse wheel | Zoom (FOV) |
| Space | Pause |
| 1 / 2 | Toggle character / light |
| M | Wireframe |
| Z / X / C | Post-processing filter |
| R | Reset |
| I | Show / hide UI |

## 9. AI Usage

- Used Claude as a tutor and reviewer: explaining concepts when I was stuck, suggesting options to compare, reviewing code before commits, and helping with the English in this README and with averaging the logs.

- All code, final design decisions and measurements are my own.

## 10. Build

- Windows, Visual Studio 2022, C++17 (/std:c++17), x64
- All libraries are included in extern/. Open the .sln and build in Release x64.
- The Mixamo animation files are not included (see section 12), so the character will not animate without them.

## 11. Libraries

- GLFW
- glad
- GLM
- Assimp
- Dear ImGui
- stb_image

## 12. Assets

- Model: made by my team in a university project. The professor allowed us to redistribute it.
- Animations: Mixamo (Adobe). The raw files are not included, because Mixamo does not allow redistributing them.
- Floor texture: provided by the professor for the same course.

Assets that cannot be redistributed are not included in this repository.

## 13. Licenses

The source code in this repository is under the [MIT License](LICENSE).
The MIT License does not cover the assets in section 12 or the third-party libraries below.

Third-party libraries keep their own licenses. Each license file is included next to the library.

| Library | License | File |
|---|---|---|
| GLFW | zlib/libpng | `extern/glfw/LICENSE.md` |
| glad (0.1.36, generated loader) | Public Domain, WTFPL or CC0 for the generated code. `khrplatform.h` is under the Khronos license in its header. | `extern/glad/include/KHR/khrplatform.h` |
| GLM | Happy Bunny License or MIT | `extern/glm/copying.txt` |
| Assimp | BSD 3-Clause | `extern/Assimp/LICENSE` |
| Dear ImGui | MIT | `extern/imgui/LICENSE.txt` |
| stb_image (v2.26) | MIT or Public Domain | end of `_myApp_/stb_image.h` |
