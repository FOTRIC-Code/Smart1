#include "uvc_demo_common.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>

// ---------------------------------------------------------------------------
// Low-level helpers
// ---------------------------------------------------------------------------

bool is_ok(int rc)
{
    return rc == UVC_EC_OK;
}

int check_rc(const char* label, int rc)
{
    std::cout << label << " => " << rc << "\n";
    return rc;
}

int check_file_rc(const char* label, int rc)
{
    std::cout << label << " => " << rc << "\n";
    return rc;
}

float kelvin_to_celsius(float kelvin)
{
    return kelvin - kKelvinOffset;
}

std::string bytes_to_string(const BYTE* data, size_t size)
{
    size_t count = 0;
    while (count < size && data[count] != 0)
    {
        ++count;
    }
    return std::string(reinterpret_cast<const char*>(data), count);
}

// ---------------------------------------------------------------------------
// Parse helpers
// ---------------------------------------------------------------------------

bool parse_int_value(const std::string& text, int& out)
{
    char* end = nullptr;
    errno = 0;
    const long value = std::strtol(text.c_str(), &end, 10);
    if (errno != 0 || end == text.c_str() || *end != '\0')
    {
        return false;
    }
    out = static_cast<int>(value);
    return true;
}

bool parse_ushort_value(const std::string& text, USHORT& out)
{
    int value = 0;
    if (!parse_int_value(text, value) || value < 0 || value > std::numeric_limits<USHORT>::max())
    {
        return false;
    }
    out = static_cast<USHORT>(value);
    return true;
}

bool parse_float_value(const std::string& text, float& out)
{
    char* end = nullptr;
    errno = 0;
    const float value = std::strtof(text.c_str(), &end);
    if (errno != 0 || end == text.c_str() || *end != '\0')
    {
        return false;
    }
    out = value;
    return true;
}

bool parse_bool_value(const std::string& text, BYTE& out)
{
    if (text == "1" || text == "true" || text == "TRUE" || text == "yes" || text == "on")
    {
        out = 1;
        return true;
    }
    if (text == "0" || text == "false" || text == "FALSE" || text == "no" || text == "off")
    {
        out = 0;
        return true;
    }
    return false;
}

bool parse_shutter_mode_value(const std::string& text, uvc_enum_shutter_mode& out)
{
    if (text == "automatic" || text == "auto")
    {
        out = uvc_enum_shutter_mode::AUTOMATIC;
        return true;
    }
    if (text == "manual")
    {
        out = uvc_enum_shutter_mode::MANUAL;
        return true;
    }
    return false;
}

bool parse_command(const std::string& text, DemoCommand& out)
{
    if (text == "refresh-devices")
    {
        out = DemoCommand::RefreshDevices;
    }
    else if (text == "login")
    {
        out = DemoCommand::Login;
    }
    else if (text == "thermal-get")
    {
        out = DemoCommand::ThermalGet;
    }
    else if (text == "thermal-set")
    {
        out = DemoCommand::ThermalSet;
    }
    else if (text == "shutter-mode-get")
    {
        out = DemoCommand::ShutterModeGet;
    }
    else if (text == "shutter-mode-set")
    {
        out = DemoCommand::ShutterModeSet;
    }
    else if (text == "shutter-time-get")
    {
        out = DemoCommand::ShutterTimeGet;
    }
    else if (text == "shutter-time-set")
    {
        out = DemoCommand::ShutterTimeSet;
    }
    else if (text == "frame-list")
    {
        out = DemoCommand::FrameList;
    }
    else if (text == "range-list")
    {
        out = DemoCommand::RangeList;
    }
    else if (text == "range-get")
    {
        out = DemoCommand::RangeGet;
    }
    else if (text == "range-set")
    {
        out = DemoCommand::RangeSet;
    }
    else if (text == "display-get")
    {
        out = DemoCommand::DisplayGet;
    }
    else if (text == "display-set")
    {
        out = DemoCommand::DisplaySet;
    }
    else if (text == "capture")
    {
        out = DemoCommand::Capture;
    }
    else if (text == "calibrate")
    {
        out = DemoCommand::Calibrate;
    }
    else if (text == "upgrade")
    {
        out = DemoCommand::Upgrade;
    }
    else if (text == "restart")
    {
        out = DemoCommand::Restart;
    }
    else if (text == "reset")
    {
        out = DemoCommand::Reset;
    }
    else
    {
        return false;
    }
    return true;
}

