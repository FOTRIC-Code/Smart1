#pragma once

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32) || defined(_WIN64)
#if defined(NETSDK_EXPORT)
#define IRTEK_NETSDK_API __declspec(dllexport)
#else
#define IRTEK_NETSDK_API __declspec(dllimport)
#endif
#define IRTEK_NETSDK_CALL __stdcall
#else
#define IRTEK_NETSDK_API __attribute__((visibility("default")))
#define IRTEK_NETSDK_CALL
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define IRTEK_NETSDK_NAME "IRtekNetSDK"

#define NET_LEN_16 16
#define NET_LEN_32 32
#define NET_LEN_64 64
#define NET_LEN_128 128
#define NET_LEN_256 256

#define NET_MAX_RANGES 4
#define NET_MAX_NTP_SERVERS 4
#define NET_MAX_DNS 4
#define NET_MAX_GATEWAYS 4
#define NET_MAX_LUT_POINTS 64
#define NET_MAX_PALETTE_COLORS 256
#define NET_MAX_MARKER_POINTS 8
#define NET_MAX_MARKERS 16
#define NET_MAX_ACTIVE_ALARMS 16

typedef void* NET_HANDLE;
typedef unsigned char BYTE;
typedef unsigned short USHORT;
typedef int INT32;
typedef unsigned int UINT32;
typedef unsigned long long UINT64;

typedef enum
{
    NET_EC_OK = 0,                  /* 成功 / Success */
    NET_EC_FAIL = -1,               /* 通用失败 / General failure */
    NET_EC_MEMORY = -2,             /* 内存不足 / Out of memory */
    NET_EC_NULL_POINTER = -3,       /* 空指针 / Null pointer */
    NET_EC_INVALID_PARAM = -4,      /* 参数无效 / Invalid parameter */
    NET_EC_NOT_SUPPORTED = -5,      /* 不支持 / Not supported */
    NET_EC_NOT_INITIALIZED = -6,    /* 未初始化 / Not initialized */
    NET_EC_HTTP_ERROR = -7,         /* HTTP 错误 / HTTP error */
    NET_EC_AUTH_FAILED = -8,        /* 认证失败 / Authentication failed */
    NET_EC_TIMEOUT = -9             /* 超时 / Timeout */
} net_enum_error_code;

typedef enum
{
    NET_SHUTTER_AUTOMATIC = 0,      /* 自动快门 / Automatic shutter */
    NET_SHUTTER_MANUAL = 1          /* 手动快门 / Manual shutter */
} net_enum_shutter_mode;

typedef enum
{
    NET_IMAGE_FLIP_NONE = 0,        /* 不翻转 / No flip */
    NET_IMAGE_FLIP_UP_DOWN = 1,     /* 上下翻转 / Vertical flip */
    NET_IMAGE_FLIP_LEFT_RIGHT = 2,  /* 左右翻转 / Horizontal flip */
    NET_IMAGE_FLIP_CENTER = 3       /* 中心翻转 / Center flip (both) */
} net_enum_image_flip_mode;

typedef enum
{
    NET_TIME_MODE_LOCAL = 0,        /* 本地时间 / Local time */
    NET_TIME_MODE_NTP = 1           /* NTP 时间 / NTP time */
} net_enum_time_mode;

/* ------------- VIEWER (只读 / Read-only) -------------
 * 只能查看设备、网络、热成像、传感器和外设状态；不能修改任何配置。
 * Can only view device, network, thermal, sensor and peripheral status;
 * cannot modify any configuration.
 */
/* ------------- OPERATOR (日常操作 / Daily operation) -------------
 * 可进行日常调优和控制：图像/视频参数、热成像参数、传感器运行模式。
 * Allows daily tuning and control: image/video parameters, thermal
 * parameters, sensor operating modes.
 */
/* ------------- ROOT (管理员 / Administrator) -------------
 * 可进行系统级和维护级操作：用户管理、网络/时间/固件配置、
 * 事件与告警、设备重启/恢复出厂、传感器底层维护等。
 * Allows system-level and maintenance operations: user management,
 * network/time/firmware configuration, events and alarms,
 * device reboot/factory reset, sensor low-level maintenance, etc.
 */
typedef enum
{
    NET_USER_LEVEL_ROOT = 0,        /* 管理员 / Administrator */
    NET_USER_LEVEL_OPERATOR = 1,    /* 操作员 / Operator */
    NET_USER_LEVEL_VIEWER = 2       /* 观察者 / Viewer */
} net_enum_user_level;

typedef enum
{
    NET_OVERLAY_ALIGN_TOP_LEFT = 0,     /* 左上 / Top-left */
    NET_OVERLAY_ALIGN_TOP_RIGHT = 1,    /* 右上 / Top-right */
    NET_OVERLAY_ALIGN_BOTTOM_LEFT = 2,  /* 左下 / Bottom-left */
    NET_OVERLAY_ALIGN_BOTTOM_RIGHT = 3  /* 右下 / Bottom-right */
} net_enum_overlay_align;

typedef enum
{
    NET_OVERLAY_LIGHT = 0,         /* 常亮 / Steady */
    NET_OVERLAY_BLINK = 1          /* 闪烁 / Blinking */
} net_enum_overlay_display_mode;

typedef enum
{
    NET_EXCEPTION_NETWORK_DISCONNECTED = 1, /* 网络断开 / Network disconnected */
    NET_EXCEPTION_NETWORK_RESUMED = 2,      /* 网络恢复 / Network resumed */
    NET_EXCEPTION_VIDEO_LOST = 3            /* 视频丢失 / Video lost */
} net_enum_exception_type;

typedef enum
{
    NET_H264 = 1,                      /* H.264 视频流 / H.264 video stream */
    NET_RGBA = 2,                      /* RGBA 视频流 / RGBA video stream */
    NET_YUV422 = 3,                    /* YUV422 视频流 / YUV422 video stream */
    NET_BGRA = 4                       /* BGRA 视频流 / BGRA video stream */
} net_enum_stream_type;

