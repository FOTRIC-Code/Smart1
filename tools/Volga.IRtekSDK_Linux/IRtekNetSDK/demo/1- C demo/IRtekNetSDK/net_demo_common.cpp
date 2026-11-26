#include "net_demo_common.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <sstream>

// ---------------------------------------------------------------------------
// String / parse utilities
// ---------------------------------------------------------------------------

const char* as_cstr(const BYTE* value)
{
    return reinterpret_cast<const char*>(value);
}

void copy_text(BYTE* dst, size_t dst_len, const std::string& src)
{
    if (!dst || dst_len == 0)
    {
        return;
    }
    std::snprintf(reinterpret_cast<char*>(dst), dst_len, "%s", src.c_str());
}

std::string to_lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string flag_key(const std::string& arg)
{
    if (arg.rfind("--", 0) == 0)
    {
        return arg.substr(2);
    }
    if (arg.rfind("-", 0) == 0)
    {
        return arg.substr(1);
    }
    return arg;
}

const char* ec_to_text(INT32 ec)
{
    switch (ec)
    {
    case NET_EC_OK: return "OK";
    case NET_EC_FAIL: return "FAIL";
    case NET_EC_MEMORY: return "MEMORY";
    case NET_EC_NULL_POINTER: return "NULL_POINTER";
    case NET_EC_INVALID_PARAM: return "INVALID_PARAM";
    case NET_EC_NOT_SUPPORTED: return "NOT_SUPPORTED";
    case NET_EC_NOT_INITIALIZED: return "NOT_INITIALIZED";
    case NET_EC_HTTP_ERROR: return "HTTP_ERROR";
    case NET_EC_AUTH_FAILED: return "AUTH_FAILED";
    case NET_EC_TIMEOUT: return "TIMEOUT";
    default: return "UNKNOWN";
    }
}

void log_call(std::ostream& out, const char* api, INT32 rc, const std::string& detail)
{
    out << api << " => " << rc;
    if (rc > 0)
    {
        out << " (handle)";
    }
    else
    {
        out << " (" << ec_to_text(rc) << ")";
    }
    if (!detail.empty())
    {
        out << " " << detail;
    }
    out << "\n";
}

void log_note(std::ostream& out, const std::string& message)
{
    out << "[note] " << message << "\n";
}

std::optional<std::string> get_value(const NetDemoOptions& options, const std::string& key)
{
    const auto it = options.values.find(key);
    if (it == options.values.end())
    {
        return std::nullopt;
    }
    return it->second;
}

bool has_flag(const NetDemoOptions& options, const std::string& key)
{
    return options.flags.find(key) != options.flags.end();
}

std::optional<bool> parse_bool_value(const std::string& raw)
{
    const std::string value = to_lower(raw);
    if (value == "1" || value == "true" || value == "yes" || value == "on" || value == "enabled")
    {
        return true;
    }
    if (value == "0" || value == "false" || value == "no" || value == "off" || value == "disabled")
    {
        return false;
    }
    return std::nullopt;
}

std::optional<bool> get_bool_option(const NetDemoOptions& options, const std::string& key)
{
    const auto value = get_value(options, key);
    if (!value.has_value())
    {
        return std::nullopt;
    }
    return parse_bool_value(*value);
}

std::optional<float> parse_float_value(const std::string& raw)
{
    if (raw.empty())
    {
        return std::nullopt;
    }
    char* end = nullptr;
    const float parsed = std::strtof(raw.c_str(), &end);
    if (!end || *end != '\0')
    {
        return std::nullopt;
    }
    return parsed;
}

std::optional<float> get_float_option(const NetDemoOptions& options, const std::string& key)
{
    const auto value = get_value(options, key);
    if (!value.has_value())
    {
        return std::nullopt;
    }
    return parse_float_value(*value);
}

std::string format_bool(BYTE value)
{
    return value != 0 ? "true" : "false";
}

// ---------------------------------------------------------------------------
// Enum text / parse helpers
// ---------------------------------------------------------------------------

