# IRtekUVCSDK（Linux / rk3506）C ABI 契约

本文档是 `irtek_thermal` Python 绑定（cffi/ctypes，用户程序直调 `libIRtekUVCSDK.so`）以及任何 FFI 使用方的**绑定契约基线**。内容按证据强度分级标注：

- **【实测】** 可由本仓库制品直接验证（头文件、`.so` 二进制、编译器布局实测、net_app 源码）。
- **【生产实践】** net_app 线上固件的现行行为，已通过生产验证，绑定侧必须继承。
- **【待厂商确认】** 制品中不存在承诺，需 fotric.cn 书面确认；在此之前按文中给出的保守规则实现。

> 绑定目标工件：`app/net_app/libs/irtek/lib/libIRtekUVCSDK.so` + `include/IRtekUVCSDK/IRtekUVCSDK.h`（即 Volga.IRtekSDK_Linux 的 rk3506 版）。

## 0. 工件指纹

| 项 | 值 | 证据 |
|---|---|---|
| 架构 | ELF 32-bit LSB ARM EABI5，ARMv7，NEON | `readelf -h/-A` |
| 浮点 ABI | **hard-float**（`Tag_ABI_VFP_args: VFP registers`，VFPv4） | `readelf -A` |
| 构建工具链 | Buildroot GCC 12.4.0 | `.comment` 段 |
| SONAME | `libIRtekUVCSDK.so`（**无版本后缀**） | `readelf -d` |
| 运行时依赖 | `libusb-1.0.so.0`、`libstdc++.so.6`、`libm.so.6`、`libgcc_s.so.1`、`libc.so.6` | `readelf -d` |
| 固件安装位置 | `/usr/lib/net_app/`；OTA 后优先 `/userdata/parkia/app/lib/`（net_app RPATH 顺序：`/userdata/parkia/app/lib:/usr/lib/net_app`） | `net_app.mk`、`CMakeLists.txt` |
| 运行时版本 | 仅可经 `uvc_get_sdk_version()` 查询（4×u16）；仓库无 version.h/changelog | 头文件 |

rootfs 为同一 Buildroot 工具链产出的 armhf glibc 环境，Python/libffi 若加入 rootfs，其 FFI 调用约定与库天然一致，无软/硬浮点错配风险。

## P0-1. ABI / 结构体布局

**结论：头文件 `#pragma pack(1)` 布局即 ABI 契约。**【实测】全部对外结构体位于 `#pragma pack(push, 1)` 内，无任何隐式填充；用真实头文件分别在 x86-64（g++ 13）与 ARM32（clang 18 `-target armv7-linux-gnueabihf`）下编译验证：**除含指针的 `uvc_frame_data` 与 `HANDLE` 外，所有结构体在两架构下 sizeof/offsetof 逐字节一致**（复现方法见附录 C）。因此按头文件声明书写的 cffi/ctypes 定义（`_pack_=1` + 平台原生指针）在两个平台上都自动正确。

基元类型（头文件 typedef）：`BYTE`=u8，`SHORT`=i16，`USHORT`=u16，`INT32`=i32，`UINT32`=u32，`UINT64`=u64，`HANDLE`=void*（ARM32=4B / x86-64=8B）。C++ `bool`=1B（回调参数 `found` 用）。多字节字段均为小端。

枚举尺寸：`uvc_enum_shutter_mode` / `uvc_enum_image_flip_mode` / `uvc_enum_sr_mode` / `uvc_enum_upgrade_state` 均为 `enum class : BYTE` → **1 字节**；`uvc_enum_error_code : INT32`（在 pack 区外，显式底层类型）→ 4 字节，即所有 API 返回值为 int32。FFI 侧把前四者映射为 u8 常量、返回值按 i32 处理。

### 结构体布局表（rk3506 / ARM32，全部经编译器 static_assert 验证）

`uvc_device` — sizeof=**68**

| 字段 | 类型 | offset | size |
|---|---|---|---|
| path | BYTE[64] | 0 | 64 |
| vid | USHORT | 64 | 2 |
| pid | USHORT | 66 | 2 |

⚠ `path` 是 **bus/port 二进制 id，不是 C 字符串**（net_app 实证，见 `ARCHITECTURE.md` 硬性规则第 6 条）：禁止 `%s`/strdup，按 64 字节定长二进制比较/hex 展示。

`uvc_frame_info` — sizeof=**140**

