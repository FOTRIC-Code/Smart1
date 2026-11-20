# SmartOne SDK (RK3506B)

SmartOne 是基于 Rockchip RK3506B 的完整固件 SDK：Linux 6.1 BSP、U-Boot、Buildroot，
以及设备业务应用（`main_app` / `net_app` / `www`），可以在本地从源码编出整包固件和 OTA 包。

## 开发自己的应用（推荐：Python，免编译固件）

整个流程只有三步：**连接设备 → 把 Python 脚本传上去 → 设备自动拉起运行**；
连接方法、部署命令、验证与回退都在 [docs/user-app.md](docs/user-app.md)。

最省事的方式是把下面这段复制给你的 AI 助手（Cursor / Claude Code / WorkBuddy 等），
它会读同一篇文档，帮你写完程序、部署到设备并验证：

```text
你是我的设备应用开发助手。我要用 Python 给 SmartOne（RK3506B）设备写一个用户程序，全程不编译固件。
先 clone 仓库 https://github.com/FOTRIC-Code/SmartOne（很小，不含 SDK 底包），
通读 docs/user-app.md 并严格按它执行，然后问清我这个程序要实现什么功能。
硬约束：设备上没有 pip；脚本首行必须是 #!/usr/bin/python3；不要执行 ./build.sh。
设备用 USB 连接 POWER 口，地址固定 192.168.123.100（root 口令 root），动手前先 ssh 确认能连上。
写完后部署、重启，用 ssh 验证进程与日志，最后告诉我怎么用。
```

**Node.js 目前不支持**——设备 rootfs 未内置 Node 运行时。

## 编译固件（Ubuntu 22.04 / 24.04）

只在需要改固件本身（kernel / U-Boot / rootfs / `app/*` 源码）时才需要这套环境。约定为 Ubuntu 22.04 或 24.04
（ext4 文件系统，内核 ≥ 4.15，SDK 构建时会自检）；只有 Windows 电脑时，先按
[docs/windows-wsl-setup.md](docs/windows-wsl-setup.md) 配好 WSL2 Ubuntu 再继续（该文档文末附 Windows 专用提示词）。

### 交给 AI 助手（复制即用：配环境）

不想手动执行下面步骤？把这段复制给你的 AI 助手，它会把环境配好：

```text
你是我的开发环境助手。请把这台机器配成能编译 SmartOne（RK3506B）固件 SDK 的环境。
仓库 https://github.com/FOTRIC-Code/SmartOne，细节一律以仓库内文档为准。

先确认系统：Ubuntu 22.04 / 24.04 直接继续；只有 Windows 时先按 docs/windows-wsl-setup.md
配好 WSL2 Ubuntu 22.04；macOS 或其它发行版直接告诉我不可行。然后 clone 仓库到 Linux 家目录
（不要放 /mnt/c 或任何 Windows 盘符下），严格按 README「编译固件」一节的「手动操作」和
「首次构建」执行，依赖细节见 docs/build-host-deps.md。

动手前先告诉我代价（约 1.8GB 下载、1~2 小时编译、磁盘与内存要求），然后直接开始——
我需要中止会打断你。需要 sudo、或需要我本人操作的（BIOS 开虚拟化、管理员权限、重启、
设 Linux 账号密码），先停下来等我。不要修改 buildroot/configs/*defconfig、工具链或 C 库相关配置。

验收：dist/ 下生成 parkia_cube_fw_V*.img 和 parkia_cube_tiny_fw_V*.bin，
把两个文件的完整路径、版本号、大小告诉我；没有生成就是没成功，不要报成功。
```

### 手动操作

```shell
# 基础依赖
sudo apt update
sudo apt install -y git make gcc g++ rsync expect expect-dev \
  flex bison device-tree-compiler u-boot-tools libssl-dev \
  python3 python-is-python3 bc cpio unzip zip file wget curl lz4 \
  pkg-config libncurses-dev

# clone 到 Linux 家目录（不要放在 /mnt/c 或任何 Windows 盘符下）
cd ~ && git clone https://github.com/FOTRIC-Code/SmartOne.git && cd SmartOne
```

完整依赖与分阶段说明见 [docs/build-host-deps.md](docs/build-host-deps.md)。
U-Boot 编译需要 `python2`、内核编译需要 `python3`（且 `python` 指向 python3）；缺失时 SDK 会自动调用
`device/rockchip/common/scripts/install-python*.sh` 安装到用户目录（只需保证 `~/.local/bin` 在 PATH 中）。
编译 www 前端还需要 Node 20 LTS + pnpm（或 npm）：装到 `/opt/nodejs/current` 或用 nvm，脚本都会自动识别。

### 首次构建

需要下载两个包，它们**分别发布在两个 release 下**：

| release | 资产 | 大小 | 内容 |
|---|---|---|---|
| `sdk` | `SmartOne-SDK-v<版本>.tar.xz` | 约 1.2 GB | kernel、u-boot、Buildroot 主体、工具链、Rockchip 官方工具 |
| `dl` | `SmartOne-dl-v<版本>.tar.gz` | 约 620 MB | Buildroot 源码缓存（即"现成的 dl"，含全部源码包） |

**先把源码缓存准备好**：把 dl 包下到 `docs/` 下即可——`build.sh` 校验 sha256 通过后会直接解压到
`buildroot/dl/` 并跳过下载；**否则编译期要由 Buildroot 逐个在线下载源码包，会慢很多**：

```shell
curl -fL --retry 3 -C - -o docs/dl.tar.gz \
  https://github.com/FOTRIC-Code/SmartOne/releases/download/dl/SmartOne-dl-v1.0.0.tar.gz
```

