# Repository Map

本仓库是 SmartOne（RK3506B）的完整固件 SDK，包含芯片 BSP（Linux 6.1 内核、U-Boot、Buildroot）与业务代码（`app/` 应用、buildroot 包、rootfs overlay、docs）。

## Top-Level Rules

- 固件构建/打包/OTA 统一入口：`./build.sh`。默认增量（只重编 `app/*` + www，重打包固件）；`--rebuild` 全量（不删产物，各构建系统增量判断，app 包强制重装；从零构建先 `clean`）；`clean` 清理产物；`publish` 发布打包（全量 + 签名 + sha256 清单）。产物均输出到 `dist/`：整包 `parkia_cube_fw_V*.img`、tiny OTA `parkia_cube_tiny_fw_V*.bin`；支持 `-k` 签名。
- SDK 高级目标（kernel/uboot/menuconfig/shell/cleanall 等）直接透传：`./build.sh kernel`、`./build.sh menuconfig`、`./build.sh help`（查看全部 SDK 目标）。
- Buildroot 对 `app/*` 这些 local 源码包用 stamp 记录进度，普通 make 感知不到源码改动；`./build.sh` 默认模式已显式 `<pkg>-rebuild`，若手动操作 buildroot 须自行 `make <pkg>-rebuild`。
- kernel / u-boot / Buildroot 主体 / 工具链 / Rockchip 官方工具属于 SmartOne-SDK 底包，不在 git 里：`build.sh` 按开头 `PARKIA_SDK_*` 变量（导出时生成，勿手改）自动从 GitHub release 下载、sha256 校验后解压（`./build.sh fetch-sdk` 手动重取）。这些目录已在 `.gitignore` 中，要跟踪其中的文件需 `git add -f`；`.smartone_sdk/MANIFEST` 列出全部 SDK 文件。
- SDK 底包与 dl 包分别发布在两个 release 标签下：底包在 `sdk` 标签（`PARKIA_SDK_URL`），dl 包在 `dl` 标签（`PARKIA_SDK_DL_URL`）；`SMARTONE_SDK_URL` / `SMARTONE_SDK_DL_URL` 环境变量可分别覆盖两个下载地址。修改 `build.sh` 时保持两个标签与变量一一对应。
- `buildroot/dl/` 为空时先用 dl 包（Release `dl` 标签下的 `SmartOne-dl-v<X.Y.Z>.tar.gz`，下载到 `docs/dl.tar.gz` 后再解压；sha256 与 `PARKIA_SDK_DL` 一致时 `build.sh` 会跳过下载），缺的包再由 Buildroot 在线下载。
- 新增 `app/net_app` 的 `.c` / `.cpp` 文件必须加进 `app/net_app/CMakeLists.txt` 的 `NET_APP_SOURCES`。
- 用户程序有两条路线，对外说明在 `docs/user-app.md`：**Python 脚本**（设备固件已内置 Python 3，`main_app` 用 `execl()` 拉起 `app_bin`、由内核解析 shebang，因此带 `#!/usr/bin/python3` 的脚本可直接当用户程序，无需编译、也不需要 Buildroot 工具链；经 `tools/developer-kit/python/irtek_thermal.py` 绑定（cffi/ctypes 直调预置的 `libIRtekUVCSDK.so`）**可直采热像温度**，示例在 `samples/thermal_logger/`，ABI 契约见 `docs/irtek-uvc-sdk-abi.md`）与**交叉编译 ELF**（C/C++，Developer Kit 位于 `tools/developer-kit/`，最小示例在 `samples/user_app/`）。两条路线都由脚本部署到 `/usr/bin/<name>`、更新 `app_bin` 后重启。Windows 上部署用 `tools/developer-kit/windows/*.cmd`（原生，无需 Git Bash/WSL）；脚本文件行尾必须 LF（`.gitattributes` 已强制）。
- 设备端**没有 Node.js 运行时**（defconfig 未启用 `nodejs`，启用需改 `rockchip_rk3506_board_defconfig` 并重编固件），也没有 `pip`；Python 只能用标准库加固件已内置的模块。
- zlm（ZLMediaKit）为预编译依赖：`app/net_app/libs/zlm/`（`libmk_api.so` + `include/` 头文件），详见该目录 README。安装到 **`/usr/lib/net_app/`**（app 私有），不要进系统 `/usr/lib`；irtek 同理（`app/net_app/libs/irtek/lib/`）。OTA 后在 `/userdata/parkia/app/lib/`（RPATH 优先找这里，不改 S14）。
- 多 app 共享的 libs 只放 `app/shared/libs/`（json/log/mongoose/file/platform/time/network/upgrade），各 app 的 `libs/` 只保留私有库（如 net_app 的 irtek/minih264/provisioning/zlm）；引用统一走 `APP_SHARED_ROOT`（CMake）与 `APP_DEFAULT_LOG_PATH`（日志路径）。

## Where To Look

| Task | Location | Notes |
|---|---|---|
| 设备后端 | `app/net_app/` | mongoose HTTP:80、UVC 热像、Wi-Fi/BLE 配网 |
| 入口守护 | `app/main_app/` | BLE 广播/GATT、配网、拉起 `app_bin`（默认 net_app） |
| 用户程序开发 | `tools/developer-kit/` | Buildroot SDK 脚本、部署脚本、Skill |
| 用户程序示例 | `samples/user_app/` | 最小可部署 ELF 示例（C） |
| 热像 Python 绑定 | `tools/developer-kit/python/irtek_thermal.py` | cffi/ctypes 直采热像；示例 `samples/thermal_logger/`，契约 `docs/irtek-uvc-sdk-abi.md` |
| 厂商热像 SDK 参考 | `tools/Volga.IRtekSDK_Linux/` | fotric 交付包（UVC/Net/File SDK 头文件+demo；文本入库、`.so` 不入库），来源/指纹/许可状态见其 MANIFEST.md |
| Web 前端 | `app/www/` | Vue 3 SPA，产出装到 /var/www |
| HTTP API | `app/net_app/ARCHITECTURE.md`（§7.8–7.10）、`app/net_app/ALARM.md` | 接口表 + 告警实现 |
| 产品架构 | `docs/architecture.md` | 进程划分、端口、启动流程 |
| 用户程序说明 | `docs/user-app.md` | USB 连接、Python 免编译路线、交叉编译、切换 `app_bin` |
| 编译宿主依赖 | `docs/build-host-deps.md` | Ubuntu apt 一键安装与分阶段说明 |
| Windows 开发环境 | `docs/windows-wsl-setup.md` | WSL2 Ubuntu 22.04 搭建、Windows/WSL 分工、AI 提示词 |
| Buildroot 包 | `buildroot/package/*_app/` | 自研包的编译 / 安装集成 |
| rootfs 定制 | `device/rockchip/common/overlays/rootfs/` | custom、rk3506-board、rk3506-config |

## Anti-Patterns

- 不要在 `http_api_*.c` 和 `config_manager.c` 之外调用 `config_get_*` / `config_set_*`。
- 不要绕过 `g_config` 在业务模块里直接读运行时配置。
- 不要把 vendored 库（mongoose、zlm、irtek）当作可自行改造的一方代码。