| 字段 | 类型 | offset | size |
|---|---|---|---|
| interface_num | USHORT | 0 | 2 |
| alt_setting | USHORT | 2 | 2 |
| frame_index | USHORT | 4 | 2 |
| format_index | USHORT | 6 | 2 |
| width | USHORT | 8 | 2 |
| height | USHORT | 10 | 2 |
| format | BYTE[128] | 12 | 128 |

`uvc_frame_data` — sizeof=**12**（x86-64 为 16，见下）

| 字段 | 类型 | offset(ARM32) | size(ARM32) | offset(x86-64) |
|---|---|---|---|---|
| data | BYTE* | 0 | 4 | 0 (8B) |
| length | UINT32 | 4 | 4 | 8 |
| width | USHORT | 8 | 2 | 12 |
| height | USHORT | 10 | 2 | 14 |

`uvc_device_info` — sizeof=**416**

| 字段 | 类型 | offset | size |
|---|---|---|---|
| model | BYTE[128] | 0 | 128 |
| serial_no | BYTE[16] | 128 | 16 |
| id | BYTE[32] | 144 | 32 |
| brand | BYTE[32] | 176 | 32 |
| company | BYTE[64] | 208 | 64 |
| firmware_name | BYTE[128] | 272 | 128 |
| firmware_version | BYTE[16] | 400 | 16 |

字符串字段按定长 NUL 填充处理，不保证 NUL 结尾，读取须定长拷贝后自行补零（net_app 做法）。

`uvc_lens_info` — sizeof=**150**

| 字段 | 类型 | offset | size |
|---|---|---|---|
| name | BYTE[64] | 0 | 64 |
| focal | float | 64 | 4 |
| vfov | float | 68 | 4 |
| hfov | float | 72 | 4 |
| band | BYTE[32] | 76 | 32 |
| range_map | uvc_temp_range_map[4] | 108 | 40 |
| ranges_count | USHORT | 148 | 2 |

`uvc_temp_range` — sizeof=**8**：min(f32)@0、max(f32)@4。
`uvc_temp_range_map` — sizeof=**10**：id(u16)@0、range@2（其内 min@2、max@6）。
`uvc_size` — sizeof=**4**：width(u16)@0、height(u16)@2。
`uvc_sdk_version` — sizeof=**8**：major/minor/patch/build 各 u16，offset 0/2/4/6。

`uvc_lut_table` — sizeof=**388**

| 字段 | 类型 | offset | size |
|---|---|---|---|
| range_id | USHORT | 0 | 2 |
| ad | USHORT[64] | 2 | 128 |
| t | float[64]（开氏度） | 130 | 256 |
| lut_count | USHORT | 386 | 2 |

`uvc_thermal_params` — sizeof=**20**：emissivity/distance/refl_temp/atm_temp/rel_humidity 各 f32，offset 0/4/8/12/16。
`uvc_image_params` — sizeof=**7**：contrast(u16)@0、brightness(u16)@2、dde_enable(u8)@4、dde_value(u16)@5。
`uvc_dpc_point` — sizeof=**4**：y@0、x@2；`uvc_dpc_params` — sizeof=**258**：points[64]@0、count@256。

错误码（返回值）：`0`=OK；`-1`=FAIL；`-2`=MEMORY；`-3`=NULL_POINTER；`-4`=INVALID_PARAM；`-5`=NOT_SUPPORTED；`-6`=NOT_INITIALIZED（未调 `uvc_sdk_init`）。

## P0-2. 帧 payload 布局

**结论：以下布局是 net_app 生产固件逐帧解析并验证通过的实证布局**（参考实现：`app/net_app/thermal/uvc_frame_utils.h`，即 SDK demo 的仓库内同源实现）。其作为跨机型/跨模组固件版本的厂商级稳定契约【待厂商确认】；绑定侧按本节末尾的防御规则实现即可安全兼容。

帧缓冲：`uvc_frame_data{ data, length, width=W, height=H }`，恒有 `length ≥ W·H·2`。布局（偏移单位：字节）：

