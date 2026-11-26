#include "thermal_file_demo.h"

#include "IRtekFileSDK.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
constexpr float kKelvinOffset = 273.15f;
constexpr size_t kKeyLength = 16;
constexpr size_t kNameLength = 32;
constexpr const char* kTagUserDataKey = "TAG";
constexpr const char* kDemoUserDataKey = "CLI_DEMO";

struct HandleCloser
{
    HANDLE handle = nullptr;

    ~HandleCloser()
    {
        if (handle != nullptr)
        {
            ir_file_close(handle);
            handle = nullptr;
        }
    }
};

std::string decode_version(UINT32 value)
{
    return std::to_string((value >> 24) & 0xff) + "."
        + std::to_string((value >> 16) & 0xff) + "."
        + std::to_string((value >> 8) & 0xff) + "."
        + std::to_string(value & 0xff);
}

const char* file_type_name(ir_file_enum_file_type type)
{
    switch (type)
    {
    case IR_FILE_THERMAL_PICTURE:
        return "thermal_picture";
    case IR_FILE_THERMAL_VIDEO:
        return "thermal_video";
    case IR_FILE_VISUAL_PICTURE:
        return "visual_picture";
    case IR_FILE_GRID_THERMAL_PICTURE:
        return "grid_thermal_picture";
    default:
        return "other";
    }
}

const char* gain_mode_name(ir_file_enum_gain_mode mode)
{
    switch (mode)
    {
    case IR_FILE_FIXED:
        return "fixed";
    case IR_FILE_TOUCH:
        return "touch";
    case IR_FILE_AUTO:
    default:
        return "auto";
    }
}

const char* alarm_type_name(ir_file_enum_alarm_type type)
{
    switch (type)
    {
    case IR_FILE_BELOW:
        return "below";
    case IR_FILE_ABOVE:
        return "above";
    case IR_FILE_BETWEEN:
        return "between";
    case IR_FILE_MIX_PALETTE:
        return "mix_palette";
    case IR_FILE_OUTSIDE:
        return "outside";
    case IR_FILE_HUMIDITY:
        return "humidity";
    case IR_FILE_INSULATION:
        return "insulation";
    case IR_FILE_NORMAL:
    default:
        return "normal";
    }
}

const char* temp_value_type_name(ir_file_enum_temp_value_type type)
{
    switch (type)
    {
    case IR_FILE_MIN:
        return "min";
    case IR_FILE_AVG:
        return "avg";
    case IR_FILE_MAX:
    default:
        return "max";
    }
}

const char* color_dist_name(ir_file_enum_color_dist_type type)
{
    switch (type)
    {
    case IR_FILE_T_TWB:
        return "t_twb";
    case IR_FILE_TEMP_LINEAR:
        return "temperature_linear";
    case IR_FILE_AD_LINEAR:
    default:
        return "ad_linear";
    }
}

const char* view_type_name(ir_file_enum_thermal_view_type type)
{
    switch (type)
    {
    case IR_FILE_PIC_IN_PIC:
        return "pic_in_pic";
    case IR_FILE_T_DEF:
        return "t_def";
    case IR_FILE_FUSION:
        return "fusion";
    case IR_FILE_THERMAL:
    default:
        return "thermal";
    }
}

const char* image_enh_name(ir_file_enum_image_enh_type type)
{
    switch (type)
    {
    case IR_FILE_IR_EDGE:
        return "ir_edge";
    case IR_FILE_NONE:
    default:
        return "none";
    }
}

const char* audio_type_name(ir_file_enum_audio_type type)
{
    switch (type)
    {
    case IR_FILE_WAV:
        return "wav";
    case IR_FILE_MP3:
    default:
        return "mp3";
    }
}

bool save_ppm_bgr(const std::string& path, int width, int height, const std::vector<BYTE>& bgr)
{
    std::ofstream output(path, std::ios::binary);
    if (!output.is_open())
    {
        return false;
    }

    output << "P6\n" << width << " " << height << "\n255\n";
    for (int y = 0; y < height; ++y)
    {
        const BYTE* row = bgr.data() + static_cast<size_t>(y) * width * 3;
        for (int x = 0; x < width; ++x)
        {
            output.put(static_cast<char>(row[x * 3 + 2]));
            output.put(static_cast<char>(row[x * 3 + 1]));
            output.put(static_cast<char>(row[x * 3 + 0]));
        }
    }

    return true;
}

std::string bytes_to_string(const BYTE* data, size_t length)
{
    size_t actual_length = length;
    while (actual_length > 0 && data[actual_length - 1] == 0)
    {
        --actual_length;
    }
    return std::string(reinterpret_cast<const char*>(data), actual_length);
}

std::string fixed_bytes_to_string(const BYTE* data, size_t length)
{
    size_t actual_length = 0;
    while (actual_length < length && data[actual_length] != 0)
    {
        ++actual_length;
    }
    return std::string(reinterpret_cast<const char*>(data), actual_length);
}

std::string escape_newlines(const std::string& value)
{
    std::string escaped;
    escaped.reserve(value.size());
    for (char ch : value)
    {
        if (ch == '\n')
        {
            escaped += "\\n";
        }
        else if (ch == '\r')
        {
            escaped += "\\r";
        }
        else
        {
            escaped.push_back(ch);
        }
    }
    return escaped;
}

std::string yes_no(BYTE value)
{
    return value == 0 ? "false" : "true";
}

std::string format_temperature(float kelvin)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2)
        << kelvin << " K (" << (kelvin - kKelvinOffset) << " C)";
    return oss.str();
}

std::string format_argb(UINT32 argb)
{
    std::ostringstream oss;
    oss << "0x" << std::uppercase << std::hex << std::setw(8) << std::setfill('0') << argb;
    return oss.str();
}

std::string format_point_f(const ir_file_point_f& point)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3)
        << "(" << point.x << ", " << point.y << ")";
    return oss.str();
}

std::string format_rect_f(const ir_file_rectangle_f& rect)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3)
        << "(" << rect.x << ", " << rect.y << ", "
        << rect.width << ", " << rect.height << ")";
    return oss.str();
}

