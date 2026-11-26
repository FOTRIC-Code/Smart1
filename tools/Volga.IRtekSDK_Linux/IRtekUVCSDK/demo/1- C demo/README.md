# IRtekUVCSDK Linux C++ Demo

This Linux CLI demo now mirrors the main Windows `MainForm.cs` button flows instead of folding most SDK calls into one `exercise_safe_controls` path.

## Module Structure

The demo is split into focused modules under `IRtekUVCSDKDemo/`:

| File | Description |
|------|-------------|
| `main.cpp` | Entry point: parse CLI args, print help, dispatch to module functions |
| `uvc_demo_common.h/cpp` | Shared types (`DemoOptions`, `DemoCommand`), parse/print helpers, RAII wrappers (`ScopedSdk`, `ScopedLogin`), query helpers, `with_logged_in_device` |
| `uvc_demo_connection.h/cpp` | `refresh-devices`, `login` — device discovery and login chain |
| `uvc_demo_stream_info.h/cpp` | `frame-list` — stream frame information query |
| `uvc_demo_params.h/cpp` | `thermal-get/set`, `shutter-mode-get/set`, `shutter-time-get/set`, `range-list/get/set`, `display-get/set` |
| `uvc_demo_capture.h/cpp` | `capture` — single-frame capture and IR file save chain (IRtekFileSDK) |
| `uvc_demo_live_stream.h/cpp` | Frame streaming infrastructure: `wait_for_frame`, `capture_single_frame`, temperature display |
| `uvc_demo_device.h/cpp` | `calibrate`, `upgrade`, `restart`, `reset` — device management |

Support libraries under `IRtekUVCSDK/` (unchanged):

| File | Description |
|------|-------------|
| `irtek_file_sdk_loader.h/cpp` | Runtime loader for `libIRtekFileSDK.so` |
| `uvc_frame_utils.h` | UVC frame payload parser and LUT temperature interpolation |

## Build

```bash
SDK_VARIANT=ubuntu22 ./build_demo.sh
```

For `rk3506`, also export `CMAKE_TOOLCHAIN_FILE`, `TOOLCHAIN_PREFIX`, and `SYSROOT` before running the build script.

## Run

```bash
SDK_VARIANT=ubuntu22 ./run_demo.sh <command> [options]
```

The CLI is stateless: every subcommand initializes the SDK, refreshes devices, logs into the selected device, runs one Windows-like flow, then logs out.

## Windows Button Flow Mapping

- `refresh-devices`
  - Mirrors `tsbRefreshDevices_Click`
  - Core SDK call: `uvc_get_uvc_list`

- `login`
  - Mirrors `tsbLogin_Click`
  - Core order: `uvc_login_by_path`/`uvc_login` -> `LoadProperties` -> `LoadThermalParams` -> `LoadFrameList` -> `LoadRanges` -> `UpdateRendererParams` -> stream smoke test

- `thermal-get`
  - Mirrors the thermal-parameter load path
  - Core SDK call: `uvc_get_thermal_params`

- `thermal-set --emissivity ... --refl-temp ... --distance ... --atm-temp ... --rel-humidity ...`
  - Mirrors `ApplyThermalParams`
  - Core SDK call: `uvc_set_thermal_params`

- `shutter-mode-get`
  - Core SDK call: `uvc_get_thermal_shutter_mode`

- `shutter-mode-set --mode automatic|manual`
  - Mirrors `cbShutterMode_SelectedIndexChanged`
  - Core SDK call: `uvc_set_thermal_shutter_mode`

- `shutter-time-get`
  - Core SDK call: `uvc_get_thermal_shutter_time`

- `shutter-time-set --minutes <n>`
  - Mirrors `nudShutterTime_ValueChanged`
  - Core SDK call: `uvc_set_thermal_shutter_time`

- `frame-list`
  - Mirrors `LoadFrameList`
  - Core SDK call: `uvc_get_frame_list`

- `range-list`
  - Mirrors the range-list population part of `LoadRanges`
  - Core SDK call: `uvc_get_lens_info`

- `range-get`
  - Mirrors `LoadRanges`
  - Core order: `uvc_get_lens_info` -> `uvc_get_thermal_range`

- `range-set --range-id <n>`
  - Mirrors `tscbFileFormat_SelectedIndexChanged`
  - Core SDK call: `uvc_set_thermal_range`

- `display-get`
  - Mirrors `btnDisplayGet_Click`
  - Core SDK call: `uvc_get_image_params`

- `display-set --contrast ... --brightness ... --dde-enable ... --dde-value ...`
  - Mirrors `btnDisplaySet_Click`
  - Core SDK call: `uvc_set_image_params`

- `capture --output <path> [--preview-output <ppm-path>]`
  - Mirrors `tsbCapture_Click` plus `SaveThermalImageAsIrFile`
  - Core order: grab frame -> `ir_file_create` -> `ir_file_set_thermal_size` -> `ir_file_set_isp_type` -> `ir_file_set_color_dist` -> LUT/range/camera/lens/thermal params -> palette/alarm/gain -> `ir_file_append_frame` -> `ir_file_save`

- `calibrate`
  - Mirrors `tsbCalibrate_Click`
  - Core SDK call: `uvc_thermal_calibrate`

- `upgrade --firmware <path>`
  - Mirrors `btnUpgrade_Click`
  - Core order: `uvc_device_upgrade` -> repeated `uvc_get_upgrade_state`

- `restart`
  - Mirrors `btnDeviceRestart_Click`
  - Core SDK call: `uvc_device_restart`

- `reset`
  - Mirrors `btnDeviceReset_Click`
  - Core SDK call: `uvc_device_reset`

## Examples

```bash
SDK_VARIANT=ubuntu22 ./run_demo.sh refresh-devices
SDK_VARIANT=ubuntu22 ./run_demo.sh login --device-index 0
SDK_VARIANT=ubuntu22 ./run_demo.sh thermal-set --device-index 0 \
  --emissivity 0.98 --refl-temp 293.15 --distance 1.0 --atm-temp 293.15 --rel-humidity 0.50
SDK_VARIANT=ubuntu22 ./run_demo.sh display-set --device-index 0 \
  --contrast 50 --brightness 50 --dde-enable 0 --dde-value 50
SDK_VARIANT=ubuntu22 ./run_demo.sh capture --device-index 0 \
  --output thermal_capture.jpg --preview-output thermal_preview.ppm
SDK_VARIANT=ubuntu22 ./run_demo.sh upgrade --device-index 0 --firmware /path/to/fw.bin
```

## Intentional Gaps

- Windows `LoadImageFlip()` is GUI-side bitmap state only; it does not issue `uvc_get_image_flip` or `uvc_set_image_flip`, so no fake "matching button" command was added for it.
- `capture` omits `ir_file_set_cover_image`, because the Windows demo sources that JPEG cover from the live GUI bitmap.
- Marker drawing, palette toggles, and alarm visualization remain GUI-specific and are not translated into standalone CLI commands here.
- Linux SDK still exposes extra APIs such as `uvc_get/set_image_flip`, `uvc_get/set_thermal_sr`, and `uvc_set_thermal_dpc`, but they are not part of the Windows `MainForm.cs` button chain targeted by this demo refresh.