typedef enum
{
    NET_ALARM_HIGHESTGREATER = 1,     /* 最高温大于阈值 / Max temp above threshold */
    NET_ALARM_HIGHESTLESS = 2,        /* 最高温小于阈值 / Max temp below threshold */
    NET_ALARM_LOWESTGREATER = 3,      /* 最低温大于阈值 / Min temp above threshold */
    NET_ALARM_LOWESTLESS = 4,         /* 最低温小于阈值 / Min temp below threshold */
    NET_ALARM_AVERAGEGREATER = 5,     /* 平均温大于阈值 / Avg temp above threshold */
    NET_ALARM_AVERAGELESS = 6,        /* 平均温小于阈值 / Avg temp below threshold */
} net_enum_thermal_temp_alarm_type;

typedef enum
{
    NET_THERMAL_ALARM_NONE = 0,       /* 无报警 / No alarm */
    NET_THERMAL_ALARM_ALERT = 1,      /* 提醒 / Alert */
    NET_THERMAL_ALARM_WARNING = 2,    /* 预警 / Warning */
    NET_THERMAL_ALARM_ALARM = 3       /* 报警 / Alarm */
} net_enum_thermal_temp_alarm_level;

typedef enum
{
    NET_THERMAL_CAPTURE_PURE_IMAGE = 0, /* 纯图像 / Pure image */
    NET_THERMAL_CAPTURE_IRTEK_IMAGE = 1 /* IRtek 图像 / IRtek image */
} net_enum_thermal_capture_mode;

typedef enum 
{
    NET_THERMAL_T_TWB = 1,              /* 高温差均衡成像分布 / High-contrast balanced imaging distribution */
    NET_THERMAL_TEMP_LINEAR = 2,        /* 温度线性分布 / Temperature linear distribution */
    NET_THERMAL_AD_LINEAR = 3,          /* 信号线性分布 / Signal linear distribution */
} net_enum_color_dist_type;

typedef enum 
{
    NET_THERMAL_FIXED = 1,              /* 固定模式 / Fixed mode */
    NET_THERMAL_AUTO = 2,               /* 自动模式 / Auto mode */
} net_enum_gain_mode;

typedef enum 
{
    IR_FILE_SPOT = 1,               /* 点 / Spot */
    IR_FILE_RECTANGLE = 3,          /* 矩形 / Rectangle */
    IR_FILE_ELLIPSE = 4,            /* 椭圆 / Ellipse */
    IR_FILE_POLYGON = 5,            /* 多边形 / Polygon */
    IR_FILE_POLYLINE = 6            /* 折线 / Polyline */
} net_enum_marker_type;

typedef enum
{
    NET_THERMAL_DATA_RAW = 0x01,        /* 原始数据 / Raw data */
    NET_THERMAL_DATA_GLOBAL = 0x02,     /* 全局数据 / Global data */
    NET_THERMAL_DATA_MARKER = 0x04,     /* 标记数据 / Marker data */
} net_enum_thermal_data_type;

typedef enum
{
    NET_ALARM_THERMAL_TEMP = 1          /* 热成像温度报警 / Thermal temperature alarm */
} net_enum_alarm_type;

typedef enum
{
    NONE = 0,                       /* 无 / None */
    SR = 1                          /* 超分辨率模式 / Super-resolution mode */
} net_enum_sr_mode;

typedef enum
{
    CENTIGRADE = 0,                 /* 摄氏度 / Celsius */
    KELVIN = 1,                     /* 开尔文 / Kelvin */
    FAHRENHEIT = 2                  /* 华氏度 / Fahrenheit */
} net_enum_temp_unit;

typedef enum
{
    METER = 0,                      /* 米 / Meter */
    FOOT = 1                        /* 英尺 / Foot */
} net_enum_distance_unit;

typedef struct
{
    BYTE ip[NET_LEN_32];         /* 设备 IP 地址 / Device IP address */
    USHORT port_no;              /* 设备端口号 / Device port number */
    BYTE user_name[NET_LEN_32];  /* 登录用户名 / Login username */
    BYTE password[NET_LEN_32];   /* 登录密码 / Login password */
    UINT32 timeout_ms;           /* 登录超时时间，单位毫秒 / Login timeout in milliseconds */
} net_login_info;

typedef struct
{
    USHORT major;            /* 主版本号 / Major version */
    USHORT minor;            /* 次版本号 / Minor version */
    USHORT patch;            /* 补丁版本号 / Patch version */
    USHORT build;            /* 构建号 / Build number */
} net_sdk_version;

typedef struct
{
    float min;               /* 温度下限 / Temperature lower bound */
    float max;               /* 温度上限 / Temperature upper bound */
} net_temp_range;

typedef struct
{
    USHORT id;               /* 量程编号 / Range ID */
    net_temp_range range;    /* 对应的温度范围 / Corresponding temperature range */
} net_temp_range_map;

typedef struct
{
    USHORT width;            /* 宽度 / Width */
    USHORT height;           /* 高度 / Height */
} net_size;

typedef struct
{
    BYTE name[NET_LEN_128];              /* 设备名称 / Device name */
    BYTE model[NET_LEN_128];             /* 设备型号 / Device model */
    BYTE spec[NET_LEN_128];              /* 设备规格 / Device specification */
    BYTE serial_no[NET_LEN_16];          /* 设备序列号 / Device serial number */
    BYTE id[NET_LEN_32];                 /* 设备 ID / Device ID */
    BYTE brand[NET_LEN_32];              /* 品牌 / Brand */
    BYTE company[NET_LEN_64];            /* 公司名称 / Company name */
    BYTE firmware_name[NET_LEN_128];     /* 固件名称 / Firmware name */
    BYTE firmware_version[NET_LEN_128];  /* 固件版本 / Firmware version */
} net_device_info;

typedef struct
{
    BYTE name[NET_LEN_64];                           /* 镜头名称 / Lens name */
    float focal;                                     /* 焦距 / Focal length */
    float vfov;                                      /* 垂直视场角 / Vertical FOV */
    float hfov;                                      /* 水平视场角 / Horizontal FOV */
    BYTE band[NET_LEN_32];                           /* 光谱波段 / Spectral band */
    net_temp_range_map range_map[NET_MAX_RANGES];    /* 量程映射表 / Range mapping table */
    USHORT ranges_count;                             /* 量程映射数量 / Number of range mappings */
} net_thermal_lens_info;

typedef struct
{
    net_enum_time_mode time_mode;    /* 时间模式 / Time mode */
    BYTE local_time[NET_LEN_32];     /* 本地时间字符串，time_mode 为 NET_TIME_MODE_LOCAL 时有效 / Local time string, valid when time_mode is NET_TIME_MODE_LOCAL */
    BYTE time_zone[NET_LEN_32];      /* 时区字符串 / Timezone string */
} net_time_info;

