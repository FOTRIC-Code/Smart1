# thermal_logger —— 全时温度记录示例（Python，零编译）

演示「持续采集 → 自动记录 → 曲线呈现」的最小闭环：按固定间隔采集热像温度
（ROI 或全帧的最高/最低/平均/中心温），落 CSV，内置 HTTP 页面实时展示温度曲线。

纯 Python 标准库 + 随固件预置的 `libIRtekUVCSDK.so`（经 `irtek_thermal` 绑定调用），
**不需要任何编译环境**：Windows/macOS 上用自带 ssh/scp 即可部署。

## 工作原理

```
thermal_logger.py ── import ──> irtek_thermal.py ── cffi/ctypes ──> libIRtekUVCSDK.so
                                                                      └─ libusb 直采 UVC 热像模组
```

- 绑定模块源码：`tools/developer-kit/python/irtek_thermal.py`
- ABI 契约基线：`docs/irtek-uvc-sdk-abi.md`（结构体布局、帧格式、回调纪律的权威说明）
- 温度语义：帧内 LUT 插值输出**黑体等效温度**（发射率=1）；发射率/距离校正需 FileSDK，
  本示例不做校正。

## 设备前提（一次性检查）

1. **热像模组已接入** USB（未接模组时 open() 会报 `DeviceNotFoundError`）。
2. **Python FFI 可用**（二选一即可）：

   ```sh
   ssh root@192.168.123.100 "python3 -c 'import cffi'"     # 优先（smbus-cffi 依赖链自带）
   ssh root@192.168.123.100 "python3 -c 'import ctypes'"   # 回退
   ```

   都不通过时：在 `buildroot/configs/rockchip_rk3506_board_defconfig` 启用
   `BR2_PACKAGE_PYTHON3_CTYPES`（或 python-cffi），重出固件。
3. 绑定自检（不打开设备，只验证 FFI 可用 + 库可加载 + ABI 布局）：

   ```sh
   ssh root@192.168.123.100 "python3 /usr/bin/irtek_thermal.py"
   ```

## 部署

```sh
# 1. 绑定模块放到设备（与脚本同目录即可被 import）
scp tools/developer-kit/python/irtek_thermal.py root@192.168.123.100:/usr/bin/

# 2. 安装用户程序（自动更新 app_bin 并重启）
#    Linux/macOS:
bash tools/developer-kit/install_user_app.sh 192.168.123.100 \
    samples/thermal_logger/thermal_logger.py
#    Windows（原生，无需 Git Bash/WSL，用系统自带 OpenSSH）:
#    tools\developer-kit\windows\install_user_app.cmd 192.168.123.100 samples\thermal_logger\thermal_logger.py
```

> ⚠️ 部署后 `app_bin` 指向本程序，**net_app 不再启动**（预期行为：热像设备同一时刻
> 只能被一个进程占用）。本程序不含 Web 配网等 net_app 功能。恢复原功能：
> Linux/macOS 用 `bash tools/developer-kit/update_app_bin.sh 192.168.123.100 /usr/bin/net_app`；
> Windows 用 `tools\developer-kit\windows\update_app_bin.cmd 192.168.123.100 /usr/bin/net_app`
> （改完需 `ssh root@192.168.123.100 reboot` 重启生效）。

## 运行与验证

```sh
ssh root@192.168.123.100 "tail -f /tmp/thermal_logger.log"
# 浏览器打开 http://192.168.123.100:8080/ 看实时曲线
# CSV 数据在 /userdata/parkia/thermal_log.csv
```

main_app 默认不带参数启动；要传参需先手动运行验证：

```sh
ssh root@192.168.123.100 "python3 /usr/bin/thermal_logger.py --interval 1 --roi 140,100,80,60 --http 8080"
```

## 参数

| 参数 | 默认 | 说明 |
|---|---|---|
| `--interval` | 1.0 | 采样间隔（秒） |
| `--roi x,y,w,h` | 无 | 采样区域；缺省全帧（全帧只算最高/最低/中心，**不算平均温**——逐像素插值慢） |
| `--csv` | `/userdata/parkia/thermal_log.csv` | CSV 输出路径 |
| `--http` | 8080 | 曲线页端口，`0` 关闭 |
| `--count` | 0 | 采样 N 次后退出（调试用，默认持续） |
| `--device-index` | 0 | 多设备时选择序号 |

## 常见问题

- **`BackendUnavailableError: no usable FFI backend`**：设备 Python 无 cffi/ctypes，见「设备前提 2」。
- **`cannot load libIRtekUVCSDK.so`**：固件未预置 irtek 库（检查 `/usr/lib/net_app/` 与
  `/userdata/parkia/app/lib/`），或用 `IRTEK_UVC_SDK_PATH` 环境变量指定 .so 路径。
- **`DeviceNotFoundError`**：模组未接入/未枚举，看 `lsusb` 与模组供电。
- **温度与上位机软件有偏差**：本示例输出黑体等效温度（发射率=1），属预期；
  需要校正温度请在此基础上接入 FileSDK 校正路径。
- **回调线程纪律**：不要在 `irtek_thermal` 之外直接注册 SDK 回调做重活——
  绑定的 ring buffer 设计（回调内仅 memcpy）是有意为之，详见契约文档 P0-4。