std::string overlay_align_text(net_enum_overlay_align align)
{
    switch (align)
    {
    case NET_OVERLAY_ALIGN_TOP_LEFT: return "top-left";
    case NET_OVERLAY_ALIGN_TOP_RIGHT: return "top-right";
    case NET_OVERLAY_ALIGN_BOTTOM_LEFT: return "bottom-left";
    case NET_OVERLAY_ALIGN_BOTTOM_RIGHT: return "bottom-right";
    default: return "unknown";
    }
}

std::optional<net_enum_overlay_align> parse_overlay_align(const std::string& raw)
{
    const std::string value = to_lower(raw);
    if (value == "top-left" || value == "tl")
    {
        return NET_OVERLAY_ALIGN_TOP_LEFT;
    }
    if (value == "top-right" || value == "tr")
    {
        return NET_OVERLAY_ALIGN_TOP_RIGHT;
    }
    if (value == "bottom-left" || value == "bl")
    {
        return NET_OVERLAY_ALIGN_BOTTOM_LEFT;
    }
    if (value == "bottom-right" || value == "br")
    {
        return NET_OVERLAY_ALIGN_BOTTOM_RIGHT;
    }
    return std::nullopt;
}

std::string overlay_mode_text(net_enum_overlay_display_mode mode)
{
    switch (mode)
    {
    case NET_OVERLAY_BLINK: return "blink";
    case NET_OVERLAY_LIGHT:
    default: return "light";
    }
}

std::optional<net_enum_overlay_display_mode> parse_overlay_mode(const std::string& raw)
{
    const std::string value = to_lower(raw);
    if (value == "light" || value == "steady")
    {
        return NET_OVERLAY_LIGHT;
    }
    if (value == "blink")
    {
        return NET_OVERLAY_BLINK;
    }
    return std::nullopt;
}

std::string stream_type_text(net_enum_stream_type type)
{
    switch (type)
    {
    case NET_H264: return "H264";
    case NET_RGBA: return "RGBA";
    case NET_YUV422: return "YUV422";
    case NET_BGRA:
    default: return "BGRA";
    }
}

std::optional<net_enum_stream_type> parse_stream_type(const std::string& raw)
{
    const std::string value = to_lower(raw);
    if (value == "bgra")
    {
        return NET_BGRA;
    }
    if (value == "rgba")
    {
        return NET_RGBA;
    }
    if (value == "yuv422")
    {
        return NET_YUV422;
    }
    if (value == "h264")
    {
        return NET_H264;
    }
    return std::nullopt;
}

std::string image_flip_text(net_enum_image_flip_mode mode)
{
    switch (mode)
    {
    case NET_IMAGE_FLIP_UP_DOWN: return "up-down";
    case NET_IMAGE_FLIP_LEFT_RIGHT: return "left-right";
    case NET_IMAGE_FLIP_CENTER: return "center";
    case NET_IMAGE_FLIP_NONE:
    default: return "none";
    }
}

std::optional<net_enum_image_flip_mode> parse_image_flip_mode(const std::string& raw)
{
    const std::string value = to_lower(raw);
    if (value == "none")
    {
        return NET_IMAGE_FLIP_NONE;
    }
    if (value == "up-down" || value == "vertical")
    {
        return NET_IMAGE_FLIP_UP_DOWN;
    }
    if (value == "left-right" || value == "horizontal")
    {
        return NET_IMAGE_FLIP_LEFT_RIGHT;
    }
    if (value == "center" || value == "both")
    {
        return NET_IMAGE_FLIP_CENTER;
    }
    return std::nullopt;
}

std::optional<net_enum_thermal_capture_mode> parse_capture_mode(const std::string& raw)
{
    const std::string value = to_lower(raw);
    if (value == "pure" || value == "jpeg")
    {
        return NET_THERMAL_CAPTURE_PURE_IMAGE;
    }
    if (value == "irtek" || value == "thermal")
    {
        return NET_THERMAL_CAPTURE_IRTEK_IMAGE;
    }
    return std::nullopt;
}

std::string time_mode_text(net_enum_time_mode mode)
{
    return mode == NET_TIME_MODE_NTP ? "ntp" : "local";
}