typedef struct
{
    USHORT http_port_no;             /* HTTP 端口号 / HTTP port number */
    USHORT rtsp_port_no;             /* RTSP 端口号 / RTSP port number */
    USHORT rtp_port_no;              /* RTP 端口号 / RTP port number */
} net_port_info;

typedef struct
{
    BYTE host_name[NET_LEN_128];    /* NTP 主机名或地址 / NTP hostname or address */
    USHORT port_no;                 /* NTP 端口号 / NTP port number */
    UINT32 syn_interval;            /* 同步间隔 / Synchronization interval */
} net_ntp_info;

typedef struct
{
    UINT32 id;                                   /* 网络接口 ID / Network interface ID */
    BYTE name[NET_LEN_32];                       /* 网卡名称 / NIC name */
    BYTE dhcp;                                   /* 是否启用 DHCP / DHCP enabled */
    BYTE ip[NET_LEN_64];                         /* IP 地址 / IP address */
    BYTE netmask[NET_LEN_64];                    /* 子网掩码 / Subnet mask */
    BYTE dns[NET_MAX_DNS][NET_LEN_64];           /* DNS 地址列表 / DNS address list */
    USHORT dns_count;                            /* DNS 数量 / DNS count */
    BYTE gateway[NET_MAX_GATEWAYS][NET_LEN_64];  /* 网关地址列表 / Gateway address list */
    USHORT gateway_count;                        /* 网关数量 / Gateway count */
} net_network_info;

typedef struct
{
    BYTE use_ssl;                    /* 是否启用 SSL/TLS / SSL/TLS enabled */
    BYTE ip[NET_LEN_64];             /* FTP 服务器地址 / FTP server address */
    USHORT port_no;                  /* FTP 端口号 / FTP port number */
    BYTE user_name[NET_LEN_32];      /* FTP 用户名 / FTP username */
    BYTE pwd[NET_LEN_64];            /* FTP 密码 / FTP password */
    BYTE enable_annoy;               /* 是否启用匿名登录 / Anonymous login enabled */
    BYTE path_template[NET_LEN_128]; /* 文件路径模板 / File path template, e.g. {DEVICE_NAME}/{DEVICE_SERIAL_NO}/123/{DEVICE_IP}/{CHANNEL_NAME}/{CHANNEL_ID}/{yyyyMMddHHmmssfff}.jpg */
} net_ftp_info;

typedef struct
{
    BYTE url[NET_LEN_256]; /* Webhook 地址 / Webhook URL */
} net_webhook_info;

typedef struct
{
    BYTE path[NET_LEN_128];          /* 升级包本地路径 / Local path to upgrade package */
} net_upgrade_info;

typedef struct
{
    UINT32 id;                       /* 用户 ID / User ID */
    net_enum_user_level user_level;  /* 用户权限级别 / User permission level */
    BYTE user_name[NET_LEN_64];      /* 用户名 / Username */
} net_user_info;

typedef struct
{
    UINT32 id;                       /* 用户 ID / User ID */
    net_enum_user_level user_level;  /* 用户权限级别 / User permission level */
    BYTE user_name[NET_LEN_64];      /* 用户名 / Username */
    BYTE login_pwd[NET_LEN_64];      /* 当前登录密码摘要，用于鉴权 / Current login password digest for authentication */
    BYTE pwd[NET_LEN_64];            /* 新密码摘要 / New password digest */
} net_user_pwd_info;

typedef struct
{
    UINT32 id;                         /* 视频流 ID / Stream ID */
    UINT32 bit_rate;                   /* 码率 / Bitrate */
    BYTE codec_type[NET_LEN_16];       /* 编码类型 / Codec type */
    BYTE disable_skip;                 /* 是否禁用跳帧 / Frame skip disabled */
    BYTE enable_rc;                    /* 是否启用码率控制 / Rate control enabled */
    UINT32 gop_size;                   /* GOP 长度 / GOP size */
    UINT32 h264_profile;               /* H.264 Profile / H.264 profile */
    UINT32 initial_qp;                 /* 初始 QP 值 / Initial QP value */
    UINT32 max_frame_rate;             /* 最大帧率 / Max frame rate */
    UINT32 maximum_qp;                /* 最大 QP 值 / Maximum QP value */
    BYTE quality_control_type[NET_LEN_16]; /* 码控类型 / Quality control type */
    USHORT resolution_width;           /* 分辨率宽度 / Resolution width */
    USHORT resolution_height;          /* 分辨率高度 / Resolution height */
} net_stream_info;

typedef struct
{
    UINT32 id;                /* 视频流 ID / Stream ID */
    UINT32 connection_count;  /* 当前连接数 / Current connection count */
} net_stream_status;

typedef struct
{
    USHORT contrast;      /* 对比度 / Contrast */
    USHORT brightness;    /* 亮度 / Brightness */
    BYTE dde_enable;      /* 是否启用 DDE / DDE enabled */
    USHORT dde_value;     /* DDE 强度 / DDE intensity */
} net_image_params;

typedef struct
{
    UINT32 id;                 /* 通道 ID / Channel ID */
    BYTE name[NET_LEN_64];     /* 通道名称 / Channel name */
} net_channel_info;

typedef struct
{
    net_enum_overlay_align align;            /* 对齐方式 / Alignment */
    BYTE enabled;                            /* 是否启用 / Enabled */
    net_enum_overlay_display_mode mode;      /* 显示模式 / Display mode */
    UINT32 x;                                /* X 坐标偏移 / X offset */
    UINT32 y;                                /* Y 坐标偏移 / Y offset */
} net_overlay_block;

typedef struct
{
    net_enum_overlay_align align;            /* 对齐方式 / Alignment */
    BYTE enabled;                            /* 是否启用 / Enabled */
    net_enum_overlay_display_mode mode;      /* 显示模式 / Display mode */
    UINT32 x;                                /* X 坐标偏移 / X offset */
    UINT32 y;                                /* Y 坐标偏移 / Y offset */
    BYTE text[NET_LEN_128];                   /* 文本内容 / Text content */
} net_overlay_custom_block;

