---
name: device-app-dev
description: Build and deploy a user ELF for Parkia.Cube-class devices with the Developer Kit scripts. Use when the user asks to create a device app, compile a user process with the Buildroot SDK, deploy an ELF to `/usr/bin`, update `app_bin`, or install a custom app onto the board.
disable-model-invocation: true
---

# Device App Dev

Use this skill when developing a user-space device app that will be started by `main_app`.

## Workflow

Copy this checklist and keep it updated:

```text
Task Progress:
- [ ] 1. Confirm the board is reachable (USB direct connection, fixed address 192.168.123.100) and the target app name
- [ ] 2. Build the app with the Buildroot SDK (skip for Python scripts — no compilation needed)
- [ ] 3. Install it with the Developer Kit script
- [ ] 4. Reboot and verify the process/logs
```

## Rules

- Build with the Buildroot SDK, not the host compiler.
- Use `bash tools/developer-kit/build_user_app.sh` to compile.
- Use `bash tools/developer-kit/install_user_app.sh` to deploy.
- Do not edit `/etc/cfg/main_app/config.json` by hand unless the user explicitly asks.
- Default install mode is a custom path under `/usr/bin/<elf-name>`, then update `app_bin`.
- Only use `/usr/bin/net_app` when the user explicitly wants the compatibility path.

## Build

If the user did not provide a source directory, default to `samples/user_app/`.

```sh
bash tools/developer-kit/build_user_app.sh \
  --sdk-root /path/to/arm-buildroot-linux-gnueabihf_sdk-buildroot
```

If `BUILDROOT_SDK_ROOT` is already exported:

```sh
bash tools/developer-kit/build_user_app.sh
```

## Install

The file can be a cross-compiled ELF or an executable script (e.g. Python with a
`#!/usr/bin/python3` shebang) — the install script handles both. Custom path mode:

```sh
bash tools/developer-kit/install_user_app.sh 192.168.123.100 ./path/to/user_app
```

Compatibility mode:

```sh
bash tools/developer-kit/install_user_app.sh 192.168.123.100 ./path/to/user_app net_app
```

## Verify

After reboot, verify with SSH:

```sh
ssh root@192.168.123.100 ps | grep <process_name>
ssh root@192.168.123.100 cat /tmp/user_app.log
```

## Connect to the board

Use the USB direct connection: plug the board's `POWER` port into the PC; the board
comes up as a USB RNDIS network adapter with the fixed address `192.168.123.100`.
Use `ssh root@192.168.123.100` (password `root`) as the connection and debugging tool.

Do not point the user at `ipconfig.exe` or other device-search tools — the project does
not ship them.

## References

- Deployment guide: [`docs/user-app.md`](../../../../docs/user-app.md)
- Developer Kit overview: [`tools/developer-kit/README.md`](../../README.md)
- Sample app: [`samples/user_app/README.md`](../../../../samples/user_app/README.md)
