# Developer Kit

用于用户程序开发的宿主机脚本与 Skill。

## 目录

```text
tools/developer-kit/
  build_user_app.sh
  install_user_app.sh
  update_app_bin.sh
  device/
  linux/
  mac/
  windows/
  skills/
```

## 核心脚本

- `build_user_app.sh`：调用 Buildroot SDK 交叉编译用户程序（仅 C/C++ 需要；Python 脚本免编译）
- `install_user_app.sh`：上传可执行文件（交叉编译的 ELF，或带 `#!/usr/bin/python3` shebang 的 Python 脚本）、更新 `app_bin`、重启设备
- `update_app_bin.sh`：只更新 `app_bin`
- `device/set_app_bin.py`：设备侧载荷，三个部署脚本（`update_app_bin.sh` 与 Windows 两个 `.ps1`）共用的**唯一**远端逻辑副本——经 ssh 管道到设备 `python3 -` 执行，原子改写 `app_bin`。修改只动这一个文件，勿在任何部署脚本里另存副本

## 默认行为

`install_user_app.sh` 默认采用“自定义路径模式”：

- 本地 `user_app` 默认上传到 `/usr/bin/user_app`
- 脚本同步更新 `/etc/cfg/main_app/config.json` 的 `app_bin`
- 然后重启设备

如需兼容旧流程，可把远端名指定为 `net_app`。

## 平台入口

- Linux: `linux/*.sh`
- macOS: `mac/*.command`
- Windows: `windows/*.cmd` / `windows/*.ps1`

> Windows 的 `install_user_app` / `update_app_bin` 为**原生实现**（PowerShell + 系统自带
> OpenSSH），无需 Git Bash / WSL / Python；`build_user_app`（仅 C/C++ 需要）仍须在
> WSL2 + Buildroot SDK 中执行，见 `docs/windows-wsl-setup.md`。

## 详细文档

见 [`docs/user-app.md`](../../docs/user-app.md)。