typedef struct
{
    net_overlay_block channel_name_overlay;  /* 通道名叠加 / Channel name overlay */
    net_overlay_block time_overlay;          /* 时间叠加 / Time overlay */
    net_overlay_custom_block custom_overlay; /* 自定义信息叠加 / Custom info overlay */
} net_overlay_info;

typedef struct
{
    USHORT range_id;                          /* 量程编号 / Range ID */
    USHORT length;                            /* LUT 有效长度 / LUT valid length */
    USHORT ad[NET_MAX_LUT_POINTS];            /* AD 值数组 / AD value array */
    float t[NET_MAX_LUT_POINTS];              /* 温度值数组 / Temperature value array */
} net_lut_table;

typedef struct
{
    BYTE marker_name[NET_LEN_64];     /* 标识名称 / Marker name */
    BYTE enabled;                     /* 是否启用 / Enabled */
    net_enum_thermal_temp_alarm_type alarm_type;   /* 报警类型 / Alarm type */
    float alarm_temp;                 /* 报警温度 / Alarm temperature */
    UINT32 alarm_delay_time;          /* 报警延迟时间 / Alarm delay time */
    float threshold_temp;             /* 阈值温度 / Threshold temperature */
    UINT32 threshold_delay_time;      /* 阈值延迟时间 / Threshold delay time */
    float trigger_temp;               /* 触发温度 / Trigger temperature */
    UINT32 trigger_delay_time;        /* 触发延迟时间 / Trigger delay time */
} net_thermal_temp_alarm;

typedef struct
{
    USHORT interval;                  /* 报警时间间隔 / Alarm time interval */
} net_thermal_temp_alarm_interval;

typedef struct
{
    float emissivity;     /* 发射率 / Emissivity */
    float distance;       /* 测量距离 / Measurement distance */
    float refl_temp;      /* 反射温度 / Reflected temperature */
    float atm_temp;       /* 大气温度 / Atmospheric temperature */
    float rel_humidity;   /* 相对湿度 / Relative humidity */
} net_thermal_params;

typedef struct
{
    net_enum_gain_mode mode;    /* 温宽模式，默认为自动模式 / Gain mode, defaults to auto */
    float max;                  /* 温宽上限值，取值范围 0~5750 / Gain upper bound, range 0~5750 */
    float min;                  /* 温宽下限值，取值范围 0~5750 / Gain lower bound, range 0~5750 */
    BYTE fixedSpan;             /* 是否使用固定温宽跨度，0x01 启用，其它不启用，默认 0x00 / Fixed span enabled (0x01=on), default 0x00 */
    float span;                 /* 温宽跨度，取值范围 0.1~5750，默认 2.0 / Gain span, range 0.1~5750, default 2.0 */
} net_gain;

/**
 * ref_no 与调色板名称对应关系 / ref_no to palette name mapping:
 *   1  = Iron
 *   2  = Grey
 *   3  = Iron10
 *   4  = Rainbow
 *   5  = Grey10
 *   6  = GreyRed
 *   7  = MidGrey
 *   8  = Yellow
 *   9  = Rain
 *   10 = Rain10
 *   11 = Blue
 *   12 = GlowBow
 *   13 = Medical
 *   14 = MidGreen
 *   15 = Prism
 *   16 = Medical10
 */
typedef struct
{
    UINT32 ref_no;             /* 调色板编号 / Palette reference number */
    BYTE name[NET_LEN_64];    /* 调色板名称 / Palette name */
} net_preset_palette;

typedef struct
{
    UINT32 ref_no;                           /* 调色板编号 / Palette reference number */
    UINT32 colors[NET_MAX_PALETTE_COLORS];   /* 调色板颜色值 / Palette color values */
    USHORT color_count;                      /* 颜色数量 / Color count */
    BYTE inverted;                           /* 是否反转 / Inverted */
} net_palette;

typedef struct
{
    net_enum_overlay_align align;            /* 对齐方式 / Alignment */
    BYTE enabled;                            /* 是否启用 / Enabled */
    BYTE follow;                             /* 是否跟随显示 / Follow display */
    BYTE max_enabled;                        /* 是否显示最高温 / Show max temperature */
    BYTE min_enabled;                        /* 是否显示最低温 / Show min temperature */
    BYTE avg_enabled;                        /* 是否显示平均温 / Show avg temperature */
    BYTE emissivity_enabled;                 /* 是否显示发射率 / Show emissivity */
    UINT32 x;                                /* X 坐标偏移 / X offset */
    UINT32 y;                                /* Y 坐标偏移 / Y offset */
} net_overlay_follow_block;

typedef struct
{
    net_enum_exception_type exception_type;  /* 异常类型 / Exception type */
    UINT32 stream_id;                /* 关联的视频流 ID / Associated stream ID */
    BYTE message[NET_LEN_128];       /* 异常消息 / Exception message */
} net_exception_info;

typedef struct
{
    UINT32 stream_id;                            /* 视频流 ID / Stream ID */
    net_enum_stream_type stream_type;            /* 解码视频流类型 / Decoded stream type */
    USHORT width;                                /* 图像宽度 / Image width */
    USHORT height;                               /* 图像高度 / Image height */
    const BYTE* data;                            /* 视频流数据指针 / Stream data pointer */
    UINT32 data_length;                          /* 数据长度 / Data length */
    UINT64 timestamp_us;                         /* 时间戳，单位微秒 / Timestamp in microseconds */
    BYTE key_frame;                              /* 是否关键帧 / Key frame flag */
} net_stream_data;

typedef struct
{
    net_overlay_block unit_overlay;               /* 单位叠加 / Unit overlay */
    net_overlay_block plt_overlay;                /* 调色板叠加 / Palette overlay */
    net_overlay_block thermal_params_overlay;     /* 测温参数叠加 / Thermal params overlay */
    net_overlay_follow_block global_overlay;      /* 全局温度叠加 / Global temperature overlay */
    net_overlay_follow_block marker_overlay;      /* 测温标识叠加 / Marker temperature overlay */
    net_overlay_block alarm_overlay;              /* 报警信息叠加 / Alarm info overlay */
} net_thermal_overlay_info;

typedef struct
{
    USHORT x;    /* X 坐标 / X coordinate */
    USHORT y;    /* Y 坐标 / Y coordinate */
} net_point;