std::optional<net_enum_time_mode> parse_time_mode(const std::string& raw)
{
    const std::string value = to_lower(raw);
    if (value == "local")
    {
        return NET_TIME_MODE_LOCAL;
    }
    if (value == "ntp")
    {
        return NET_TIME_MODE_NTP;
    }
    return std::nullopt;
}

std::optional<net_enum_user_level> parse_user_level(const std::string& raw)
{
    const std::string value = to_lower(raw);
    if (value == "root" || value == "admin")
    {
        return NET_USER_LEVEL_ROOT;
    }
    if (value == "operator")
    {
        return NET_USER_LEVEL_OPERATOR;
    }
    if (value == "viewer" || value == "read-only")
    {
        return NET_USER_LEVEL_VIEWER;
    }
    return std::nullopt;
}

std::string user_level_text(net_enum_user_level level)
{
    switch (level)
    {
    case NET_USER_LEVEL_ROOT: return "root";
    case NET_USER_LEVEL_OPERATOR: return "operator";
    case NET_USER_LEVEL_VIEWER:
    default: return "viewer";
    }
}

std::optional<net_enum_shutter_mode> parse_shutter_mode(const std::string& raw)
{
    const std::string value = to_lower(raw);
    if (value == "automatic" || value == "auto")
    {
        return NET_SHUTTER_AUTOMATIC;
    }
    if (value == "manual")
    {
        return NET_SHUTTER_MANUAL;
    }
    return std::nullopt;
}

std::string shutter_mode_text(net_enum_shutter_mode mode)
{
    return mode == NET_SHUTTER_MANUAL ? "manual" : "automatic";
}

std::optional<net_enum_temp_unit> parse_temp_unit(const std::string& raw)
{
    const std::string value = to_lower(raw);
    if (value == "c" || value == "celsius" || value == "centigrade")
    {
        return CENTIGRADE;
    }
    if (value == "k" || value == "kelvin")
    {
        return KELVIN;
    }
    if (value == "f" || value == "fahrenheit")
    {
        return FAHRENHEIT;
    }
    return std::nullopt;
}

std::string temp_unit_text(net_enum_temp_unit unit)
{
    switch (unit)
    {
    case KELVIN: return "K";
    case FAHRENHEIT: return "F";
    case CENTIGRADE:
    default: return "C";
    }
}

std::optional<net_enum_distance_unit> parse_distance_unit(const std::string& raw)
{
    const std::string value = to_lower(raw);
    if (value == "m" || value == "meter" || value == "meters")
    {
        return METER;
    }
    if (value == "ft" || value == "foot" || value == "feet")
    {
        return FOOT;
    }
    return std::nullopt;
}

std::string distance_unit_text(net_enum_distance_unit unit)
{
    return unit == FOOT ? "ft" : "m";
}

std::optional<net_enum_color_dist_type> parse_color_dist_type(const std::string& raw)
{
    const std::string value = to_lower(raw);
    if (value == "twb")
    {
        return NET_THERMAL_T_TWB;
    }
    if (value == "temp-linear" || value == "temperature-linear")
    {
        return NET_THERMAL_TEMP_LINEAR;
    }
    if (value == "ad-linear")
    {
        return NET_THERMAL_AD_LINEAR;
    }
    return std::nullopt;
}

std::string color_dist_text(net_enum_color_dist_type type)
{
    switch (type)
    {
    case NET_THERMAL_TEMP_LINEAR: return "temp-linear";
    case NET_THERMAL_AD_LINEAR: return "ad-linear";
    case NET_THERMAL_T_TWB:
    default: return "twb";
    }
}

std::optional<net_enum_gain_mode> parse_gain_mode(const std::string& raw)
{
    const std::string value = to_lower(raw);
    if (value == "fixed")
    {
        return NET_THERMAL_FIXED;
    }
    if (value == "auto")
    {
        return NET_THERMAL_AUTO;
    }
    return std::nullopt;
}

std::string gain_mode_text(net_enum_gain_mode mode)
{
    return mode == NET_THERMAL_FIXED ? "fixed" : "auto";
}

