# IRtekSDK_Linux Delivery Layout

`Volga.IRtekSDK_Linux` is organized as a Linux delivery package that mirrors the Windows SDK demo structure while keeping Linux-native CLI demos.

## Package Structure

- `ThermalImage/`
  - `header/` - packaged `IRtekFileSDK` public headers
  - `library/<variant>/library/` - runtime `.so` files (`rk3506`, `ubuntu22`)
  - `library/<variant>/package/IRtekFileSDK/` - optional CMake package files
  - `demo/1- C demo/` - Linux CLI demo in Windows-like two-layer layout
- `IRtekUVCSDK/`
  - `header/` - packaged `IRtekUVCSDK` and `IRtekFileSDK` headers
  - `library/<variant>/library/` - runtime `.so` files (`rk3506`, `ubuntu22`)
  - `library/<variant>/package/IRtekUVCSDK/` - optional CMake package files
  - `demo/1- C demo/` - Linux CLI demo in Windows-like two-layer layout
- `IRtekNetSDK/`
  - `header/` - packaged `IRtekNetSDK` public header
  - `library/<variant>/library/` - runtime `.so` files (`rk3506`, `ubuntu22`)
  - `library/<variant>/package/IRtekNetSDK/` - optional CMake package files
  - `demo/1- C demo/` - Linux CLI demo in Windows-like two-layer layout

`AcousticImage/` is intentionally not part of the Linux package.

## Library Placement Rules

All Linux demos now resolve dependencies from the current repository instead of sibling SDK install trees:

- `ThermalImage/library/<variant>/library/libIRtekFileSDK.so`
- `IRtekUVCSDK/library/<variant>/library/libIRtekUVCSDK.so`
- `IRtekNetSDK/library/<variant>/library/libIRtekNetSDK.so`

Optional package metadata can be copied into `library/<variant>/package/<SDK>/`.

`IRtekUVCSDK` can optionally load `libIRtekFileSDK.so` for preview rendering. `IRtekNetSDK` can optionally use FFmpeg and `IRtekFileSDK` runtime libraries when they are placed under the package library directories.

## Demo Entry Points

Each packaged demo provides:

- `build_demo.sh`
- `run_demo.sh`
- a support directory and an executable project directory under the demo folder

Use the per-demo scripts inside each product directory to build or run the Linux CLI demos.