typedef struct
{
    float temperature;    /* 温度值 / Temperature value */
    net_point point;      /* 对应坐标 / Corresponding coordinate */
} net_temp_point;

typedef struct
{
    net_temp_point max;   /* 最高温点 / Max temperature point */
    net_temp_point min;   /* 最低温点 / Min temperature point */
    float avg_temp;       /* 平均温度 / Average temperature */
} net_temp_point_info;

typedef struct
{
    UINT32 id;                                  /* 标识 ID / Marker ID */
    net_enum_marker_type type;                  /* 标识类型 / Marker type */
    BYTE name[NET_LEN_64];                      /* 标识名称 / Marker name */
    UINT32 local_params;                        /* 是否启用该测温对象的参数 / Use local params for this marker */
    float refl_temp;                            /* 反射温度 / Reflected temperature */
    float distance;                             /* 测量距离 / Measurement distance */
    float emissivity;                           /* 发射率 / Emissivity */
    net_point points[NET_MAX_MARKER_POINTS];    /* 组成测温对象的点集 / Points composing the marker */
    USHORT points_count;                        /* 点数量 / Point count */
} net_marker_info;

typedef struct
{
    net_enum_temp_unit temp_unit;          /* 温度单位 / Temperature unit */
    net_enum_distance_unit distance_unit;  /* 距离单位 / Distance unit */
} net_unit_info;

typedef struct
{
    BYTE marker_name[NET_LEN_64];                 /* 标识名称 / Marker name */
    net_enum_thermal_temp_alarm_level alarm_level;     /* 报警级别 / Alarm level */
} net_marker_alarm_info;

typedef struct
{
    UINT32 marker_id;                  /* 标识 ID / Marker ID */
    net_enum_marker_type type;         /* 标识类型 / Marker type */
    BYTE marker_name[NET_LEN_64];      /* 标识名称 / Marker name */
    net_temp_point_info temp_point_info; /* 该标识的温度统计信息 / Temperature statistics for this marker */
} net_marker_temp_info;

typedef struct
{
    BYTE ip[NET_LEN_64];                 /* 设备 IP 地址 / Device IP address */
    BYTE netmask[NET_LEN_64];            /* 设备子网掩码 / Device subnet mask */
    BYTE name[NET_LEN_128];              /* 设备名称 / Device name */
    BYTE model[NET_LEN_128];             /* 设备型号 / Device model */
    BYTE serial_no[NET_LEN_16];          /* 设备序列号 / Device serial number */
    BYTE id[NET_LEN_32];                 /* 设备 ID / Device ID */
    BYTE firmware_version[NET_LEN_128];  /* 固件版本 / Firmware version */
    BYTE firmware_name[NET_LEN_128];     /* 固件名称 / Firmware name */
    BYTE wandering;                      /* 是否冲突（地址冲突/可恢复） / Address conflict (recoverable) */
} net_discovered_device;

typedef struct
{
    BYTE local_file[NET_LEN_128];    /* 保存图片到本地文件的路径 / Local file path for captured image */
} net_capture_file;

typedef struct
{
    USHORT width;                  /* 原始图像宽度 / Raw image width */
    USHORT height;                 /* 原始图像高度 / Raw image height */
    USHORT range_id;               /* 测温量程 ID / Temperature range ID */
    const USHORT* data;            /* 原始 AD 数据指针（get：至下次 get/关闭句柄；流回调：仅回调期间有效） / Raw AD data pointer (get: valid until next get/handle close; stream callback: valid during callback only) */
    UINT32 points_count;           /* 原始数据点数 / Raw data point count */
    UINT32 lut_len;                /* LUT 数据长度 / LUT data length */
    UINT32 lut_env_temp;           /* LUT 环境温度 / LUT ambient temperature */
    UINT32 sensor_temp;            /* 机芯温度 / Sensor core temperature */
    UINT32 shutter_state;          /* 挡片状态 / Shutter state */
    UINT32 frame_no;               /* 帧序号 / Frame number */
    UINT64 pts;                    /* 同步时间戳 / Synchronization timestamp */
    UINT32 raw_type;               /* 帧类型 / Frame type */
    const float* lut_temp_value;   /* LUT 温度值指针（生命周期同 data） / LUT temperature values (same lifetime as data) */
    const USHORT* lut_ad_value;    /* LUT AD 值指针（生命周期同 data） / LUT AD values (same lifetime as data) */
    UINT32 shutter_adv;            /* 挡片温度 / Shutter temperature */
} net_raw_frame;

typedef struct
{
    net_enum_thermal_data_type data_types;      /* 本次携带的数据类型集合 / Data types included in this frame */
    UINT64 timestamp_us;                        /* 数据时间戳，单位微秒 / Data timestamp in microseconds */
    net_raw_frame raw_frame;                    /* 原始帧数据 / Raw frame data */
    net_temp_point_info global_temp_info;       /* 全局温度信息 / Global temperature info */
    net_marker_temp_info marker_temps[NET_MAX_MARKERS]; /* 测温对象的温度信息 / Marker temperature info */
    USHORT markers_count;                       /* 测温对象的数量 / Marker count */
} net_thermal_data;

typedef struct
{
    net_enum_thermal_data_type data_types;             /* 默认输出的数据类型 / Default output data types */
    UINT32 sample_interval;                            /* 采样间隔时间，单位毫秒，最小 100 / Sample interval in ms, minimum 100 */
} net_thermal_data_config;

typedef struct
{
    net_point points[NET_LEN_64];    /* 坏点坐标列表 / Dead pixel coordinate list */
    USHORT count;                    /* 坏点数量 / Dead pixel count */
} net_dpc_params;

typedef struct
{
    BYTE trigger_ftp_enabled;           /* 是否触发 FTP 上传 / FTP upload trigger enabled */
    BYTE trigger_webhook_enabled;       /* 是否触发 Webhook 上传 / Webhook trigger enabled */
} net_alarm_trigger;

typedef struct
{
    BYTE ip[NET_LEN_64];                          /* 设备 IP 地址 / Device IP address */
    USHORT port_no;                               /* 设备端口号 / Device port number */
    UINT32 channel_id;                            /* 报警通道号 / Alarm channel ID */
} net_alarm_device_info;