std::optional<net_enum_thermal_temp_alarm_type> parse_alarm_type(const std::string& raw)
{
    const std::string value = to_lower(raw);
    if (value == "highest-greater" || value == "max-gt")
    {
        return NET_ALARM_HIGHESTGREATER;
    }
    if (value == "highest-less" || value == "max-lt")
    {
        return NET_ALARM_HIGHESTLESS;
    }
    if (value == "lowest-greater" || value == "min-gt")
    {
        return NET_ALARM_LOWESTGREATER;
    }
    if (value == "lowest-less" || value == "min-lt")
    {
        return NET_ALARM_LOWESTLESS;
    }
    if (value == "average-greater" || value == "avg-gt")
    {
        return NET_ALARM_AVERAGEGREATER;
    }
    if (value == "average-less" || value == "avg-lt")
    {
        return NET_ALARM_AVERAGELESS;
    }
    return std::nullopt;
}

std::string alarm_type_text(net_enum_thermal_temp_alarm_type type)
{
    switch (type)
    {
    case NET_ALARM_HIGHESTLESS: return "highest-less";
    case NET_ALARM_LOWESTGREATER: return "lowest-greater";
    case NET_ALARM_LOWESTLESS: return "lowest-less";
    case NET_ALARM_AVERAGEGREATER: return "average-greater";
    case NET_ALARM_AVERAGELESS: return "average-less";
    case NET_ALARM_HIGHESTGREATER:
    default: return "highest-greater";
    }
}

std::string marker_type_text(net_enum_marker_type type)
{
    switch (type)
    {
    case IR_FILE_SPOT: return "spot";
    case IR_FILE_RECTANGLE: return "rectangle";
    case IR_FILE_ELLIPSE: return "ellipse";
    case IR_FILE_POLYGON: return "polygon";
    case IR_FILE_POLYLINE: return "polyline";
    default: return "unknown";
    }
}

net_enum_thermal_data_type thermal_stream_mask(const NetDemoOptions& options)
{
    if (options.thermal_stream_all || has_flag(options, "thermal-stream-all"))
    {
        return static_cast<net_enum_thermal_data_type>(
            NET_THERMAL_DATA_RAW | NET_THERMAL_DATA_GLOBAL | NET_THERMAL_DATA_MARKER);
    }
    return static_cast<net_enum_thermal_data_type>(NET_THERMAL_DATA_GLOBAL | NET_THERMAL_DATA_MARKER);
}

// ---------------------------------------------------------------------------
// on_exception callback (used by DemoSession)
// ---------------------------------------------------------------------------

static void IRTEK_NETSDK_CALL on_exception(const net_exception_info* exception_info)
{
    if (!exception_info)
    {
        return;
    }
    std::fprintf(stderr,
                 "[exception] type=%u stream_id=%u message=%s\n",
                 static_cast<unsigned>(exception_info->exception_type),
                 exception_info->stream_id,
                 as_cstr(exception_info->message));
}

// ---------------------------------------------------------------------------
// DemoSession
// ---------------------------------------------------------------------------

static void print_sdk_version(std::ostream& out)
{
    net_sdk_version version{};
    const INT32 rc = net_get_sdk_version(&version);
    log_call(out, "net_get_sdk_version", rc);
    if (rc == NET_EC_OK)
    {
        out << "IRtekNetSDK version: "
            << version.major << "."
            << version.minor << "."
            << version.patch << "."
            << version.build << "\n";
    }
}

DemoSession::DemoSession(std::ostream& out) : out_(out) {}

DemoSession::~DemoSession()
{
    close(false);
}

bool DemoSession::init(const NetDemoOptions& options)
{
    const INT32 init_rc = net_sdk_init();
    log_call(out_, "net_sdk_init", init_rc);
    if (init_rc != NET_EC_OK)
    {
        return false;
    }
    initialized_ = true;

    const INT32 log_rc = net_enable_log(options.enable_log ? 1 : 0);
    log_call(out_, "net_enable_log", log_rc, std::string("enabled=") + (options.enable_log ? "true" : "false"));
    print_sdk_version(out_);
    return true;
}

