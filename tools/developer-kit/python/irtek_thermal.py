# -*- coding: utf-8 -*-
"""irtek_thermal —— libIRtekUVCSDK.so 的纯 Python 绑定（零编译，cffi 优先 / ctypes 回退）。

让 SmartOne（RK3506B）设备上的 Python 用户程序不经过 net_app，直接采集热像温度。
设备上两个 .so 已随固件预置（/usr/lib/net_app/，OTA 后 /userdata/parkia/app/lib/ 优先），
本模块只负责 dlopen 与封装，源码不含任何厂商二进制。

绑定契约基线：docs/irtek-uvc-sdk-abi.md（关键规则已内联到本文件的防御逻辑中）：
  - 结构体 #pragma pack(1) 布局即 ABI 契约（ARM32 与 x86-64 逐字节一致，
    仅 uvc_frame_data/HANDLE 因指针尺寸不同而不同）——加载时做 sizeof 自检。
  - 帧布局：像素区 W*(H-4) 个 uint16 AD + 尾部 4 行元数据（lut_length/sensor_temp/
    shutter_state/frame_no/pts/raw_type + LUT 温度表(float32 K) + LUT AD 表(u16)）。
  - 温度 = 对帧内嵌 LUT 做线性插值（端点外钳制），单位开尔文。
    注意：帧内 LUT 是【黑体等效温度】（发射率=1）；发射率/距离等校正需走 FileSDK，
    本绑定不做校正，输出字段一律以 blackbody 语义命名与注释。
  - shutter_state != 0：挡片动作中，帧不可用（get_frame 默认丢弃此类帧）。
  - 回调纪律：SDK 传输线程内只做 memcpy + 置事件，不调任何 SDK API、不做重活；
    缓冲视为回调返回即失效。
  - 就绪探测：login 后先轮询配置 GET（8 次 x 250ms，任一成功）再开流，
    否则可能在 libIRtekUVCSDK 内部崩溃（net_app 生产实证）。
  - 停流顺序：uvc_set_stream_callback(h, &frame, NULL) -> uvc_logout -> uvc_sdk_cleanup。
  - 独占：libusb claim 按进程排他，同一时刻只有一个进程能开流（即本程序与 net_app 互斥）。

设备前提：固件 Python 需含 cffi 或 ctypes 之一。验证：
    ssh root@192.168.123.100 "python3 -c 'import cffi'"      # 优先
    ssh root@192.168.123.100 "python3 -c 'import ctypes'"    # 回退
两者都没有时需在固件 defconfig 中补 BR2_PACKAGE_PYTHON3_CTYPES（或 python-cffi）重出固件。
"""

from __future__ import annotations

import bisect
import hashlib
import logging
import os
import struct
import threading
import time
import zlib
from array import array
from collections import namedtuple

__version__ = "1.0.0"
CONTRACT_VERSION = "irtek-uvc-sdk-abi.md v1"

log = logging.getLogger("irtek_thermal")

KELVIN_TO_CELSIUS = -273.15

# 就绪探测参数（继承 net_app 生产值）
DEVICE_READY_ATTEMPTS = 8
DEVICE_READY_DELAY_S = 0.25

# 帧元数据布局常量（见契约 P0-2）
META_ROWS = 4                 # 尾部元数据行数
META_FIXED_SIZE = 32          # 元数据固定头字节数
MAX_LUT_ENTRIES = 4096        # 内存安全哨兵（net_app 同款）

# 错误码（契约 P0-1）
_ERROR_NAMES = {
    0: "OK", -1: "FAIL", -2: "MEMORY", -3: "NULL_POINTER",
    -4: "INVALID_PARAM", -5: "NOT_SUPPORTED", -6: "NOT_INITIALIZED",
}

# 结构体 sizeof 契约表（ARM32 / x86-64），用于加载时 ABI 自检
_STRUCT_SIZES = {
    "uvc_device":         (68, 68),
    "uvc_frame_info":     (140, 140),
    "uvc_frame_data":     (12, 16),   # 仅此项因指针尺寸不同
    "uvc_device_info":    (416, 416),
    "uvc_lens_info":      (150, 150),
    "uvc_temp_range":     (8, 8),
    "uvc_temp_range_map": (10, 10),
    "uvc_size":           (4, 4),
    "uvc_sdk_version":    (8, 8),
    "uvc_lut_table":      (388, 388),
    "uvc_thermal_params": (20, 20),
    "uvc_image_params":   (7, 7),
}

# .so 搜索路径（顺序即优先级：OTA 更新版 > 出厂预置版 > 系统默认）
_LIB_SEARCH_PATHS = [
    "/userdata/parkia/app/lib/libIRtekUVCSDK.so",
    "/usr/lib/net_app/libIRtekUVCSDK.so",
]


class ThermalError(RuntimeError):
    """绑定层统一异常。"""

    def __init__(self, message, code=None):
        self.code = code
        if code is not None:
            message = "%s (uvc_ec=%d %s)" % (message, code, _ERROR_NAMES.get(code, "?"))
        super().__init__(message)


class BackendUnavailableError(ThermalError):
    """无可用 FFI 后端或 ABI 自检失败。"""


class DeviceNotFoundError(ThermalError):
    pass


class DeviceNotReadyError(ThermalError):
    pass


class StreamError(ThermalError):
    pass


class FrameTimeoutError(ThermalError):
    pass


FrameInfo = namedtuple("FrameInfo", [
    "interface_num", "alt_setting", "frame_index", "format_index",
    "width", "height", "format",
])

DeviceEntry = namedtuple("DeviceEntry", ["vid", "pid", "path"])


def kelvin_to_celsius(k):
    return k + KELVIN_TO_CELSIUS