typedef struct
{
    UINT64 timestamp_us;                          /* 报警数据时间戳，单位微秒 / Alarm data timestamp in microseconds */
    net_alarm_device_info dev_info;               /* 报警来源设备信息 / Alarm source device info */
    net_raw_frame raw_frame;                      /* 原始帧数据 / Raw frame data */
    net_temp_point_info global_temp_info;         /* 报警时的全局温度统计信息 / Global temperature statistics at alarm time */
    net_marker_alarm_info marker_alarms[NET_MAX_MARKERS];         /* 各标识报警状态（可含 global） / Alarm status for each marker (may include global) */
    USHORT marker_alarms_count;                   /* marker_alarms 有效个数（可含 global） / Valid marker_alarms count (may include global) */
    net_marker_temp_info marker_temps[NET_MAX_MARKERS];           /* 报警时的标识温度统计信息 / Marker temperature statistics at alarm time */
    USHORT markers_count;                         /* marker_temps 有效个数（不含 global） / Valid marker_temps count (excludes global) */
    UINT32 message_length;                        /* 报警消息长度 / Alarm message length */
    BYTE* message;                                /* 报警消息 / Alarm message */
} net_alarm_thermal_temp_info;

/**
 * type 与 alarm_info 对应关系 / type to alarm_info mapping:
 *   NET_ALARM_THERMAL_TEMP : net_alarm_thermal_temp_info 热成像温度报警信息 / Thermal temperature alarm info
 */
typedef struct
{
    net_enum_alarm_type type;                          /* 报警类型 / Alarm type */
    UINT32 alarm_info_length;                          /* 报警信息长度 / Alarm info length */
    BYTE* alarm_info;                                  /* 报警消息 / Alarm info payload */
} net_alarm_info;

typedef void(IRTEK_NETSDK_CALL* on_net_exception_cb)(const net_exception_info* exception_info);
typedef void(IRTEK_NETSDK_CALL* on_net_alarm_cb)(const net_alarm_info* alarm_info);
typedef void(IRTEK_NETSDK_CALL* on_net_stream_received_cb)(const net_stream_data* stream_data);
typedef void(IRTEK_NETSDK_CALL* on_net_thermal_data_stream_received_cb)(const net_thermal_data* frame);

/* SDK 生命周期 / SDK lifecycle */
IRTEK_NETSDK_API INT32 net_sdk_init(void); /* 初始化 SDK / Initialize SDK */
IRTEK_NETSDK_API INT32 net_sdk_cleanup(void); /* 释放 SDK / Cleanup SDK */
IRTEK_NETSDK_API INT32 net_enable_log(BYTE enable); /* 启停 SDK 日志 / Enable/disable SDK logging */
IRTEK_NETSDK_API INT32 net_login(const net_login_info* info, NET_HANDLE* handle); /* 设备登录 / Device login */
IRTEK_NETSDK_API INT32 net_logout(NET_HANDLE* handle); /* 设备注销 / Device logout */
IRTEK_NETSDK_API INT32 net_get_sdk_version(net_sdk_version* version); /* 获取 SDK 版本信息 / Get SDK version */
IRTEK_NETSDK_API INT32 net_set_exception_callback(NET_HANDLE handle, on_net_exception_cb cb); /* 设置异常回调 / Set exception callback */
IRTEK_NETSDK_API INT32 net_start_alarm_listener(NET_HANDLE handle, on_net_alarm_cb cb); /* 开启报警监听 / Start alarm listener */
IRTEK_NETSDK_API INT32 net_stop_alarm_listener(NET_HANDLE alarm_listener_handle); /* 停止报警监听，alarm_listener_handle 为开启报警监听时返回的句柄 / Stop alarm listener; alarm_listener_handle is the handle returned by net_start_alarm_listener */
IRTEK_NETSDK_API INT32 net_get_alarm_trigger(NET_HANDLE handle, const net_enum_alarm_type* type, net_alarm_trigger* trigger); /* 获取报警联动 / Get alarm trigger */
IRTEK_NETSDK_API INT32 net_set_alarm_trigger(NET_HANDLE handle, const net_enum_alarm_type* type, const net_alarm_trigger* trigger); /* 设置报警联动 / Set alarm trigger */
IRTEK_NETSDK_API INT32 net_search_devices(net_discovered_device* devices, USHORT length, USHORT* out_length, UINT32 timeout_ms); /* 搜索设备 / Search devices */

/* 设备管理 / Device management */
IRTEK_NETSDK_API INT32 net_get_device_info(NET_HANDLE handle, net_device_info* info); /* 获取设备信息 / Get device info */
IRTEK_NETSDK_API INT32 net_set_device_info(NET_HANDLE handle, const net_device_info* info); /* 设置设备信息 / Set device info */
IRTEK_NETSDK_API INT32 net_device_restart(NET_HANDLE handle); /* 设备重启 / Restart device */
IRTEK_NETSDK_API INT32 net_device_reset(NET_HANDLE handle); /* 设备重置 / Factory reset device */
IRTEK_NETSDK_API INT32 net_device_upgrade(NET_HANDLE handle, const net_upgrade_info* info); /* 远程升级 / Remote upgrade */
IRTEK_NETSDK_API INT32 net_get_ntp_server(NET_HANDLE handle, net_ntp_info* info); /* 获取 NTP 服务 / Get NTP server */
IRTEK_NETSDK_API INT32 net_set_ntp_server(NET_HANDLE handle, const net_ntp_info* info); /* 设置 NTP 服务 / Set NTP server */
IRTEK_NETSDK_API INT32 net_get_time(NET_HANDLE handle, net_time_info* info); /* 获取设备时间 / Get device time */
IRTEK_NETSDK_API INT32 net_set_time(NET_HANDLE handle, const net_time_info* info); /* 设置设备时间 / Set device time */
IRTEK_NETSDK_API INT32 net_get_port(NET_HANDLE handle, net_port_info* info); /* 获取设备服务端口 / Get device service ports */
IRTEK_NETSDK_API INT32 net_set_port(NET_HANDLE handle, const net_port_info* info); /* 设置设备服务端口 / Set device service ports */
IRTEK_NETSDK_API INT32 net_get_networks(NET_HANDLE handle, net_network_info* networks, USHORT length, USHORT* out_length); /* 获取网络参数列表 / Get network parameter list */
IRTEK_NETSDK_API INT32 net_get_network(NET_HANDLE handle, UINT32 id, net_network_info* info); /* 获取网络参数 / Get network parameters */
IRTEK_NETSDK_API INT32 net_set_network(NET_HANDLE handle, UINT32 id, const net_network_info* info); /* 设置网络参数 / Set network parameters */
IRTEK_NETSDK_API INT32 net_get_ftp(NET_HANDLE handle, net_ftp_info* info); /* 获取 FTP 配置 / Get FTP configuration */
IRTEK_NETSDK_API INT32 net_set_ftp(NET_HANDLE handle, const net_ftp_info* info); /* 设置 FTP 配置 / Set FTP configuration */
IRTEK_NETSDK_API INT32 net_test_ftp(NET_HANDLE handle, const net_ftp_info* info); /* 测试 FTP 配置 / Test FTP configuration */
IRTEK_NETSDK_API INT32 net_get_webhook(NET_HANDLE handle, net_webhook_info* info); /* 获取 Webhook 地址 / Get Webhook URL */
IRTEK_NETSDK_API INT32 net_set_webhook(NET_HANDLE handle, const net_webhook_info* info); /* 设置 Webhook 地址 / Set Webhook URL */

