# Per-pixel dodecahedron with orbiting point light

This package is source-only. All current probe targets rebuilt successfully
before cleanup; binaries and inspection reports are generated, not bundled.
From the workspace root:

```sh
cmake -S sysprobe -B /tmp/x7-sysprobe-build
cmake --build /tmp/x7-sysprobe-build --parallel
```

Console observations and earlier calculator-replacement instructions below
remain historical evidence. DOOM and Omega now use their own launcher entries
and `res/*` resource directories; that game result does not establish a new
launch route for every probe.

`sysprobe-dodeca-fragment.app` is a separate variant of the working fast
framebuffer demo. It retains the 480×272 video mode, 175% projected size,
time-based rotation, bounded callbacks and current/recent-buffer guard.
Lighting is evaluated at each covered pixel, with an orbiting point light.
The existing flat-shaded source variants can be rebuilt for comparison.

## Run and build

Install this `.app` as `calculat.app`, using the same tools-menu launch route
as the preceding demos. **START exits**; system quit requests and detected
framebuffer errors also exit. There is **no automatic timeout or frame cap**
in this variant. This follows the disabled timeout in the supplied shared
demo source, and also removes the fast demo's remaining 12,000-frame guard
for this target. The commented timeout code in the existing demo is preserved.

```sh
# From the workspace root:
cmake -S . -B build
cmake --build build --target sysprobe-dodeca-fragment

# Or build all source-package variants:
sysprobe/build.sh
```

CMake writes `build/sysprobe/sysprobe-dodeca-fragment.app`; the shell script
writes ignored `sysprobe/sysprobe-dodeca-fragment.app`. Neither output is
bundled; build it before installing. The compile definitions are `PROBE_UI`, `PROBE_DODECA`,
`PROBE_FB`, `PROBE_FAST`, and `PROBE_FRAGMENT`.

Diagnostics go to `/mnt/card/appprobe-dodeca-fragment.txt`, with startup
failures in `/mnt/card/appprobe-dodeca-fragment-start-error.txt`. The log starts
with `X7 fragment framebuffer dodecahedron demo v1`. It records projection
scale 175%, fragment shading enabled, orbit phase period 1024, and automatic
timeout disabled. Exit writes the existing frame/tick/buffer-guard counters;
no diagnostic card writes are added during routine rendering. The RAM frame
buckets cover the first 6400 OS ticks; later frames increment their overflow
counter rather than overwriting memory. Logs are overwritten on subsequent
runs. For timing definitions and limits, see [DEMOS.md](DEMOS.md).

## Fragment shading

[fragment.c](fragment.c) and [fragment.h](fragment.h) implement the shader;
[dodeca_render.c](dodeca_render.c) integrates it with the existing span
rasterizer. Each pentagon retains its geometric face normal. This is a faceted
solid with per-pixel lighting, not a rounded solid with interpolated vertex
normals, and not interpolation of lighting computed only at the vertices.

For a pixel `(x,y)`, a precomputed world-space ray is formed from the camera
basis and focal length. In mathematical notation:

```text
u = x - width/2
v = height/2 - y
R = forward + right*(u/focal_length) + up*(v/focal_length)
C = camera position
N = face normal
Q = a point on the face
P = C + R * dot(N, Q-C) / dot(N, R)
```

Ray/face-plane intersection reconstructs the surface point in perspective;
affine interpolation of vertex positions in screen space is not used. The
integer implementation uses 1024 units per world unit. Columns and rows are
precomputed at initialization to avoid rebuilding the basis per fragment.
Parallel/backward/extreme intersections return ambient color, and this
variant requires focal length 100–1000 to bound intermediate arithmetic.
The standard enlarged scene uses 525 and fits these bounds.

At `P`, the shader evaluates the direction and distance to the current light:

```text
L = light_position - P
cosine = max(0, dot(N, L/length(L)))
intensity = 64 + 191*cosine * 64/(64 + distance_in_world_units_squared)
```