def interpolate_temperature_kelvin(ad, lut_ad, lut_temp):
    """AD -> 开尔文，对 LUT 线性插值、端点外钳制（net_app 参考实现的直译）。"""
    n = len(lut_ad)
    if n == 0:
        return float("nan")
    if n == 1 or ad <= lut_ad[0]:
        return lut_temp[0]
    if ad >= lut_ad[-1]:
        return lut_temp[-1]
    i = bisect.bisect_left(lut_ad, ad)
    if i < n and lut_ad[i] == ad:
        return lut_temp[i]
    x0, x1 = lut_ad[i - 1], lut_ad[i]
    y0, y1 = lut_temp[i - 1], lut_temp[i]
    if x1 == x0:
        return y0
    return y0 + (ad - x0) * (y1 - y0) / float(x1 - x0)


class ParsedFrame(object):
    """一帧解析结果。温度均为【黑体等效温度】（发射率=1），单位见方法名后缀。

    raw: array('H')，image_width x image_height 的 AD 原始值（行主序）。
    """

    __slots__ = ("width", "height", "image_height", "raw",
                 "lut_length", "lut_ad", "lut_temp", "lut_crc",
                 "sensor_temp_raw", "shutter_state", "frame_no",
                 "pts_us", "raw_type", "captured_at")

    @property
    def has_lut(self):
        return self.lut_length > 0

    @property
    def shutter_active(self):
        """挡片/NUC 动作中：本帧温度不可信。"""
        return self.shutter_state != 0

    def temp_k_at(self, x, y):
        ad = self.raw[y * self.width + x]
        return interpolate_temperature_kelvin(ad, self.lut_ad, self.lut_temp)

    def temp_c_at(self, x, y):
        return kelvin_to_celsius(self.temp_k_at(x, y))

    def center_temp_k(self):
        return self.temp_k_at(self.width // 2, self.image_height // 2)

    def center_temp_c(self):
        return kelvin_to_celsius(self.center_temp_k())

    def _clamp_roi(self, x, y, w, h):
        x0 = max(0, min(int(x), self.width - 1))
        y0 = max(0, min(int(y), self.image_height - 1))
        x1 = max(x0 + 1, min(int(x + w), self.width))
        y1 = max(y0 + 1, min(int(y + h), self.image_height))
        return x0, y0, x1, y1

    def roi_stats(self, x=0, y=0, w=None, h=None):
        """ROI 统计：max/min/avg（开尔文与摄氏）+ 最值位置 + 像素数。

        不传 ROI 即全帧；注意全帧 avg 需逐像素插值（384x288 约 11 万像素），
        在 Cortex-A7 上约数百毫秒，高频采样请使用小 ROI。
        """
        if not self.has_lut:
            raise ThermalError("frame has no embedded LUT, temperature unavailable")
        if w is None:
            w = self.width
        if h is None:
            h = self.image_height
        x0, y0, x1, y1 = self._clamp_roi(x, y, w, h)

        max_ad = -1
        min_ad = 0x10000
        max_pos = min_pos = None
        temp_sum = 0.0
        count = 0
        lut_ad, lut_temp = self.lut_ad, self.lut_temp
        interp = interpolate_temperature_kelvin
        raw, width = self.raw, self.width

        for yy in range(y0, y1):
            row = raw[yy * width + x0: yy * width + x1]
            for xx, ad in enumerate(row, x0):
                if ad > max_ad:
                    max_ad, max_pos = ad, (xx, yy)
                if ad < min_ad:
                    min_ad, min_pos = ad, (xx, yy)
                temp_sum += interp(ad, lut_ad, lut_temp)
                count += 1

        max_k = interp(max_ad, lut_ad, lut_temp)
        min_k = interp(min_ad, lut_ad, lut_temp)
        avg_k = temp_sum / count if count else float("nan")
        return {
            "roi": (x0, y0, x1 - x0, y1 - y0),
            "count": count,
            "max_k": max_k, "min_k": min_k, "avg_k": avg_k,
            "max_c": kelvin_to_celsius(max_k),
            "min_c": kelvin_to_celsius(min_k),
            "avg_c": kelvin_to_celsius(avg_k),
            "max_pos": max_pos, "min_pos": min_pos,
        }

    def max_temp_k(self):
        """全帧最高温（快路径：先取 max AD 再单次插值）。"""
        return interpolate_temperature_kelvin(max(self.raw), self.lut_ad, self.lut_temp)

    def min_temp_k(self):
        return interpolate_temperature_kelvin(min(self.raw), self.lut_ad, self.lut_temp)

    def max_temp_c(self):
        return kelvin_to_celsius(self.max_temp_k())

    def min_temp_c(self):
        return kelvin_to_celsius(self.min_temp_k())


class FrameParseError(ThermalError):
    pass


def parse_frame(buf, width, height):
    """按契约 P0-2 解析帧缓冲（bytes-like）。校验失败抛 FrameParseError。"""
    length = len(buf)
    if width == 0 or height == 0:
        raise FrameParseError("empty frame geometry")
    if length < width * height * 2:
        raise FrameParseError("short frame: %d < %d" % (length, width * height * 2))

    image_height = height - META_ROWS if height > META_ROWS else height
    pixel_count = width * image_height
    meta_off = pixel_count * 2

    f = ParsedFrame()
    f.width = width
    f.height = height
    f.image_height = image_height
    f.raw = array("H")
    f.raw.frombytes(bytes(buf[:meta_off]))  # 小端平台原生序（ARM32/x86-64 均为 LE）
    f.captured_at = time.time()

    f.lut_length = 0
    f.lut_ad = array("H")
    f.lut_temp = array("f")
    f.lut_crc = 0
    f.sensor_temp_raw = f.shutter_state = f.frame_no = f.raw_type = 0
    f.pts_us = 0

    if height <= META_ROWS:
        return f  # 无元数据区

    lut_length, = struct.unpack_from("<I", buf, meta_off)
    (f.sensor_temp_raw, f.shutter_state, f.frame_no,
     f.pts_us, f.raw_type) = struct.unpack_from("<IIIQI", buf, meta_off + 8)

    # 防御校验（契约 P0-2 防御规则）
    max_n_by_layout = (8 * width - META_FIXED_SIZE) // 6
    if lut_length == 0:
        return f  # 该帧无 LUT，像素仍可用
    if lut_length > MAX_LUT_ENTRIES or lut_length > max_n_by_layout:
        raise FrameParseError("bad lut_length: %d (layout cap %d)" % (lut_length, max_n_by_layout))
    lut_end = meta_off + META_FIXED_SIZE + 6 * lut_length
    if lut_end > width * height * 2:
        raise FrameParseError("lut overruns frame: end=%d" % lut_end)

    f.lut_length = lut_length
    f.lut_temp = array("f")
    f.lut_temp.frombytes(bytes(buf[meta_off + META_FIXED_SIZE:
                                    meta_off + META_FIXED_SIZE + 4 * lut_length]))
    f.lut_ad = array("H")
    f.lut_ad.frombytes(bytes(buf[meta_off + META_FIXED_SIZE + 4 * lut_length: lut_end]))
    f.lut_crc = zlib.crc32(buf[meta_off + META_FIXED_SIZE: lut_end])
    return f


# ---------------------------------------------------------------------------
# cffi 后端
# ---------------------------------------------------------------------------

_CFFI_CDEF = """
typedef unsigned char BYTE;
typedef unsigned short USHORT;
typedef int INT32;
typedef unsigned int UINT32;
typedef unsigned long long UINT64;
typedef void* HANDLE;

typedef struct { BYTE path[64]; USHORT vid; USHORT pid; } uvc_device;
typedef struct { USHORT interface_num; USHORT alt_setting; USHORT frame_index;
                 USHORT format_index; USHORT width; USHORT height;
                 BYTE format[128]; } uvc_frame_info;
typedef struct { BYTE* data; UINT32 length; USHORT width; USHORT height; } uvc_frame_data;
typedef struct { BYTE model[128]; BYTE serial_no[16]; BYTE id[32]; BYTE brand[32];
                 BYTE company[64]; BYTE firmware_name[128]; BYTE firmware_version[16];
               } uvc_device_info;
typedef struct { float min; float max; } uvc_temp_range;
typedef struct { USHORT id; uvc_temp_range range; } uvc_temp_range_map;
typedef struct { BYTE name[64]; float focal; float vfov; float hfov; BYTE band[32];
                 uvc_temp_range_map range_map[4]; USHORT ranges_count; } uvc_lens_info;
typedef struct { USHORT width; USHORT height; } uvc_size;
typedef struct { USHORT range_id; USHORT ad[64]; float t[64]; USHORT lut_count; } uvc_lut_table;
typedef struct { float emissivity; float distance; float refl_temp; float atm_temp;
                 float rel_humidity; } uvc_thermal_params;
typedef struct { USHORT contrast; USHORT brightness; BYTE dde_enable;
                 USHORT dde_value; } uvc_image_params;
typedef struct { USHORT major; USHORT minor; USHORT patch; USHORT build; } uvc_sdk_version;

typedef void (*on_frame_received_cb)(const uvc_frame_data* frame);

INT32 uvc_sdk_init(void);
INT32 uvc_sdk_cleanup(void);
INT32 uvc_enable_log(uint8_t enable);
INT32 uvc_get_sdk_version(uvc_sdk_version* version);
INT32 uvc_get_uvc_list(uvc_device* device_list, USHORT length, USHORT* out_length);
INT32 uvc_login(USHORT vid, USHORT pid, HANDLE* h);
INT32 uvc_logout(HANDLE* h);
INT32 uvc_get_frame_list(HANDLE h, uvc_frame_info* frames, USHORT length, USHORT* out_length);
INT32 uvc_set_stream_callback(HANDLE h, uvc_frame_info* frame, on_frame_received_cb cb);
INT32 uvc_get_device_info(HANDLE h, uvc_device_info* info);
INT32 uvc_get_lens_info(HANDLE h, uvc_lens_info* info);
INT32 uvc_get_thermal_params(HANDLE h, uvc_thermal_params* param);
INT32 uvc_set_thermal_params(HANDLE h, const uvc_thermal_params* param);
INT32 uvc_get_thermal_range(HANDLE h, USHORT* range_id);
INT32 uvc_set_thermal_range(HANDLE h, const USHORT* range_id);
INT32 uvc_get_thermal_resolution(HANDLE h, uvc_size* size);
INT32 uvc_get_image_params(HANDLE h, uvc_image_params* param);
INT32 uvc_thermal_calibrate(HANDLE h);
"""


class _CffiBackend(object):
    name = "cffi"

    def __init__(self):
        from cffi import FFI
        self._ffi = FFI()
        # packed=True 等价头文件 #pragma pack(1)（cffi 忽略 #pragma，必须用关键字参数）
        self._ffi.cdef(_CFFI_CDEF, packed=True)
        self._lib = None
        self._lib_path = None
        self._cb_ref = None  # 保持回调强引用，防 GC

    # -- 加载与自检 --------------------------------------------------------
    def load(self, path=None):
        candidates = [path] if path else []
        candidates += _LIB_SEARCH_PATHS + ["libIRtekUVCSDK.so"]
        last_err = None
        for cand in candidates:
            try:
                self._lib = self._ffi.dlopen(cand)
                self._lib_path = cand
                break
            except OSError as e:
                last_err = e
        if self._lib is None:
            raise BackendUnavailableError(
                "cannot load libIRtekUVCSDK.so: %s (searched %s)"
                % (last_err, candidates))
        self._abi_self_check()

    def _abi_self_check(self):
        ffi, bits = self._ffi, struct.calcsize("P") * 8
        idx = 0 if bits == 32 else 1
        bad = []
        for name, sizes in _STRUCT_SIZES.items():
            actual = ffi.sizeof(name)
            if actual != sizes[idx]:
                bad.append("%s=%d (expect %d)" % (name, actual, sizes[idx]))
        if bad:
            raise BackendUnavailableError("ABI self-check failed: " + ", ".join(bad))

    # -- 基础封装 ----------------------------------------------------------
    def _rc(self, rc, what):
        if rc != 0:
            raise ThermalError("%s failed" % what, code=rc)

    def sdk_init(self):
        self._rc(self._lib.uvc_sdk_init(), "uvc_sdk_init")

    def sdk_cleanup(self):
        self._rc(self._lib.uvc_sdk_cleanup(), "uvc_sdk_cleanup")

    def enable_log(self, on):
        self._lib.uvc_enable_log(1 if on else 0)

    def sdk_version(self):
        v = self._ffi.new("uvc_sdk_version*")
        self._rc(self._lib.uvc_get_sdk_version(v), "uvc_get_sdk_version")
        return (v.major, v.minor, v.patch, v.build)

    def list_devices(self, maximum=8):
        ffi = self._ffi
        arr = ffi.new("uvc_device[]", maximum)
        out_n = ffi.new("USHORT*")
        self._rc(self._lib.uvc_get_uvc_list(arr, maximum, out_n), "uvc_get_uvc_list")
        result = []
        for i in range(out_n[0]):
            d = arr[i]
            result.append(DeviceEntry(
                vid=d.vid, pid=d.pid,
                path=bytes(ffi.buffer(ffi.addressof(d, "path"), 64)[:])))
        return result

    def login(self, vid, pid):
        h = self._ffi.new("HANDLE*")
        self._rc(self._lib.uvc_login(vid, pid, h), "uvc_login")
        return h  # 保留 cdata，logout 需要 HANDLE*

    def logout(self, h):
        self._rc(self._lib.uvc_logout(h), "uvc_logout")

    def get_frame_list(self, h, maximum=16):
        ffi = self._ffi
        arr = ffi.new("uvc_frame_info[]", maximum)
        out_n = ffi.new("USHORT*")
        self._rc(self._lib.uvc_get_frame_list(h[0], arr, maximum, out_n),
                 "uvc_get_frame_list")
        result = []
        for i in range(out_n[0]):
            f = arr[i]
            result.append(FrameInfo(
                f.interface_num, f.alt_setting, f.frame_index, f.format_index,
                f.width, f.height,
                bytes(ffi.buffer(ffi.addressof(f, "format"), 128)[:])))
        return result

    def alloc_buffer(self, size):
        return self._ffi.new("uint8_t[]", size)

    def buffer_snapshot(self, buf, n):
        return self._ffi.buffer(buf, n)[:]

    def start_stream(self, h, frame_info, dest_buf, dest_capacity, notify):
        """notify(length, width, height) 在帧拷贝完成后于 SDK 线程内调用。"""
        ffi = self._ffi

        def _relay(frame_ptr):
            # 契约 P0-4：回调内只做 memcpy + 通知；绝不调用 SDK API；异常不外抛
            try:
                f = frame_ptr[0]
                if f.data != ffi.NULL and 0 < f.length <= dest_capacity:
                    ffi.memmove(dest_buf, f.data, f.length)
                    notify(f.length, f.width, f.height)
                else:
                    notify(0, f.width, f.height)
            except Exception:
                pass

        self._cb_ref = ffi.callback("void(const uvc_frame_data*)", error=None)(_relay)
        fi = ffi.new("uvc_frame_info*")
        fi.interface_num = frame_info.interface_num
        fi.alt_setting = frame_info.alt_setting
        fi.frame_index = frame_info.frame_index
        fi.format_index = frame_info.format_index
        fi.width = frame_info.width
        fi.height = frame_info.height
        ffi.memmove(ffi.addressof(fi[0], "format"), frame_info.format,
                    min(len(frame_info.format), 128))
        rc = self._lib.uvc_set_stream_callback(h[0], fi, self._cb_ref)
        return rc

    def stop_stream(self, h, frame_info):
        ffi = self._ffi
        fi = ffi.new("uvc_frame_info*")
        fi.interface_num = frame_info.interface_num
        fi.alt_setting = frame_info.alt_setting
        fi.frame_index = frame_info.frame_index
        fi.format_index = frame_info.format_index
        fi.width = frame_info.width
        fi.height = frame_info.height
        ffi.memmove(ffi.addressof(fi[0], "format"), frame_info.format,
                    min(len(frame_info.format), 128))
        self._lib.uvc_set_stream_callback(h[0], fi, ffi.NULL)
        self._cb_ref = None

    # -- 配置类 GET/SET ----------------------------------------------------
    def get_lens_info(self, h):
        v = self._ffi.new("uvc_lens_info*")
        self._rc(self._lib.uvc_get_lens_info(h[0], v), "uvc_get_lens_info")
        return {"ranges_count": v.ranges_count, "focal": v.focal,
                "vfov": v.vfov, "hfov": v.hfov}

    def get_thermal_params(self, h):
        v = self._ffi.new("uvc_thermal_params*")
        self._rc(self._lib.uvc_get_thermal_params(h[0], v), "uvc_get_thermal_params")
        return {"emissivity": v.emissivity, "distance": v.distance,
                "refl_temp": v.refl_temp, "atm_temp": v.atm_temp,
                "rel_humidity": v.rel_humidity}

    def set_thermal_params(self, h, params):
        v = self._ffi.new("uvc_thermal_params*")
        for k, val in params.items():
            setattr(v, k, float(val))
        self._rc(self._lib.uvc_set_thermal_params(h[0], v), "uvc_set_thermal_params")

    def get_thermal_range(self, h):
        v = self._ffi.new("USHORT*")
        self._rc(self._lib.uvc_get_thermal_range(h[0], v), "uvc_get_thermal_range")
        return v[0]

    def get_thermal_resolution(self, h):
        v = self._ffi.new("uvc_size*")
        self._rc(self._lib.uvc_get_thermal_resolution(h[0], v),
                 "uvc_get_thermal_resolution")
        return (v.width, v.height)

    def get_image_params(self, h):
        v = self._ffi.new("uvc_image_params*")
        self._rc(self._lib.uvc_get_image_params(h[0], v), "uvc_get_image_params")
        return {"contrast": v.contrast, "brightness": v.brightness,
                "dde_enable": v.dde_enable, "dde_value": v.dde_value}

    def thermal_calibrate(self, h):
        self._rc(self._lib.uvc_thermal_calibrate(h[0]), "uvc_thermal_calibrate")


# ---------------------------------------------------------------------------
# ctypes 后端
# ---------------------------------------------------------------------------

def _build_ctypes_backend():
    import ctypes as C

    class UvcDevice(C.Structure):
        _pack_ = 1
        _fields_ = [("path", C.c_uint8 * 64), ("vid", C.c_uint16), ("pid", C.c_uint16)]

    class UvcFrameInfo(C.Structure):
        _pack_ = 1
        _fields_ = [("interface_num", C.c_uint16), ("alt_setting", C.c_uint16),
                    ("frame_index", C.c_uint16), ("format_index", C.c_uint16),
                    ("width", C.c_uint16), ("height", C.c_uint16),
                    ("format", C.c_uint8 * 128)]

    class UvcFrameData(C.Structure):
        _pack_ = 1
        _fields_ = [("data", C.POINTER(C.c_uint8)), ("length", C.c_uint32),
                    ("width", C.c_uint16), ("height", C.c_uint16)]

    class UvcLensInfo(C.Structure):
        _pack_ = 1
        _fields_ = [("name", C.c_uint8 * 64), ("focal", C.c_float),
                    ("vfov", C.c_float), ("hfov", C.c_float),
                    ("band", C.c_uint8 * 32),
                    ("range_map", C.c_uint8 * 40),  # uvc_temp_range_map[4]，不解析
                    ("ranges_count", C.c_uint16)]

    class UvcSize(C.Structure):
        _pack_ = 1
        _fields_ = [("width", C.c_uint16), ("height", C.c_uint16)]

    class UvcThermalParams(C.Structure):
        _pack_ = 1
        _fields_ = [("emissivity", C.c_float), ("distance", C.c_float),
                    ("refl_temp", C.c_float), ("atm_temp", C.c_float),
                    ("rel_humidity", C.c_float)]

    class UvcImageParams(C.Structure):
        _pack_ = 1
        _fields_ = [("contrast", C.c_uint16), ("brightness", C.c_uint16),
                    ("dde_enable", C.c_uint8), ("dde_value", C.c_uint16)]

    class UvcSdkVersion(C.Structure):
        _pack_ = 1
        _fields_ = [("major", C.c_uint16), ("minor", C.c_uint16),
                    ("patch", C.c_uint16), ("build", C.c_uint16)]

    class UvcLutTable(C.Structure):
        _pack_ = 1
        _fields_ = [("range_id", C.c_uint16), ("ad", C.c_uint16 * 64),
                    ("t", C.c_float * 64), ("lut_count", C.c_uint16)]

    class UvcTempRange(C.Structure):
        _pack_ = 1
        _fields_ = [("min", C.c_float), ("max", C.c_float)]

    class UvcTempRangeMap(C.Structure):
        _pack_ = 1
        _fields_ = [("id", C.c_uint16), ("range", UvcTempRange)]

    class UvcDeviceInfo(C.Structure):
        _pack_ = 1
        _fields_ = [("model", C.c_uint8 * 128), ("serial_no", C.c_uint8 * 16),
                    ("id", C.c_uint8 * 32), ("brand", C.c_uint8 * 32),
                    ("company", C.c_uint8 * 64), ("firmware_name", C.c_uint8 * 128),
                    ("firmware_version", C.c_uint8 * 16)]

    FrameCb = C.CFUNCTYPE(None, C.POINTER(UvcFrameData))

    class CtypesBackend(object):
        name = "ctypes"

        def __init__(self):
            self._C = C
            self._lib = None
            self._lib_path = None
            self._cb_ref = None

        def load(self, path=None):
            candidates = [path] if path else []
            candidates += _LIB_SEARCH_PATHS + ["libIRtekUVCSDK.so"]
            last_err = None
            for cand in candidates:
                try:
                    self._lib = C.CDLL(cand)
                    self._lib_path = cand
                    break
                except OSError as e:
                    last_err = e
            if self._lib is None:
                raise BackendUnavailableError(
                    "cannot load libIRtekUVCSDK.so: %s (searched %s)"
                    % (last_err, candidates))
            self._abi_self_check()
            self._bind_signatures()

        def _abi_self_check(self):
            import struct as _s
            bits = _s.calcsize("P") * 8
            idx = 0 if bits == 32 else 1
            structs = {
                "uvc_device": UvcDevice, "uvc_frame_info": UvcFrameInfo,
                "uvc_frame_data": UvcFrameData, "uvc_device_info": UvcDeviceInfo,
                "uvc_lens_info": UvcLensInfo, "uvc_temp_range": UvcTempRange,
                "uvc_temp_range_map": UvcTempRangeMap, "uvc_size": UvcSize,
                "uvc_sdk_version": UvcSdkVersion, "uvc_lut_table": UvcLutTable,
                "uvc_thermal_params": UvcThermalParams,
                "uvc_image_params": UvcImageParams,
            }
            bad = []
            for name, sizes in _STRUCT_SIZES.items():
                actual = C.sizeof(structs[name])
                if actual != sizes[idx]:
                    bad.append("%s=%d (expect %d)" % (name, actual, sizes[idx]))
            if bad:
                raise BackendUnavailableError("ABI self-check failed: " + ", ".join(bad))

        def _bind_signatures(self):
            C = self._C
            L = self._lib
            for fn in ("uvc_sdk_init", "uvc_sdk_cleanup", "uvc_enable_log",
                       "uvc_get_sdk_version", "uvc_get_uvc_list", "uvc_login",
                       "uvc_logout", "uvc_get_frame_list", "uvc_set_stream_callback",
                       "uvc_get_device_info", "uvc_get_lens_info",
                       "uvc_get_thermal_params", "uvc_set_thermal_params",
                       "uvc_get_thermal_range", "uvc_set_thermal_range",
                       "uvc_get_thermal_resolution", "uvc_get_image_params",
                       "uvc_thermal_calibrate"):
                getattr(L, fn).restype = C.c_int32
            L.uvc_login.argtypes = [C.c_uint16, C.c_uint16, C.POINTER(C.c_void_p)]
            L.uvc_logout.argtypes = [C.POINTER(C.c_void_p)]
            L.uvc_get_uvc_list.argtypes = [C.POINTER(UvcDevice), C.c_uint16,
                                           C.POINTER(C.c_uint16)]
            L.uvc_get_frame_list.argtypes = [C.c_void_p, C.POINTER(UvcFrameInfo),
                                             C.c_uint16, C.POINTER(C.c_uint16)]
            L.uvc_set_stream_callback.argtypes = [C.c_void_p, C.POINTER(UvcFrameInfo),
                                                  FrameCb]
            L.uvc_get_thermal_params.argtypes = [C.c_void_p, C.POINTER(UvcThermalParams)]
            L.uvc_set_thermal_params.argtypes = [C.c_void_p, C.POINTER(UvcThermalParams)]
            L.uvc_get_thermal_range.argtypes = [C.c_void_p, C.POINTER(C.c_uint16)]
            L.uvc_get_thermal_resolution.argtypes = [C.c_void_p, C.POINTER(UvcSize)]
            L.uvc_get_lens_info.argtypes = [C.c_void_p, C.POINTER(UvcLensInfo)]
            L.uvc_get_image_params.argtypes = [C.c_void_p, C.POINTER(UvcImageParams)]
            L.uvc_thermal_calibrate.argtypes = [C.c_void_p]
            L.uvc_get_sdk_version.argtypes = [C.POINTER(UvcSdkVersion)]
            L.uvc_enable_log.argtypes = [C.c_uint8]

        def _rc(self, rc, what):
            if rc != 0:
                raise ThermalError("%s failed" % what, code=rc)

        def sdk_init(self):
            self._rc(self._lib.uvc_sdk_init(), "uvc_sdk_init")

        def sdk_cleanup(self):
            self._rc(self._lib.uvc_sdk_cleanup(), "uvc_sdk_cleanup")

        def enable_log(self, on):
            self._lib.uvc_enable_log(1 if on else 0)

        def sdk_version(self):
            v = UvcSdkVersion()
            self._rc(self._lib.uvc_get_sdk_version(self._C.byref(v)),
                     "uvc_get_sdk_version")
            return (v.major, v.minor, v.patch, v.build)

        def list_devices(self, maximum=8):
            C = self._C
            arr = (UvcDevice * maximum)()
            out_n = C.c_uint16(0)
            self._rc(self._lib.uvc_get_uvc_list(arr, maximum, C.byref(out_n)),
                     "uvc_get_uvc_list")
            return [DeviceEntry(vid=d.vid, pid=d.pid, path=bytes(bytearray(d.path)))
                    for d in arr[:out_n.value]]

        def login(self, vid, pid):
            h = self._C.c_void_p()
            self._rc(self._lib.uvc_login(vid, pid, self._C.byref(h)), "uvc_login")
            return h

        def logout(self, h):
            self._rc(self._lib.uvc_logout(self._C.byref(h)), "uvc_logout")

        def get_frame_list(self, h, maximum=16):
            C = self._C
            arr = (UvcFrameInfo * maximum)()
            out_n = C.c_uint16(0)
            self._rc(self._lib.uvc_get_frame_list(h, arr, maximum, C.byref(out_n)),
                     "uvc_get_frame_list")
            return [FrameInfo(f.interface_num, f.alt_setting, f.frame_index,
                              f.format_index, f.width, f.height,
                              bytes(bytearray(f.format)))
                    for f in arr[:out_n.value]]

        def alloc_buffer(self, size):
            return (self._C.c_uint8 * size)()

        def buffer_snapshot(self, buf, n):
            return bytes(bytearray(buf[:n]))

        def start_stream(self, h, frame_info, dest_buf, dest_capacity, notify):
            C = self._C

            def _relay(frame_ptr):
                try:
                    f = frame_ptr.contents
                    if bool(f.data) and 0 < f.length <= dest_capacity:
                        C.memmove(dest_buf, f.data, f.length)
                        notify(f.length, f.width, f.height)
                    else:
                        notify(0, f.width, f.height)
                except Exception:
                    pass

            self._cb_ref = FrameCb(_relay)
            fi = UvcFrameInfo(frame_info.interface_num, frame_info.alt_setting,
                              frame_info.frame_index, frame_info.format_index,
                              frame_info.width, frame_info.height,
                              (self._C.c_uint8 * 128)(*frame_info.format[:128]))
            self._fi_ref = fi  # 保持引用
            return self._lib.uvc_set_stream_callback(h, self._C.byref(fi), self._cb_ref)

        def stop_stream(self, h, frame_info):
            fi = UvcFrameInfo(frame_info.interface_num, frame_info.alt_setting,
                              frame_info.frame_index, frame_info.format_index,
                              frame_info.width, frame_info.height,
                              (self._C.c_uint8 * 128)(*frame_info.format[:128]))
            # CFUNCTYPE 参数传 None -> NULL
            self._lib.uvc_set_stream_callback(h, self._C.byref(fi), self._C.cast(None, FrameCb))
            self._cb_ref = None
            self._fi_ref = None

        def get_lens_info(self, h):
            v = UvcLensInfo()
            self._rc(self._lib.uvc_get_lens_info(h, self._C.byref(v)), "uvc_get_lens_info")
            return {"ranges_count": v.ranges_count, "focal": v.focal,
                    "vfov": v.vfov, "hfov": v.hfov}

        def get_thermal_params(self, h):
            v = UvcThermalParams()
            self._rc(self._lib.uvc_get_thermal_params(h, self._C.byref(v)),
                     "uvc_get_thermal_params")
            return {"emissivity": v.emissivity, "distance": v.distance,
                    "refl_temp": v.refl_temp, "atm_temp": v.atm_temp,
                    "rel_humidity": v.rel_humidity}

        def set_thermal_params(self, h, params):
            v = UvcThermalParams()
            for k, val in params.items():
                setattr(v, k, float(val))
            self._rc(self._lib.uvc_set_thermal_params(h, self._C.byref(v)),
                     "uvc_set_thermal_params")

        def get_thermal_range(self, h):
            v = self._C.c_uint16(0)
            self._rc(self._lib.uvc_get_thermal_range(h, self._C.byref(v)),
                     "uvc_get_thermal_range")
            return v.value

        def get_thermal_resolution(self, h):
            v = UvcSize()
            self._rc(self._lib.uvc_get_thermal_resolution(h, self._C.byref(v)),
                     "uvc_get_thermal_resolution")
            return (v.width, v.height)

        def get_image_params(self, h):
            v = UvcImageParams()
            self._rc(self._lib.uvc_get_image_params(h, self._C.byref(v)),
                     "uvc_get_image_params")
            return {"contrast": v.contrast, "brightness": v.brightness,
                    "dde_enable": v.dde_enable, "dde_value": v.dde_value}

        def thermal_calibrate(self, h):
            self._rc(self._lib.uvc_thermal_calibrate(h), "uvc_thermal_calibrate")

    return CtypesBackend


# ---------------------------------------------------------------------------
# 后端选择与高层 API
# ---------------------------------------------------------------------------

def _load_backend(lib_path=None):
    """cffi 优先，ctypes 回退；ABI 自检不过则视为不可用。"""
    errors = []
    try:
        be = _CffiBackend()
        be.load(lib_path)
        return be
    except ImportError:
        errors.append("cffi: module not installed")
    except BackendUnavailableError as e:
        errors.append("cffi: %s" % e)
    except ThermalError as e:
        errors.append("cffi: %s" % e)

    try:
        be = _build_ctypes_backend()()
        be.load(lib_path)
        return be
    except ImportError:
        errors.append("ctypes: module not installed")
    except BackendUnavailableError as e:
        errors.append("ctypes: %s" % e)
    except ThermalError as e:
        errors.append("ctypes: %s" % e)

    raise BackendUnavailableError(
        "no usable FFI backend (%s). 固件需启用 BR2_PACKAGE_PYTHON3_CTYPES "
        "或 python-cffi 后重出固件。" % "; ".join(errors))


def library_info(backend):
    """返回 .so 路径、sha256 与 SDK 版本（契约 P1-9：打包时应记录版本+哈希）。"""
    path = backend._lib_path
    info = {"path": path, "backend": backend.name, "sdk_version": backend.sdk_version()}
    if path and os.path.isfile(path):
        h = hashlib.sha256()
        with open(path, "rb") as fp:
            for chunk in iter(lambda: fp.read(1 << 20), b""):
                h.update(chunk)
        info["sha256"] = h.hexdigest()
    return info


class ThermalCamera(object):
    """高层 API。

    用法：
        cam = ThermalCamera()
        cam.open()                       # 发现设备 -> login -> 就绪探测 -> 开流
        frame = cam.get_frame(2.0)       # 取一帧（跳过挡片帧/坏帧）
        stats = frame.roi_stats(40, 60, 80, 60)
        cam.close()
    支持 with 语法。线程安全：get_frame 可从任意线程调用。
    """

    def __init__(self, lib_path=None, device_index=0, enable_sdk_log=False):
        self._lib_path = lib_path or os.environ.get("IRTEK_UVC_SDK_PATH")
        self._device_index = device_index
        self._enable_sdk_log = enable_sdk_log

        self._backend = None
        self._handle = None
        self._selected_frame = None
        self._streaming = False

        self._buf = None
        self._buf_capacity = 0
        self._buf_len = 0
        self._buf_frame_wh = (0, 0)
        self._buf_lock = threading.Lock()
        self._frame_event = threading.Event()

        # 统计（继承 net_app 的计数口径）
        self.stat_callbacks = 0
        self.stat_drop_empty = 0
        self.stat_drop_parse = 0
        self.stat_drop_shutter = 0
        self.stat_accepted = 0

    # -- 生命周期 ----------------------------------------------------------
    def open(self):
        if self._streaming:
            return
        be = self._backend or _load_backend(self._lib_path)
        self._backend = be
        be.sdk_init()
        if self._enable_sdk_log:
            be.enable_log(True)
        log.info("irtek_thermal: backend=%s lib=%s sdk=%s",
                 be.name, be._lib_path, be.sdk_version())

        devices = be.list_devices()
        if not devices:
            be.sdk_cleanup()
            raise DeviceNotFoundError("no UVC thermal device found")
        if self._device_index >= len(devices):
            be.sdk_cleanup()
            raise DeviceNotFoundError(
                "device_index %d out of range (%d device(s))"
                % (self._device_index, len(devices)))
        dev = devices[self._device_index]
        log.info("irtek_thermal: login vid=0x%04x pid=0x%04x", dev.vid, dev.pid)
        try:
            self._handle = be.login(dev.vid, dev.pid)
        except ThermalError:
            be.sdk_cleanup()
            raise

        try:
            self._wait_device_ready()
            self._start_stream()
        except ThermalError:
            try:
                be.logout(self._handle)
            finally:
                be.sdk_cleanup()
            self._handle = None
            raise

    def close(self):
        be = self._backend
        if be is None:
            return
        if self._streaming and self._handle is not None:
            # 契约 P1-7：先停流再 logout
            try:
                be.stop_stream(self._handle, self._selected_frame)
            except ThermalError as e:
                log.warning("irtek_thermal: stop stream: %s", e)
            self._streaming = False
        if self._handle is not None:
            try:
                be.logout(self._handle)
            except ThermalError as e:
                log.warning("irtek_thermal: logout: %s", e)
            self._handle = None
        try:
            be.sdk_cleanup()
        except ThermalError:
            pass

    def __enter__(self):
        self.open()
        return self

    def __exit__(self, *exc):
        self.close()

    def _wait_device_ready(self):
        """契约 P1-6 关键坑：热插拔通告可能早于 vendor control 就绪，
        直接开流会在 .so 内部崩溃。login 后轮询配置 GET，任一成功才继续。"""
        be = self._backend
        probes = (be.get_lens_info, be.get_thermal_params,
                  be.get_thermal_range, be.get_image_params)
        for attempt in range(1, DEVICE_READY_ATTEMPTS + 1):
            for probe in probes:
                try:
                    probe(self._handle)
                    if attempt > 1:
                        log.info("irtek_thermal: device ready after %d attempt(s)", attempt)
                    return
                except ThermalError:
                    continue
            if attempt < DEVICE_READY_ATTEMPTS:
                log.warning("irtek_thermal: device controls not ready (%d/%d)",
                            attempt, DEVICE_READY_ATTEMPTS)
                time.sleep(DEVICE_READY_DELAY_S)
        raise DeviceNotReadyError("device controls not ready after %d attempts"
                                  % DEVICE_READY_ATTEMPTS)

    def _start_stream(self):
        be = self._backend
        frames = be.get_frame_list(self._handle)
        if not frames:
            raise StreamError("UVC frame list is empty")
        for f in frames:
            log.info("irtek_thermal: frame %ux%u iface=%u alt=%u idx=%u fmt=%u %r",
                     f.width, f.height, f.interface_num, f.alt_setting,
                     f.frame_index, f.format_index, f.format.split(b"\0")[0])

        # 继承 net_app：选分辨率最大的流；失败则逐个重试
        candidates = sorted(frames, key=lambda f: f.width * f.height, reverse=True)
        last_rc = None
        for selected in candidates:
            capacity = selected.width * selected.height * 2
            self._buf = be.alloc_buffer(capacity)
            self._buf_capacity = capacity
            rc = be.start_stream(self._handle, selected,
                                 self._buf, capacity, self._on_copied)
            if rc == 0:
                self._selected_frame = selected
                self._streaming = True
                log.info("irtek_thermal: streaming %ux%u frame_index=%u",
                         selected.width, selected.height, selected.frame_index)
                return
            last_rc = rc
            log.warning("irtek_thermal: start stream on %ux%u failed rc=%d, retry next",
                        selected.width, selected.height, rc)
        raise StreamError("failed to start UVC stream on all frame formats",
                          code=last_rc)

    # -- 帧接收（SDK 传输线程上下文）---------------------------------------
    def _on_copied(self, length, width, height):
        # 契约 P0-4：只做记录 + 置事件。本函数由后端在回调线程内调用。
        self.stat_callbacks += 1
        if length == 0:
            self.stat_drop_empty += 1
            return
        with self._buf_lock:
            self._buf_len = length
            self._buf_frame_wh = (width, height)
        self._frame_event.set()

    # -- 取帧 --------------------------------------------------------------
    def get_frame(self, timeout=2.0, skip_shutter=True):
        """取下一帧可用帧；默认丢弃挡片帧与解析失败帧（计数仍累计）。

        timeout 内无可用帧抛 FrameTimeoutError。"""
        if not self._streaming:
            raise StreamError("not streaming; call open() first")
        deadline = time.monotonic() + timeout
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise FrameTimeoutError("no usable frame within %.1fs" % timeout)
            self._frame_event.wait(remaining)
            with self._buf_lock:
                if self._buf_len == 0:
                    self._frame_event.clear()
                    continue
                snap = self._backend.buffer_snapshot(self._buf, self._buf_len)
                w, h = self._buf_frame_wh
                self._buf_len = 0
                self._frame_event.clear()
            try:
                frame = parse_frame(snap, w, h)
            except FrameParseError as e:
                self.stat_drop_parse += 1
                log.debug("irtek_thermal: drop unparsable frame: %s", e)
                continue
            if skip_shutter and frame.shutter_active:
                self.stat_drop_shutter += 1
                continue
            self.stat_accepted += 1
            return frame

    # -- 便捷统计 ----------------------------------------------------------
    def get_roi_stats(self, x=0, y=0, w=None, h=None, timeout=2.0):
        """取一帧并做 ROI 统计。不传 ROI 为全帧（avg 较慢，见 ParsedFrame.roi_stats）。"""
        return self.get_frame(timeout=timeout).roi_stats(x, y, w, h)

    # -- 配置接口（薄封装）---------------------------------------------------
    def get_thermal_params(self):
        return self._backend.get_thermal_params(self._handle)

    def set_thermal_params(self, **params):
        """params: emissivity/distance/refl_temp/atm_temp/rel_humidity（开尔文/米）。
        注意契约 P0-3：帧内 LUT 视为不随参数变化，本调用仅 best-effort 推给设备。"""
        self._backend.set_thermal_params(self._handle, params)

    def get_thermal_range(self):
        return self._backend.get_thermal_range(self._handle)

    def get_thermal_resolution(self):
        return self._backend.get_thermal_resolution(self._handle)

    def get_lens_info(self):
        return self._backend.get_lens_info(self._handle)

    def get_image_params(self):
        return self._backend.get_image_params(self._handle)

    def thermal_calibrate(self):
        self._backend.thermal_calibrate(self._handle)

    def library_info(self):
        return library_info(self._backend)

    def stats(self):
        return {
            "callbacks": self.stat_callbacks,
            "drop_empty": self.stat_drop_empty,
            "drop_parse": self.stat_drop_parse,
            "drop_shutter": self.stat_drop_shutter,
            "accepted": self.stat_accepted,
        }


def probe_environment():
    """部署前自检：FFI 后端可用性 + .so 可加载性（不打开设备）。

    返回 dict；ok=True 表示可以 open()。用法：
        python3 -c "import irtek_thermal, json; print(json.dumps(irtek_thermal.probe_environment()))"
    """
    result = {"cffi": None, "ctypes": None, "ok": False}
    try:
        import cffi  # noqa: F401
        result["cffi"] = True
    except ImportError:
        result["cffi"] = False
    try:
        import ctypes  # noqa: F401
        result["ctypes"] = True
    except ImportError:
        result["ctypes"] = False
    try:
        be = _load_backend()
        result["ok"] = True
        result.update(library_info(be))
    except ThermalError as e:
        result["error"] = str(e)
    return result


if __name__ == "__main__":
    # 设备自检入口：python3 /usr/bin/irtek_thermal.py
    import json
    print(json.dumps(probe_environment(), ensure_ascii=False, indent=2))