/* 用户管理 / User management */
IRTEK_NETSDK_API INT32 net_get_users(NET_HANDLE handle, net_user_info* users, USHORT length, USHORT* out_length); /* 获取用户列表 / Get user list */
IRTEK_NETSDK_API INT32 net_get_user(NET_HANDLE handle, UINT32 id, net_user_info* user); /* 获取用户信息 / Get user info */
IRTEK_NETSDK_API INT32 net_update_user(NET_HANDLE handle, const net_user_pwd_info* user); /* 添加或修改用户 / Add or update user */
IRTEK_NETSDK_API INT32 net_delete_user(NET_HANDLE handle, UINT32 id); /* 删除用户 / Delete user */

/* 视频流 / Streaming */
IRTEK_NETSDK_API INT32 net_get_streams(NET_HANDLE handle, net_stream_info* streams, USHORT length, USHORT* out_length); /* 获取视频流列表 / Get stream list */
IRTEK_NETSDK_API INT32 net_get_stream(NET_HANDLE handle, UINT32 stream_id, net_stream_info* stream); /* 获取视频流参数 / Get stream parameters */
IRTEK_NETSDK_API INT32 net_get_stream_status(NET_HANDLE handle, UINT32 stream_id, net_stream_status* status); /* 获取视频流状态 / Get stream status */
IRTEK_NETSDK_API INT32 net_start_stream_play(NET_HANDLE handle, net_stream_info* stream, net_enum_stream_type stream_type, on_net_stream_received_cb cb); /* 开启视频流回调 / Start stream playback */
IRTEK_NETSDK_API INT32 net_stop_stream_play(NET_HANDLE stream_handle); /* 停止视频流回调，stream_handle 为开启视频流回调时返回的句柄 / Stop stream playback; stream_handle is the handle returned by net_start_stream_play */
IRTEK_NETSDK_API INT32 net_set_stream_callback(NET_HANDLE stream_handle, on_net_stream_received_cb cb); /* 设置视频流回调，stream_handle 为开启视频流回调时返回的句柄 / Set stream callback; stream_handle is the handle returned by net_start_stream_play */
IRTEK_NETSDK_API INT32 net_start_stream_record(NET_HANDLE stream_handle, const BYTE* local_file, USHORT length); /* 开始本地录制视频流，stream_handle 为开启视频流回调时返回的句柄 / Start local recording; stream_handle is the handle returned by net_start_stream_play */
IRTEK_NETSDK_API INT32 net_stop_stream_record(NET_HANDLE stream_handle); /* 停止本地录制视频流，stream_handle 为开启视频流回调时返回的句柄 / Stop local recording; stream_handle is the handle returned by net_start_stream_play */

/* 显示 / Display */
IRTEK_NETSDK_API INT32 net_get_channels(NET_HANDLE handle, net_channel_info* channel, USHORT length, USHORT* out_length); /* 获取通道列表 / Get channel list */
IRTEK_NETSDK_API INT32 net_get_channel(NET_HANDLE handle, UINT32 channel_id, net_channel_info* channel); /* 获取通道信息 / Get channel info */
IRTEK_NETSDK_API INT32 net_set_channel(NET_HANDLE handle, UINT32 channel_id, const net_channel_info* channel); /* 设置通道信息 / Set channel info */
IRTEK_NETSDK_API INT32 net_get_overlay(NET_HANDLE handle, UINT32 channel_id, net_overlay_info* overlay); /* 获取字符叠加 / Get text overlay */
IRTEK_NETSDK_API INT32 net_set_overlay(NET_HANDLE handle, UINT32 channel_id, const net_overlay_info* overlay); /* 设置字符叠加 / Set text overlay */
IRTEK_NETSDK_API INT32 net_get_image_params(NET_HANDLE handle, UINT32 channel_id, net_image_params* params); /* 获取图像参数 / Get image parameters */
IRTEK_NETSDK_API INT32 net_set_image_params(NET_HANDLE handle, UINT32 channel_id, const net_image_params* params); /* 设置图像参数 / Set image parameters */
IRTEK_NETSDK_API INT32 net_get_image_flip(NET_HANDLE handle, UINT32 channel_id, net_enum_image_flip_mode* mode); /* 获取图像翻转模式 / Get image flip mode */
IRTEK_NETSDK_API INT32 net_set_image_flip(NET_HANDLE handle, UINT32 channel_id, const net_enum_image_flip_mode* mode); /* 设置图像翻转模式 / Set image flip mode */

