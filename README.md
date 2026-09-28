# Tsar Bomba air-burst simulation (C++17 / OpenGL 4.3 compute)
Visual recreation of an aircraft release, parachute descent, and nuclear airburst. The aircraft and bomb are exterior display models.
This is an artistic reconstruction, not a validated predictor of nuclear effects.
Presets: Tsar Bomba (50 Mt, 4 km burst height, Arctic scene) and a 1 Mt airburst (2 km) for comparison.
```
tsar-bomba-sim/
  CMakeLists.txt        FetchContent: GLFW 3.4, GLM 1.0.1, glad 0.1.36 (GL 4.3 core), stb_image_write
  src/physics.hpp       USSA-76 atmosphere, Sedov-Taylor shock, fireball radius/temperature curve
  src/gl_util.hpp       shader loader (prepends common.glsl), texture + dispatch helpers
  src/fluid.hpp         GPU solver driver (ping-pong 3D textures)
  src/renderer.hpp      raymarch -> HDR -> bloom + auto-exposure -> shock distortion + ACES
  src/main.cpp          window, orbit camera, time control, presets, screenshots
  src/scene.hpp         time-compressed flight staging and visual scale references
  src/models.hpp        procedural aircraft, propellers, bomb casing, and parachute
  shaders/common.glsl   shared uniforms, atmosphere lookup, Planck colour, noise
  shaders/seed.comp curl.comp forces.comp advect.comp divergence.comp jacobi.comp project.comp light.comp
  shaders/fullscreen.vert raymarch.frag bloom.frag post.frag
  shaders/detail.comp  tileable 3D noise for rendering detail
  shaders/billows.comp  rounded cellular cloud detail (render-only)
  shaders/model.vert model.frag cloud_bounds.comp
```
## Build and run
Needs a GPU/driver with OpenGL 4.3 core (Windows or Linux). **macOS is not supported**: Apple stops at OpenGL 4.1, with no compute shaders.
Linux packages: `build-essential cmake python3 libgl-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev`
(python3 is used by glad's generator at build time).
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/tsar                              # flyover -> drop -> explosion, 128x64x128 grid
./build/tsar --skip-intro                 # start at detonation
./build/tsar --fixed-camera               # stationary overview for visual comparison
./build/tsar --preset 2                   # 1 Mt
./build/tsar --grid 64 --iters 20 --steps 96    # lighter settings for integrated GPUs / Mesa llvmpipe
./build/tsar --shot 480 cloud.png         # hidden window: simulate to t = 480 s, save PNG, exit
./build/tsar --intro-shot 2 aircraft.png  # aircraft approach
./build/tsar --intro-shot 10 drop.png     # parachute descent
```
Options: `--preset 1|2`, `--grid N` (N x N/2 x N cells), `--iters N` , `--steps N` (raymarch samples), `--size W H`.
Use `--help` for a usage summary. Grid size must be a multiple of 8 from 16 to 256;
iterations 1..1000, samples 1..4096, and window dimensions 8..8192. Larger settings use substantially more GPU memory.
Screenshot time must be finite and nonnegative. Invalid arguments and failed PNG writes return a nonzero exit code.
`--shot` measures seconds after detonation and bypasses the introduction. `--intro-shot` accepts 0 <= T < 24.
Screenshot mode still needs a graphical display and OpenGL 4.3; on a display-less Linux host, run it through Xvfb.
The first configure downloads the pinned dependencies. glad uses its bundled OpenGL specification, so generation does not need another download.
Shaders are loaded from the source tree at run time (path baked in by CMake), so edits apply on restart.
## Checks
Command-line checks run without a display. Enable GPU smoke tests on a desktop (or inside Xvfb):
```
cmake -S . -B build -DTSAR_GPU_TESTS=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
```
The rendering checks compile every shader and exercise approach, release, parachute, camera pullback,
fixed-camera framing, time zero, the early fireball, and both cloud presets. Playback checks cover
pause, camera takeover, reset, resize, generated cellular detail and exposure stability.
The timeline test checks continuous release,
downward descent, canopy deployment, and removal of the bomb at detonation.
They also exercise non-power-of-two textures and odd window dimensions, check OpenGL errors and finite HDR pixels, and verify PNG output.
These are software checks, not validation of the physical model. Use `-DTSAR_GPU_TESTS=OFF` for command-line checks only.
## Controls
| Key / mouse | Action |
|---|---|
| Left drag, arrow keys | orbit |
| Scroll | zoom |
| Right drag, W / S | raise / lower the look-at point |
| Space | pause |
| `=` / `-` | speed x2 / /2 |
| L | toggle logarithmic early time (one decade per 1.5 s until the grid takes over) |
| I | skip the aircraft/drop sequence |
| C | toggle cinematic camera / restore automatic framing |
| F | toggle stationary reference overview |
| R | restart; 1 / 2 switch presets |
| P | save screenshot; Esc quits |
The window title shows time, speed, fireball temperature and shock radius.
Camera input takes over from automatic framing at the current viewpoint. R replays the whole sequence.
## Historical scale and staging
The default sequence starts alongside a cosmetic Tu-95V model with Soviet markings, four engines,
and animated counter-rotating propellers. The bomb detaches after five seconds, deploys a parachute,
and descends to the existing airburst point. The camera follows close by, then pulls back for the flash.
The entire introduction lasts 24 playback seconds; the descent and aircraft travel are cinematic animation.

The 50 Mt preset uses an approximately 8 km fireball diameter. By 480 simulation seconds, the displayed
cloud envelope approaches 64 km high and 95 km wide. A GPU bounds pass measures the smoke envelope
and applies a rendering transform to meet these visual targets. This transform does not alter the solver
or establish the historical growth rate. The 1 Mt comparison keeps its existing cloud scale.
The explosion camera starts about 34 km from its focus, widening to about 100 km for the mature cloud;
this is closer than the previous 136 km default. All scene geometry uses metres.

Historical references: [Atomic Heritage Foundation / Nuclear Museum](https://ahf.nuclearmuseum.org/ahf/history/tsar-bomba/)
for the Tu-95V, parachute, release altitude, and reported cloud dimensions;
[National WWII Museum](https://www.nationalww2museum.org/war/articles/tsar-bomba-largest-atomic-test-world-history)
for the approximately 8 km fireball diameter. The result is an artistic reconstruction with approximate historical scale.

The supplied Tsar footage and Castle Bravo footage were reviewed separately: they are different tests,
and edited video time is not elapsed event time. See [reference observations and accuracy limits](docs/REFERENCE_NOTES.md)
for the comparison, rendering changes, and remaining approximations.
## Visual detail
The renderer adds smoothly moving, mip-filtered cellular billows, eroded cloud edges, shaded dust,
local lobe shadows, soft indirect cloud lighting, two-layer distance haze, and a textured fireball surface.
A procedural snow-and-rock landscape and a raised initial viewpoint give the scene depth and scale.
The orbit camera stays above the landscape to avoid clipping through it.
The detail textures are generated once on the GPU; no image downloads or external assets are needed.
These are artistic rendering approximations: detail is not additional fluid resolution, and the landscape
does not change the solver's flat ground boundary. The existing physical-model limitations still apply.
For cleaner cloud edges in still images, increase raymarch samples, for example:
```
./build/tsar --steps 320 --shot 480 cloud.png
```
## Model
- E = W x 4.184e15 J. Shock R = 1.03 (E t^2 / rho)^(1/5) with rho at burst height, switching to sound speed once the
  Sedov-Taylor front slows below it. Drawn in post as a refraction shell, plus a mirrored reflected shell and a Mach-stem ring.
- Fireball R_max = 1 km x W^0.4, t_2 = 1 s x W^0.5. Temperature keyframes: first flash (20,000 K), breakaway dip (2,800 K at
  0.078 t_2), 7,500 K second maximum at t_2, then cooling. Colour is Planck at 610/550/465 nm; brightness is sigma T^4, faded
  below the ~800 K Draper point.
- At 2 t_2 the fireball is seeded on the grid with a flattened bottom. Grid solver: semi-Lagrangian advection (RK2 backtrace),
  buoyancy g theta'/theta, vorticity confinement, anelastic continuity div(rho(z) u) = 0 through a density-weighted Jacobi solve,
  USSA-76 background to 86 km. theta' holds heat per volume at ambient density; a separate incandescent-temperature tracer
  cools as dT/dt = -k (T^4 - T_a^4) and drives glow. Ground dust/snow is raised by the blast front and the afterwinds.
- Domain: 160 x 80 km for 50 Mt, scaled by (W/50)^0.25.
