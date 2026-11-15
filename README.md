# OpenGL Skeletal Animation Pipeline

A C++/OpenGL program that loads FBX characters and plays skeletal animation at runtime.

The first version was a university assignment. It depended on a textbook framework
and baked every animation into a fixed table of matrices before playback, so clips
could not be switched or blended. I am rebuilding it into a runtime pipeline and
will measure each change.

Work in progress. Latest work is on the `main` branch.

## Current state

- Window, input, main loop and shader loading written directly on GLFW + glad (OpenGL 4.5 Core)
- Keyframe sampling every frame: lerp for position and scale, slerp for rotation
- Skeleton stored in its own array, independent of Assimp data after loading
- Mesh and animation clips loaded from separate FBX files
- Idle / Walking clips switched by input; clips are looked up by name once and referenced by index per frame
- `tools/FbxInspector`: prints clip duration, ticks per second, channels, key counts and a loop check

## Next

1. Measurement tools: scoped CPU timer, GPU timer queries, memory
2. Baseline in Release with vsync off, 1 / 10 / 100 characters
3. Data-oriented changes: flattened bone hierarchy, integer indices instead of string lookups, SoA layout
4. Re-measure under the same conditions
5. Clip blending

## Build

- Visual Studio 2022, x64, C++17
- Dependencies are in `extern/`: GLFW 3.5.1, glad, GLM, Assimp
- The Assimp DLL is copied next to the executable by a post-build step

## Controls

| Key | Action |
| --- | --- |
| W / S | Walk forward / backward |
| A / D, mouse drag | Turn |
| Mouse wheel | Zoom (FOV) |
| Space | Pause |
| 1 / 2 | Toggle character / light |
| M | Wireframe |
| Z / X / C | Post-processing filter |
| R | Reset |