/* 热成像 / Thermal */
IRTEK_NETSDK_API INT32 net_get_thermal_lens_info(NET_HANDLE handle, net_thermal_lens_info* info); /* 获取热像镜头信息 / Get thermal lens info */
IRTEK_NETSDK_API INT32 net_thermal_calibrate(NET_HANDLE handle); /* 执行热像校正 / Perform thermal calibration */
IRTEK_NETSDK_API INT32 net_get_thermal_shutter_mode(NET_HANDLE handle, net_enum_shutter_mode* mode); /* 获取热像校正模式 / Get thermal shutter mode */
IRTEK_NETSDK_API INT32 net_set_thermal_shutter_mode(NET_HANDLE handle, const net_enum_shutter_mode* mode); /* 设置热像校正模式 / Set thermal shutter mode */
IRTEK_NETSDK_API INT32 net_get_thermal_shutter_time(NET_HANDLE handle, USHORT* minutes); /* 获取热像自动校正时间 / Get thermal auto-calibration interval */
IRTEK_NETSDK_API INT32 net_set_thermal_shutter_time(NET_HANDLE handle, const USHORT* minutes); /* 设置挡片自动校正时间 / Set shutter auto-calibration interval */
IRTEK_NETSDK_API INT32 net_get_thermal_resolution(NET_HANDLE handle, net_size* size); /* 获取热像分辨率 / Get thermal resolution */
IRTEK_NETSDK_API INT32 net_get_thermal_range(NET_HANDLE handle, USHORT* range_id); /* 获取热像测温量程 / Get thermal temperature range */
IRTEK_NETSDK_API INT32 net_set_thermal_range(NET_HANDLE handle, const USHORT* range_id); /* 设置热像测温量程 / Set thermal temperature range */
IRTEK_NETSDK_API INT32 net_get_thermal_lut(NET_HANDLE handle, net_lut_table* lut); /* 获取热像 LUT 表 / Get thermal LUT table */
IRTEK_NETSDK_API INT32 net_get_thermal_params(NET_HANDLE handle, net_thermal_params* params); /* 获取热像测温参数 / Get thermal measurement parameters */
IRTEK_NETSDK_API INT32 net_set_thermal_params(NET_HANDLE handle, const net_thermal_params* params); /* 设置热像测温参数 / Set thermal measurement parameters */
IRTEK_NETSDK_API INT32 net_get_thermal_color_dist(NET_HANDLE handle, net_enum_color_dist_type* type); /* 获取热像色彩分布类型 / Get thermal color distribution type */
IRTEK_NETSDK_API INT32 net_set_thermal_color_dist(NET_HANDLE handle, const net_enum_color_dist_type* type); /* 设置热像色彩分布类型 / Set thermal color distribution type */
IRTEK_NETSDK_API INT32 net_get_thermal_gain(NET_HANDLE handle, net_gain* gain); /* 获取热像温宽模式 / Get thermal gain mode */
IRTEK_NETSDK_API INT32 net_set_thermal_gain(NET_HANDLE handle, const net_gain* gain); /* 设置热像温宽模式 / Set thermal gain mode */
IRTEK_NETSDK_API INT32 net_get_thermal_preset_palettes(NET_HANDLE handle, net_preset_palette* palettes, USHORT length, USHORT* out_length); /* 获取热像调色板列表 / Get thermal preset palette list */
IRTEK_NETSDK_API INT32 net_get_thermal_palette(NET_HANDLE handle, net_palette* palette); /* 获取热像调色板 / Get thermal palette */
IRTEK_NETSDK_API INT32 net_set_thermal_palette(NET_HANDLE handle, const net_palette* palette); /* 设置热像调色板 / Set thermal palette */
IRTEK_NETSDK_API INT32 net_get_thermal_overlay(NET_HANDLE handle, net_thermal_overlay_info* overlay); /* 获取热像叠加 / Get thermal overlay */
IRTEK_NETSDK_API INT32 net_set_thermal_overlay(NET_HANDLE handle, const net_thermal_overlay_info* overlay); /* 设置热像叠加 / Set thermal overlay */
IRTEK_NETSDK_API INT32 net_get_thermal_unit(NET_HANDLE handle, net_unit_info* info); /* 获取热像单位 / Get thermal unit */
IRTEK_NETSDK_API INT32 net_set_thermal_unit(NET_HANDLE handle, const net_unit_info* info); /* 设置热像单位 / Set thermal unit */
IRTEK_NETSDK_API INT32 net_get_thermal_capture(NET_HANDLE handle, net_enum_thermal_capture_mode mode, net_capture_file* capture_file); /* 获取热像抓拍文件 / Get thermal capture file */
IRTEK_NETSDK_API INT32 net_get_thermal_markers(NET_HANDLE handle, net_marker_info* markers, USHORT length, USHORT* out_length); /* 获取热像测温标识列表 / Get thermal marker list */
IRTEK_NETSDK_API INT32 net_set_thermal_markers(NET_HANDLE handle, const net_marker_info* markers, USHORT length); /* 设置热像测温标识列表 / Set thermal marker list */
IRTEK_NETSDK_API INT32 net_get_thermal_temp_alarms(NET_HANDLE handle, net_thermal_temp_alarm* alarms, USHORT length, USHORT* out_length); /* 获取热像测温报警列表 / Get thermal temperature alarm list */
IRTEK_NETSDK_API INT32 net_set_thermal_temp_alarms(NET_HANDLE handle, const net_thermal_temp_alarm* alarms, USHORT length); /* 设置热像测温报警列表 / Set thermal temperature alarm list */
IRTEK_NETSDK_API INT32 net_get_thermal_temp_alarm_interval(NET_HANDLE handle, net_thermal_temp_alarm_interval* interval); /* 获取热像测温报警时间间隔 / Get thermal alarm interval */
IRTEK_NETSDK_API INT32 net_set_thermal_temp_alarm_interval(NET_HANDLE handle, const net_thermal_temp_alarm_interval* interval); /* 设置热像测温报警时间间隔 / Set thermal alarm interval */

IRTEK_NETSDK_API INT32 net_set_thermal_dpc(NET_HANDLE handle, const net_dpc_params* param); /* 设置热像坏点 / Set thermal dead pixel correction */

IRTEK_NETSDK_API INT32 net_start_thermal_data_stream(NET_HANDLE handle, const net_thermal_data_config* config, on_net_thermal_data_stream_received_cb cb); /* 开启热像数据流回调，配置为临时配置，不会保存到设备 / Start thermal data stream callback; config is temporary and not persisted to device */
IRTEK_NETSDK_API INT32 net_stop_thermal_data_stream(NET_HANDLE thermal_data_handle); /* 停止热像数据流回调，thermal_data_handle 为开启热像数据流回调时返回的句柄 / Stop thermal data stream; thermal_data_handle is the handle returned by net_start_thermal_data_stream */

IRTEK_NETSDK_API INT32 net_get_thermal_data(NET_HANDLE handle, const UINT32 data_types, net_thermal_data* realdata); /* 获取热像数据 / Get thermal data snapshot */

#ifdef __cplusplus
}
#endif
