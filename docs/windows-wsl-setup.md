# Windows 开发环境（WSL2）

SDK 只能在 Linux（Ubuntu 22.04 / 24.04）下编译。只有 Windows 电脑时，先用 WSL2 装一个 Ubuntu 22.04，
**配好环境后再 clone 仓库**。之后编译在 WSL 里做，连板用 USB（设备 `POWER` 口），烧录在 Windows 上做。

可以手动按本文操作，也可以把文末的提示词交给 AI 工具（如 Cursor）代为执行；
如果你是在 Linux 上搭环境（不需要 WSL），用 [README](../README.md) 里的通用提示词。

## 0. 必须由你本人完成的步骤

下面几步 AI 无法代劳，遇到时请手动完成：

- **开启 CPU 虚拟化**：任务管理器 → 性能 → CPU，确认"虚拟化：已启用"；未启用需进 BIOS 打开（Intel VT-x / AMD SVM）。
- **管理员权限**：安装 WSL 需要"以管理员身份运行"PowerShell。
- **重启电脑**：WSL 安装完成后按提示重启。
- **设置 Linux 账号**：第一次打开 Ubuntu 时输入新的用户名和密码（与 Windows 账号无关，`sudo` 时要用）。

## 1. 环境要求

| 项目 | 要求 |
|---|---|
| 系统 | Windows 10 22H2 或 Windows 11 |
| WSL | WSL2 + Ubuntu 22.04 |
| 磁盘 | 系统盘剩余 ≥ 100 GB（首次全量构建产物很大） |
| 内存 | 建议 16 GB，给 WSL 分配 ≥ 8 GB |
| 网络 | 首次构建需访问外网，下载约 1.8 GB（SDK 底包 1.2 GB + 源码缓存 620 MB） |

## 2. 安装 WSL2 和 Ubuntu 22.04

以管理员身份打开 PowerShell：

```powershell
wsl --install -d Ubuntu-22.04
```

重启电脑后，从开始菜单打开"Ubuntu 22.04"，设置用户名和密码。确认版本为 WSL2：

```powershell
wsl -l -v      # VERSION 一列应为 2
```

已装过旧的 WSL1 发行版时，执行 `wsl --set-version Ubuntu-22.04 2`。

## 3. 给 WSL 分配资源

在 Windows 用户目录新建或编辑 `%UserProfile%\.wslconfig`：

```ini
[wsl2]
memory=8GB
processors=8
swap=8GB
```

保存后在 PowerShell 执行 `wsl --shutdown`，再重新打开 Ubuntu 生效。内存更大的电脑可以把 `memory` 调高，编译会更快。

## 4. 在 Ubuntu 里安装编译依赖

以下命令都在 **Ubuntu 终端** 中执行。

```shell
sudo apt update
sudo apt install -y git make gcc g++ rsync expect expect-dev \
  flex bison device-tree-compiler u-boot-tools libssl-dev \
  python3 python-is-python3 bc cpio unzip zip file wget curl lz4 \
  pkg-config libncurses-dev
```

再安装 Node.js 20 和 pnpm（编译 Web 前端用），方法见 [build-host-deps.md](build-host-deps.md) 的"Node.js / pnpm"一节。

## 5. clone 仓库、预置源码缓存并首次构建

```shell
cd ~
git clone https://github.com/FOTRIC-Code/SmartOne.git
cd SmartOne

# 1) 先把 Buildroot 源码缓存下好（约 620MB），
#    否则编译期要由 Buildroot 逐个在线下载源码包，会慢很多
curl -fL --retry 3 -C - -o docs/dl.tar.gz \
  https://github.com/FOTRIC-Code/SmartOne/releases/download/dl/SmartOne-dl-v1.0.0.tar.gz

# 2) 全量构建（SDK 底包约 1.2GB，build.sh 会自动下载、校验、解压）
./build.sh --rebuild
```

首次构建共需下载约 1.8GB（SDK 底包约 1.2GB + 源码缓存约 620MB），校验后解压到仓库目录；
下载中断时重新执行同一命令即可续传。

首次全量构建需要较长时间（通常 1～2 小时，取决于网络和机器性能）。完成后 `dist/` 下会生成：

- `parkia_cube_fw_V<版本>.img`：整包固件
- `parkia_cube_tiny_fw_V<版本>.bin`：OTA 升级包

> **仓库必须放在 WSL 自己的目录（如 `~/SmartOne`），不要放在 `/mnt/c/...` 或其它 Windows 盘符下。**
> 放在 Windows 盘上编译会非常慢，并且会因为文件权限、大小写、符号链接问题而失败。