The integer computation clamps intensity to 0–255. The material matches the
previous demo: ambient plus Lambert diffuse with squared-distance attenuation,
blue/teal facet colors, and dark outlines. There is no specular highlight,
shadow map, texture or gamma correction. Palette lookup tables precompute the
12 face colors at all 256 intensity levels, so the fragment loop does not call
HSV conversion. Per-fragment intersection, integer square root and divisions
still cost substantially more than filling flat-color spans. The native
16-bit output can visibly quantize smooth gradients; no dithering is applied.

The background retains the fast paired-pixel clear. Filled faces use one
16-bit color write per pixel because neighboring fragments need not match.
Painter ordering and back-face culling are unchanged. This convex scene still
does not need a depth buffer or polygon near-plane clipping. The normal and
face plane are prepared once per visible face, shared across its triangles.

## Light motion and animation

The light starts at **(−2,3,2)**, with radius **sqrt(8)** about the vertical
world **Y axis**. Y remains 3; X/Z rotate about the origin. The existing sine
lookup is linearly interpolated at quarter-table-step resolution, and one
light orbit takes **1024 object-animation phase steps** (3072 OS ticks with
one phase step per three ticks). This is four times the complete repeating
object-rotation period. Tick duration is not established, so this is not a
calibrated number of wall-clock seconds. Both object and light follow elapsed
time rather than submission count, avoiding the previous startup spin burst.

The projected light marker follows the moving light when it falls on screen.
It is a diagnostic overlay, not depth-tested geometry; it may cross a screen
border. Host clipping tests distinguish that marker from the contained object.
The orbit affects illumination continuously, including when the object later
returns to an identical orientation.

## Validation and status

The MIPS CMake target builds with warnings as errors, no undefined symbols,
all 243 root syscall stubs and the existing vendor app/GUI ABI checks. Existing
bitmap/base-framebuffer/fast targets also build. No new firmware traps or
unrecovered GPU operations are introduced.

`tests/fragment.c` compares 2000 lighting samples to an independent
floating-point reference, checks back lighting/ambient behavior, verifies the
central ray intersection and confirms a color gradient within one plane.
`tests/dodeca_render.c` runs 1200 rotation frames under ASan/UBSan, checks the
object remains inside the screen, checks a quarter/full light orbit, and checks
that repeated object geometry is lit differently. A host preview is provided
as [dodeca-fragment-preview.png](dodeca-fragment-preview.png).

`tests/dodeca_fragment_flow.c` covers START, system quit, GUI/window/DC/timer/
submission/query failures, current/recent-buffer exclusion and clean shutdown.
It rejects installation of an exit timer; a lightweight renderer fixture
verifies that passing 12,000 frames does not request exit. Like the earlier
mocks it uses temporary host-width ABI adaptations on this 64-bit development
host; actual O32 layout is checked in the cross build. No host test simulates
LCD DMA, watchdog timing or console performance.

Host checks pass. The first console run **worked**, with a user-estimated
frame rate of about **15 FPS** and visible color-depth limitations. The
[root diagnostic log](../appprobe-dodeca-fragment.txt) records 571 measured-loop
frames over 4040 OS ticks (572 including startup), 3081 render ticks and
37 submission ticks. Rendering accounts for about 76% of measured loop ticks;
query/message/scheduler/input costs are outside those render timings. The
15 FPS figure is the user's visual estimate, not measured LCD refresh rate.
START/DOWN requested exit; video stop, blending disable/destroy and pool free
all returned zero, with no address-query failures, no unavailable buffers and
zero assertion failures. No protected candidate was rejected in this run.

The observed bands are consistent with native 5:6:5 quantization combined with
integer intensity/palette lookup and no dithering. Ordered dithering could
reduce visible bands without changing video mode; reducing per-fragment square
roots/divisions or rendering at lower resolution could improve speed. Neither
change has been implemented or tested in this version.
Keep the verified [framebuffer guard](FRAMEBUFFER.md#buffer-ownership-the-startup-noise-fix)
when experimenting with speed; successful submission is not a completion fence.
