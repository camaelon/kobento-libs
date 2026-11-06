# kobento-libs

> Part of [Kobento](https://mcp.kobento.dev/), an independent implementation of RemoteCompose, not affiliated
> with Google. [How the repositories fit together](https://mcp.kobento.dev/about/) ·
> [Status and known gaps](https://mcp.kobento.dev/status/)

The Kobento C++ libraries: the RemoteCompose engine (`rcX`) for a compact binary UI / canvas format.

`rcX` reads a binary document — a stream of opcodes describing layout,
canvas drawing, expressions, animations, particle systems, paths, shaders
and bitmaps — and renders it interactively. The same document can play on
desktop, on iOS, headlessly to PNG, or be inspected as JSON.

```
   .rc / .rcd  →  rccore  →  rcskia (Skia)  →  pixels
                        ↘  rc2json (debug)
```

## Status

Pre-1.0. The macOS desktop pipeline (engine + Skia bridge + GLFW viewer +
headless tools) is solid. Linux builds the same desktop tools and a CPU/OpenGL
viewer. The iOS pipeline (static libs + SwiftUI app with a Metal-backed Skia
view) is solid for arm64 device builds. See
[docs/PROGRESS.md](docs/PROGRESS.md) for milestone-by-milestone status.

Windows is not in this tree yet; the engine itself is portable C++17.

## What's in here

```
kobento-libs/
├── lib/
│   ├── rccore/      Engine — wire format, document model, expressions,
│   │               layout, paint state, time variables, particles, etc.
│   ├── rcskia/      Skia bridge — implements the engine's PaintContext
│   │               abstraction on top of SkCanvas.
│   └── rcplayer/    Desktop player runtime — render backends (Metal / CPU),
│                   playlist + document loading, custom-component hosts for
│                   embedded video / web / sub-documents, zip decks,
│                   voice-over, PDF and video export. Link this to build your own
│                   player.
├── tests/           The engine's tests (ctest).
├── samples/         A handful of representative .rc files for smoke
│                   testing and demos (3d/: ten 3D charts, JSON + .rc).
└── docs/            Milestone progress, the RemoteCompose spec and introduction,
                    proposals (sound, surface, video) and format notes.
```

The rest of Kobento builds on these, from sibling checkouts next to this one:

| repository | what | uses |
| :--- | :--- | :--- |
| [kobento-tools](https://github.com/camaelon/kobento-tools) | `rc2image`, `rc2json`, `rc2layout`, json2rc, converters, conformance, inspector | `add_subdirectory(../kobento-libs)` |
| [kobento-desktop](https://github.com/camaelon/kobento-desktop) | `rcviewer`, the macOS / Linux desktop viewer | `add_subdirectory(../kobento-libs)` |
| [kobento-ios](https://github.com/camaelon/kobento-ios) | the iOS static libs, the Core Graphics backend, the SwiftUI viewer | `lib/rccore`, `lib/rcskia` |
| [kobento-ts](https://github.com/camaelon/kobento-ts) | the TypeScript web player | — |
| [kobento-mcp](https://github.com/camaelon/kobento-mcp) | the MCP servers | kobento-tools, kobento-ts |

Embedded with `add_subdirectory`, the tests are off (`RCX_BUILD_TESTS`).

## Quick start (macOS / Linux)

```sh
# Configure + build the libraries and tests. Skia, GLFW and miniz are auto-fetched by CMake.
cmake -B build
cmake --build build -j
ctest --test-dir build
```

The first `cmake -B build` takes a couple of minutes — Skia (~150 MB pre-built)
is downloaded once into `build/_deps/skia-src/` and reused thereafter.

To render or view a document, build the tools or the viewer next to this checkout:

```sh
git clone https://github.com/camaelon/kobento-tools ../kobento-tools
cmake -S ../kobento-tools/cpp -B ../kobento-tools/cpp/build && cmake --build ../kobento-tools/cpp/build -j
../kobento-tools/cpp/build/rc2image/rc2image samples/canvas.rc out.png

git clone https://github.com/camaelon/kobento-desktop ../kobento-desktop
cmake -S ../kobento-desktop -B ../kobento-desktop/build && cmake --build ../kobento-desktop/build -j
../kobento-desktop/build/viewer/rcviewer samples/balls_animation_example.rc
```

On Linux, install CMake, git, pkg-config, GLFW, OpenGL, FontConfig, FreeType,
and X11 development packages first. The Linux viewer uses the CPU backend;
`--metal` and AVFoundation video formats remain macOS-only.

## Profiling the 3D path

`RC_PROF=1` splits a render into its CPU geometry front-half and its fill, and reports the
painter's sort separately. Silent unless the variable is set, and the rendered output is
byte-identical either way.

```sh
RC_PROF=1 ../kobento-tools/cpp/build/rc2image/rc2image doc.rc out.png 800 800
```

```
RC_PROF  triangles submitted=6060 kept=4908
RC_PROF  software   transform     0.99 ms (  8.2%)   fill    11.14 ms ( 91.8%)
RC_PROF  accelerated front-half (buildCanvasVertices)     0.00 ms   of which sort     0.00 ms
```

Reading it: transform scales with triangle count and is flat in resolution; fill scales
with covered pixels. Which dominates is a property of the document, not of the engine —
`city3d` is 98% fill at 1,728 triangles, while `hydrogen_orbitals3d` at 33k triangles is
70% transform. The crossover sits near 15–20k triangles at 800px. Measure before optimising;
the intuition here has been wrong more than once.

The transform figure includes two clock reads per triangle, on the order of 0.3 ms per 6k
triangles, so read small absolute values as an upper bound. Ratios and scaling are sound —
for per-triangle cost, vary the input and take the slope.

## iOS build

The iOS static libraries and the SwiftUI app are in
[kobento-ios](https://github.com/camaelon/kobento-ios), built against this checkout.

See [BUILDING.md](BUILDING.md) for the full per-platform walk-through and
common-pitfall list.

## License

Apache 2.0. See [LICENSE](LICENSE).