bool DemoSession::login(const NetDemoOptions& options)
{
    net_login_info login{};
    copy_text(login.ip, sizeof(login.ip), options.ip);
    login.port_no = options.port;
    copy_text(login.user_name, sizeof(login.user_name), options.user_name);
    copy_text(login.password, sizeof(login.password), options.password);
    login.timeout_ms = kLoginTimeoutMs;

    const INT32 login_rc = net_login(&login, &handle_);
    log_call(out_, "net_login", login_rc, "ip=" + options.ip + " port=" + std::to_string(options.port));
    return login_rc == NET_EC_OK && handle_ != nullptr;
}

void DemoSession::set_exception_callback()
{
    const INT32 rc = net_set_exception_callback(handle_, on_exception);
    log_call(out_, "net_set_exception_callback", rc);
}

NET_HANDLE DemoSession::handle() const
{
    return handle_;
}

void DemoSession::close(bool log_calls)
{
    if (handle_ != nullptr)
    {
        INT32 rc = net_logout(&handle_);
        if (log_calls)
        {
            log_call(out_, "net_logout", rc);
        }
        handle_ = nullptr;
    }

    if (initialized_)
    {
        INT32 rc = net_sdk_cleanup();
        if (log_calls)
        {
            log_call(out_, "net_sdk_cleanup", rc);
        }
        initialized_ = false;
    }
}

// ---------------------------------------------------------------------------
// Shared data-fetch helpers
// ---------------------------------------------------------------------------

bool get_channels(NET_HANDLE handle, std::vector<net_channel_info>& channels, USHORT& count, std::ostream& out)
{
    channels.assign(kMaxListEntries, net_channel_info{});
    count = 0;
    const INT32 rc = net_get_channels(handle, channels.data(), static_cast<USHORT>(channels.size()), &count);
    log_call(out, "net_get_channels", rc);
    return rc == NET_EC_OK && count > 0;
}

bool get_streams(NET_HANDLE handle, std::vector<net_stream_info>& streams, USHORT& count, std::ostream& out)
{
    streams.assign(kMaxListEntries, net_stream_info{});
    count = 0;
    const INT32 rc = net_get_streams(handle, streams.data(), static_cast<USHORT>(streams.size()), &count);
    log_call(out, "net_get_streams", rc);
    return rc == NET_EC_OK && count > 0;
}

bool get_networks(NET_HANDLE handle, std::vector<net_network_info>& networks, USHORT& count, std::ostream& out)
{
    networks.assign(4, net_network_info{});
    count = 0;
    const INT32 rc = net_get_networks(handle, networks.data(), static_cast<USHORT>(networks.size()), &count);
    log_call(out, "net_get_networks", rc);
    return rc == NET_EC_OK && count > 0;
}

bool get_markers(NET_HANDLE handle, std::vector<net_marker_info>& markers, USHORT& count, std::ostream& out)
{
    markers.assign(NET_MAX_MARKERS, net_marker_info{});
    count = 0;
    const INT32 rc = net_get_thermal_markers(handle, markers.data(), static_cast<USHORT>(markers.size()), &count);
    log_call(out, "net_get_thermal_markers", rc);
    if (rc != NET_EC_OK)
    {
        return false;
    }

    for (USHORT i = 0; i < count; ++i)
    {
        if (markers[i].type == static_cast<net_enum_marker_type>(2))
        {
            markers[i].type = IR_FILE_POLYLINE;
        }
    }
    return true;
}

bool get_alarm_rules(NET_HANDLE handle, std::vector<net_thermal_temp_alarm>& alarms, USHORT& count, std::ostream& out)
{
    alarms.assign(NET_MAX_MARKERS, net_thermal_temp_alarm{});
    count = 0;
    const INT32 rc = net_get_thermal_temp_alarms(handle, alarms.data(), static_cast<USHORT>(alarms.size()), &count);
    log_call(out, "net_get_thermal_temp_alarms", rc);
    return rc == NET_EC_OK;
}