namespace
{
bool validate_required_flags(const DemoOptions& options)
{
    switch (options.command)
    {
    case DemoCommand::ThermalSet:
        return options.has_emissivity && options.has_refl_temp && options.has_distance &&
               options.has_atm_temp && options.has_rel_humidity;
    case DemoCommand::ShutterModeSet:
        return options.has_shutter_mode;
    case DemoCommand::ShutterTimeSet:
        return options.has_shutter_time;
    case DemoCommand::RangeSet:
        return options.has_range_id;
    case DemoCommand::DisplaySet:
        return options.has_contrast && options.has_brightness &&
               options.has_dde_enable && options.has_dde_value;
    case DemoCommand::Upgrade:
        return !options.firmware_path.empty();
    default:
        return true;
    }
}
} // namespace

bool parse_args(int argc, char** argv, DemoOptions& options)
{
    if (argc <= 1)
    {
        options.command = DemoCommand::Help;
        return true;
    }

    const std::string first = argv[1];
    if (first == "--help" || first == "-h" || first == "help")
    {
        options.command = DemoCommand::Help;
        return true;
    }

    if (!parse_command(first, options.command))
    {
        std::cerr << "Unknown command: " << first << "\n";
        print_usage(argv[0]);
        return false;
    }

    auto require_value = [&](int& index, const std::string& arg_name) -> const char* {
        if (index + 1 >= argc)
        {
            std::cerr << "Missing value for " << arg_name << "\n";
            return nullptr;
        }
        return argv[++index];
    };

    for (int i = 2; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--device-index")
        {
            const char* value = require_value(i, arg);
            if (!value || !parse_int_value(value, options.device_index))
            {
                std::cerr << "Invalid device index\n";
                return false;
            }
        }
        else if (arg == "--enable-log")
        {
            options.enable_log = true;
        }
        else if (arg == "--wait-ms")
        {
            const char* value = require_value(i, arg);
            if (!value || !parse_int_value(value, options.wait_ms))
            {
                std::cerr << "Invalid wait-ms\n";
                return false;
            }
            options.wait_ms = std::max(options.wait_ms, 100);
        }
        else if (arg == "--output")
        {
            const char* value = require_value(i, arg);
            if (!value)
            {
                return false;
            }
            options.output_path = value;
        }
        else if (arg == "--preview-output")
        {
            const char* value = require_value(i, arg);
            if (!value)
            {
                return false;
            }
            options.preview_output_path = value;
        }
        else if (arg == "--no-stream-smoke")
        {
            options.stream_smoke_test = false;
        }
        else if (arg == "--firmware")
        {
            const char* value = require_value(i, arg);
            if (!value)
            {
                return false;
            }
            options.firmware_path = value;
        }
        else if (arg == "--poll-timeout-ms")
        {
            const char* value = require_value(i, arg);
            if (!value || !parse_int_value(value, options.upgrade_timeout_ms))
            {
                std::cerr << "Invalid poll-timeout-ms\n";
                return false;
            }
            options.upgrade_timeout_ms = std::max(options.upgrade_timeout_ms, 1000);
        }
        else if (arg == "--poll-interval-ms")
        {
            const char* value = require_value(i, arg);
            if (!value || !parse_int_value(value, options.upgrade_poll_interval_ms))
            {
                std::cerr << "Invalid poll-interval-ms\n";
                return false;
            }
            options.upgrade_poll_interval_ms = std::max(options.upgrade_poll_interval_ms, 100);
        }
        else if (arg == "--emissivity")
        {
            const char* value = require_value(i, arg);
            if (!value || !parse_float_value(value, options.emissivity))
            {
                std::cerr << "Invalid emissivity\n";
                return false;
            }
            options.has_emissivity = true;
        }
        else if (arg == "--refl-temp")
        {
            const char* value = require_value(i, arg);
            if (!value || !parse_float_value(value, options.refl_temp))
            {
                std::cerr << "Invalid refl-temp\n";
                return false;
            }
            options.has_refl_temp = true;
        }
        else if (arg == "--distance")
        {
            const char* value = require_value(i, arg);
            if (!value || !parse_float_value(value, options.distance))
            {
                std::cerr << "Invalid distance\n";
                return false;
            }
            options.has_distance = true;
        }
        else if (arg == "--atm-temp")
        {
            const char* value = require_value(i, arg);
            if (!value || !parse_float_value(value, options.atm_temp))
            {
                std::cerr << "Invalid atm-temp\n";
                return false;
            }
            options.has_atm_temp = true;
        }
        else if (arg == "--rel-humidity")
        {
            const char* value = require_value(i, arg);
            if (!value || !parse_float_value(value, options.rel_humidity))
            {
                std::cerr << "Invalid rel-humidity\n";
                return false;
            }
            options.has_rel_humidity = true;
        }
        else if (arg == "--mode")
        {
            const char* value = require_value(i, arg);
            if (!value || !parse_shutter_mode_value(value, options.shutter_mode))
            {
                std::cerr << "Invalid shutter mode\n";
                return false;
            }
            options.has_shutter_mode = true;
        }
        else if (arg == "--minutes")
        {
            const char* value = require_value(i, arg);
            if (!value || !parse_ushort_value(value, options.shutter_time))
            {
                std::cerr << "Invalid shutter minutes\n";
                return false;
            }
            options.has_shutter_time = true;
        }
        else if (arg == "--range-id")
        {
            const char* value = require_value(i, arg);
            if (!value || !parse_ushort_value(value, options.range_id))
            {
                std::cerr << "Invalid range id\n";
                return false;
            }
            options.has_range_id = true;
        }
        else if (arg == "--contrast")
        {
            const char* value = require_value(i, arg);
            if (!value || !parse_ushort_value(value, options.contrast))
            {
                std::cerr << "Invalid contrast\n";
                return false;
            }
            options.has_contrast = true;
        }
        else if (arg == "--brightness")
        {
            const char* value = require_value(i, arg);
            if (!value || !parse_ushort_value(value, options.brightness))
            {
                std::cerr << "Invalid brightness\n";
                return false;
            }
            options.has_brightness = true;
        }
        else if (arg == "--dde-enable")
        {
            const char* value = require_value(i, arg);
            if (!value || !parse_bool_value(value, options.dde_enable))
            {
                std::cerr << "Invalid dde-enable\n";
                return false;
            }
            options.has_dde_enable = true;
        }
        else if (arg == "--dde-value")
        {
            const char* value = require_value(i, arg);
            if (!value || !parse_ushort_value(value, options.dde_value))
            {
                std::cerr << "Invalid dde-value\n";
                return false;
            }
            options.has_dde_value = true;
        }
        else if (arg == "--help" || arg == "-h")
        {
            options.command = DemoCommand::Help;
            return true;
        }
        else
        {
            std::cerr << "Unknown argument: " << arg << "\n";
            print_usage(argv[0]);
            return false;
        }
    }

    if (!validate_required_flags(options))
    {
        std::cerr << "Missing required options for command " << first << "\n";
        print_usage(argv[0]);
        return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// Print / format helpers
// ---------------------------------------------------------------------------

void print_usage(const char* argv0)
{
    std::cout
        << "Usage: " << argv0 << " <command> [options]\n"
        << "\n"
        << "Commands mirroring Windows MainForm.cs button chains:\n"
        << "  refresh-devices                     Refresh device list\n"
        << "  login                               Login + load properties/thermal/frame/range/renderer state\n"
        << "  thermal-get                         Get thermal parameters\n"
        << "  thermal-set                         Set thermal parameters\n"
        << "  shutter-mode-get                    Get shutter mode\n"
        << "  shutter-mode-set                    Set shutter mode\n"
        << "  shutter-time-get                    Get shutter auto-calibration time\n"
        << "  shutter-time-set                    Set shutter auto-calibration time\n"
        << "  frame-list                          List stream frames\n"
        << "  range-list                          List supported ranges\n"
        << "  range-get                           Get current range\n"
        << "  range-set                           Set current range\n"
        << "  display-get                         Get image params\n"
        << "  display-set                         Set image params\n"
        << "  capture                             Capture one frame and save thermal image\n"
        << "  calibrate                           Run thermal calibration\n"
        << "  upgrade                             Upgrade firmware and poll state\n"
        << "  restart                             Restart device\n"
        << "  reset                               Reset device to factory defaults\n"
        << "\n"
        << "Common options:\n"
        << "  --device-index <n>                  Select device from refresh-devices output, default 0\n"
        << "  --enable-log                        Enable SDK logging\n"
        << "  --wait-ms <ms>                      Stream wait timeout for login/capture, default 3000\n"
        << "\n"
        << "Command options:\n"
        << "  thermal-set --emissivity <f> --refl-temp <kelvin> --distance <m>\n"
        << "              --atm-temp <kelvin> --rel-humidity <f>\n"
        << "  shutter-mode-set --mode <automatic|manual>\n"
        << "  shutter-time-set --minutes <n>\n"
        << "  range-set --range-id <n>\n"
        << "  display-set --contrast <0-100> --brightness <0-100>\n"
        << "              --dde-enable <0|1|true|false> --dde-value <0-100>\n"
        << "  capture --output <path> [--preview-output <ppm-path>]\n"
        << "  login --no-stream-smoke             Skip the StartStream-equivalent smoke test\n"
        << "  upgrade --firmware <path> [--poll-timeout-ms <ms>] [--poll-interval-ms <ms>]\n"
        << "\n"
        << "Notes:\n"
        << "  - CLI commands are stateless: each subcommand init/logins, performs one Windows-like flow,\n"
        << "    then logs out and cleans up.\n"
        << "  - capture follows the Windows IR file save chain; cover-image embedding is omitted because\n"
        << "    the Linux CLI has no GUI bitmap source.\n";
}

std::string shutter_mode_to_string(uvc_enum_shutter_mode mode)
{
    switch (mode)
    {
    case uvc_enum_shutter_mode::AUTOMATIC:
        return "automatic";
    case uvc_enum_shutter_mode::MANUAL:
        return "manual";
    default:
        return "unknown";
    }
}

std::string upgrade_state_to_string(uvc_enum_upgrade_state state)
{
    switch (state)
    {
    case uvc_enum_upgrade_state::IDLE:
        return "idle";
    case uvc_enum_upgrade_state::RECEIVING:
        return "receiving";
    case uvc_enum_upgrade_state::STAGED:
        return "staged";
    case uvc_enum_upgrade_state::FLASHING:
        return "flashing";
    case uvc_enum_upgrade_state::DONE:
        return "done";
    case uvc_enum_upgrade_state::FAILED:
        return "failed";
    default:
        return "unknown";
    }
}

void print_device_brief(size_t index, const uvc_device& device)
{
    std::cout << "device[" << index << "] vid=0x" << std::hex << std::setw(4) << std::setfill('0') << device.vid
              << " pid=0x" << std::setw(4) << device.pid
              << " path=" << bytes_to_string(device.path, sizeof(device.path))
              << std::dec << "\n";
}

void print_device_info(const uvc_device_info& info)
{
    std::cout << "  model: " << bytes_to_string(info.model, sizeof(info.model)) << "\n";
    std::cout << "  serial: " << bytes_to_string(info.serial_no, sizeof(info.serial_no)) << "\n";
    std::cout << "  id: " << bytes_to_string(info.id, sizeof(info.id)) << "\n";
    std::cout << "  brand: " << bytes_to_string(info.brand, sizeof(info.brand)) << "\n";
    std::cout << "  company: " << bytes_to_string(info.company, sizeof(info.company)) << "\n";
    std::cout << "  firmware: "
              << bytes_to_string(info.firmware_name, sizeof(info.firmware_name))
              << " "
              << bytes_to_string(info.firmware_version, sizeof(info.firmware_version))
              << "\n";
}

void print_lens_info(const uvc_lens_info& lens_info)
{
    std::cout << "  lens: " << bytes_to_string(lens_info.name, sizeof(lens_info.name))
              << " hfov=" << lens_info.hfov
              << " vfov=" << lens_info.vfov
              << " focal=" << lens_info.focal
              << " band=" << bytes_to_string(lens_info.band, sizeof(lens_info.band))
              << "\n";
}

void print_thermal_resolution(const uvc_size& size)
{
    std::cout << "  thermal_resolution: " << size.width << "x" << size.height << "\n";
}

void print_thermal_params(const uvc_thermal_params& params)
{
    std::cout << "  emissivity: " << params.emissivity << "\n";
    std::cout << "  refl_temp: " << params.refl_temp << " K (" << kelvin_to_celsius(params.refl_temp) << " C)\n";
    std::cout << "  distance: " << params.distance << " m\n";
    std::cout << "  atm_temp: " << params.atm_temp << " K (" << kelvin_to_celsius(params.atm_temp) << " C)\n";
    std::cout << "  rel_humidity: " << params.rel_humidity << "\n";
}

void print_image_params(const uvc_image_params& params)
{
    std::cout << "  contrast: " << params.contrast << "\n";
    std::cout << "  brightness: " << params.brightness << "\n";
    std::cout << "  dde_enable: " << static_cast<int>(params.dde_enable) << "\n";
    std::cout << "  dde_value: " << params.dde_value << "\n";
}

void print_frame_list(const std::vector<uvc_frame_info>& frames)
{
    for (size_t i = 0; i < frames.size(); ++i)
    {
        const uvc_frame_info& frame = frames[i];
        std::cout << "  frame[" << i << "] "
                  << frame.width << "x" << frame.height
                  << " format=" << bytes_to_string(frame.format, sizeof(frame.format))
                  << " if=" << frame.interface_num
                  << " alt=" << frame.alt_setting
                  << " fi=" << frame.frame_index
                  << " fo=" << frame.format_index
                  << "\n";
    }
}

void print_range_list(const uvc_lens_info& lens_info, const USHORT* current_range_id)
{
    for (USHORT i = 0; i < lens_info.ranges_count && i < 4; ++i)
    {
        const bool is_current = current_range_id && lens_info.range_map[i].id == *current_range_id;
        std::cout << "  range[" << i << "] id=" << lens_info.range_map[i].id
                  << " raw=[" << lens_info.range_map[i].range.min
                  << ", " << lens_info.range_map[i].range.max << "]"
                  << (is_current ? " <current>" : "")
                  << "\n";
    }
}

void print_lut_summary(const uvc_lut_table& lut)
{
    std::cout << "  lut_count: " << lut.lut_count << "\n";
    if (lut.lut_count > 0)
    {
        std::cout << "  lut_range_id: " << lut.range_id << "\n";
        std::cout << "  lut_first: ad=" << lut.ad[0] << " temp=" << lut.t[0] << " K\n";
        std::cout << "  lut_last: ad=" << lut.ad[lut.lut_count - 1] << " temp=" << lut.t[lut.lut_count - 1] << " K\n";
    }
}

// ---------------------------------------------------------------------------
// Frame / utility
// ---------------------------------------------------------------------------

const uvc_frame_info* select_best_frame(const std::vector<uvc_frame_info>& frames)
{
    if (frames.empty())
    {
        return nullptr;
    }

    return &(*std::max_element(
        frames.begin(),
        frames.end(),
        [](const uvc_frame_info& lhs, const uvc_frame_info& rhs) {
            const int lhs_score = static_cast<int>(lhs.width) * static_cast<int>(lhs.height);
            const int rhs_score = static_cast<int>(rhs.width) * static_cast<int>(rhs.height);
            if (lhs_score != rhs_score)
            {
                return lhs_score < rhs_score;
            }
            return lhs.frame_index < rhs.frame_index;
        }));
}

bool save_ppm(const std::string& path, int width, int height, const std::vector<uint8_t>& bgr)
{
    std::ofstream output(path, std::ios::binary);
    if (!output.is_open())
    {
        return false;
    }

    output << "P6\n" << width << " " << height << "\n255\n";
    for (int y = 0; y < height; ++y)
    {
        const uint8_t* row = bgr.data() + static_cast<size_t>(y) * width * 3;
        for (int x = 0; x < width; ++x)
        {
            const uint8_t b = row[x * 3 + 0];
            const uint8_t g = row[x * 3 + 1];
            const uint8_t r = row[x * 3 + 2];
            output.put(static_cast<char>(r));
            output.put(static_cast<char>(g));
            output.put(static_cast<char>(b));
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// Device list
// ---------------------------------------------------------------------------

bool list_devices(std::vector<uvc_device>& devices)
{
    devices.assign(16, uvc_device {});
    USHORT out_length = 0;
    const int rc = check_rc(
        "uvc_get_uvc_list",
        uvc_get_uvc_list(devices.data(), static_cast<USHORT>(devices.size()), &out_length));
    if (!is_ok(rc))
    {
        return false;
    }

    devices.resize(out_length);
    for (size_t i = 0; i < devices.size(); ++i)
    {
        print_device_brief(i, devices[i]);
    }

    if (devices.empty())
    {
        std::cerr << "No UVC thermal devices found.\n";
        return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// Query helpers
// ---------------------------------------------------------------------------

bool query_device_info(HANDLE handle, uvc_device_info& info)
{
    std::memset(&info, 0, sizeof(info));
    const int rc = check_rc("uvc_get_device_info", uvc_get_device_info(handle, &info));
    if (!is_ok(rc))
    {
        return false;
    }
    print_device_info(info);
    return true;
}

bool query_lens_info(HANDLE handle, uvc_lens_info& lens_info)
{
    std::memset(&lens_info, 0, sizeof(lens_info));
    const int rc = check_rc("uvc_get_lens_info", uvc_get_lens_info(handle, &lens_info));
    if (!is_ok(rc))
    {
        return false;
    }
    print_lens_info(lens_info);
    return true;
}

bool query_thermal_resolution(HANDLE handle)
{
    uvc_size thermal_size {};
    const int rc = check_rc("uvc_get_thermal_resolution", uvc_get_thermal_resolution(handle, &thermal_size));
    if (!is_ok(rc))
    {
        return false;
    }
    print_thermal_resolution(thermal_size);
    return true;
}

bool query_thermal_params(HANDLE handle, uvc_thermal_params& params)
{
    params = {};
    const int rc = check_rc("uvc_get_thermal_params", uvc_get_thermal_params(handle, &params));
    if (!is_ok(rc))
    {
        return false;
    }
    print_thermal_params(params);
    return true;
}

bool query_shutter_mode(HANDLE handle, uvc_enum_shutter_mode& mode)
{
    mode = uvc_enum_shutter_mode::AUTOMATIC;
    const int rc = check_rc("uvc_get_thermal_shutter_mode", uvc_get_thermal_shutter_mode(handle, &mode));
    if (!is_ok(rc))
    {
        return false;
    }
    std::cout << "  shutter_mode: " << shutter_mode_to_string(mode) << "\n";
    return true;
}

bool query_shutter_time(HANDLE handle, USHORT& minutes)
{
    minutes = 0;
    const int rc = check_rc("uvc_get_thermal_shutter_time", uvc_get_thermal_shutter_time(handle, &minutes));
    if (!is_ok(rc))
    {
        return false;
    }
    std::cout << "  shutter_time_minutes: " << minutes << "\n";
    return true;
}

bool query_frame_list(HANDLE handle, std::vector<uvc_frame_info>& frames)
{
    frames.assign(16, uvc_frame_info {});
    USHORT out_length = 0;
    const int rc = check_rc(
        "uvc_get_frame_list",
        uvc_get_frame_list(handle, frames.data(), static_cast<USHORT>(frames.size()), &out_length));
    if (!is_ok(rc))
    {
        return false;
    }

    frames.resize(out_length);
    if (frames.empty())
    {
        std::cerr << "No stream frames exposed by device.\n";
        return false;
    }

    print_frame_list(frames);
    return true;
}

bool query_current_range(HANDLE handle, USHORT& range_id)
{
    range_id = 0;
    const int rc = check_rc("uvc_get_thermal_range", uvc_get_thermal_range(handle, &range_id));
    if (!is_ok(rc))
    {
        return false;
    }
    std::cout << "  current_range_id: " << range_id << "\n";
    return true;
}

bool query_display_params(HANDLE handle, uvc_image_params& params)
{
    params = {};
    const int rc = check_rc("uvc_get_image_params", uvc_get_image_params(handle, &params));
    if (!is_ok(rc))
    {
        return false;
    }
    print_image_params(params);
    return true;
}

bool query_lut(HANDLE handle, uvc_lut_table& lut)
{
    lut = {};
    const int rc = check_rc("uvc_get_thermal_lut", uvc_get_thermal_lut(handle, &lut));
    if (!is_ok(rc))
    {
        return false;
    }
    print_lut_summary(lut);
    return true;
}

// ---------------------------------------------------------------------------
// RAII helpers
// ---------------------------------------------------------------------------

namespace
{
void UVC_CALLBACK on_hotplug(const uvc_device* device, bool found)
{
    if (!device)
    {
        return;
    }

    std::cout << "[hotplug] " << (found ? "add " : "remove ")
              << "vid=0x" << std::hex << std::setw(4) << std::setfill('0') << device->vid
              << " pid=0x" << std::setw(4) << device->pid
              << " path=" << bytes_to_string(device->path, sizeof(device->path))
              << std::dec << "\n";
}
} // namespace

ScopedSdk::ScopedSdk(bool enable_log)
{
    if (is_ok(check_rc("uvc_sdk_init", uvc_sdk_init())))
    {
        initialized_ = true;
        check_rc("uvc_enable_log", uvc_enable_log(enable_log));
        check_rc("uvc_set_hotplug_callback", uvc_set_hotplug_callback(on_hotplug));
    }
}

ScopedSdk::~ScopedSdk()
{
    if (!initialized_)
    {
        return;
    }
    check_rc("uvc_set_hotplug_callback", uvc_set_hotplug_callback(nullptr));
    check_rc("uvc_sdk_cleanup", uvc_sdk_cleanup());
}

bool ScopedSdk::ok() const
{
    return initialized_;
}

ScopedLogin::~ScopedLogin()
{
    if (handle_ != nullptr)
    {
        check_rc("uvc_logout", uvc_logout(&handle_));
    }
}

bool ScopedLogin::login(const uvc_device& device)
{
    device_ = device;

    BYTE path[sizeof(device.path)] {};
    std::memcpy(path, device.path, sizeof(path));
    int rc = check_rc("uvc_login_by_path", uvc_login_by_path(path, static_cast<USHORT>(sizeof(path)), &handle_));
    if (!is_ok(rc))
    {
        rc = check_rc("uvc_login", uvc_login(device.vid, device.pid, &handle_));
    }
    return is_ok(rc);
}

HANDLE ScopedLogin::handle() const
{
    return handle_;
}

// ---------------------------------------------------------------------------
// Login helpers
// ---------------------------------------------------------------------------

bool open_logged_in_device(const DemoOptions& options, ScopedLogin& login, uvc_device& out_device)
{
    std::vector<uvc_device> devices;
    if (!list_devices(devices))
    {
        return false;
    }

    if (options.device_index < 0 || static_cast<size_t>(options.device_index) >= devices.size())
    {
        std::cerr << "device-index out of range\n";
        return false;
    }

    out_device = devices[static_cast<size_t>(options.device_index)];
    std::cout << "Selected device[" << options.device_index << "]\n";
    return login.login(out_device);
}

bool with_logged_in_device(const DemoOptions& options, const std::function<bool(HANDLE)>& fn)
{
    ScopedSdk sdk(options.enable_log);
    if (!sdk.ok())
    {
        return false;
    }

    ScopedLogin login;
    uvc_device device {};
    if (!open_logged_in_device(options, login, device))
    {
        return false;
    }

    return fn(login.handle());
}