std::string format_timestamp(UINT64 milliseconds_since_epoch)
{
    const std::time_t seconds = static_cast<std::time_t>(milliseconds_since_epoch / 1000);
    const UINT64 millis = milliseconds_since_epoch % 1000;

    std::tm time_info{};
    if (const std::tm* local_time = std::localtime(&seconds))
    {
        time_info = *local_time;
    }

    std::ostringstream oss;
    oss << std::put_time(&time_info, "%Y-%m-%d %H:%M:%S")
        << "." << std::setw(3) << std::setfill('0') << millis;
    return oss.str();
}

void print_section(std::ostream& out, int index, const std::string& title)
{
    out << "\n[" << index << "] " << title << "\n";
}

void print_call(std::ostream& out, const std::string& name, INT32 result, const std::string& detail)
{
    out << "  - " << name << ": "
        << (result == IR_FILE_EC_OK ? "OK" : "FAIL(" + std::to_string(result) + ")");
    if (!detail.empty())
    {
        out << " -> " << detail;
    }
    out << "\n";
}

std::vector<BYTE> make_bytes(const std::string& value)
{
    return std::vector<BYTE>(value.begin(), value.end());
}

bool read_handle_buffer(HANDLE handle, INT32 (*getter)(HANDLE, ir_file_buffer*), std::vector<BYTE>& data, INT32& result)
{
    ir_file_buffer buffer{};
    result = getter(handle, &buffer);
    if (result != IR_FILE_EC_OK)
    {
        data.clear();
        return false;
    }

    data.assign(buffer.length, 0);
    if (buffer.length == 0)
    {
        return true;
    }

    buffer.buffer = data.data();
    result = getter(handle, &buffer);
    if (result != IR_FILE_EC_OK)
    {
        data.clear();
        return false;
    }

    return true;
}

bool read_audio(HANDLE handle, ir_file_audio& audio, std::vector<BYTE>& data, INT32& result)
{
    std::memset(&audio, 0, sizeof(audio));
    result = ir_file_get_audio(handle, &audio);
    if (result != IR_FILE_EC_OK)
    {
        data.clear();
        return false;
    }

    data.assign(audio.length, 0);
    if (audio.length == 0)
    {
        return true;
    }

    audio.buffer = data.data();
    result = ir_file_get_audio(handle, &audio);
    if (result != IR_FILE_EC_OK)
    {
        data.clear();
        return false;
    }

    return true;
}

bool read_user_data(HANDLE handle, const std::string& key, std::string& value, INT32& result)
{
    ir_file_kvPair pair{};
    std::memset(pair.key, 0, sizeof(pair.key));
    const size_t key_copy_length = std::min(key.size(), sizeof(pair.key));
    std::memcpy(pair.key, key.data(), key_copy_length);

    result = ir_file_get_user_data(handle, &pair);
    if (result != IR_FILE_EC_OK)
    {
        value.clear();
        return false;
    }

    std::vector<BYTE> bytes(pair.vLength, 0);
    if (pair.vLength != 0)
    {
        pair.vBuffer = bytes.data();
        result = ir_file_get_user_data(handle, &pair);
        if (result != IR_FILE_EC_OK)
        {
            value.clear();
            return false;
        }
    }

    value = bytes_to_string(bytes.data(), bytes.size());
    return true;
}

INT32 set_user_data(HANDLE handle, const std::string& key, const std::string& value)
{
    ir_file_kvPair pair{};
    std::memset(pair.key, 0, sizeof(pair.key));
    const size_t key_copy_length = std::min(key.size(), sizeof(pair.key));
    std::memcpy(pair.key, key.data(), key_copy_length);

    std::vector<BYTE> bytes = make_bytes(value);
    pair.vLength = static_cast<UINT32>(bytes.size());
    pair.vBuffer = bytes.empty() ? nullptr : bytes.data();
    return ir_file_set_user_data(handle, &pair);
}

INT32 delete_user_data(HANDLE handle, const std::string& key)
{
    ir_file_kvPair pair{};
    std::memset(pair.key, 0, sizeof(pair.key));
    const size_t key_copy_length = std::min(key.size(), sizeof(pair.key));
    std::memcpy(pair.key, key.data(), key_copy_length);
    pair.vLength = 0;
    pair.vBuffer = nullptr;
    return ir_file_set_user_data(handle, &pair);
}

bool read_user_data_keys(HANDLE handle, std::vector<std::string>& keys, INT32& result)
{
    ir_file_keys key_list{};
    result = ir_file_get_keys(handle, &key_list);
    if (result != IR_FILE_EC_OK)
    {
        keys.clear();
        return false;
    }

    std::vector<BYTE> raw_keys(static_cast<size_t>(key_list.count) * kKeyLength, 0);
    if (key_list.count != 0)
    {
        key_list.keys = reinterpret_cast<ir_file_key*>(raw_keys.data());
        result = ir_file_get_keys(handle, &key_list);
        if (result != IR_FILE_EC_OK)
        {
            keys.clear();
            return false;
        }
    }

    keys.clear();
    keys.reserve(key_list.count);
    for (UINT32 i = 0; i < key_list.count; ++i)
    {
        const BYTE* key_ptr = raw_keys.data() + static_cast<size_t>(i) * kKeyLength;
        keys.push_back(fixed_bytes_to_string(key_ptr, kKeyLength));
    }
    return true;
}

bool read_preset_palettes(std::vector<ir_file_preset_palette>& palettes, INT32& result)
{
    ir_file_preset_palettes palette_list{};
    result = ir_file_get_preset_palettes(&palette_list);
    if (result != IR_FILE_EC_OK)
    {
        palettes.clear();
        return false;
    }

    palettes.assign(palette_list.count, ir_file_preset_palette{});
    if (palette_list.count == 0)
    {
        return true;
    }

    palette_list.palettes = palettes.data();
    result = ir_file_get_preset_palettes(&palette_list);
    if (result != IR_FILE_EC_OK)
    {
        palettes.clear();
        return false;
    }

    return true;
}

std::vector<std::string> split_lines(const std::string& value)
{
    std::vector<std::string> lines;
    std::string current;
    std::istringstream stream(value);
    while (std::getline(stream, current))
    {
        if (!current.empty())
        {
            lines.push_back(current);
        }
    }
    return lines;
}

std::string join_lines(const std::vector<std::string>& values)
{
    std::ostringstream oss;
    for (size_t i = 0; i < values.size(); ++i)
    {
        if (i != 0)
        {
            oss << '\n';
        }
        oss << values[i];
    }
    return oss.str();
}