std::optional<UINT32> resolve_channel_id(NET_HANDLE handle, const NetDemoOptions& options, std::ostream& out)
{
    std::vector<net_channel_info> channels;
    USHORT count = 0;
    if (!get_channels(handle, channels, count, out))
    {
        return std::nullopt;
    }

    const auto requested = get_integral_option<UINT32>(options, "channel-id");
    if (!requested.has_value())
    {
        return channels[0].id;
    }

    for (USHORT i = 0; i < count; ++i)
    {
        if (channels[i].id == *requested)
        {
            return channels[i].id;
        }
    }

    log_note(out, "requested --channel-id was not found");
    return std::nullopt;
}

std::optional<UINT32> resolve_network_id(NET_HANDLE handle, const NetDemoOptions& options, std::ostream& out)
{
    std::vector<net_network_info> networks;
    USHORT count = 0;
    if (!get_networks(handle, networks, count, out))
    {
        return std::nullopt;
    }

    const auto requested = get_integral_option<UINT32>(options, "network-id");
    if (!requested.has_value())
    {
        return networks[0].id;
    }

    for (USHORT i = 0; i < count; ++i)
    {
        if (networks[i].id == *requested)
        {
            return networks[i].id;
        }
    }

    log_note(out, "requested --network-id was not found");
    return std::nullopt;
}

bool load_selected_stream(NET_HANDLE handle, const NetDemoOptions& options, net_stream_info& stream, std::ostream& out)
{
    std::vector<net_stream_info> streams;
    USHORT count = 0;
    if (!get_streams(handle, streams, count, out))
    {
        return false;
    }

    UINT32 stream_id = streams[0].id;
    if (const auto requested = get_integral_option<UINT32>(options, "stream-id"); requested.has_value())
    {
        stream_id = *requested;
    }

    std::memset(&stream, 0, sizeof(stream));
    const INT32 rc = net_get_stream(handle, stream_id, &stream);
    log_call(out, "net_get_stream", rc, "stream_id=" + std::to_string(stream_id));
    return rc == NET_EC_OK;
}

// ---------------------------------------------------------------------------
// Overlay patch helpers
// ---------------------------------------------------------------------------

bool patch_overlay_block(
    const NetDemoOptions& options,
    const std::string& prefix,
    net_overlay_block& block,
    std::ostream& out,
    bool& changed)
{
    if (const auto enabled = get_bool_option(options, prefix + "-enabled"); enabled.has_value())
    {
        block.enabled = *enabled ? 1 : 0;
        changed = true;
    }
    if (const auto align = get_value(options, prefix + "-align"); align.has_value())
    {
        const auto parsed = parse_overlay_align(*align);
        if (!parsed.has_value())
        {
            log_note(out, "invalid align for --" + prefix + "-align");
            return false;
        }
        block.align = *parsed;
        changed = true;
    }
    if (const auto x = get_integral_option<UINT32>(options, prefix + "-x"); x.has_value())
    {
        block.x = *x;
        changed = true;
    }
    if (const auto y = get_integral_option<UINT32>(options, prefix + "-y"); y.has_value())
    {
        block.y = *y;
        changed = true;
    }
    return true;
}

bool patch_custom_overlay(const NetDemoOptions& options, net_overlay_custom_block& block, std::ostream& out, bool& changed)
{
    if (const auto enabled = get_bool_option(options, "custom-enabled"); enabled.has_value())
    {
        block.enabled = *enabled ? 1 : 0;
        changed = true;
    }
    if (const auto align = get_value(options, "custom-align"); align.has_value())
    {
        const auto parsed = parse_overlay_align(*align);
        if (!parsed.has_value())
        {
            log_note(out, "invalid align for --custom-align");
            return false;
        }
        block.align = *parsed;
        changed = true;
    }
    if (const auto mode = get_value(options, "custom-mode"); mode.has_value())
    {
        const auto parsed = parse_overlay_mode(*mode);
        if (!parsed.has_value())
        {
            log_note(out, "invalid mode for --custom-mode");
            return false;
        }
        block.mode = *parsed;
        changed = true;
    }
    if (const auto x = get_integral_option<UINT32>(options, "custom-x"); x.has_value())
    {
        block.x = *x;
        changed = true;
    }
    if (const auto y = get_integral_option<UINT32>(options, "custom-y"); y.has_value())
    {
        block.y = *y;
        changed = true;
    }
    if (const auto text = get_value(options, "custom-text"); text.has_value())
    {
        copy_text(block.text, sizeof(block.text), *text);
        changed = true;
    }
    return true;
}

