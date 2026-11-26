# IRtekNetSDK Linux C++ Demo

This Linux CLI demo mirrors the main Windows `2- C# demo` button chains by exposing them as independent commands.

## Structure

The demo is split into functional modules under `IRtekNetSDK/`:

| File | Description |
|------|-------------|
| `net_demo_common.h/cpp` | Shared utilities: parse helpers, DemoSession, data-fetch helpers, overlay patch helpers, enum converters |
| `net_demo_connection.h/cpp` | **连接** – device search, connect-flow, properties-get, device restart/reset/upgrade/rename |
| `net_demo_stream_info.h/cpp` | **流信息** – channel/stream query and display |
| `net_demo_params.h/cpp` | **参数读写** – thermal params get/set, thermal OSD, time/NTP, network/FTP/port/webhook, users, display/channel, markers |
| `net_demo_capture.h/cpp` | **拍照** – thermal snapshot capture |
| `net_demo_record.h/cpp` | **录制** – video stream recording |
| `net_demo_live_stream.h/cpp` | **实时流和温度** – thermal data stream, thermal LUT |
| `net_demo_alarm.h/cpp` | **报警** – alarm get/set/listen |

Entry point: `IRtekNetSDKDemo/main.cpp` – CLI argument parsing, help, and command dispatch to module functions.

## Build

```bash
SDK_VARIANT=ubuntu22 ./build_demo.sh
```

For `rk3506`, also export `CMAKE_TOOLCHAIN_FILE`, `TOOLCHAIN_PREFIX`, and `SYSROOT` before running the build script.

The build expects `IRtekNetSDK/library/<variant>/library/libIRtekNetSDK.so` to exist.

## Command model

```bash
SDK_VARIANT=ubuntu22 ./run_demo.sh <command> [options]
```

Common login options:

```bash
--ip <address> --port <port> --user <name> --password <pwd>
```

## Windows mapping

- `Search` -> `search`
- `Connect` startup chain -> `connect-flow`
- `Save` -> `capture --output <file> [--capture-mode irtek|pure]`
- `Record` -> `record-stream --output <file>`
- `Get Thermal Data` -> `thermal-data-get`
- `Get Thermal LUT` -> `thermal-lut-get`
- `Calibrate` -> `thermal-calibrate`
- `Properties Get` -> `properties-get`
- `Thermal Params Get/Set` -> `thermal-params-get` / `thermal-params-set`
- `Thermal OSD Get/Set` -> `thermal-osd-get` / `thermal-osd-set`
- `Get Time / Sync Time / Apply Time / NTP Apply` -> `time-get` / `time-sync` / `time-apply` / `ntp-apply`
- `Net Refresh / Net Apply / FTP Apply / FTP Test / Port Apply / Webhook Get / Webhook Apply` -> `network-refresh` / `network-apply` / `ftp-apply` / `ftp-test` / `port-apply` / `webhook-get` / `webhook-apply`
- `User Refresh / Add / Update / Delete` -> `users-get` / `user-add` / `user-update` / `user-delete`
- `Display Get / Display Set / Set Channel Name` -> `display-get` / `display-set` / `channel-set`
- `Set Device Name` -> `device-name-set`
- `Get Rules / Set Rules / Alarm Listener Start` -> `alarm-get` / `alarm-set` / `alarm-listen`
- Marker toolbar (`Spot`, `Rectangle`, `Line`, `Delete`) -> `marker-add-spot`, `marker-add-rectangle`, `marker-add-line`, `marker-delete`
- `Restart / Reset / Upgrade` -> `device-restart`, `device-reset`, `device-upgrade`

`Select` is not a separate CLI command because it is only a GUI edit mode toggle and does not call a core SDK API.

## Examples

Search:

```bash
SDK_VARIANT=ubuntu22 ./run_demo.sh search
```

Run the Windows connect sequence on Linux, including thermal and video stream probes:

```bash
SDK_VARIANT=ubuntu22 ./run_demo.sh connect-flow --ip 192.168.10.177
```

Read properties:

```bash
SDK_VARIANT=ubuntu22 ./run_demo.sh properties-get --ip 192.168.10.177
```

Read thermal data and LUT:

```bash
SDK_VARIANT=ubuntu22 ./run_demo.sh thermal-data-get --ip 192.168.10.177
SDK_VARIANT=ubuntu22 ./run_demo.sh thermal-lut-get --ip 192.168.10.177
```

Apply thermal parameters:

```bash
SDK_VARIANT=ubuntu22 ./run_demo.sh thermal-params-set \
  --ip 192.168.10.177 \
  --emissivity 0.95 \
  --distance 3.0 \
  --refl-temp 23.0 \
  --shutter-mode automatic \
  --shutter-time 10 \
  --temp-unit c \
  --distance-unit m
```

Update time and NTP:

```bash
SDK_VARIANT=ubuntu22 ./run_demo.sh time-sync --ip 192.168.10.177
SDK_VARIANT=ubuntu22 ./run_demo.sh ntp-apply --ip 192.168.10.177 --ntp-host pool.ntp.org --ntp-port 123 --ntp-interval 60
```

Read network and apply webhook:

```bash
SDK_VARIANT=ubuntu22 ./run_demo.sh network-refresh --ip 192.168.10.177
SDK_VARIANT=ubuntu22 ./run_demo.sh webhook-apply --ip 192.168.10.177 --webhook-url http://host:8080/alarm
```

Add and delete markers:

```bash
SDK_VARIANT=ubuntu22 ./run_demo.sh marker-add-spot --ip 192.168.10.177 --x 100 --y 80
SDK_VARIANT=ubuntu22 ./run_demo.sh marker-add-rectangle --ip 192.168.10.177 --x 20 --y 20 --x2 120 --y2 100
SDK_VARIANT=ubuntu22 ./run_demo.sh marker-delete --ip 192.168.10.177 --marker-name S01
```

Read and update alarm rules:

```bash
SDK_VARIANT=ubuntu22 ./run_demo.sh alarm-get --ip 192.168.10.177
SDK_VARIANT=ubuntu22 ./run_demo.sh alarm-set \
  --ip 192.168.10.177 \
  --marker-name S01 \
  --alarm-enabled true \
  --alarm-type highest-greater \
  --alarm-temp 60 \
  --warning-temp 45 \
  --alert-temp 35 \
  --alarm-interval 5 \
  --alarm-trigger-ftp true \
  --alarm-trigger-webhook true
```

## Compatibility notes

- Windows `warning_temp` / `warning_delay_time` map to Linux header fields `threshold_temp` / `threshold_delay_time`.
- Windows `alert_temp` / `alert_delay_time` map to Linux header fields `trigger_temp` / `trigger_delay_time`.
- Windows line markers are normalized to Linux `IR_FILE_POLYLINE`.
- Legacy no-command usage still works:
  - no `--ip` -> `search`
  - `--ip` only -> `properties-get`
  - `--thermal-snapshot` -> `thermal-data-get`
  - `--listen-alarm` -> `alarm-listen`