std::string summarize_strings(const std::vector<std::string>& values, size_t limit = 5)
{
    if (values.empty())
    {
        return "(none)";
    }

    std::ostringstream oss;
    const size_t count = std::min(values.size(), limit);
    for (size_t i = 0; i < count; ++i)
    {
        if (i != 0)
        {
            oss << ", ";
        }
        oss << '"' << escape_newlines(values[i]) << '"';
    }
    if (values.size() > limit)
    {
        oss << ", ...";
    }
    return oss.str();
}

std::string summarize_presets(const std::vector<ir_file_preset_palette>& palettes, size_t limit = 5)
{
    if (palettes.empty())
    {
        return "(none)";
    }

    std::ostringstream oss;
    const size_t count = std::min(palettes.size(), limit);
    for (size_t i = 0; i < count; ++i)
    {
        if (i != 0)
        {
            oss << ", ";
        }
        oss << static_cast<int>(palettes[i].refNo) << ":" << fixed_bytes_to_string(palettes[i].name, kNameLength);
    }
    if (palettes.size() > limit)
    {
        oss << ", ...";
    }
    return oss.str();
}

std::string format_thermal_params(const ir_file_thermal_params& value)
{
    std::ostringstream oss;
    oss << "emissivity=" << std::fixed << std::setprecision(3) << value.emissivity
        << ", refl=" << format_temperature(value.reflTemp)
        << ", distance=" << value.distance << " m"
        << ", atm=" << format_temperature(value.atmTemp)
        << ", humidity=" << (value.relHumidity * 100.0f) << "%"
        << ", extOptTrans=" << value.extOptTrans
        << ", extOptTemp=" << format_temperature(value.extOptTemp);
    return oss.str();
}

std::string format_reference(const ir_file_reference& value)
{
    std::ostringstream oss;
    oss << "enabled=" << yes_no(value.enabled)
        << ", name=\"" << fixed_bytes_to_string(value.name, sizeof(value.name)) << "\""
        << ", valueType=" << temp_value_type_name(value.valueType)
        << ", temp=" << format_temperature(value.temp);
    return oss.str();
}

std::string format_gain(const ir_file_gain& value)
{
    std::ostringstream oss;
    oss << "mode=" << gain_mode_name(value.mode)
        << ", min=" << format_temperature(value.min)
        << ", max=" << format_temperature(value.max)
        << ", fixedSpan=" << yes_no(value.fixedSpan)
        << ", span=" << std::fixed << std::setprecision(2) << value.span << " K";
    return oss.str();
}

std::string format_alarm(const ir_file_alarm& value)
{
    std::ostringstream oss;
    oss << "type=" << alarm_type_name(value.type)
        << ", custom=" << yes_no(value.custom)
        << ", color=" << format_argb(value.color)
        << ", min=" << format_temperature(value.min)
        << ", max=" << format_temperature(value.max)
        << ", rhLimit=" << std::fixed << std::setprecision(1) << (value.rhLimit * 100.0f) << "%"
        << ", indoor=" << format_temperature(value.indoorTemp)
        << ", outdoor=" << format_temperature(value.outdoorTemp)
        << ", heatIndex=" << value.heatIndex;
    return oss.str();
}

std::string format_alarm_v2(const ir_file_alarm_v2& value)
{
    std::ostringstream oss;
    oss << format_alarm(*reinterpret_cast<const ir_file_alarm*>(&value))
        << ", threshold=" << format_temperature(value.threshold);
    return oss.str();
}

std::string format_palette(const ir_file_palette& value)
{
    std::ostringstream oss;
    oss << "refNo=" << static_cast<int>(value.refNo)
        << ", inverted=" << yes_no(value.inverted)
        << ", firstColor=" << format_argb(value.colors[0])
        << ", lastColor=" << format_argb(value.colors[255]);
    return oss.str();
}

std::string format_palette_v2(const ir_file_palette_v2& value)
{
    std::ostringstream oss;
    oss << "refNo=" << static_cast<int>(value.refNo)
        << ", inverted=" << yes_no(value.inverted)
        << ", srcFirst=" << format_argb(value.src_colors[0])
        << ", destFirst=" << format_argb(value.dest_colors[0]);
    return oss.str();
}

std::string format_view_info(const ir_file_view_info& value)
{
    std::ostringstream oss;
    oss << "type=" << view_type_name(value.type)
        << ", min=" << format_temperature(value.min)
        << ", max=" << format_temperature(value.max)
        << ", alpha=" << static_cast<int>(value.alpha)
        << ", area=" << format_rect_f(value.area);
    return oss.str();
}

ir_file_enum_color_dist_type alternate_color_dist(ir_file_enum_color_dist_type current)
{
    return current == IR_FILE_AD_LINEAR ? IR_FILE_TEMP_LINEAR : IR_FILE_AD_LINEAR;
}
} // namespace

void print_thermal_demo_usage(const char* argv0, std::ostream& os)
{
    os
        << "Usage: " << argv0 << " <input-file> [command] [command-options]\n"
        << "\n"
        << "Default command:\n"
        << "  demo                 Keep the original aggregated getter/setter walkthrough\n"
        << "\n"
        << "Primary Windows button-chain mappings:\n"
        << "  open-read            Open + sync markers/deltas + read metadata/media\n"
        << "  save                 Set cover image, then ir_file_save/ir_file_save_fjpg\n"
        << "  save-as              Set cover image, then ir_file_save_as/ir_file_save_as_fjpg\n"
        << "  save-frame           ir_file_save_frame_as for the current frame\n"
        << "  marker-list          List markers via ir_file_get_marker_names/get_marker\n"
        << "  marker-get           Read one marker via ir_file_get_marker\n"
        << "  marker-add           Add/update one marker via ir_file_set_marker\n"
        << "  marker-rename        Rename one marker via ir_file_rename_marker\n"
        << "  marker-local-params  Toggle/update marker local params via ir_file_set_marker\n"
        << "  marker-remove        Remove one marker via ir_file_remove_marker\n"
        << "  delta-list           List deltas via ir_file_get_delta_names/get_delta\n"
        << "  delta-get            Read one delta via ir_file_get_delta\n"
        << "  delta-add            Add/update one delta via ir_file_set_delta\n"
        << "  delta-remove         Remove one delta via ir_file_remove_delta\n"
        << "  frame-goto           Set current frame index via ir_file_set_frame_index\n"
        << "  frame-next           Advance one frame like the Next button\n"
        << "  frame-prev           Move back one frame like the Previous button\n"
        << "  frame-stop           Reset to frame 0 like the Stop button\n"
        << "  play-steps           Simulate timer ticks with repeated ir_file_set_frame_index\n"
        << "  clip-stream          Save a frame range via ir_file_save_as_stream/append_frame/save\n"
        << "  rotate               Rotate via ir_file_set_rotation\n"
        << "  mirror               Mirror via ir_file_set_mirror\n"
        << "  auto-adjust          Get/set gain with mode=auto\n"
        << "  palette-invert       Toggle palette inversion via ir_file_get_palette/set_palette\n"
        << "  export-marker-temperature  Export one marker temperature CSV\n"
        << "  export-marker-curve        Export one marker temperature curve CSV\n"
        << "  export-delta-curve         Export one delta curve CSV\n"
        << "  export-frame-ad            Export one frame AD CSV\n"
        << "  export-frame-temperature   Export one frame temperature CSV\n"
        << "\n"
        << "Legacy demo options (for the default `demo` command):\n"
        << "  --output <path>      Export the current thermal image preview to PPM\n"
        << "  --frame-index <n>    Select the starting frame before the CLI demo\n"
        << "  --no-export          Skip writing the thermal preview file\n"
        << "  --help               Show this help\n";
}