文件名与 sha256 必须与 `build.sh` 中的 `PARKIA_SDK_DL` 一致（版本随 SDK 升级而变）。然后全量构建：

```shell
./build.sh --rebuild     # 首次为全量构建，之后日常用 ./build.sh 增量
```

SDK 底包的下载、校验、解压由 `build.sh` 自动完成，中断后重跑同一命令即可续传
（`./build.sh fetch-sdk` 可单独重取）。源码缓存解压在 `buildroot/dl/`，之后的构建会复用，
缓存里缺少的包由 Buildroot 在线下载；也可以把缓存放到仓库外：`export BR2_DL_DIR=/path/to/dl`。

## 发布内容与 build.sh

| 内容 | 发布方式 |
|---|---|
| 应用源码（`app/`）、文档、示例、Developer Kit、构建脚本、板级配置 | 本 git 仓库（经常更新） |
| SmartOne-SDK 底包（kernel、u-boot、Buildroot 主体、工具链、Rockchip 官方工具） | [Releases](https://github.com/FOTRIC-Code/SmartOne/releases) 的 `sdk` 标签，资产 `SmartOne-SDK-v<X.Y.Z>.tar.xz` |
| Buildroot 源码缓存（"现成的 dl"） | [Releases](https://github.com/FOTRIC-Code/SmartOne/releases) 的 `dl` 标签，资产 `SmartOne-dl-v<X.Y.Z>.tar.gz` |
| 编译好的稳定固件（整包 img + tiny OTA 包） | 尚未在 Releases 发布；当前请自行构建（产物在 `dist/`） |

| 命令 | 用途 |
|---|---|
| `./build.sh` | 增量构建：只重编 `app/*` + www 前端，重打包 rootfs / 固件 |
| `./build.sh --rebuild` | 全量构建：整编 u-boot / kernel / Buildroot rootfs / app（克隆后必须先跑一次） |
| `./build.sh clean` | 清理所有构建产物（从零构建：`clean && --rebuild`） |
| `./build.sh publish` | 全量构建 + 签名 + sha256 清单（额外产出 `sha256_V<版本>.txt`） |
| `./build.sh fetch-sdk` | 重新下载、校验并解压 SmartOne-SDK 底包（缺失或版本不对时其它命令会自动执行） |
| `./build.sh --help` | 完整参数：版本号 `-v`、启动介质 `--boot-medium`、OTA 签名私钥 `-k`、`--rebuild-third-party` 等 |

产物：`dist/parkia_cube_fw_V<版本>.img`（整包固件，烧录用）与 `dist/parkia_cube_tiny_fw_V<版本>.bin`
（tiny OTA 包：main_app + net_app + `lib/` + www + data.json + 可选签名）。
项目模式之外的参数原样透传给 Rockchip SDK 构建脚本：`./build.sh help`、`kernel`、`menuconfig`、`shell`、`cleanall`。

- `build.sh` 开头的 `PARKIA_SDK_VERSION` 指定当前代码对应的 SDK 版本。`git pull` 后如果 SDK 版本变了，
  下次执行 `build.sh` 会自动删除旧版 SDK 文件并换成新版，不会动仓库里的文件。
- SDK 解压出的目录（如 `kernel-6.1/`、`buildroot/` 的大部分内容）已写进 `.gitignore`，
  `git status` 不会显示。如果要把这些目录下的某个文件纳入自己的版本管理，用 `git add -f <文件>`。
  SDK 升级时会覆盖这些文件，升级后用 `git diff` 检查，需要时 `git checkout -- <文件>` 恢复自己的改动。

## 安全须知（量产前必读）

- 固件默认账号口令仅用于开发（系统 root 口令为 `root`，见
  `buildroot/configs/rockchip_rk3506_board_defconfig` 的 `BR2_TARGET_GENERIC_ROOT_PASSWD`；
  Web / HTTP 管理账号默认为 `admin/admin`），**量产前必须修改**。
- SDK 中的 AVB、TEE、安全启动相关密钥（如 `tools/linux/Linux_SecurityAVB/avb_keys/`、
  `external/security/rk_tee_user/**/keys/`）均为 Rockchip 公开的测试密钥，**不能用于量产**，
  请自行生成并妥善保管。
- `./build.sh -k` 使用的 OTA 签名私钥请勿提交到仓库。

## 许可证与第三方组件

本仓库的自有代码以 Apache License 2.0 发布（见 [LICENSE](LICENSE)）。第三方组件遵循各自的许可证：

- `app/net_app/libs/zlm/`：ZLMediaKit，MIT 许可；
- `app/net_app/libs/irtek/`：fotric.cn 专有二进制 SDK，仅随本 SDK 分发；
- 其余组件（Linux、U-Boot、Buildroot 及其软件包、Rockchip BSP）遵循各自目录中的许可证。

更多说明见 [AGENTS.md](AGENTS.md) 与 [docs/](docs/)。

---

# Rockchip Linux SDK

Rockchip Linux SDK for the Rockchip SOC boards
  - wiki <http://opensource.rock-chips.com/wiki_Main_Page>.

## Quick Start

1. Check supported targets:
```shell
   ~ $ make help
```
2. Choose SDK's defconfig:
```shell
   ~ $ make rockchip_defconfig
```
3. Change SDK's configs:
```shell
   ~ $ make menuconfig
   ~ $ make savedefconfig
```
4. Run "make" to build the images, logs saved at "output/log/"
5. Flash the generated "output/firmware/update.img" to your device
6. Boot your device and enjoy it