| 偏移 | 大小 | 内容 |
|---|---|---|
| 0 | W·(H−4)·2 | 像素区：uint16 小端、行主序 AD 原始值，图像为 W×(H−4) |
| M+0 | 4 | `lut_length`（u32，记作 N） |
| M+4 | 4 | 保留 |
| M+8 | 4 | `sensor_temp`（u32） |
| M+12 | 4 | `shutter_state`（u32） |
| M+16 | 4 | `frame_no`（u32，帧序号） |
| M+20 | 8 | `pts`（u64；net_app 按**微秒**处理，`pts/1000` 转 ms 给 FileSDK） |
| M+28 | 4 | `raw_type`（u32） |
| M+32 | 4N | LUT 温度表，float32，**开尔文** |
| M+32+4N | 2N | LUT AD 表，uint16 |

其中 M = W·(H−4)·2，即尾部 4 行元数据区起始；元数据区总大小 8W 字节，须容纳 32+6N → **N ≤ (8W−32)/6**（W=384 时 N≤506；net_app 解析器的 `N≤4096` 仅是内存安全哨兵）。

逐条回答：

- **布局是稳定契约还是 demo 假设**：对当前 rk3506 模组 + 当前模组固件，是生产验证的事实布局【生产实践】；厂商是否承诺跨版本稳定【待厂商确认】。
- **元数据是否恒为 4 行**：net_app 全链路按 `image_height = H − 4`（判定 `H > 4`）运行至今【生产实践】。绑定应以「`length ≥ W·H·2` 且元数据区能容纳 32+6N」做运行时校验，而不是盲信 H−4。
- **`raw_type` 取值与含义**：net_app 仅透传、未消费，头文件与制品无语义说明【待厂商确认】。绑定原样透传，不做解释。
- **`sensor_temp`**：同上，透传字段（u32；可能为传感器温度定点值，未证实）。
- **`shutter_state`**：net_app 观测值为 0/1，**1 = 挡片（NUC）动作中，该帧丢弃不用**【生产实践】。其他取值未定义，绑定应视为「非 0 即不可用」。

防御规则（绑定必须实现）：解析前校验 `length ≥ W·H·2`；`N==0` 或越界则该帧无 LUT（像素仍可用）；任何校验失败丢帧计数，不抛异常到 SDK 回调线程。

## P0-3. LUT 语义与生效时机

- **单位**：开尔文（头文件 `uvc_lut_table.t` 注释「单位: 开氏度」；net_app 以 `K−273.15` 转 °C）【实测】。
- **AD→温度**：对 LUT 做**线性插值**，端点外钳制。官方参考实现即 `app/net_app/thermal/uvc_frame_utils.h` 的 `interpolate_temperature_kelvin()`（`lower_bound` 二分 + 线性），C /Python 直译即可，以其为准【生产实践】。
- **测温参数（发射率/距离/反射温度/环境温度/湿度）是否影响 LUT**：**不影响温度换算路径的语义定位**——本产品 UVC 模组不做硬件测温，上述参数由软件换算路径消费（`ARCHITECTURE.md` 第 287 行注；net_app 把参数推给设备仅 best-effort，实际换算经 FileSDK `ir_file_set_thermal_params` + `ir_file_get_temp_spot`/marker 施加校正）【实测+生产实践】。因此**帧内嵌 LUT 应视为发射率=1 的黑体等效辐射温度表**；直接插值得到的是未校正温度。需要校正温度（发射率≠1、距离/大气修正）时，必须走 FileSDK 或自行实现校正公式。`uvc_set_thermal_params` 后 LUT 是否/何时变化【待厂商确认】。
- **`uvc_set_thermal_range` 切换量程**：LUT 是否重载、切换期帧是否可信【待厂商确认】。绑定规则：切换后以「`frame_no` 推进 + 帧内 LUT 内容变化（memcmp）+ 丢弃 `shutter_state≠0` 帧」判定过渡期结束；过渡期温度数据标记不可信。net_app 即用 memcmp 检测 LUT 变化【生产实践】。
- **帧内嵌 LUT 与 `uvc_get_thermal_lut`**：`uvc_lut_table` 硬上限 64 项，而帧内 LUT 为数百项级 → 两者不同量级；net_app 从不调用 `uvc_get_thermal_lut`，一律用帧内 LUT【实测+生产实践】。是否同源同值【待厂商确认】。**绑定契约：一律使用最新帧的内嵌 LUT。**

## P0-4. 回调线程模型与缓冲所有权