bool parse_thermal_demo_args(int argc, char** argv, ThermalDemoOptions& options, bool& help_requested, std::ostream& err)
{
    help_requested = false;
    bool command_explicitly_set = false;
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h")
        {
            help_requested = true;
            return false;
        }

        if (options.input_path.empty())
        {
            if (!arg.empty() && arg[0] == '-')
            {
                err << "Missing input file path before option: " << arg << "\n";
                return false;
            }
            options.input_path = arg;
            continue;
        }

        if (!command_explicitly_set && !arg.empty() && arg[0] != '-')
        {
            options.command = arg;
            command_explicitly_set = true;
            continue;
        }

        options.command_args.push_back(arg);
    }

    if (options.input_path.empty())
    {
        err << "Missing input file path.\n";
        return false;
    }

    if (!command_explicitly_set)
    {
        options.command = "demo";
    }

    if (options.command == "demo")
    {
        for (size_t i = 0; i < options.command_args.size(); ++i)
        {
            const std::string& arg = options.command_args[i];
            if (arg == "--output" && i + 1 < options.command_args.size())
            {
                options.output_path = options.command_args[++i];
            }
            else if (arg == "--frame-index" && i + 1 < options.command_args.size())
            {
                options.frame_index = std::atoi(options.command_args[++i].c_str());
            }
            else if (arg == "--no-export")
            {
                options.export_image = false;
            }
            else
            {
                err << "Unknown demo option: " << arg << "\n";
                return false;
            }
        }
    }

    return true;
}