## 6. Windows 与 WSL 之间互相访问文件

- Windows 资源管理器访问 WSL 里的文件：`\\wsl$\Ubuntu-22.04\home\<Linux用户名>\SmartOne`
- 在 Ubuntu 终端执行 `explorer.exe .` 可直接用资源管理器打开当前目录
- 用 Cursor / VS Code 打开工程：在 Ubuntu 终端进入 `~/SmartOne` 后执行 `cursor .`（或 `code .`），编辑器会以 WSL 模式打开

## 7. 日常开发：哪一步在哪一侧做

| 步骤 | 在哪做 | 怎么做 |
|---|---|---|
| 编译固件 | WSL | `./build.sh`（改了代码后增量构建） |
| 编译自己的应用（C/C++） | WSL | `tools/developer-kit/build_user_app.sh`，见 [user-app.md](user-app.md) |
| 连接设备 | Windows / WSL | USB 线接设备 `POWER` 口，WSL 内 `ssh root@192.168.123.100` |
| 下载应用到板子 | WSL | `tools/developer-kit/install_user_app.sh <板子IP> <本地文件>`（ELF 或 Python 脚本都行） |
| 烧录整包固件 | Windows | RKDevTool，选择 `\\wsl$\...\SmartOne\dist\` 下的 `.img` |

> **写 Python 用户程序不需要 WSL，也不需要编译**：设备固件已内置 Python 3，脚本带
> `#!/usr/bin/python3` 上传即可运行，见 [user-app.md](user-app.md) 的「路线 A」。
> 只有 C / C++ 用户程序才需要上面这套跨平台编译环境。

说明：

- 连板用 USB：把设备的 `POWER` 口接到电脑，Windows 会多出一个 USB 网卡（RNDIS），
  设备地址固定为 `192.168.123.100`，WSL 内一般可直接 `ssh root@192.168.123.100`。
  若不通，先在 Windows 上用 `ipconfig` 确认该网卡已获取到地址。
- 不需要额外的设备搜索工具，也不需要手动配置 IP 或局域网。
- 烧录工具和 USB 驱动在仓库 `tools/windows/` 下（首次构建时随 SDK 下载）：先解压安装 `DriverAssitant_v5.13.zip`（Rockchip USB 驱动），
  再使用 `tools/windows/RKDevTool/` 中的 RKDevTool 烧录。可以直接从 `\\wsl$\...` 路径复制这两个目录到 Windows 本地使用。

## 8. 常见问题

| 现象 | 处理 |
|---|---|
| `wsl --install` 报"虚拟机平台未启用"或 0x80370102 | BIOS 中开启 CPU 虚拟化，并在"启用或关闭 Windows 功能"里勾选"虚拟机平台" |
| 构建时被系统杀掉（`Killed`）或卡死 | WSL 内存不足：调大 `.wslconfig` 的 `memory` / `swap` 后 `wsl --shutdown` |
| 构建非常慢、权限报错、符号链接报错 | 仓库放在了 `/mnt/c`：移到 `~/` 下重新 clone |
| 下载源码包失败 | 检查网络或代理；重新执行 `./build.sh --rebuild` 会从中断处继续 |
| 构建报 SDK 底包或 dl 包下载失败 / sha256 mismatch | 多为网络中断或文件损坏：重跑 `./build.sh --rebuild` 会续传并重新校验；仍失败可删除 `.smartone_sdk/cache/` 下对应文件再重试 |
| 系统盘空间不足 | 清理后重试；或用 `wsl --export` / `wsl --import` 把 Ubuntu 迁移到其它盘 |

## 附：交给 AI 执行的提示词

把下面这段复制给 Cursor 等 AI 工具即可。AI 会逐步检查并执行，遇到第 0 节那些需要你本人操作的步骤时会停下来提示你。

```text
请帮我在这台 Windows 电脑上配置 SmartOne（RK3506B）固件 SDK 的开发环境。
打开 https://github.com/FOTRIC-Code/SmartOne 仓库里的 docs/windows-wsl-setup.md，严格按第 0~5 节执行
（装 WSL2 Ubuntu、分配资源、装依赖、clone 到 Linux 家目录、预置 dl 包、./build.sh --rebuild）。
遇到需要我本人操作的步骤（管理员 PowerShell、BIOS 开虚拟化、重启、设 Linux 账号密码），停下来等我。
每一步执行前先说明要做什么；失败时贴出原始错误并给出修复方法。
验收：WSL 内 dist/ 下生成 parkia_cube_fw_V*.img 和 parkia_cube_tiny_fw_V*.bin，把完整路径与大小告诉我。
```