- **回调线程**：SDK 内部存在独立热插拔线程（`uvc_manager::hotpulg_function` lambda 线程）与**每设备 USB 传输线程**（`uvc_device_manager::start_bulk_transfer` / `start_iso_transfer` lambda 线程，组帧经 `emit_bulk_frame`）【实测，`.so` 符号】。net_app 实证：**首帧可在 `uvc_set_stream_callback` 调用线程之外的线程到达**，回调发生在 SDK 传输线程上下文【生产实践】。「每路流恰好一个线程」的精确表述【待厂商确认】；绑定不得对回调线程做任何假设（线程 id、数量、与热插拔线程的关系）。
- **回调内可调 SDK API / 重入**：无任何文档承诺【待厂商确认】。**契约：回调内禁止调用任何 SDK API。** net_app 回调内只做：字段校验 → `memcpy` 像素与 LUT → 互斥锁内交换缓冲 → 立即返回【生产实践】。
- **`uvc_frame_data.data` 所有权/有效期**：无文档承诺【待厂商确认】。**契约：视为回调返回即失效，回调内完成全部拷贝。** net_app 严格按此实现。
- **耗时容忍 / 内部帧队列 / 丢帧策略**：无文档承诺【待厂商确认】。对 Python 绑定的推论：FFI 回调进入解释器有 GIL 抖动，**绑定层应在回调内只做 `memcpy` 到预分配 ring buffer + 置事件**，由 Python 工作线程异步消费；禁止在回调里做任何解释器重活（分配大对象、日志、锁竞争）。参考数据率：384×292 帧 ≈ 219 KiB/帧，A7 上纯 memcpy 开销可忽略【生产实践】。
- 热插拔回调同样来自 SDK 线程：net_app 将其排队到自有 worker 线程后再做 login/开流，**禁止在 `on_uvc_found_cb` 内直接 login/开流**【生产实践】。

## P1-5. 独占与多实例

- 设备占用经 libusb（`NEEDED libusb-1.0.so.0`）【实测】。USB interface claim 在内核层面按进程排他 → **同一时刻只有一个进程能登录并开流**；第二个进程的行为无厂商承诺，按 libusb 语义将在 claim 处失败（login 或开流报错）【推断，待厂商确认】。
- **产品层事实**：net_app 是该 UVC 设备的默认属主。用户程序直采前必须协调：net_app 停流/退出（或产品形态上由用户程序替代 net_app）。这是部署前提，不是 SDK 行为。
- 同进程多设备：API 全句柄化（`uvc_login` 返回独立 `HANDLE`，内部按 device_manager 组织），设计上支持多设备多句柄；未实测【待厂商确认】。

## P1-6. 热插拔与断连

- `on_uvc_found_cb` 经 `uvc_set_hotplug_callback` 注册，由 SDK 热插拔线程在设备 arrive/remove 时触发，`found` 为 C++ bool（1B）【实测+生产实践】。
- 掉线：net_app 收 remove 后 `close_session()`（停流+logout）；**设备恢复后必须重新 `uvc_login`**，不存在句柄自动复活【生产实践】。
- **关键坑（必须继承到绑定）**：热插拔通告可能早于模组 vendor control 就绪，此时立即开流会**在 libIRtekUVCSDK 内部崩溃**（PROBE/COMMIT / bulk worker，net_app 文件头注释实证）。规避：login 后先轮询任一配置 GET（`uvc_get_lens_info` / `uvc_get_thermal_params` / `uvc_get_thermal_range` / `uvc_get_image_params`）成功再开流；net_app 参数为 8 次 × 250 ms【生产实践】。

## P1-7. 初始化 / 清理约束

- 顺序（生产验证）：`uvc_sdk_init()` →（可选 `uvc_enable_log`、`uvc_set_hotplug_callback`）→ `uvc_login`/`uvc_login_by_path` → 配置就绪探测 → `uvc_get_frame_list` + `uvc_set_stream_callback` 开流。停止： **`uvc_set_stream_callback(h, &frame, NULL)` 停流 → `uvc_logout(&h)` → `uvc_sdk_cleanup()`**【生产实践】。logout 前必须先停流（即按上述顺序）。
- 未 init 调 API 返回 `UVC_EC_NOT_INITIALIZED(-6)`【头文件】。
- cleanup 是否收敛全部内部线程：内部线程对象随 manager 生命周期管理【符号证据】，精确收敛语义【待厂商确认】；按上述生产顺序调用即可。

## P1-8. 挡片校正