int run_thermal_file_demo_aggregate(const ThermalDemoOptions& options, std::ostream& out, std::ostream& err)
{
    INT32 result = IR_FILE_EC_FAIL;
    UINT32 sdk_version = 0;
    result = ir_file_get_sdk_version(&sdk_version, nullptr);
    if (result == IR_FILE_EC_OK)
    {
        out << "IRtek Thermal CLI demo\n";
        out << "SDK version: " << decode_version(sdk_version) << "\n";
    }

    print_section(out, 1, "Open and identify file");
    ir_file_enum_file_type peek_type = IR_FILE_OTHER;
    result = ir_file_peek_file_type(options.input_path.c_str(), &peek_type);
    print_call(out, "ir_file_peek_file_type", result, options.input_path + " -> " + file_type_name(peek_type));
    if (result != IR_FILE_EC_OK)
    {
        err << "Failed to identify input file: " << options.input_path << "\n";
        return 1;
    }

    HandleCloser handle_guard;
    result = ir_file_open(options.input_path.c_str(), &handle_guard.handle);
    print_call(out, "ir_file_open", result, options.input_path);
    if (result != IR_FILE_EC_OK || handle_guard.handle == nullptr)
    {
        err << "Failed to open input file: " << options.input_path << "\n";
        return 1;
    }

    print_section(out, 2, "Read file and frame metadata");
    std::vector<BYTE> raw_buffer;
    result = IR_FILE_EC_FAIL;
    std::string file_name;
    if (read_handle_buffer(handle_guard.handle, ir_file_get_file_name, raw_buffer, result))
    {
        file_name = bytes_to_string(raw_buffer.data(), raw_buffer.size());
    }
    print_call(out, "ir_file_get_file_name", result, file_name.empty() ? "(empty)" : file_name);

    ir_file_enum_file_type file_type = IR_FILE_OTHER;
    result = ir_file_get_file_type(handle_guard.handle, &file_type);
    print_call(out, "ir_file_get_file_type", result, file_type_name(file_type));

    USHORT file_version = 0;
    result = ir_file_get_file_version(handle_guard.handle, &file_version);
    print_call(out, "ir_file_get_file_version", result, std::to_string(file_version));

    UINT32 frame_count = 0;
    result = ir_file_get_frame_count(handle_guard.handle, &frame_count);
    print_call(out, "ir_file_get_frame_count", result, std::to_string(frame_count));

    float frame_rate = 0.0f;
    result = ir_file_get_frame_rate(handle_guard.handle, &frame_rate);
    {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << frame_rate << " fps";
        print_call(out, "ir_file_get_frame_rate", result, oss.str());
    }

    UINT32 current_frame_index = 0;
    result = ir_file_get_frame_index(handle_guard.handle, &current_frame_index);
    print_call(out, "ir_file_get_frame_index", result, std::to_string(current_frame_index));

    if (options.frame_index >= 0)
    {
        result = ir_file_set_frame_index(handle_guard.handle, static_cast<UINT32>(options.frame_index));
        print_call(out, "ir_file_set_frame_index(user)", result, std::to_string(options.frame_index));
        if (result == IR_FILE_EC_OK)
        {
            result = ir_file_get_frame_index(handle_guard.handle, &current_frame_index);
            print_call(out, "ir_file_get_frame_index(after user set)", result, std::to_string(current_frame_index));
        }
    }

    const UINT32 selected_frame_index = current_frame_index;
    if (frame_count != 0)
    {
        const UINT32 demo_frame_index = frame_count > 1 ? ((selected_frame_index + 1) % frame_count) : 0;
        result = ir_file_set_frame_index(handle_guard.handle, demo_frame_index);
        print_call(out, "ir_file_set_frame_index(demo)", result, std::to_string(demo_frame_index));
        if (result == IR_FILE_EC_OK)
        {
            UINT32 verify_frame_index = 0;
            result = ir_file_get_frame_index(handle_guard.handle, &verify_frame_index);
            print_call(out, "ir_file_get_frame_index(after demo set)", result, std::to_string(verify_frame_index));

            result = ir_file_set_frame_index(handle_guard.handle, selected_frame_index);
            print_call(out, "ir_file_set_frame_index(restore)", result, std::to_string(selected_frame_index));
        }
    }

    print_section(out, 3, "Read properties used by MainForm");
    ir_file_temp_range temp_range{};
    result = ir_file_get_temp_range(handle_guard.handle, &temp_range);
    if (result == IR_FILE_EC_OK)
    {
        print_call(out, "ir_file_get_temp_range", result,
            format_temperature(temp_range.min) + " .. " + format_temperature(temp_range.max));
    }
    else
    {
        print_call(out, "ir_file_get_temp_range", result, "");
    }

    ir_file_size thermal_size{};
    result = ir_file_get_thermal_size(handle_guard.handle, &thermal_size);
    print_call(out, "ir_file_get_thermal_size", result,
        std::to_string(thermal_size.width) + "x" + std::to_string(thermal_size.height));
    if (result != IR_FILE_EC_OK)
    {
        err << "ir_file_get_thermal_size failed; cannot continue demo.\n";
        return 1;
    }

    ir_file_camera_info camera_info{};
    result = ir_file_get_camera_info(handle_guard.handle, &camera_info);
    if (result == IR_FILE_EC_OK)
    {
        std::ostringstream oss;
        oss << "model=\"" << fixed_bytes_to_string(camera_info.model, sizeof(camera_info.model)) << "\""
            << ", serial=\"" << fixed_bytes_to_string(camera_info.serialNo, sizeof(camera_info.serialNo)) << "\""
            << ", brand=\"" << fixed_bytes_to_string(camera_info.brand, sizeof(camera_info.brand)) << "\""
            << ", company=\"" << fixed_bytes_to_string(camera_info.company, sizeof(camera_info.company)) << "\""
            << ", logoBytes=" << camera_info.logoLength;
        print_call(out, "ir_file_get_camera_info", result, oss.str());
    }
    else
    {
        print_call(out, "ir_file_get_camera_info", result, "");
    }

    ir_file_lens_info_v2 lens_info{};
    result = ir_file_get_lens_info_v2(handle_guard.handle, &lens_info);
    if (result == IR_FILE_EC_OK)
    {
        std::ostringstream oss;
        oss << "name=\"" << fixed_bytes_to_string(lens_info.name, sizeof(lens_info.name)) << "\""
            << ", focalLength=" << lens_info.focalLength << " mm"
            << ", hfov=" << lens_info.hfov << " deg"
            << ", band=\"" << fixed_bytes_to_string(lens_info.band, sizeof(lens_info.band)) << "\"";
        print_call(out, "ir_file_get_lens_info_v2", result, oss.str());
    }
    else
    {
        print_call(out, "ir_file_get_lens_info_v2", result, "");
    }

    ir_file_mapping_info mapping_info{};
    result = ir_file_get_mapping_info(handle_guard.handle, &mapping_info);
    if (result == IR_FILE_EC_OK)
    {
        std::ostringstream oss;
        oss << "visualSize=" << mapping_info.visualSize.width << "x" << mapping_info.visualSize.height
            << ", area=" << format_rect_f(mapping_info.mappingArea);
        print_call(out, "ir_file_get_mapping_info", result, oss.str());
    }
    else
    {
        print_call(out, "ir_file_get_mapping_info", result, "");
    }

    ir_file_zoom_info zoom_info{};
    result = ir_file_get_zoom_info(handle_guard.handle, &zoom_info);
    if (result == IR_FILE_EC_OK)
    {
        std::ostringstream oss;
        oss << "enabled=" << yes_no(zoom_info.enabled)
            << ", location=" << format_point_f(zoom_info.location)
            << ", ratio=" << std::fixed << std::setprecision(2) << zoom_info.ratio;
        print_call(out, "ir_file_get_zoom_info", result, oss.str());
    }
    else
    {
        print_call(out, "ir_file_get_zoom_info", result, "");
    }

    ir_file_geo_location geo_location{};
    result = ir_file_get_geo_location(handle_guard.handle, &geo_location);
    if (result == IR_FILE_EC_OK)
    {
        std::ostringstream oss;
        oss << "gpsValid=" << yes_no(geo_location.isGPSValid)
            << ", lon=" << geo_location.longitude
            << ", lat=" << geo_location.latitude
            << ", alt=" << geo_location.altitude
            << ", compassValid=" << yes_no(geo_location.isCompassValid)
            << ", direction=" << geo_location.direction;
        print_call(out, "ir_file_get_geo_location", result, oss.str());
    }
    else
    {
        print_call(out, "ir_file_get_geo_location", result, "");
    }

    ir_file_enum_image_enh_type image_enhancement = IR_FILE_NONE;
    result = ir_file_get_image_enh(handle_guard.handle, &image_enhancement);
    print_call(out, "ir_file_get_image_enh", result, image_enh_name(image_enhancement));

    UINT64 shot_time = 0;
    result = ir_file_get_shot_time(handle_guard.handle, &shot_time);
    print_call(out, "ir_file_get_shot_time", result, format_timestamp(shot_time));

    UINT64 modified_time = 0;
    result = ir_file_get_modified_time(handle_guard.handle, &modified_time);
    print_call(out, "ir_file_get_modified_time", result, format_timestamp(modified_time));

    print_section(out, 4, "Read images, palette bands, audio, and frames");
    raw_buffer.clear();
    result = IR_FILE_EC_FAIL;
    if (read_handle_buffer(handle_guard.handle, ir_file_get_cover_image, raw_buffer, result))
    {
        print_call(out, "ir_file_get_cover_image", result, std::to_string(raw_buffer.size()) + " bytes");
    }
    else
    {
        print_call(out, "ir_file_get_cover_image", result, "");
    }

    raw_buffer.clear();
    result = IR_FILE_EC_FAIL;
    if (read_handle_buffer(handle_guard.handle, ir_file_get_visual_image, raw_buffer, result))
    {
        print_call(out, "ir_file_get_visual_image", result, std::to_string(raw_buffer.size()) + " bytes");
    }
    else
    {
        print_call(out, "ir_file_get_visual_image", result, "");
    }

    ir_file_audio audio{};
    std::vector<BYTE> audio_bytes;
    result = IR_FILE_EC_FAIL;
    if (read_audio(handle_guard.handle, audio, audio_bytes, result))
    {
        std::ostringstream oss;
        oss << "type=" << audio_type_name(audio.type) << ", bytes=" << audio_bytes.size();
        print_call(out, "ir_file_get_audio", result, oss.str());
    }
    else
    {
        print_call(out, "ir_file_get_audio", result, "");
    }

    std::vector<BYTE> palette_band(static_cast<size_t>(256) * 3, 0);
    ir_file_image palette_image{};
    palette_image.offset = 0;
    palette_image.data = palette_band.data();
    result = ir_file_get_palette_image(handle_guard.handle, &palette_image);
    print_call(out, "ir_file_get_palette_image", result, result == IR_FILE_EC_OK ? "256x1 BGR band" : "");

    std::vector<ir_file_preset_palette> preset_palettes;
    result = IR_FILE_EC_FAIL;
    if (read_preset_palettes(preset_palettes, result))
    {
        std::ostringstream oss;
        oss << "count=" << preset_palettes.size() << ", sample=" << summarize_presets(preset_palettes);
        print_call(out, "ir_file_get_preset_palettes", result, oss.str());
    }
    else
    {
        print_call(out, "ir_file_get_preset_palettes", result, "");
    }

    ir_file_palette_v2 palette_v2{};
    result = ir_file_get_palette_v2(handle_guard.handle, &palette_v2);
    print_call(out, "ir_file_get_palette_v2", result, result == IR_FILE_EC_OK ? format_palette_v2(palette_v2) : "");
    if (result == IR_FILE_EC_OK)
    {
        std::vector<BYTE> preset_palette_band(static_cast<size_t>(256) * 3, 0);
        ir_file_image preset_palette_image{};
        preset_palette_image.offset = 0;
        preset_palette_image.data = preset_palette_band.data();
        const INT32 preset_result = ir_file_get_preset_palette_image(palette_v2.refNo, &preset_palette_image);
        print_call(out, "ir_file_get_preset_palette_image", preset_result,
            preset_result == IR_FILE_EC_OK ? ("refNo=" + std::to_string(palette_v2.refNo) + ", 256x1 BGR band") : "");
    }

    ir_file_frame frame{};
    frame.length = static_cast<UINT32>(thermal_size.width) * static_cast<UINT32>(thermal_size.height);
    std::vector<USHORT> frame_data(frame.length, 0);
    frame.buffer = frame_data.data();
    frame.timeStamp = 0;
    result = ir_file_get_frame(handle_guard.handle, selected_frame_index, &frame);
    if (result == IR_FILE_EC_OK)
    {
        const auto minmax = std::minmax_element(frame_data.begin(), frame_data.end());
        std::ostringstream oss;
        oss << "index=" << selected_frame_index
            << ", time=" << format_timestamp(frame.timeStamp)
            << ", samples=" << frame.length
            << ", minAD=" << *minmax.first
            << ", maxAD=" << *minmax.second;
        print_call(out, "ir_file_get_frame", result, oss.str());
    }
    else
    {
        print_call(out, "ir_file_get_frame", result, "");
    }

    std::vector<BYTE> thermal_bgr(static_cast<size_t>(thermal_size.width) * thermal_size.height * 3, 0);
    ir_file_image thermal_image{};
    thermal_image.offset = 0;
    thermal_image.data = thermal_bgr.data();
    result = ir_file_get_thermal_image(handle_guard.handle, &thermal_image);
    print_call(out, "ir_file_get_thermal_image", result,
        result == IR_FILE_EC_OK ? (std::to_string(thermal_size.width) + "x" + std::to_string(thermal_size.height) + " BGR image") : "");

    if (result == IR_FILE_EC_OK && options.export_image)
    {
        if (!save_ppm_bgr(options.output_path, thermal_size.width, thermal_size.height, thermal_bgr))
        {
            err << "Failed to write preview image to " << options.output_path << "\n";
            return 1;
        }
        out << "  - export preview: OK -> " << options.output_path << "\n";
    }
    else if (!options.export_image)
    {
        out << "  - export preview: skipped (--no-export)\n";
    }

    if (!frame_data.empty())
    {
        std::vector<BYTE> frame_bgr(static_cast<size_t>(thermal_size.width) * thermal_size.height * 3, 0);
        ir_file_image frame_image{};
        frame_image.offset = 0;
        frame_image.data = frame_bgr.data();
        result = ir_file_get_thermal_image_from_frame(handle_guard.handle, &frame, IR_FILE_BGR, &frame_image);
        print_call(out, "ir_file_get_thermal_image_from_frame", result,
            result == IR_FILE_EC_OK ? "current frame converted to BGR image" : "");
    }

    print_section(out, 5, "Demonstrate getters and setters used by MainForm");
    ir_file_thermal_params thermal_params{};
    result = ir_file_get_thermal_params(handle_guard.handle, &thermal_params);
    print_call(out, "ir_file_get_thermal_params", result, result == IR_FILE_EC_OK ? format_thermal_params(thermal_params) : "");
    if (result == IR_FILE_EC_OK)
    {
        ir_file_thermal_params changed = thermal_params;
        changed.emissivity = std::clamp(
            thermal_params.emissivity >= 0.99f ? thermal_params.emissivity - 0.01f : thermal_params.emissivity + 0.01f,
            0.01f, 1.0f);
        INT32 set_result = ir_file_set_thermal_params(handle_guard.handle, &changed);
        print_call(out, "ir_file_set_thermal_params", set_result,
            set_result == IR_FILE_EC_OK ? ("temporary emissivity=" + std::to_string(changed.emissivity)) : "");
        if (set_result == IR_FILE_EC_OK)
        {
            ir_file_thermal_params verify{};
            const INT32 verify_result = ir_file_get_thermal_params(handle_guard.handle, &verify);
            print_call(out, "ir_file_get_thermal_params(verify)", verify_result,
                verify_result == IR_FILE_EC_OK ? format_thermal_params(verify) : "");
            const INT32 restore_result = ir_file_set_thermal_params(handle_guard.handle, &thermal_params);
            print_call(out, "ir_file_set_thermal_params(restore)", restore_result, "restored original values");
        }
    }

    ir_file_reference reference{};
    result = ir_file_get_reference(handle_guard.handle, &reference);
    print_call(out, "ir_file_get_reference", result, result == IR_FILE_EC_OK ? format_reference(reference) : "");
    if (result == IR_FILE_EC_OK)
    {
        if (reference.enabled != 0)
        {
            float reference_temperature = 0.0f;
            const INT32 ref_temp_result = ir_file_get_reference_temp(handle_guard.handle, &reference_temperature);
            print_call(out, "ir_file_get_reference_temp", ref_temp_result,
                ref_temp_result == IR_FILE_EC_OK ? format_temperature(reference_temperature) : "");
        }

        ir_file_reference changed = reference;
        std::string mutation_note = "round-trip with original values";
        if (fixed_bytes_to_string(reference.name, sizeof(reference.name)).empty())
        {
            changed.temp = reference.temp + 1.0f;
            mutation_note = "temporary temp=" + format_temperature(changed.temp);
        }

        INT32 set_result = ir_file_set_reference(handle_guard.handle, &changed);
        print_call(out, "ir_file_set_reference", set_result, mutation_note);
        if (set_result == IR_FILE_EC_OK)
        {
            ir_file_reference verify{};
            const INT32 verify_result = ir_file_get_reference(handle_guard.handle, &verify);
            print_call(out, "ir_file_get_reference(verify)", verify_result,
                verify_result == IR_FILE_EC_OK ? format_reference(verify) : "");
            const INT32 restore_result = ir_file_set_reference(handle_guard.handle, &reference);
            print_call(out, "ir_file_set_reference(restore)", restore_result, "restored original values");
        }
    }

    ir_file_gain gain{};
    result = ir_file_get_gain(handle_guard.handle, &gain);
    print_call(out, "ir_file_get_gain", result, result == IR_FILE_EC_OK ? format_gain(gain) : "");
    if (result == IR_FILE_EC_OK)
    {
        ir_file_gain roundtrip_gain = gain;
        INT32 set_result = ir_file_set_gain(handle_guard.handle, &roundtrip_gain);
        print_call(out, "ir_file_set_gain", set_result, "round-trip with original values");
        if (set_result == IR_FILE_EC_OK)
        {
            ir_file_gain verify{};
            const INT32 verify_result = ir_file_get_gain(handle_guard.handle, &verify);
            print_call(out, "ir_file_get_gain(verify)", verify_result,
                verify_result == IR_FILE_EC_OK ? format_gain(verify) : "");
        }
    }

    ir_file_alarm alarm{};
    result = ir_file_get_alarm(handle_guard.handle, &alarm);
    print_call(out, "ir_file_get_alarm", result, result == IR_FILE_EC_OK ? format_alarm(alarm) : "");
    if (result == IR_FILE_EC_OK)
    {
        INT32 set_result = ir_file_set_alarm(handle_guard.handle, &alarm);
        print_call(out, "ir_file_set_alarm", set_result, "round-trip with original values");
    }

    ir_file_alarm_v2 alarm_v2{};
    result = ir_file_get_alarm_v2(handle_guard.handle, &alarm_v2);
    print_call(out, "ir_file_get_alarm_v2", result, result == IR_FILE_EC_OK ? format_alarm_v2(alarm_v2) : "");

    ir_file_palette palette{};
    result = ir_file_get_palette(handle_guard.handle, &palette);
    print_call(out, "ir_file_get_palette", result, result == IR_FILE_EC_OK ? format_palette(palette) : "");
    if (result == IR_FILE_EC_OK)
    {
        ir_file_palette changed = palette;
        changed.inverted = changed.inverted == 0 ? 1 : 0;
        INT32 set_result = ir_file_set_palette(handle_guard.handle, &changed);
        print_call(out, "ir_file_set_palette", set_result,
            set_result == IR_FILE_EC_OK ? ("temporary inverted=" + yes_no(changed.inverted)) : "");
        if (set_result == IR_FILE_EC_OK)
        {
            ir_file_palette verify{};
            const INT32 verify_result = ir_file_get_palette(handle_guard.handle, &verify);
            print_call(out, "ir_file_get_palette(verify)", verify_result,
                verify_result == IR_FILE_EC_OK ? format_palette(verify) : "");
            const INT32 restore_result = ir_file_set_palette(handle_guard.handle, &palette);
            print_call(out, "ir_file_set_palette(restore)", restore_result, "restored original values");
        }
    }

    ir_file_enum_color_dist_type color_dist = IR_FILE_AD_LINEAR;
    result = ir_file_get_color_dist(handle_guard.handle, &color_dist);
    print_call(out, "ir_file_get_color_dist", result, color_dist_name(color_dist));
    if (result == IR_FILE_EC_OK)
    {
        const ir_file_enum_color_dist_type changed = alternate_color_dist(color_dist);
        INT32 set_result = ir_file_set_color_dist(handle_guard.handle, changed);
        print_call(out, "ir_file_set_color_dist", set_result, color_dist_name(changed));
        if (set_result == IR_FILE_EC_OK)
        {
            ir_file_enum_color_dist_type verify = IR_FILE_AD_LINEAR;
            const INT32 verify_result = ir_file_get_color_dist(handle_guard.handle, &verify);
            print_call(out, "ir_file_get_color_dist(verify)", verify_result, color_dist_name(verify));
            const INT32 restore_result = ir_file_set_color_dist(handle_guard.handle, color_dist);
            print_call(out, "ir_file_set_color_dist(restore)", restore_result, color_dist_name(color_dist));
        }
    }

    ir_file_view_info view_info{};
    result = ir_file_get_view_info(handle_guard.handle, &view_info);
    print_call(out, "ir_file_get_view_info", result, result == IR_FILE_EC_OK ? format_view_info(view_info) : "");
    if (result == IR_FILE_EC_OK)
    {
        INT32 set_result = ir_file_set_view_info(handle_guard.handle, &view_info);
        print_call(out, "ir_file_set_view_info", set_result, "round-trip with original values");
        if (set_result == IR_FILE_EC_OK)
        {
            ir_file_view_info verify{};
            const INT32 verify_result = ir_file_get_view_info(handle_guard.handle, &verify);
            print_call(out, "ir_file_get_view_info(verify)", verify_result,
                verify_result == IR_FILE_EC_OK ? format_view_info(verify) : "");
        }
    }

    print_section(out, 6, "Demonstrate note, tag, and user data helpers");
    raw_buffer.clear();
    result = IR_FILE_EC_FAIL;
    std::string note;
    if (read_handle_buffer(handle_guard.handle, ir_file_get_note, raw_buffer, result))
    {
        note = bytes_to_string(raw_buffer.data(), raw_buffer.size());
        print_call(out, "ir_file_get_note", result, "\"" + escape_newlines(note) + "\"");

        const std::string changed_note = note + (note.empty() ? "cli-demo-note" : " [cli-demo]");
        ir_file_buffer note_buffer{};
        std::vector<BYTE> note_bytes = make_bytes(changed_note);
        note_buffer.length = static_cast<UINT32>(note_bytes.size());
        note_buffer.buffer = note_bytes.empty() ? nullptr : note_bytes.data();
        INT32 set_result = ir_file_set_note(handle_guard.handle, &note_buffer);
        print_call(out, "ir_file_set_note", set_result, "\"" + escape_newlines(changed_note) + "\"");
        if (set_result == IR_FILE_EC_OK)
        {
            std::vector<BYTE> verify_note_bytes;
            INT32 verify_result = IR_FILE_EC_FAIL;
            if (read_handle_buffer(handle_guard.handle, ir_file_get_note, verify_note_bytes, verify_result))
            {
                print_call(out, "ir_file_get_note(verify)", verify_result,
                    "\"" + escape_newlines(bytes_to_string(verify_note_bytes.data(), verify_note_bytes.size())) + "\"");
            }
            else
            {
                print_call(out, "ir_file_get_note(verify)", verify_result, "");
            }

            ir_file_buffer restore_buffer{};
            std::vector<BYTE> restore_note_bytes = make_bytes(note);
            restore_buffer.length = static_cast<UINT32>(restore_note_bytes.size());
            restore_buffer.buffer = restore_note_bytes.empty() ? nullptr : restore_note_bytes.data();
            const INT32 restore_result = restore_note_bytes.empty()
                ? ir_file_set_note(handle_guard.handle, nullptr)
                : ir_file_set_note(handle_guard.handle, &restore_buffer);
            print_call(out, "ir_file_set_note(restore)", restore_result, "restored original note");
        }
    }
    else
    {
        print_call(out, "ir_file_get_note", result, "");
    }

    std::string tag_blob;
    result = IR_FILE_EC_FAIL;
    if (read_user_data(handle_guard.handle, kTagUserDataKey, tag_blob, result))
    {
        std::vector<std::string> tags = split_lines(tag_blob);
        print_call(out, "tag mapping (GetTagAnnotation -> TAG user data)", result, summarize_strings(tags));

        std::vector<std::string> changed_tags = tags;
        changed_tags.push_back("cli-demo-tag");
        const INT32 set_result = set_user_data(handle_guard.handle, kTagUserDataKey, join_lines(changed_tags));
        print_call(out, "tag mapping (SetTagAnnotation -> TAG user data)", set_result, summarize_strings(changed_tags));
        if (set_result == IR_FILE_EC_OK)
        {
            std::string verify_tag_blob;
            INT32 verify_result = IR_FILE_EC_FAIL;
            if (read_user_data(handle_guard.handle, kTagUserDataKey, verify_tag_blob, verify_result))
            {
                print_call(out, "tag mapping verify", verify_result, summarize_strings(split_lines(verify_tag_blob)));
            }
            else
            {
                print_call(out, "tag mapping verify", verify_result, "");
            }

            const INT32 restore_result = set_user_data(handle_guard.handle, kTagUserDataKey, tag_blob);
            print_call(out, "tag mapping restore", restore_result, "restored original TAG data");
        }
    }
    else
    {
        print_call(out, "tag mapping (GetTagAnnotation -> TAG user data)", result, "");
    }

    std::vector<std::string> user_keys;
    result = IR_FILE_EC_FAIL;
    if (read_user_data_keys(handle_guard.handle, user_keys, result))
    {
        print_call(out, "ir_file_get_keys", result, summarize_strings(user_keys));
        for (const std::string& key : user_keys)
        {
            std::string value;
            INT32 value_result = IR_FILE_EC_FAIL;
            if (read_user_data(handle_guard.handle, key, value, value_result))
            {
                print_call(out, "ir_file_get_user_data(" + key + ")", value_result, "\"" + escape_newlines(value) + "\"");
            }
            else
            {
                print_call(out, "ir_file_get_user_data(" + key + ")", value_result, "");
            }
        }
    }
    else
    {
        print_call(out, "ir_file_get_keys", result, "");
    }

    const std::string demo_user_value = "cli-demo-value";
    result = set_user_data(handle_guard.handle, kDemoUserDataKey, demo_user_value);
    print_call(out, "ir_file_set_user_data(add/update)", result,
        std::string(kDemoUserDataKey) + "=\"" + demo_user_value + "\"");
    if (result == IR_FILE_EC_OK)
    {
        std::string verify_demo_user_value;
        INT32 verify_result = IR_FILE_EC_FAIL;
        if (read_user_data(handle_guard.handle, kDemoUserDataKey, verify_demo_user_value, verify_result))
        {
            print_call(out, "ir_file_get_user_data(" + std::string(kDemoUserDataKey) + ")", verify_result,
                "\"" + escape_newlines(verify_demo_user_value) + "\"");
        }
        else
        {
            print_call(out, "ir_file_get_user_data(" + std::string(kDemoUserDataKey) + ")", verify_result, "");
        }

        const INT32 delete_result = delete_user_data(handle_guard.handle, kDemoUserDataKey);
        print_call(out, "ir_file_set_user_data(delete)", delete_result, std::string(kDemoUserDataKey) + " removed");
    }

    out << "\nNote: setter calls above only modify the in-memory handle for this demo. "
        << "This CLI sample does not call ir_file_save(), so the source file is not persisted.\n";

    return 0;
}
