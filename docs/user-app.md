# 用户程序开发

本文面向"只做用户程序、不改固件"的开发者。按语言选一条路线即可：

| 路线 | 语言 | 需要编译固件 / 交叉工具链 | 门槛 |
|---|---|---|---|
| **A. Python 脚本** | Python 3 | **不需要** | 有固件 + ssh 即可；**可直采热像**（`irtek_thermal` 绑定，见 A.4） |
| B. 交叉编译 ELF | C / C++ | 需要先构建一次 Buildroot 工具链 | 下载约 1.8GB + 1~2 小时 |

两条路线最终是同一件事：把一个可执行文件放到板子上，让 `app_bin` 指向它。
重启后由 `main_app` 守护拉起——崩溃后按 1/2/4…30s 退避重启，60s 内退出超过 5 次则停止拉起
（实现见 `app/main_app/main.c`）。

## 需求分流（动手前先对号）

| 需求涉及 | 路线选择 |
|---|---|
| 热像温度采集、ROI 统计、温度记录/曲线 | **A** —— `irtek_thermal` 绑定直采（A.4），无需 C 工具链 |
| 串口 / GPIO / I2C / SPI 外设 | **A** —— 固件内置 `pyserial` / `python-periphery` / `spidev` / `smbus-cffi` |
| 本地 HTTP 服务、Web 曲线页、sqlite/CSV 落盘 | **A** —— 标准库即可 |
| 需要 net_app 的告警、配网、Web 界面、HTTP API | **不要替换 `app_bin`**，与 net_app 并行（见文末） |
| 视频编码、录像画面叠加、伪彩渲染 | **B**（或改固件）：参考 net_app 的 minih264 / FileSDK 链路 |
| 需要 numpy / pillow 等 C 扩展、Node.js | 改固件（超出"不改固件"范围） |

> **注意：`app_bin` 是单槽位**——指向你的程序后，`net_app` 不再自启，它提供的 HTTP API
> （告警、配网、Web 界面）会全部随之消失。但**热像采集能力不再依赖 net_app**：
> Python 程序可经 `irtek_thermal` 绑定直接调 `libIRtekUVCSDK.so` 采集（见 A.4）。
> 如果你的程序同时要热像数据又要 net_app 的其它接口，见文末「与 net_app 并行运行」。

