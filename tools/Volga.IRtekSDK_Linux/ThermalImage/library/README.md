# Library Layout

`Volga.IRtekSDK_Linux/ThermalImage/library/` keeps the packaged Linux runtime files.

Expected delivery layout:

- `rk3506/library/` - shared libraries used at build/runtime
- `rk3506/package/IRtekFileSDK/` - optional CMake package files copied from the SDK release
- `ubuntu20/library/` - shared libraries used at build/runtime
- `ubuntu20/package/IRtekFileSDK/` - optional CMake package files copied from the SDK release

The user can replace or supplement the `.so` and CMake package files in these directories.
