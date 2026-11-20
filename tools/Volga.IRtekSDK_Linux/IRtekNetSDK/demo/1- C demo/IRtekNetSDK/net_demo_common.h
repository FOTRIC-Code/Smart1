#pragma once

#include "IRtekNetSDK.h"

#include <cstdlib>
#include <iosfwd>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

constexpr USHORT kMaxListEntries = 16;
constexpr UINT32 kLoginTimeoutMs = 30000;

// ---------------------------------------------------------------------------
// Options
// ---------------------------------------------------------------------------

struct NetDemoOptions
{
    std::string command;
    std::string ip;
    unsigned short port = 80;
    std::string user_name = "admin";
    std::string password = "admin";
    unsigned int search_timeout_ms = 2000;
    unsigned int alarm_wait_ms = 5000;
    unsigned int stream_wait_ms = 1500;
    unsigned int thermal_wait_ms = 1000;
    unsigned int record_wait_ms = 3000;
    bool enable_log = false;
    bool thermal_snapshot = false;
    bool listen_alarm = false;
    bool thermal_stream_all = false;
    std::unordered_map<std::string, std::string> values;
    std::unordered_set<std::string> flags;
};

// ---------------------------------------------------------------------------
// String / parse utilities
// ---------------------------------------------------------------------------

const char* as_cstr(const BYTE* value);
void copy_text(BYTE* dst, size_t dst_len, const std::string& src);
std::string to_lower(std::string value);
std::string flag_key(const std::string& arg);
const char* ec_to_text(INT32 ec);
void log_call(std::ostream& out, const char* api, INT32 rc, const std::string& detail = {});
void log_note(std::ostream& out, const std::string& message);

std::optional<std::string> get_value(const NetDemoOptions& options, const std::string& key);
bool has_flag(const NetDemoOptions& options, const std::string& key);

std::optional<bool> parse_bool_value(const std::string& raw);
std::optional<bool> get_bool_option(const NetDemoOptions& options, const std::string& key);

template <typename T>
inline std::optional<T> parse_integral(const std::string& raw)
{
    if (raw.empty())
    {
        return std::nullopt;
    }
    char* end = nullptr;
    const unsigned long long parsed = std::strtoull(raw.c_str(), &end, 10);
    if (!end || *end != '\0')
    {
        return std::nullopt;
    }
    return static_cast<T>(parsed);
}

template <typename T>
inline std::optional<T> get_integral_option(const NetDemoOptions& options, const std::string& key)
{
    const auto value = get_value(options, key);
    if (!value.has_value())
    {
        return std::nullopt;
    }
    return parse_integral<T>(*value);
}

std::optional<float> parse_float_value(const std::string& raw);
std::optional<float> get_float_option(const NetDemoOptions& options, const std::string& key);
std::string format_bool(BYTE value);

// ---------------------------------------------------------------------------
// Enum text / parse helpers
// ---------------------------------------------------------------------------

std::string overlay_align_text(net_enum_overlay_align align);
std::optional<net_enum_overlay_align> parse_overlay_align(const std::string& raw);

std::string overlay_mode_text(net_enum_overlay_display_mode mode);
std::optional<net_enum_overlay_display_mode> parse_overlay_mode(const std::string& raw);

std::string stream_type_text(net_enum_stream_type type);
std::optional<net_enum_stream_type> parse_stream_type(const std::string& raw);

std::string image_flip_text(net_enum_image_flip_mode mode);
std::optional<net_enum_image_flip_mode> parse_image_flip_mode(const std::string& raw);

std::optional<net_enum_thermal_capture_mode> parse_capture_mode(const std::string& raw);

std::string time_mode_text(net_enum_time_mode mode);
std::optional<net_enum_time_mode> parse_time_mode(const std::string& raw);

std::optional<net_enum_user_level> parse_user_level(const std::string& raw);
std::string user_level_text(net_enum_user_level level);

std::optional<net_enum_shutter_mode> parse_shutter_mode(const std::string& raw);
std::string shutter_mode_text(net_enum_shutter_mode mode);

std::optional<net_enum_temp_unit> parse_temp_unit(const std::string& raw);
std::string temp_unit_text(net_enum_temp_unit unit);

std::optional<net_enum_distance_unit> parse_distance_unit(const std::string& raw);
std::string distance_unit_text(net_enum_distance_unit unit);

std::optional<net_enum_color_dist_type> parse_color_dist_type(const std::string& raw);
std::string color_dist_text(net_enum_color_dist_type type);

std::optional<net_enum_gain_mode> parse_gain_mode(const std::string& raw);
std::string gain_mode_text(net_enum_gain_mode mode);

std::optional<net_enum_thermal_temp_alarm_type> parse_alarm_type(const std::string& raw);
std::string alarm_type_text(net_enum_thermal_temp_alarm_type type);

std::string marker_type_text(net_enum_marker_type type);

net_enum_thermal_data_type thermal_stream_mask(const NetDemoOptions& options);

// ---------------------------------------------------------------------------
// DemoSession – SDK init / login / cleanup wrapper
// ---------------------------------------------------------------------------

class DemoSession
{
public:
    explicit DemoSession(std::ostream& out);
    ~DemoSession();

    bool init(const NetDemoOptions& options);
    bool login(const NetDemoOptions& options);
    void set_exception_callback();

    NET_HANDLE handle() const;

    void close(bool log_calls);

private:
    std::ostream& out_;
    NET_HANDLE handle_ = nullptr;
    bool initialized_ = false;
};

// ---------------------------------------------------------------------------
// Shared data-fetch helpers (used across modules)
// ---------------------------------------------------------------------------

bool get_channels(NET_HANDLE handle, std::vector<net_channel_info>& channels, USHORT& count, std::ostream& out);
bool get_streams(NET_HANDLE handle, std::vector<net_stream_info>& streams, USHORT& count, std::ostream& out);
bool get_networks(NET_HANDLE handle, std::vector<net_network_info>& networks, USHORT& count, std::ostream& out);
bool get_markers(NET_HANDLE handle, std::vector<net_marker_info>& markers, USHORT& count, std::ostream& out);
bool get_alarm_rules(NET_HANDLE handle, std::vector<net_thermal_temp_alarm>& alarms, USHORT& count, std::ostream& out);

std::optional<UINT32> resolve_channel_id(NET_HANDLE handle, const NetDemoOptions& options, std::ostream& out);
std::optional<UINT32> resolve_network_id(NET_HANDLE handle, const NetDemoOptions& options, std::ostream& out);
bool load_selected_stream(NET_HANDLE handle, const NetDemoOptions& options, net_stream_info& stream, std::ostream& out);

// ---------------------------------------------------------------------------
// Overlay patch helpers (shared by params and display)
// ---------------------------------------------------------------------------

bool patch_overlay_block(const NetDemoOptions& options, const std::string& prefix,
                         net_overlay_block& block, std::ostream& out, bool& changed);
bool patch_custom_overlay(const NetDemoOptions& options,
                          net_overlay_custom_block& block, std::ostream& out, bool& changed);
bool patch_follow_overlay(const NetDemoOptions& options, const std::string& prefix,
                          net_overlay_follow_block& block, std::ostream& out, bool& changed);

std::optional<net_point> build_point(const NetDemoOptions& options, const std::string& x_key, const std::string& y_key);

bool require_option_text(const NetDemoOptions& options, const std::string& key,
                         std::ostream& err, std::string& value);

// ---------------------------------------------------------------------------
// CLI helpers
// ---------------------------------------------------------------------------

bool command_requires_login(const std::string& command);
bool is_known_command(const std::string& command);
