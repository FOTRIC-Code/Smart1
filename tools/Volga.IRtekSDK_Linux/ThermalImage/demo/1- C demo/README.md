# ThermalImage Linux C Demo

This packaged demo mirrors the Windows delivery layout while using a Linux CLI executable.
Besides the original aggregated `demo` walkthrough, it now exposes separate CLI subcommands for the main Windows `ThermalFileReaderDemo/MainForm.cs` button chains so they can be triggered individually.

## Structure

- `ThermalImage/` - support code shared by the executable
- `ThermalFileReaderDemo/` - executable project
- `build_demo.sh` / `run_demo.sh` - package-oriented build and run entrypoints

## Build

```bash
./build_demo.sh
```

By default the script uses `SDK_VARIANT=ubuntu22`. If that packaged directory is absent but
`ThermalImage/library/ubuntu20/` exists, it falls back automatically unless
`THERMALIMAGE_PACKAGE_ROOT` is set explicitly. You can also select the packaged layout directly:

```bash
SDK_VARIANT=ubuntu20 ./build_demo.sh
SDK_VARIANT=ubuntu22 ./build_demo.sh
```

For `rk3506`, also export `CMAKE_TOOLCHAIN_FILE`, `TOOLCHAIN_PREFIX`, and `SYSROOT` before running
the build script.

## Run

```bash
./run_demo.sh /path/to/sample.irs
./run_demo.sh /path/to/sample.irs open-read --output preview.ppm
```

The repository does not ship a sample thermal file, so pass your own `.irs` or other file supported
by `IRtekFileSDK`. The same `SDK_VARIANT` / `THERMALIMAGE_PACKAGE_ROOT` resolution rules from the
build script apply to `run_demo.sh`.

If `libIRtekFileSDK.so` depends on additional third-party `.so` files (for example matching
`Boost.Thread` or `OpenCV` builds), point `THERMALIMAGE_EXTRA_LIBRARY_PATHS` at the directories that
contain them before building or running.

The default `demo` command still prints the original aggregated walkthrough in sections:

- open and identify the file
- read file/frame metadata and properties
- read images, palettes, audio, and frames
- demonstrate getter/setter round-trips for thermal params, reference, gain, alarm, palette, color distribution, view info, note, tags, and user data

Setter calls are intentionally non-persistent in this demo: the sample does **not** call `ir_file_save()`, so the source file is not written back.

## Command Layout

Usage:

```bash
./run_demo.sh /path/to/sample.irs [command] [command-options]
```

If `command` is omitted, the executable runs the legacy aggregated `demo` flow.

Primary Windows button-chain mappings:

- `open-read` - maps `Open` + the core SDK reads performed by `Image.Open()` and `DoGUIRefresh()`
- `save` - maps `SetCoverImage(...)` + `Save()` / `SaveFJPG()`
- `save-as` - maps `SetCoverImage(...)` + `SaveAs()` / `SaveAsFJPG()`
- `save-frame` - maps `SaveFrameAs()`
- `marker-list`, `marker-get`, `marker-add`, `marker-rename`, `marker-local-params`, `marker-remove`
- `delta-list`, `delta-get`, `delta-add`, `delta-update`, `delta-remove`
- `frame-goto`, `frame-next`, `frame-prev`, `frame-stop`, `play-steps`
- `clip-stream` - maps the clip button chain: `SaveAsStream` -> `GetFrame` -> `AppendFrame` -> `Save`
- `rotate`, `mirror`
- `auto-adjust` - maps the Auto gain button: `GetGain` -> `SetGain(mode=auto)`
- `palette-invert` - maps the palette invert button: `GetPalette` -> `SetPalette`
- `export-marker-temperature`
- `export-marker-curve`
- `export-delta-curve`
- `export-frame-ad`
- `export-frame-temperature`

Examples:

```bash
# Open/read parity with Windows open chain
./run_demo.sh sample.irs open-read --frame-index 3 --output frame3.ppm

# Save the current file after refreshing cover JPEG
./run_demo.sh sample.irs save --cover-file cover.jpg

# Save as FJPG
./run_demo.sh sample.irs save-as --format fjpg --cover-file cover.jpg --output copied.jpg

# Add a spot marker and persist it
./run_demo.sh sample.irs marker-add --type spot --point 100,80 --persist

# Add a rectangle marker with Windows-style auto naming
./run_demo.sh sample.irs marker-add --type rectangle --rect 30,40,120,90 --persist

# Rename or remove a marker
./run_demo.sh sample.irs marker-rename --name Sp1 --to Sp2 --persist
./run_demo.sh sample.irs marker-remove --name Sp2 --persist

# Add a delta between a marker max and a fixed reference
./run_demo.sh sample.irs delta-add --src-name Sp1 --src-type max --dest-temp 293.15 --persist

# Step frames like Previous/Next/Stop and playback ticks
./run_demo.sh sample.irs frame-next
./run_demo.sh sample.irs frame-stop
./run_demo.sh sample.irs play-steps --steps 10 --from-index 0

# Transform the image
./run_demo.sh sample.irs rotate --degrees 90 --persist
./run_demo.sh sample.irs mirror --direction horizontal --persist
./run_demo.sh sample.irs palette-invert --persist

# Export temperatures
./run_demo.sh sample.irs export-marker-temperature --name Ar1 --output area.csv
./run_demo.sh sample.irs export-marker-curve --name Sp1 --output spot_curve.csv
./run_demo.sh sample.irs export-delta-curve --name De1 --output delta_curve.csv
./run_demo.sh sample.irs export-frame-ad --frame-index 5 --output frame_ad.csv
./run_demo.sh sample.irs export-frame-temperature --frame-index 5 --output frame_temp.csv
```

## Mapping Notes

- Windows `Save` / `SaveAs` call `Image.SetCoverImage(BuildImage())` before persisting. The Linux CLI keeps the same SDK call order, but because this demo has no UI renderer or JPEG encoder, the CLI maps that cover refresh to either `--cover-file <jpg>` or a round-trip of the existing embedded cover/visual JPEG.
- Windows `SaveFrameAs` passes a temporary `Bitmap` and ultimately calls `ir_file_save_frame_as`. On Linux the CLI maps that to `--cover-file <jpg>` when available, otherwise it calls `ir_file_save_frame_as(..., nullptr)` and lets the SDK fall back to the original or generated black cover.
- Windows `Pause` only disables the timer and does not invoke a file SDK function. The CLI therefore models playback with `play-steps` plus `frame-next` / `frame-prev` / `frame-stop`, which are the actual SDK-touching parts.
- Marker auto names are mapped from the Windows prefixes: `Sp` (spot), `Ar` (rectangle / ellipse / polygon), `Li` (polyline), `De` (delta).
- `export-marker-temperature` intentionally follows the Windows export logic and queries temperatures with the default measurement parameters (`localParams=false`, `reflTemp=293.15`, `distance=1.0`, `emissivity=1.0`) instead of the marker's own local parameter override.
- Commands such as `marker-add`, `delta-add`, `rotate`, `mirror`, `auto-adjust`, and `palette-invert` are non-persistent unless `--persist` is passed. This matches the Windows workflow where the edit button mutates the open handle first and `Save` is a separate action.

## Legacy Demo Options

These options apply to the default `demo` command:

- `--frame-index <n>` - select a frame before exporting
- `--no-export` - skip writing the preview PPM file
- `--output <path>` - choose the preview PPM path