> **Node.js 目前不支持**：设备 rootfs 里没有 Node 运行时，要用必须先在 Buildroot 配置中启用
> nodejs 并重新编译、烧录固件——那就不属于"不改固件"了。详见文末[关于 Node.js](#关于-nodejs)。

## 0. 连接设备（两条路线通用）

**前置条件**：本机需有 `git`（克隆仓库）和 `ssh` / `scp`（Windows 10+ 自带 OpenSSH 客户端）。
路线 A 的部署在 Windows 上用 `tools\developer-kit\windows\*.cmd` 原生完成，**不需要 Git Bash / WSL**；
只有路线 B 的交叉编译才需要 WSL2 + Buildroot SDK（见 `docs/windows-wsl-setup.md`）。
文中其余 `bash` 命令在 Windows 上请用 Git Bash 或 WSL 执行。先验收环境：

```cmd
git --version && ssh -V
```

两条命令都输出版本号即环境就绪；缺什么先装什么，再继续往下。

用 USB 数据线把设备的 `POWER` 口连到电脑。设备启动后电脑会识别出一个 USB 网卡（RNDIS），
设备地址固定为 `192.168.123.100`，电脑侧通常自动获取地址，不需要额外的设备搜索工具：

```sh
ssh root@192.168.123.100     # 口令 root
```

下文命令中的 `<板子IP>` 在 USB 直连时就是 `192.168.123.100`。

## 路线 A：Python 脚本（免编译固件）

设备固件已内置 Python 3（Buildroot 配置中 `BR2_PACKAGE_PYTHON3=y`）。`main_app` 通过
`execl()` 拉起 `app_bin`，由内核解析 shebang——所以**一个带 `#!/usr/bin/python3` 的脚本
可以直接当用户程序**，全程不需要编译任何东西，也不需要 Buildroot 工具链。

（可印证：Developer Kit 的 `update_app_bin.sh` 本身就是调用板子上的 `python3` 去改
`app_bin` 配置的，运行时确实存在。）

### A.1 写程序

`my_app.py`：

```python
#!/usr/bin/python3
import time

while True:
    with open("/tmp/my_app.log", "a") as fp:
        fp.write("hello from user app\n")
    time.sleep(5)
```

注意事项：

- 第一行必须是 shebang，且指向 `/usr/bin/python3`；
- **行尾必须是 LF**。CRLF 会让内核解析 shebang 失败（`#!/usr/bin/python3\r`），程序根本起不来。
  仓库 `.gitattributes` 已强制 `*.py` 按 LF 检出；若你的克隆是在配置之前检出的（或手工编辑器
  改存过），执行 `git add --renormalize .` 或重新克隆，再用编辑器确认保存格式为 LF；
- 部署脚本会执行 `chmod 0755`，不需要自己处理可执行位；
- 标准输出不会自动落盘，需要日志就自己写文件（`/tmp/` 可写）；
- **设备上没有 `pip`**，只能用标准库加固件已内置的模块：
  `pyserial`、`python-periphery`、`spidev`、`smbus-cffi`（因此 `cffi` 可用），
  以及 `sqlite3` / `ssl` / `zlib` / `expat` / `ctypes`（视固件版本，见 A.4 的验证命令）。
  纯 Python 的第三方包可以把源码拷到板子上并用 `PYTHONPATH` 引入；带 C 扩展的（numpy、pillow 等）
  无法这样使用，必须在 Buildroot 里选中对应包后重新编译固件；
- 固件未启用 aiohttp / jinja2 / werkzeug 等 Web 框架，需要时同样要改 Buildroot 配置重编固件；
- 用户程序路线不需要构建固件，不要执行 `./build.sh`；
- 不要手改 `/etc/cfg/main_app/config.json`，`app_bin` 只通过部署脚本更新。

### A.2 部署到板子

克隆仓库后（仓库本身很小，SDK 底包并不在仓库里，**不需要执行 `./build.sh`**）：

```sh
# Linux / macOS:
bash tools/developer-kit/install_user_app.sh <板子IP> ./my_app.py my_app

# Windows（cmd/PowerShell 原生，无需 Git Bash/WSL）:
tools\developer-kit\windows\install_user_app.cmd <板子IP> my_app.py my_app
```

该脚本对脚本文件和 ELF 的处理完全一致：上传到 `/usr/bin/my_app` → `chmod 0755` →
把 `/etc/cfg/main_app/config.json` 的 `app_bin` 改成该路径 → 重启设备。

不想用脚本时，等价的手工步骤：

```sh
scp my_app.py root@<板子IP>:/usr/bin/my_app
ssh root@<板子IP> "chmod 0755 /usr/bin/my_app"
bash tools/developer-kit/update_app_bin.sh <板子IP> /usr/bin/my_app
ssh root@<板子IP> reboot
```

### A.3 验证与回退

```sh
ssh root@<板子IP> ps | grep my_app
ssh root@<板子IP> cat /tmp/my_app.log
```

想恢复默认行为（开机启动 `net_app`）：

```sh
# Linux / macOS:
bash tools/developer-kit/update_app_bin.sh <板子IP> /usr/bin/net_app
# Windows:
tools\developer-kit\windows\update_app_bin.cmd <板子IP> /usr/bin/net_app

ssh root@<板子IP> reboot
```

### A.4 Python 直采热像（irtek_thermal 绑定）

Python 用户程序可以**不经过 net_app，直接采集热像温度**：绑定模块
[`tools/developer-kit/python/irtek_thermal.py`](../tools/developer-kit/python/irtek_thermal.py)
通过 `cffi`（优先）/ `ctypes`（回退）调用固件已预置的 `libIRtekUVCSDK.so`
（`/usr/lib/net_app/`，OTA 后 `/userdata/parkia/app/lib/` 优先），
走与 net_app 相同的 libusb 采集路径。ABI 契约与实现约束见
[`docs/irtek-uvc-sdk-abi.md`](irtek-uvc-sdk-abi.md)。

**一次性环境检查**（在设备上执行）：

```sh
ssh root@<板子IP> "python3 -c 'import cffi'"      # 或 import ctypes，任一通过即可
scp tools/developer-kit/python/irtek_thermal.py root@<板子IP>:/usr/bin/
ssh root@<板子IP> "python3 /usr/bin/irtek_thermal.py"   # ABI 自检：ok:true 即可
```

FFI 两者都没有时，需在 `buildroot/configs/rockchip_rk3506_board_defconfig` 启用
`BR2_PACKAGE_PYTHON3_CTYPES` 并重编固件（这超出"不改固件"，属一次性补齐）。

**最小示例**：

```python
#!/usr/bin/python3
import irtek_thermal

with irtek_thermal.ThermalCamera() as cam:
    frame = cam.get_frame(timeout=2.0)          # 自动跳过挡片帧/坏帧
    print(frame.center_temp_c())                # 中心温（摄氏度）
    print(frame.roi_stats(140, 100, 80, 60))    # ROI 最高/最低/平均温
```

完整可部署示例见 [`samples/thermal_logger/`](../samples/thermal_logger/)：
定时采温 → CSV 落盘 → 内置 HTTP 曲线页，即「全时监控」类需求的最小闭环。

**使用约束**（都来自契约文档，违反会在设备上得到确定性失败）：

- **独占**：热像设备按进程排他（libusb claim）。net_app 正在运行时 `open()` 会失败——
  要么把你的程序设为 `app_bin`（替换 net_app），要么保留 net_app 走 HTTP 并行（见文末）。
- **温度语义**：帧内 LUT 插值输出的是**黑体等效温度**（发射率=1）。发射率/距离/大气校正
  需走 FileSDK（`libIRtekFileSDK.so`，伪彩渲染与校正温度能力它也有），本绑定不含校正。
- **回调纪律**：SDK 回调线程内只做帧拷贝；不要绕过本模块直接注册 SDK 回调。
- 绑定模块随用户程序一起部署（与脚本同目录即可 `import`）；产品化时可预置进 rootfs。

## 路线 B：交叉编译 ELF（Developer Kit）

面向 C / C++ 用户程序。目标：

1. 用 USB 直连设备（见第 0 节，地址固定 `192.168.123.100`）
2. 用 Buildroot SDK 交叉编译出 ARM ELF
3. 把 ELF 下载到板子 `/usr/bin`
4. 必要时把 `app_bin` 更新到该 ELF 路径
5. 重启后由 `main_app` 拉起用户程序

### 1. 准备 Buildroot SDK

生成工具链包：

```sh
./build.sh shell
make sdk
```

得到 `arm-buildroot-linux-gnueabihf_sdk-buildroot.tar.gz` 后，解压并执行一次：

```sh
./relocate-sdk.sh
```

后续把该目录作为 `--sdk-root` 传给交叉编译脚本，或设置环境变量：

```sh
export BUILDROOT_SDK_ROOT=/path/to/arm-buildroot-linux-gnueabihf_sdk-buildroot
```

### 2. 交叉编译用户程序

Developer Kit 脚本位于 [`tools/developer-kit/`](../tools/developer-kit/)。

构建仓库内的最小示例：

```sh
bash tools/developer-kit/build_user_app.sh
```

显式指定 SDK：

```sh
bash tools/developer-kit/build_user_app.sh \
  --sdk-root /path/to/arm-buildroot-linux-gnueabihf_sdk-buildroot
```

默认示例产物：

```text
samples/user_app/build/user_app
```

### 3. 下载到板子

#### 默认模式：自定义路径模式

`install_user_app.sh` 默认会把 ELF 安装到自定义路径，并同步更新 `app_bin`。

```sh
bash tools/developer-kit/install_user_app.sh <ip> ./samples/user_app/build/user_app
```

上面这条命令会：

1. 上传到 `/usr/bin/user_app`
2. `chmod 0755`
3. 把 `/etc/cfg/main_app/config.json` 中的 `app_bin` 改成 `/usr/bin/user_app`
4. 重启设备

### 可选模式：覆盖 `/usr/bin/net_app`

如需兼容旧流程，可显式指定远端名：

```sh
bash tools/developer-kit/install_user_app.sh <ip> ./samples/user_app/build/user_app net_app
```

此时目标路径变成 `/usr/bin/net_app`，脚本会把 `app_bin` 指回该路径。

### 4. 单独更新 `app_bin`

如只想切换启动路径，不重新上传：

```sh
bash tools/developer-kit/update_app_bin.sh <ip> /usr/bin/my_app
ssh root@<ip> reboot
```

### 5. 验证

重启后可通过以下方式确认：

```sh
ssh root@<ip> ps | grep user_app
ssh root@<ip> cat /tmp/user_app.log
```

仓库示例 `samples/user_app` 每 5 秒会向 `/tmp/user_app.log` 写一条心跳日志。

### 6. AI 工作流

Developer Kit 附带 Cursor Skill：

```text
tools/developer-kit/skills/device-app-dev/SKILL.md
```

建议把以下信息提供给 AI：

- 板子 IP（USB 直连固定为 `192.168.123.100`）
- Buildroot SDK 路径
- 目标 ELF 名称
- 是否要覆盖 `/usr/bin/net_app`

Skill 默认约束 AI：

- 用 Buildroot SDK 交叉编译
- 通过 Developer Kit 脚本下载
- 不手写其它部署路径
- 不直接手改 `config.json`

## 失败恢复（两条路线通用）

本方案不做运行时回退。

如果用户程序无法运行，按固件流程重新烧录整包：

```sh
sudo ./tools/linux/Linux_Upgrade_Tool/Linux_Upgrade_Tool/upgrade_tool uf \
  dist/parkia_cube_fw_V<version>.img
```

## 与 net_app 并行运行

先想清楚要的是什么：

- **只要热像温度数据** → 用 A.4 的 `irtek_thermal` 直采，直接替换 `app_bin` 即可。
  注意 net_app 的 HTTP API 只有热像**参数配置**端点（`thermal/thermal_params`、
  `thermal/lens_info` 等），并不对外提供实时温度数据——所以"保留 net_app 调接口取温度"
  这条路并不存在，直采才是正解。
- **同时要 net_app 的告警、配网、Web 界面** → 正确姿势是**保留 net_app、你的程序通过
  HTTP 调它**，而不是替换它：

- net_app 监听本机 `:80`，热像相关端点如 `thermal/thermal_params`、`thermal/lens_info`、
  `thermal/high_temp/alarm` 等，完整接口表见 `app/net_app/ARCHITECTURE.md` §7.8–7.10；
- 设备完成配网后接口要求 HTTP Basic 认证（默认 `admin/admin`，量产前应修改）；Python 侧用
  标准库 `urllib.request` 即可，不需要任何第三方包；
- 开发期：`app_bin` 保持 `/usr/bin/net_app`，脚本 scp 上去后用 ssh 手动跑
  （如 `nohup /usr/bin/my_app &`），随时可停；
- 要开机自启且不改固件：让 `app_bin` 指向你的程序，由你的程序代起 net_app——

  ```python
  #!/usr/bin/python3
  import subprocess
  subprocess.Popen(["/usr/bin/net_app"])  # 拉起 net_app，恢复 HTTP API 与 Web 界面
  # 之后是你的逻辑：urllib 调 http://127.0.0.1/thermal/... 取数据
  ```

  代价：`main_app` 只守护你的程序，net_app 若单独崩溃不会被拉起。要求高可靠时应改固件
  给 net_app 配独立启动项，那已超出"不改固件"的范围。

## 关于 Node.js

设备 rootfs 中**没有** Node.js 运行时。核查依据：`buildroot/configs/rockchip_rk3506_board_defconfig`
里没有启用 `BR2_PACKAGE_NODEJS`（`buildroot/package/Config.in` 中出现 `package/nodejs/Config.in`
只是 Buildroot 上游的包索引写法，不代表被选中）。

要支持 Node 程序，需要：

1. 在 `buildroot/configs/rockchip_rk3506_board_defconfig` 中启用 nodejs；
2. 重新编译并烧录固件。

注意这与"不改固件"的目标冲突，且 Node 运行时体积较大，对 SPI-NAND 的固件容量有实际影响。
在决定支持之前，用户程序请使用 Python 3（路线 A）或交叉编译 ELF（路线 B）。