bool patch_follow_overlay(
    const NetDemoOptions& options,
    const std::string& prefix,
    net_overlay_follow_block& block,
    std::ostream& out,
    bool& changed)
{
    if (const auto enabled = get_bool_option(options, prefix + "-enabled"); enabled.has_value())
    {
        block.enabled = *enabled ? 1 : 0;
        changed = true;
    }
    if (const auto align = get_value(options, prefix + "-align"); align.has_value())
    {
        const auto parsed = parse_overlay_align(*align);
        if (!parsed.has_value())
        {
            log_note(out, "invalid align for --" + prefix + "-align");
            return false;
        }
        block.align = *parsed;
        changed = true;
    }
    const auto patch_bool_field = [&](const std::string& key, BYTE& field) {
        if (const auto value = get_bool_option(options, prefix + "-" + key); value.has_value())
        {
            field = *value ? 1 : 0;
            changed = true;
        }
    };
    patch_bool_field("follow", block.follow);
    patch_bool_field("max-enabled", block.max_enabled);
    patch_bool_field("min-enabled", block.min_enabled);
    patch_bool_field("avg-enabled", block.avg_enabled);
    patch_bool_field("emissivity-enabled", block.emissivity_enabled);
    if (const auto x = get_integral_option<UINT32>(options, prefix + "-x"); x.has_value())
    {
        block.x = *x;
        changed = true;
    }
    if (const auto y = get_integral_option<UINT32>(options, prefix + "-y"); y.has_value())
    {
        block.y = *y;
        changed = true;
    }
    return true;
}

std::optional<net_point> build_point(const NetDemoOptions& options, const std::string& x_key, const std::string& y_key)
{
    const auto x = get_integral_option<USHORT>(options, x_key);
    const auto y = get_integral_option<USHORT>(options, y_key);
    if (!x.has_value() || !y.has_value())
    {
        return std::nullopt;
    }
    return net_point{*x, *y};
}

bool require_option_text(
    const NetDemoOptions& options,
    const std::string& key,
    std::ostream& err,
    std::string& value)
{
    const auto option_value = get_value(options, key);
    if (!option_value.has_value() || option_value->empty())
    {
        err << "Missing required option --" << key << "\n";
        return false;
    }
    value = *option_value;
    return true;
}

// ---------------------------------------------------------------------------
// CLI helpers
// ---------------------------------------------------------------------------

bool command_requires_login(const std::string& command)
{
    return command != "search";
}

bool is_known_command(const std::string& command)
{
    static const std::unordered_set<std::string> commands = {
        "search",
        "connect-flow",
        "properties-get",
        "capture",
        "record-stream",
        "thermal-data-get",
        "thermal-lut-get",
        "thermal-params-get",
        "thermal-params-set",
        "thermal-osd-get",
        "thermal-osd-set",
        "thermal-calibrate",
        "time-get",
        "time-sync",
        "time-apply",
        "ntp-apply",
        "network-refresh",
        "network-apply",
        "ftp-apply",
        "ftp-test",
        "port-apply",
        "webhook-get",
        "webhook-apply",
        "users-get",
        "user-add",
        "user-update",
        "user-delete",
        "display-get",
        "display-set",
        "channel-set",
        "device-name-set",
        "device-upgrade",
        "markers-get",
        "marker-add-spot",
        "marker-add-rectangle",
        "marker-add-line",
        "marker-delete",
        "alarm-get",
        "alarm-set",
        "alarm-listen",
        "device-restart",
        "device-reset"
    };
    return commands.find(command) != commands.end();
}