- `shutter_state==1` = 挡片/NUC 动作中，期间帧丢弃、温度换算不可信【生产实践】。
- 控制面：`uvc_thermal_calibrate`（立即校正）、`uvc_get/set_thermal_shutter_mode`（AUTOMATIC=0 / MANUAL=1）、`uvc_get/set_thermal_shutter_time`（自动校正周期，分钟）【头文件+net_app 用法】。

## P1-9. 版本与兼容性

- 运行时版本：`uvc_get_sdk_version()` → 4×u16；仓库内无 changelog，FileSDK 侧有 version.h（7.1.0.12），UVC SDK 无【实测】。
- **soname 无版本号 → 无 soname 级 ABI 承诺**；实际契约 = 头文件 + 导出符号集。绑定打包时应记录 `uvc_get_sdk_version()` 与 `.so` 的 sha256。
- rk3506 版与 ubuntu20/22 x86-64 版：头文件为跨平台同一份（`#ifdef WINDOWS/ANDROID/LINUX` 仅改导出宏与 `uvc_login_by_fd`），**API 面一致**是厂商打包方式保证的合理推断；x86-64 版二进制不在仓库，逐符号一致性【待厂商确认】。两架构 ABI 差异已确认仅 `uvc_frame_data`（16B vs 12B）与 `HANDLE`（8B vs 4B），由指针尺寸引起【实测】。
- **「no export to client」接口**：`.so` 额外导出 20 个头文件未声明的符号（附录 A），属厂商内部/产测接口（fab_app 经 dlopen 使用），**绑定明确禁止使用**；后续版本对它们无任何兼容性承诺。

## P2-10. FileSDK 伪彩渲染与温度录像

- **用户进程内 raw→RGB 可行**：net_app 已示范纯用户态路径（`render_with_file_sdk`）：`ir_file_create`（临时 .irs，`IR_FILE_THERMAL_PICTURE`）→ `ir_file_set_isp_type(IR_FILE_HOLLY)` → `ir_file_set_thermal_size` → `ir_file_set_lut`（直接用帧内 LUT）→ `ir_file_set_thermal_params` → `ir_file_get_thermal_image_from_frame(..., IR_FILE_BGR, ...)` 得 BGR 位图；测温点/区域用 `ir_file_get_temp_spot` / `ir_file_set_marker`。不依赖 net_app 进程，用户程序可经 `libIRtekFileSDK.so`（同目录，`irtek_file_sdk_loader.*` 是 dlopen 封装范例）复制该路径【实测+生产实践】。
- **IR 文件作为温度录像**：FileSDK API 面支持 `ir_file_create`（文件类型含录像类）+ `ir_file_append_frame` + `ir_file_get_frame_count` + `ir_file_get_frame(index)` + `ir_file_set/get_frame_rate`，即逐帧追加、事后随机访问【实测，头文件】。逐帧回读温度矩阵的完整流程 net_app 未实测（net_app 只用单帧临时文件模式），作为后续方向成立，落地前需按 `IRtekFileSDK.h` 验证多帧写入/回读路径。

## P2-11. 许可

`app/net_app/libs/irtek/LICENSE`：两个 `.so` 为 fotric.cn 专有软件，**允许且仅允许「作为本 SDK 构建的固件的一部分」再分发**——预置进 rootfs 并随 OTA 下发到 fotric 设备在许可范围内【实测，LICENSE 原文】；禁止脱离 SDK 单独再分发、禁止逆向。Python 绑定源码本身不含 `.so`，不受此限。

## 附录 A. 导出符号清单（`nm -D` 实测）

**公开 API（头文件声明，35 个，Linux 版全部导出）**：`uvc_sdk_init` `uvc_sdk_cleanup` `uvc_enable_log` `uvc_get_sdk_version` `uvc_get_uvc_list` `uvc_set_hotplug_callback` `uvc_login` `uvc_login_by_path` `uvc_logout` `uvc_get_frame_list` `uvc_set_stream_callback` `uvc_get_device_info` `uvc_get_lens_info` `uvc_device_restart` `uvc_device_reset` `uvc_device_upgrade` `uvc_get_upgrade_state` `uvc_thermal_calibrate` `uvc_get_thermal_shutter_mode` `uvc_set_thermal_shutter_mode` `uvc_get_thermal_shutter_time` `uvc_set_thermal_shutter_time` `uvc_get_thermal_resolution` `uvc_get_thermal_range` `uvc_set_thermal_range` `uvc_get_thermal_lut` `uvc_get_thermal_params` `uvc_set_thermal_params` `uvc_set_thermal_dpc` `uvc_get_thermal_sr` `uvc_set_thermal_sr` `uvc_get_image_params` `uvc_set_image_params` `uvc_get_image_flip` `uvc_set_image_flip`。（`uvc_login_by_fd` 仅 Android 构建导出，本库中不存在。）

**导出但未声明（内部/产测接口，禁止绑定使用，共 20 个）**：`uvc_get_cmd` `uvc_put_cmd` `uvc_do_regiset` `uvc_get_gain_status` `uvc_set_gain_status` `uvc_get_nor_raw` `uvc_get_raw_file` `uvc_get_sensor_recipe` `uvc_set_sensor_recipe` `uvc_get_sensor_sn` `uvc_get_sensor_temp` `uvc_get_system_seal` `uvc_set_seal_status` `uvc_write_system_hsi` `uvc_set_lens` `uvc_set_sensor_dpc` `uvc_set_sensor_env` `uvc_set_sensor_nuc` `uvc_set_sensor_register` `uvc_set_fps`。另有数据符号 `g_log_enabled` 及 `uvc_manager`/`uvc_device_manager` 的 C++ 符号。

## 附录 B. Python 绑定实现要点

1. **结构体**：ctypes 用 `class X(Structure): _pack_ = 1`；`data` 字段声明为 `POINTER(c_uint8)`——pack(1) + 平台原生指针使同一份定义在 ARM32 与 x86-64 都正确（开发机联调无需改布局）。
2. **枚举**：4 个 pack 区枚举按 `c_uint8`；返回值/错误码按 `c_int32`；`bool` 参数按 `c_uint8`（GCC C++ bool=1B）。
3. **回调**：`on_frame_received_cb` 内只做 `ctypes.memmove` 到预分配 buffer + `threading.Event`；严禁在回调内调 SDK API 或做重活；保持回调函数对象强引用（防 GC）。
4. **二进制字段**：`uvc_device.path` 按 64B 二进制；设备信息字符串定长提取、自行补零。
5. **生命周期**：严格按 P1-6/P1-7 的就绪探测与停流顺序；热插拔事件排队到 Python 线程处理。
6. **温度**：默认交付开尔文与摄氏两套；说明「帧内 LUT 插值 = 黑体等效温度」，校正温度走 FileSDK。

## 附录 C. 布局实测复现

```bash
# x86-64：直接编译运行打印 sizeof/offsetof
g++ -I app/net_app/libs/irtek/include layout.cc -o layout_x64 && ./layout_x64
# ARM32：clang 交叉 + static_assert 全字段校验（空 cstring  shim 绕过宿主机 C++ 头）
clang++ -target armv7-unknown-linux-gnueabihf -ffreestanding \
  -I shim -I app/net_app/libs/irtek/include -fsyntax-only check_arm.cc
```

全部 static_assert 通过（sizeof 与逐字段 offsetof，见 P0-1 各表）。

## 待厂商确认清单（汇总）

| # | 问题 | 现状 | 影响 |
|---|---|---|---|
| 1 | 帧元数据布局是否厂商承诺的稳定契约（跨机型/跨模组固件） | 生产实证，无书面承诺 | 中：绑定已含防御校验 |
| 2 | `raw_type` / `sensor_temp` 取值与语义 | 未消费，透传 | 低 |
| 3 | `uvc_set_thermal_params` 后帧内 LUT 是否/何时变化 | 按「不影响帧内 LUT、软件路径校正」实现 | 高：决定测温语义表述 |
| 4 | `uvc_set_thermal_range` 切换期 LUT 重载与帧可信性 | 以 frame_no+memcmp+shutter 判定过渡期 | 中 |
| 5 | 帧内 LUT 与 `uvc_get_thermal_lut` 是否同源同值 | 一律用帧内 LUT | 低 |
| 6 | 回调线程精确模型（每路流一个线程？）、回调内重入限制、`data` 缓冲有效期、内部帧队列/丢帧策略 | 最保守契约：回调内只 memcpy、不调 API、返回即失效 | 高：Python 绑定性能设计 |
| 7 | 两进程同设备行为；同进程多设备确认 | 单进程独占（libusb 语义推断） | 中 |
| 8 | `uvc_sdk_cleanup` 线程收敛语义 | 按生产顺序调用 | 低 |
| 9 | rk3506 与 x86-64 版逐符号一致性、后续 ABI 兼容承诺、changelog | soname 无版本号 | 中 |
