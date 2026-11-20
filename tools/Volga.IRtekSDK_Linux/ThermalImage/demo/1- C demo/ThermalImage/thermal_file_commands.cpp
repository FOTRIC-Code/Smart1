#include "thermal_file_demo.h"

#include "IRtekFileSDK.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace
{
constexpr float kKelvinOffset = 273.15f;
constexpr size_t kKeyLength = 16;
constexpr size_t kNameLength = 32;
constexpr const char* kSpotPrefix = "Sp";
constexpr const char* kAreaPrefix = "Ar";
constexpr const char* kPolylinePrefix = "Li";
constexpr const char* kDeltaPrefix = "De";

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

struct MarkerData
{
    ir_file_enum_marker_type type = IR_FILE_SPOT;
    std::string name;
    BYTE local_params = 0;
    float refl_temp = 293.15f;
    float distance = 1.0f;
    float emissivity = 1.0f;
    std::vector<ir_file_point> points;
    std::array<float, 3> temp_values{0.0f, 0.0f, 0.0f};
    std::array<ir_file_point, 2> temp_spots{{ir_file_point{0, 0}, ir_file_point{0, 0}}};
    float measured_value = 0.0f;
    std::string ex_info;
};

struct DeltaData
{
    std::string name;
    std::string src_name;
    ir_file_enum_temp_value_type src_type = IR_FILE_MAX;
    float src_temp = 293.15f;
    std::string dest_name;
    ir_file_enum_temp_value_type dest_type = IR_FILE_MAX;
    float dest_temp = 293.15f;
    float value = 0.0f;
};

struct FrameData
{
    UINT64 timestamp = 0;
    std::vector<USHORT> samples;
    int width = 0;
    int height = 0;
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
    case IR_FILE_OTHER:
    default:
        return "other";
    }
}

const char* marker_type_name(ir_file_enum_marker_type type)
{
    switch (type)
    {
    case IR_FILE_SPOT:
        return "spot";
    case IR_FILE_RECTANGLE:
        return "rectangle";
    case IR_FILE_ELLIPSE:
        return "ellipse";
    case IR_FILE_POLYGON:
        return "polygon";
    case IR_FILE_POLYLINE:
        return "polyline";
    default:
        return "unknown";
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

std::string yes_no(BYTE value)
{
    return value == 0 ? "false" : "true";
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

std::string format_temperature(float value)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2)
        << value << " K (" << (value - kKelvinOffset) << " C)";
    return oss.str();
}

std::string format_point(const ir_file_point& point)
{
    std::ostringstream oss;
    oss << "(" << point.x << ", " << point.y << ")";
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
        << "(" << rect.x << ", " << rect.y << ", " << rect.width << ", " << rect.height << ")";
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

std::string format_argb(UINT32 argb)
{
    std::ostringstream oss;
    oss << "0x" << std::uppercase << std::hex << std::setw(8) << std::setfill('0') << argb;
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

void print_note(std::ostream& out, const std::string& detail)
{
    out << "  - note: " << detail << "\n";
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

bool read_binary_file(const std::string& path, std::vector<BYTE>& data)
{
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open())
    {
        return false;
    }

    input.seekg(0, std::ios::end);
    const std::streamoff size = input.tellg();
    input.seekg(0, std::ios::beg);
    if (size < 0)
    {
        return false;
    }

    data.assign(static_cast<size_t>(size), 0);
    if (!data.empty())
    {
        input.read(reinterpret_cast<char*>(data.data()), size);
    }
    return static_cast<bool>(input) || input.eof();
}

bool write_text_file(const std::string& path, const std::string& content)
{
    std::ofstream output(path, std::ios::binary);
    if (!output.is_open())
    {
        return false;
    }
    output << content;
    return static_cast<bool>(output);
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
        const BYTE* row = bgr.data() + static_cast<size_t>(y) * static_cast<size_t>(width) * 3;
        for (int x = 0; x < width; ++x)
        {
            output.put(static_cast<char>(row[x * 3 + 2]));
            output.put(static_cast<char>(row[x * 3 + 1]));
            output.put(static_cast<char>(row[x * 3 + 0]));
        }
    }

    return static_cast<bool>(output);
}

std::vector<std::string> split_string(const std::string& value, char delimiter)
{
    std::vector<std::string> parts;
    std::string current;
    std::istringstream stream(value);
    while (std::getline(stream, current, delimiter))
    {
        parts.push_back(current);
    }
    return parts;
}

std::optional<int> parse_int_value(const std::string& text)
{
    char* end = nullptr;
    const long value = std::strtol(text.c_str(), &end, 10);
    if (end == nullptr || *end != '\0')
    {
        return std::nullopt;
    }
    return static_cast<int>(value);
}

std::optional<float> parse_float_value(const std::string& text)
{
    char* end = nullptr;
    const float value = std::strtof(text.c_str(), &end);
    if (end == nullptr || *end != '\0')
    {
        return std::nullopt;
    }
    return value;
}

std::optional<bool> parse_bool_value(const std::string& text)
{
    if (text == "1" || text == "true" || text == "yes" || text == "on")
    {
        return true;
    }
    if (text == "0" || text == "false" || text == "no" || text == "off")
    {
        return false;
    }
    return std::nullopt;
}

bool expect_value(const std::vector<std::string>& args, size_t& index, const std::string& option, std::string& value, std::ostream& err)
{
    if (index + 1 >= args.size())
    {
        err << "Missing value for " << option << "\n";
        return false;
    }
    value = args[++index];
    return true;
}

bool parse_point_text(const std::string& text, ir_file_point& point)
{
    const std::vector<std::string> parts = split_string(text, ',');
    if (parts.size() != 2)
    {
        return false;
    }

    const std::optional<int> x = parse_int_value(parts[0]);
    const std::optional<int> y = parse_int_value(parts[1]);
    if (!x.has_value() || !y.has_value())
    {
        return false;
    }
    if (*x < std::numeric_limits<SHORT>::min() || *x > std::numeric_limits<SHORT>::max()
        || *y < std::numeric_limits<SHORT>::min() || *y > std::numeric_limits<SHORT>::max())
    {
        return false;
    }

    point.x = static_cast<SHORT>(*x);
    point.y = static_cast<SHORT>(*y);
    return true;
}

bool parse_points_text(const std::string& text, std::vector<ir_file_point>& points)
{
    const std::vector<std::string> items = split_string(text, ';');
    points.clear();
    for (const std::string& item : items)
    {
        if (item.empty())
        {
            continue;
        }
        ir_file_point point{};
        if (!parse_point_text(item, point))
        {
            return false;
        }
        points.push_back(point);
    }
    return !points.empty();
}

bool parse_rect_text(const std::string& text, std::vector<ir_file_point>& points)
{
    const std::vector<std::string> parts = split_string(text, ',');
    if (parts.size() != 4)
    {
        return false;
    }

    const std::optional<int> x = parse_int_value(parts[0]);
    const std::optional<int> y = parse_int_value(parts[1]);
    const std::optional<int> width = parse_int_value(parts[2]);
    const std::optional<int> height = parse_int_value(parts[3]);
    if (!x.has_value() || !y.has_value() || !width.has_value() || !height.has_value())
    {
        return false;
    }
    if (*width < 0 || *height < 0)
    {
        return false;
    }

    points = {
        ir_file_point{static_cast<SHORT>(*x), static_cast<SHORT>(*y)},
        ir_file_point{static_cast<SHORT>(*x + *width), static_cast<SHORT>(*y + *height)},
    };
    return true;
}

std::optional<ir_file_enum_temp_value_type> parse_temp_value_type(const std::string& text)
{
    if (text == "max")
    {
        return IR_FILE_MAX;
    }
    if (text == "min")
    {
        return IR_FILE_MIN;
    }
    if (text == "avg")
    {
        return IR_FILE_AVG;
    }
    return std::nullopt;
}

std::optional<ir_file_enum_marker_type> parse_marker_type(const std::string& text)
{
    if (text == "spot")
    {
        return IR_FILE_SPOT;
    }
    if (text == "rectangle" || text == "rect")
    {
        return IR_FILE_RECTANGLE;
    }
    if (text == "ellipse")
    {
        return IR_FILE_ELLIPSE;
    }
    if (text == "polyline" || text == "line")
    {
        return IR_FILE_POLYLINE;
    }
    if (text == "polygon")
    {
        return IR_FILE_POLYGON;
    }
    return std::nullopt;
}

std::optional<ir_file_enum_rotate_type> parse_rotation_type(const std::string& text)
{
    if (text == "90")
    {
        return IR_FILE_ROTATE_90;
    }
    if (text == "180")
    {
        return IR_FILE_ROTATE_180;
    }
    if (text == "270")
    {
        return IR_FILE_ROTATE_270;
    }
    return std::nullopt;
}

std::optional<ir_file_enum_mirror_type> parse_mirror_type(const std::string& text)
{
    if (text == "horizontal")
    {
        return IR_FILE_MIRROR_HORIZONTAL;
    }
    if (text == "vertical")
    {
        return IR_FILE_MIRROR_VERTICAL;
    }
    return std::nullopt;
}

bool open_input_handle(const ThermalDemoOptions& options, HandleCloser& handle_guard, std::ostream& out, std::ostream& err, int section_index = 1)
{
    print_section(out, section_index, "Open and identify file");
    ir_file_enum_file_type peek_type = IR_FILE_OTHER;
    INT32 result = ir_file_peek_file_type(options.input_path.c_str(), &peek_type);
    print_call(out, "ir_file_peek_file_type", result, options.input_path + " -> " + file_type_name(peek_type));
    if (result != IR_FILE_EC_OK)
    {
        err << "Failed to identify input file: " << options.input_path << "\n";
        return false;
    }

    result = ir_file_open(options.input_path.c_str(), &handle_guard.handle);
    print_call(out, "ir_file_open", result, options.input_path);
    if (result != IR_FILE_EC_OK || handle_guard.handle == nullptr)
    {
        err << "Failed to open input file: " << options.input_path << "\n";
        return false;
    }

    return true;
}

bool read_marker_names(HANDLE handle, std::vector<std::string>& names, INT32& result)
{
    ir_file_marker_names marker_names{};
    result = ir_file_get_marker_names(handle, &marker_names);
    if (result != IR_FILE_EC_OK)
    {
        names.clear();
        return false;
    }

    std::vector<BYTE> raw(static_cast<size_t>(marker_names.count) * kNameLength, 0);
    if (marker_names.count != 0)
    {
        marker_names.names = reinterpret_cast<ir_file_marker_name*>(raw.data());
        result = ir_file_get_marker_names(handle, &marker_names);
        if (result != IR_FILE_EC_OK)
        {
            names.clear();
            return false;
        }
    }

    names.clear();
    names.reserve(marker_names.count);
    for (UINT32 i = 0; i < marker_names.count; ++i)
    {
        const BYTE* ptr = raw.data() + static_cast<size_t>(i) * kNameLength;
        names.push_back(fixed_bytes_to_string(ptr, kNameLength));
    }
    return true;
}

bool read_delta_names(HANDLE handle, std::vector<std::string>& names, INT32& result)
{
    ir_file_delta_names delta_names{};
    result = ir_file_get_delta_names(handle, &delta_names);
    if (result != IR_FILE_EC_OK)
    {
        names.clear();
        return false;
    }

    std::vector<BYTE> raw(static_cast<size_t>(delta_names.count) * kNameLength, 0);
    if (delta_names.count != 0)
    {
        delta_names.names = reinterpret_cast<ir_file_delta_name*>(raw.data());
        result = ir_file_get_delta_names(handle, &delta_names);
        if (result != IR_FILE_EC_OK)
        {
            names.clear();
            return false;
        }
    }

    names.clear();
    names.reserve(delta_names.count);
    for (UINT32 i = 0; i < delta_names.count; ++i)
    {
        const BYTE* ptr = raw.data() + static_cast<size_t>(i) * kNameLength;
        names.push_back(fixed_bytes_to_string(ptr, kNameLength));
    }
    return true;
}

template <typename FetchMarkerFn>
bool fetch_marker_common(FetchMarkerFn fetch, const std::string& name, MarkerData& marker, INT32& result)
{
    ir_file_marker raw{};
    std::memset(raw.name, 0, sizeof(raw.name));
    const size_t copy_length = std::min(name.size(), sizeof(raw.name));
    std::memcpy(raw.name, name.data(), copy_length);

    result = fetch(&raw);
    if (result != IR_FILE_EC_OK)
    {
        return false;
    }

    std::vector<ir_file_point> points(raw.pointCount);
    std::vector<BYTE> ex_buffer(raw.exInfo.length, 0);
    raw.points = points.empty() ? nullptr : points.data();
    raw.exInfo.buffer = ex_buffer.empty() ? nullptr : ex_buffer.data();

    result = fetch(&raw);
    if (result != IR_FILE_EC_OK)
    {
        return false;
    }

    marker.type = raw.type;
    marker.name = fixed_bytes_to_string(raw.name, sizeof(raw.name));
    marker.local_params = raw.localParams;
    marker.refl_temp = raw.reflTemp;
    marker.distance = raw.distance;
    marker.emissivity = raw.emissivity;
    marker.points = points;
    marker.temp_values = {raw.tempValues[0], raw.tempValues[1], raw.tempValues[2]};
    marker.temp_spots = {raw.tempSpots[0], raw.tempSpots[1]};
    marker.measured_value = raw.measuredValue;
    marker.ex_info = bytes_to_string(ex_buffer.data(), ex_buffer.size());
    return true;
}

bool fetch_marker_by_name(HANDLE handle, const std::string& name, MarkerData& marker, INT32& result)
{
    return fetch_marker_common(
        [handle](ir_file_marker* raw) { return ir_file_get_marker(handle, raw); },
        name,
        marker,
        result);
}

bool fetch_marker_by_index(HANDLE handle, UINT32 index, const std::string& name, MarkerData& marker, INT32& result)
{
    return fetch_marker_common(
        [handle, index](ir_file_marker* raw) { return ir_file_get_index_marker(handle, index, raw); },
        name,
        marker,
        result);
}

template <typename FetchDeltaFn>
bool fetch_delta_common(FetchDeltaFn fetch, const std::string& name, DeltaData& delta, INT32& result)
{
    ir_file_delta raw{};
    std::memset(raw.name, 0, sizeof(raw.name));
    const size_t copy_length = std::min(name.size(), sizeof(raw.name));
    std::memcpy(raw.name, name.data(), copy_length);
    result = fetch(&raw);
    if (result != IR_FILE_EC_OK)
    {
        return false;
    }

    delta.name = fixed_bytes_to_string(raw.name, sizeof(raw.name));
    delta.src_name = fixed_bytes_to_string(raw.srcName, sizeof(raw.srcName));
    delta.src_type = raw.srcValueType;
    delta.src_temp = raw.srcTemp;
    delta.dest_name = fixed_bytes_to_string(raw.destName, sizeof(raw.destName));
    delta.dest_type = raw.destValueType;
    delta.dest_temp = raw.destTemp;
    delta.value = raw.value;
    return true;
}

bool fetch_delta_by_name(HANDLE handle, const std::string& name, DeltaData& delta, INT32& result)
{
    return fetch_delta_common(
        [handle](ir_file_delta* raw) { return ir_file_get_delta(handle, raw); },
        name,
        delta,
        result);
}

bool fetch_delta_by_index(HANDLE handle, UINT32 index, const std::string& name, DeltaData& delta, INT32& result)
{
    return fetch_delta_common(
        [handle, index](ir_file_delta* raw) { return ir_file_get_index_delta(handle, index, raw); },
        name,
        delta,
        result);
}

INT32 set_marker(HANDLE handle, MarkerData& marker)
{
    ir_file_marker raw{};
    raw.type = marker.type;
    std::memset(raw.name, 0, sizeof(raw.name));
    const size_t copy_length = std::min(marker.name.size(), sizeof(raw.name));
    std::memcpy(raw.name, marker.name.data(), copy_length);
    raw.localParams = marker.local_params;
    raw.reflTemp = marker.refl_temp;
    raw.distance = marker.distance;
    raw.emissivity = marker.emissivity;
    raw.pointCount = static_cast<UINT32>(marker.points.size());
    raw.points = marker.points.empty() ? nullptr : marker.points.data();

    const INT32 result = ir_file_set_marker(handle, &raw);
    if (result == IR_FILE_EC_OK)
    {
        marker.temp_values = {raw.tempValues[0], raw.tempValues[1], raw.tempValues[2]};
        marker.temp_spots = {raw.tempSpots[0], raw.tempSpots[1]};
        marker.measured_value = raw.measuredValue;
    }
    return result;
}

INT32 set_delta(HANDLE handle, DeltaData& delta)
{
    ir_file_delta raw{};
    std::memset(raw.name, 0, sizeof(raw.name));
    std::memset(raw.srcName, 0, sizeof(raw.srcName));
    std::memset(raw.destName, 0, sizeof(raw.destName));
    std::memcpy(raw.name, delta.name.data(), std::min(delta.name.size(), sizeof(raw.name)));
    std::memcpy(raw.srcName, delta.src_name.data(), std::min(delta.src_name.size(), sizeof(raw.srcName)));
    std::memcpy(raw.destName, delta.dest_name.data(), std::min(delta.dest_name.size(), sizeof(raw.destName)));
    raw.srcValueType = delta.src_type;
    raw.srcTemp = delta.src_temp;
    raw.destValueType = delta.dest_type;
    raw.destTemp = delta.dest_temp;

    const INT32 result = ir_file_set_delta(handle, &raw);
    if (result == IR_FILE_EC_OK)
    {
        delta.value = raw.value;
    }
    return result;
}

std::string summarize_points(const std::vector<ir_file_point>& points)
{
    if (points.empty())
    {
        return "(none)";
    }

    std::ostringstream oss;
    const size_t count = std::min<size_t>(points.size(), 5);
    for (size_t i = 0; i < count; ++i)
    {
        if (i != 0)
        {
            oss << ", ";
        }
        oss << format_point(points[i]);
    }
    if (points.size() > count)
    {
        oss << ", ...";
    }
    return oss.str();
}

std::string summarize_names(const std::vector<std::string>& names)
{
    if (names.empty())
    {
        return "(none)";
    }

    std::ostringstream oss;
    const size_t count = std::min<size_t>(names.size(), 6);
    for (size_t i = 0; i < count; ++i)
    {
        if (i != 0)
        {
            oss << ", ";
        }
        oss << '"' << names[i] << '"';
    }
    if (names.size() > count)
    {
        oss << ", ...";
    }
    return oss.str();
}

std::string marker_summary(const MarkerData& marker)
{
    std::ostringstream oss;
    oss << "type=" << marker_type_name(marker.type)
        << ", name=\"" << marker.name << "\""
        << ", localParams=" << yes_no(marker.local_params)
        << ", points=" << summarize_points(marker.points)
        << ", max=" << format_temperature(marker.temp_values[0])
        << ", min=" << format_temperature(marker.temp_values[1])
        << ", avg=" << format_temperature(marker.temp_values[2])
        << ", measured=" << marker.measured_value;
    return oss.str();
}

std::string delta_summary(const DeltaData& delta)
{
    std::ostringstream oss;
    oss << "name=\"" << delta.name << "\""
        << ", src=\"" << delta.src_name << "\"/" << temp_value_type_name(delta.src_type)
        << ", dest=\"" << delta.dest_name << "\"/" << temp_value_type_name(delta.dest_type)
        << ", value=" << std::fixed << std::setprecision(4) << delta.value;
    return oss.str();
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

std::string summarize_presets(const std::vector<ir_file_preset_palette>& palettes)
{
    if (palettes.empty())
    {
        return "(none)";
    }

    std::ostringstream oss;
    const size_t count = std::min<size_t>(palettes.size(), 5);
    for (size_t i = 0; i < count; ++i)
    {
        if (i != 0)
        {
            oss << ", ";
        }
        oss << static_cast<int>(palettes[i].refNo) << ":" << fixed_bytes_to_string(palettes[i].name, kNameLength);
    }
    if (palettes.size() > count)
    {
        oss << ", ...";
    }
    return oss.str();
}

bool read_frame(HANDLE handle, UINT32 index, int width, int height, FrameData& frame, INT32& result)
{
    ir_file_frame raw{};
    result = ir_file_get_frame(handle, index, &raw);
    if (result != IR_FILE_EC_OK)
    {
        frame = FrameData{};
        return false;
    }
    if (raw.length != static_cast<UINT32>(width * height))
    {
        result = IR_FILE_EC_FAIL;
        frame = FrameData{};
        return false;
    }

    frame.timestamp = raw.timeStamp;
    frame.width = width;
    frame.height = height;
    frame.samples.assign(raw.length, 0);
    raw.buffer = frame.samples.data();
    result = ir_file_get_frame(handle, index, &raw);
    if (result != IR_FILE_EC_OK)
    {
        frame = FrameData{};
        return false;
    }
    return true;
}

bool read_calculated_lut(HANDLE handle, std::vector<USHORT>& ads, std::vector<float>& temps, BYTE& rad_method, INT32& result)
{
    ir_file_lut lut{};
    result = ir_file_get_calculated_lut(handle, &lut);
    if (result != IR_FILE_EC_OK)
    {
        ads.clear();
        temps.clear();
        return false;
    }

    ads.assign(lut.count, 0);
    temps.assign(lut.count, 0.0f);
    lut.adArray = ads.empty() ? nullptr : ads.data();
    lut.tempArray = temps.empty() ? nullptr : temps.data();
    result = ir_file_get_calculated_lut(handle, &lut);
    if (result != IR_FILE_EC_OK)
    {
        ads.clear();
        temps.clear();
        return false;
    }

    rad_method = lut.radMethod;
    return true;
}

bool maybe_select_frame_index(HANDLE handle, int requested_index, UINT32& selected_index, std::ostream& out, bool print_current_first = true)
{
    INT32 result = ir_file_get_frame_index(handle, &selected_index);
    if (print_current_first)
    {
        print_call(out, "ir_file_get_frame_index", result, std::to_string(selected_index));
    }
    if (result != IR_FILE_EC_OK)
    {
        return false;
    }

    if (requested_index >= 0)
    {
        result = ir_file_set_frame_index(handle, static_cast<UINT32>(requested_index));
        print_call(out, "ir_file_set_frame_index(user)", result, std::to_string(requested_index));
        if (result != IR_FILE_EC_OK)
        {
            return false;
        }

        result = ir_file_get_frame_index(handle, &selected_index);
        print_call(out, "ir_file_get_frame_index(after user set)", result, std::to_string(selected_index));
        if (result != IR_FILE_EC_OK)
        {
            return false;
        }
    }

    return true;
}

std::string next_prefixed_name(const std::vector<std::string>& existing_names, const std::string& prefix)
{
    std::set<int> used_indexes;
    for (const std::string& name : existing_names)
    {
        if (name.rfind(prefix, 0) != 0)
        {
            continue;
        }
        const std::string suffix = name.substr(prefix.size());
        const std::optional<int> value = parse_int_value(suffix);
        if (value.has_value())
        {
            used_indexes.insert(*value);
        }
    }

    int next = 1;
    while (used_indexes.find(next) != used_indexes.end())
    {
        ++next;
    }
    return prefix + std::to_string(next);
}

bool maybe_persist(HANDLE handle, bool persist, std::ostream& out)
{
    if (!persist)
    {
        print_note(out, "mutation kept in-memory only; pass --persist to append ir_file_save");
        return true;
    }

    const INT32 result = ir_file_save(handle);
    print_call(out, "ir_file_save", result, "persist source file");
    return result == IR_FILE_EC_OK;
}

std::vector<BYTE> acquire_cover_bytes(HANDLE handle, const std::string& cover_file, std::string& source_label, std::ostream& out)
{
    std::vector<BYTE> cover_bytes;
    if (!cover_file.empty())
    {
        if (read_binary_file(cover_file, cover_bytes))
        {
            source_label = "cover-file " + cover_file;
            print_note(out, "cover image source mapped from --cover-file");
            return cover_bytes;
        }
        source_label = "failed to read cover-file";
        return {};
    }

    INT32 result = IR_FILE_EC_FAIL;
    if (read_handle_buffer(handle, ir_file_get_cover_image, cover_bytes, result) && !cover_bytes.empty())
    {
        source_label = "existing cover image";
        print_call(out, "ir_file_get_cover_image", result, std::to_string(cover_bytes.size()) + " bytes");
        return cover_bytes;
    }

    cover_bytes.clear();
    if (read_handle_buffer(handle, ir_file_get_visual_image, cover_bytes, result) && !cover_bytes.empty())
    {
        source_label = "visual image fallback";
        print_call(out, "ir_file_get_visual_image", result, std::to_string(cover_bytes.size()) + " bytes");
        return cover_bytes;
    }

    source_label = "none";
    return {};
}

std::optional<ir_file_rectangle> marker_to_rect(const MarkerData& marker)
{
    if (marker.points.size() != 2)
    {
        return std::nullopt;
    }

    const int left = std::min(marker.points[0].x, marker.points[1].x);
    const int top = std::min(marker.points[0].y, marker.points[1].y);
    const int right = std::max(marker.points[0].x, marker.points[1].x);
    const int bottom = std::max(marker.points[0].y, marker.points[1].y);
    return ir_file_rectangle{
        static_cast<SHORT>(left),
        static_cast<SHORT>(top),
        static_cast<SHORT>(right - left + 1),
        static_cast<SHORT>(bottom - top + 1),
    };
}

bool query_temp_rect_like(
    HANDLE handle,
    const ir_file_rectangle& rect,
    bool ellipse,
    std::vector<ir_file_point_value>& values,
    INT32& result)
{
    ir_file_temp_rect temp_rect{};
    temp_rect.rect = rect;
    temp_rect.localParams = 0;
    temp_rect.reflTemp = 293.15f;
    temp_rect.distance = 1.0f;
    temp_rect.emissivity = 1.0f;

    result = ellipse ? ir_file_get_temp_ellipse(handle, &temp_rect) : ir_file_get_temp_rect(handle, &temp_rect);
    if (result != IR_FILE_EC_OK)
    {
        values.clear();
        return false;
    }

    values.assign(temp_rect.tempCount, ir_file_point_value{});
    temp_rect.temps = values.empty() ? nullptr : values.data();
    result = ellipse ? ir_file_get_temp_ellipse(handle, &temp_rect) : ir_file_get_temp_rect(handle, &temp_rect);
    if (result != IR_FILE_EC_OK)
    {
        values.clear();
        return false;
    }
    return true;
}

bool query_temp_points_like(
    HANDLE handle,
    const std::vector<ir_file_point>& points,
    bool polygon,
    std::vector<ir_file_point_value>& values,
    INT32& result)
{
    ir_file_temp_points temp_points{};
    temp_points.pointCount = static_cast<UINT32>(points.size());
    temp_points.points = const_cast<ir_file_point*>(points.data());
    temp_points.localParams = 0;
    temp_points.reflTemp = 293.15f;
    temp_points.distance = 1.0f;
    temp_points.emissivity = 1.0f;

    result = polygon ? ir_file_get_temp_polygon(handle, &temp_points) : ir_file_get_temp_polyline(handle, &temp_points);
    if (result != IR_FILE_EC_OK)
    {
        values.clear();
        return false;
    }

    values.assign(temp_points.tempCount, ir_file_point_value{});
    temp_points.temps = values.empty() ? nullptr : values.data();
    result = polygon ? ir_file_get_temp_polygon(handle, &temp_points) : ir_file_get_temp_polyline(handle, &temp_points);
    if (result != IR_FILE_EC_OK)
    {
        values.clear();
        return false;
    }
    return true;
}

std::string sparse_values_to_csv(const std::vector<ir_file_point_value>& input_values, int left, int top, bool sort_points, bool subtract_kelvin_offset)
{
    std::vector<ir_file_point_value> values = input_values;
    if (sort_points)
    {
        std::sort(values.begin(), values.end(), [](const ir_file_point_value& a, const ir_file_point_value& b) {
            if (a.point.y != b.point.y)
            {
                return a.point.y < b.point.y;
            }
            return a.point.x < b.point.x;
        });
    }

    std::ostringstream oss;
    int last_x = 0;
    int last_y = 0;
    bool first_value = true;
    for (const ir_file_point_value& value : values)
    {
        const int x = value.point.x - left;
        const int y = value.point.y - top;

        if (!first_value)
        {
            for (int j = 0; j < y - last_y; ++j)
            {
                oss << '\n';
                last_x = 0;
            }
        }

        for (int j = 0; j < x - last_x; ++j)
        {
            oss << ",";
        }

        const float temp = subtract_kelvin_offset ? (value.temp - kKelvinOffset) : value.temp;
        oss << std::fixed << std::setprecision(4) << temp;
        last_x = x;
        last_y = y;
        first_value = false;
    }

    return oss.str();
}

int handle_open_read_command(const ThermalDemoOptions& options, std::ostream& out, std::ostream& err)
{
    int requested_frame_index = -1;
    std::string output_path = "thermal_preview.ppm";
    bool export_preview = true;
    for (size_t i = 0; i < options.command_args.size(); ++i)
    {
        const std::string& arg = options.command_args[i];
        if (arg == "--frame-index")
        {
            std::string value;
            if (!expect_value(options.command_args, i, "--frame-index", value, err))
            {
                return 1;
            }
            requested_frame_index = std::atoi(value.c_str());
        }
        else if (arg == "--output")
        {
            if (!expect_value(options.command_args, i, "--output", output_path, err))
            {
                return 1;
            }
        }
        else if (arg == "--no-export")
        {
            export_preview = false;
        }
        else
        {
            err << "Unknown open-read option: " << arg << "\n";
            return 1;
        }
    }

    UINT32 sdk_version = 0;
    const INT32 version_result = ir_file_get_sdk_version(&sdk_version, nullptr);
    if (version_result == IR_FILE_EC_OK)
    {
        out << "IRtek Thermal CLI demo\n";
        out << "SDK version: " << decode_version(sdk_version) << "\n";
    }

    HandleCloser handle_guard;
    if (!open_input_handle(options, handle_guard, out, err, 1))
    {
        return 1;
    }

    print_section(out, 2, "Sync Windows open state");
    std::vector<std::string> marker_names;
    INT32 result = IR_FILE_EC_FAIL;
    if (read_marker_names(handle_guard.handle, marker_names, result))
    {
        print_call(out, "ir_file_get_marker_names", result, summarize_names(marker_names));
        for (const std::string& name : marker_names)
        {
            if (name.empty())
            {
                continue;
            }
            MarkerData marker;
            const INT32 marker_result = fetch_marker_by_name(handle_guard.handle, name, marker, result) ? result : result;
            print_call(out, "ir_file_get_marker(" + name + ")", marker_result, marker_result == IR_FILE_EC_OK ? marker_summary(marker) : "");
        }
    }
    else
    {
        print_call(out, "ir_file_get_marker_names", result, "");
    }

    std::vector<std::string> delta_names;
    if (read_delta_names(handle_guard.handle, delta_names, result))
    {
        print_call(out, "ir_file_get_delta_names", result, summarize_names(delta_names));
        for (const std::string& name : delta_names)
        {
            if (name.empty())
            {
                continue;
            }
            DeltaData delta;
            const INT32 delta_result = fetch_delta_by_name(handle_guard.handle, name, delta, result) ? result : result;
            print_call(out, "ir_file_get_delta(" + name + ")", delta_result, delta_result == IR_FILE_EC_OK ? delta_summary(delta) : "");
        }
    }
    else
    {
        print_call(out, "ir_file_get_delta_names", result, "");
    }

    ir_file_reference reference{};
    result = ir_file_get_reference(handle_guard.handle, &reference);
    if (result == IR_FILE_EC_OK)
    {
        std::ostringstream oss;
        oss << "enabled=" << yes_no(reference.enabled)
            << ", name=\"" << fixed_bytes_to_string(reference.name, sizeof(reference.name)) << "\""
            << ", valueType=" << temp_value_type_name(reference.valueType)
            << ", temp=" << format_temperature(reference.temp);
        print_call(out, "ir_file_get_reference", result, oss.str());
    }
    else
    {
        print_call(out, "ir_file_get_reference", result, "");
    }

    print_section(out, 3, "Read metadata and properties");
    std::vector<BYTE> raw_buffer;
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

    UINT32 selected_frame_index = 0;
    if (!maybe_select_frame_index(handle_guard.handle, requested_frame_index, selected_frame_index, out))
    {
        err << "Failed to query or set frame index.\n";
        return 1;
    }

    ir_file_temp_range temp_range{};
    result = ir_file_get_temp_range(handle_guard.handle, &temp_range);
    print_call(out, "ir_file_get_temp_range", result,
        result == IR_FILE_EC_OK ? (format_temperature(temp_range.min) + " .. " + format_temperature(temp_range.max)) : "");

    ir_file_size thermal_size{};
    result = ir_file_get_thermal_size(handle_guard.handle, &thermal_size);
    print_call(out, "ir_file_get_thermal_size", result,
        std::to_string(thermal_size.width) + "x" + std::to_string(thermal_size.height));
    if (result != IR_FILE_EC_OK)
    {
        err << "ir_file_get_thermal_size failed; cannot continue open-read.\n";
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
            << ", focalLength=" << lens_info.focalLength
            << ", hfov=" << lens_info.hfov
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

    ir_file_enum_image_enh_type image_enh = IR_FILE_NONE;
    result = ir_file_get_image_enh(handle_guard.handle, &image_enh);
    print_call(out, "ir_file_get_image_enh", result, image_enh_name(image_enh));

    UINT64 shot_time = 0;
    result = ir_file_get_shot_time(handle_guard.handle, &shot_time);
    print_call(out, "ir_file_get_shot_time", result, format_timestamp(shot_time));

    UINT64 modified_time = 0;
    result = ir_file_get_modified_time(handle_guard.handle, &modified_time);
    print_call(out, "ir_file_get_modified_time", result, format_timestamp(modified_time));

    print_section(out, 4, "Read media and current frame");
    raw_buffer.clear();
    if (read_handle_buffer(handle_guard.handle, ir_file_get_cover_image, raw_buffer, result))
    {
        print_call(out, "ir_file_get_cover_image", result, std::to_string(raw_buffer.size()) + " bytes");
    }
    else
    {
        print_call(out, "ir_file_get_cover_image", result, "");
    }

    raw_buffer.clear();
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
    if (read_audio(handle_guard.handle, audio, audio_bytes, result))
    {
        std::ostringstream oss;
        oss << "type=" << (audio.type == IR_FILE_WAV ? "wav" : "mp3")
            << ", bytes=" << audio_bytes.size();
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
    if (result == IR_FILE_EC_OK)
    {
        std::ostringstream oss;
        oss << "refNo=" << static_cast<int>(palette_v2.refNo)
            << ", inverted=" << yes_no(palette_v2.inverted)
            << ", srcFirst=" << format_argb(palette_v2.src_colors[0])
            << ", destFirst=" << format_argb(palette_v2.dest_colors[0]);
        print_call(out, "ir_file_get_palette_v2", result, oss.str());
    }
    else
    {
        print_call(out, "ir_file_get_palette_v2", result, "");
    }

    FrameData frame;
    if (!read_frame(handle_guard.handle, selected_frame_index, thermal_size.width, thermal_size.height, frame, result))
    {
        print_call(out, "ir_file_get_frame", result, "");
    }
    else
    {
        const auto minmax = std::minmax_element(frame.samples.begin(), frame.samples.end());
        std::ostringstream oss;
        oss << "index=" << selected_frame_index
            << ", time=" << format_timestamp(frame.timestamp)
            << ", samples=" << frame.samples.size()
            << ", minAD=" << *minmax.first
            << ", maxAD=" << *minmax.second;
        print_call(out, "ir_file_get_frame", result, oss.str());
    }

    std::vector<BYTE> thermal_bgr(static_cast<size_t>(thermal_size.width) * thermal_size.height * 3, 0);
    ir_file_image thermal_image{};
    thermal_image.offset = 0;
    thermal_image.data = thermal_bgr.data();
    result = ir_file_get_thermal_image(handle_guard.handle, &thermal_image);
    print_call(out, "ir_file_get_thermal_image", result,
        result == IR_FILE_EC_OK ? (std::to_string(thermal_size.width) + "x" + std::to_string(thermal_size.height) + " BGR image") : "");

    if (result == IR_FILE_EC_OK && export_preview)
    {
        if (!save_ppm_bgr(output_path, thermal_size.width, thermal_size.height, thermal_bgr))
        {
            err << "Failed to write preview image to " << output_path << "\n";
            return 1;
        }
        out << "  - export preview: OK -> " << output_path << "\n";
    }
    else if (!export_preview)
    {
        out << "  - export preview: skipped (--no-export)\n";
    }

    if (!frame.samples.empty())
    {
        std::vector<BYTE> frame_bgr(static_cast<size_t>(thermal_size.width) * thermal_size.height * 3, 0);
        ir_file_frame raw_frame{};
        raw_frame.timeStamp = frame.timestamp;
        raw_frame.length = static_cast<UINT32>(frame.samples.size());
        raw_frame.buffer = frame.samples.data();
        ir_file_image frame_image{};
        frame_image.offset = 0;
        frame_image.data = frame_bgr.data();
        result = ir_file_get_thermal_image_from_frame(handle_guard.handle, &raw_frame, IR_FILE_BGR, &frame_image);
        print_call(out, "ir_file_get_thermal_image_from_frame", result,
            result == IR_FILE_EC_OK ? "selected frame converted to BGR image" : "");
    }

    return 0;
}

int handle_save_like_command(const ThermalDemoOptions& options, std::ostream& out, std::ostream& err, bool save_as)
{
    std::string format = "default";
    std::string output_path;
    std::string cover_file;
    for (size_t i = 0; i < options.command_args.size(); ++i)
    {
        const std::string& arg = options.command_args[i];
        if (arg == "--format")
        {
            if (!expect_value(options.command_args, i, "--format", format, err))
            {
                return 1;
            }
        }
        else if (arg == "--output")
        {
            if (!expect_value(options.command_args, i, "--output", output_path, err))
            {
                return 1;
            }
        }
        else if (arg == "--cover-file")
        {
            if (!expect_value(options.command_args, i, "--cover-file", cover_file, err))
            {
                return 1;
            }
        }
        else
        {
            err << "Unknown " << (save_as ? "save-as" : "save") << " option: " << arg << "\n";
            return 1;
        }
    }

    if (format != "default" && format != "fjpg")
    {
        err << "Unsupported format: " << format << " (expected default or fjpg)\n";
        return 1;
    }

    if (save_as && output_path.empty())
    {
        err << "save-as requires --output <path>\n";
        return 1;
    }

    HandleCloser handle_guard;
    if (!open_input_handle(options, handle_guard, out, err, 1))
    {
        return 1;
    }

    print_section(out, 2, "Refresh cover then save");
    std::string source_label;
    std::vector<BYTE> cover_bytes = acquire_cover_bytes(handle_guard.handle, cover_file, source_label, out);
    if (cover_bytes.empty())
    {
        err << "Unable to obtain a JPEG cover image. Pass --cover-file <jpg> or use a source file with cover/visual JPEG data.\n";
        print_note(out, "Windows Save/SaveAs calls SetCoverImage(BuildImage()) first; Linux CLI needs a JPEG source to map that call.");
        return 1;
    }

    ir_file_buffer cover_buffer{};
    cover_buffer.length = static_cast<UINT32>(cover_bytes.size());
    cover_buffer.buffer = cover_bytes.data();
    INT32 result = ir_file_set_cover_image(handle_guard.handle, &cover_buffer);
    print_call(out, "ir_file_set_cover_image", result, source_label + ", " + std::to_string(cover_bytes.size()) + " bytes");
    if (result != IR_FILE_EC_OK)
    {
        return 1;
    }

    if (save_as)
    {
        result = (format == "fjpg")
            ? ir_file_save_as_fjpg(handle_guard.handle, output_path.c_str())
            : ir_file_save_as(handle_guard.handle, output_path.c_str());
        print_call(out, format == "fjpg" ? "ir_file_save_as_fjpg" : "ir_file_save_as", result, output_path);
    }
    else
    {
        result = (format == "fjpg")
            ? ir_file_save_fjpg(handle_guard.handle)
            : ir_file_save(handle_guard.handle);
        print_call(out, format == "fjpg" ? "ir_file_save_fjpg" : "ir_file_save", result, options.input_path);
    }

    return result == IR_FILE_EC_OK ? 0 : 1;
}

int handle_save_frame_command(const ThermalDemoOptions& options, std::ostream& out, std::ostream& err)
{
    std::string output_path;
    std::string cover_file;
    for (size_t i = 0; i < options.command_args.size(); ++i)
    {
        const std::string& arg = options.command_args[i];
        if (arg == "--output")
        {
            if (!expect_value(options.command_args, i, "--output", output_path, err))
            {
                return 1;
            }
        }
        else if (arg == "--cover-file")
        {
            if (!expect_value(options.command_args, i, "--cover-file", cover_file, err))
            {
                return 1;
            }
        }
        else
        {
            err << "Unknown save-frame option: " << arg << "\n";
            return 1;
        }
    }

    if (output_path.empty())
    {
        err << "save-frame requires --output <path>\n";
        return 1;
    }

    HandleCloser handle_guard;
    if (!open_input_handle(options, handle_guard, out, err, 1))
    {
        return 1;
    }

    print_section(out, 2, "Save current frame");
    std::string source_label;
    std::vector<BYTE> cover_bytes = acquire_cover_bytes(handle_guard.handle, cover_file, source_label, out);
    ir_file_buffer cover_buffer{};
    cover_buffer.length = static_cast<UINT32>(cover_bytes.size());
    cover_buffer.buffer = cover_bytes.empty() ? nullptr : cover_bytes.data();
    INT32 result = ir_file_save_frame_as(handle_guard.handle, output_path.c_str(), cover_bytes.empty() ? nullptr : &cover_buffer);
    if (cover_bytes.empty())
    {
        print_note(out, "save-frame called without a cover buffer; SDK falls back to original cover or a generated black cover.");
        source_label = "nullptr cover buffer";
    }
    print_call(out, "ir_file_save_frame_as", result, output_path + " <- " + source_label);
    return result == IR_FILE_EC_OK ? 0 : 1;
}

int handle_marker_list_command(const ThermalDemoOptions& options, std::ostream& out, std::ostream& err)
{
    HandleCloser handle_guard;
    if (!open_input_handle(options, handle_guard, out, err, 1))
    {
        return 1;
    }

    print_section(out, 2, "List markers");
    std::vector<std::string> names;
    INT32 result = IR_FILE_EC_FAIL;
    if (!read_marker_names(handle_guard.handle, names, result))
    {
        print_call(out, "ir_file_get_marker_names", result, "");
        return 1;
    }

    print_call(out, "ir_file_get_marker_names", result, summarize_names(names));
    for (const std::string& name : names)
    {
        if (name.empty())
        {
            continue;
        }
        MarkerData marker;
        const bool ok = fetch_marker_by_name(handle_guard.handle, name, marker, result);
        print_call(out, "ir_file_get_marker(" + name + ")", result, ok ? marker_summary(marker) : "");
    }
    return 0;
}

int handle_marker_get_command(const ThermalDemoOptions& options, std::ostream& out, std::ostream& err)
{
    std::string name;
    for (size_t i = 0; i < options.command_args.size(); ++i)
    {
        const std::string& arg = options.command_args[i];
        if (arg == "--name")
        {
            if (!expect_value(options.command_args, i, "--name", name, err))
            {
                return 1;
            }
        }
        else
        {
            err << "Unknown marker-get option: " << arg << "\n";
            return 1;
        }
    }

    if (name.empty())
    {
        err << "marker-get requires --name <marker-name>\n";
        return 1;
    }

    HandleCloser handle_guard;
    if (!open_input_handle(options, handle_guard, out, err, 1))
    {
        return 1;
    }

    print_section(out, 2, "Read marker");
    MarkerData marker;
    INT32 result = IR_FILE_EC_FAIL;
    const bool ok = fetch_marker_by_name(handle_guard.handle, name, marker, result);
    print_call(out, "ir_file_get_marker", result, ok ? marker_summary(marker) : name);
    return ok ? 0 : 1;
}

int handle_marker_add_command(const ThermalDemoOptions& options, std::ostream& out, std::ostream& err)
{
    std::string type_text;
    std::string name;
    std::string point_text;
    std::string rect_text;
    std::string points_text;
    std::optional<bool> local_params;
    std::optional<float> refl_temp;
    std::optional<float> distance;
    std::optional<float> emissivity;
    bool persist = false;

    for (size_t i = 0; i < options.command_args.size(); ++i)
    {
        const std::string& arg = options.command_args[i];
        std::string value;
        if (arg == "--type")
        {
            if (!expect_value(options.command_args, i, "--type", type_text, err))
            {
                return 1;
            }
        }
        else if (arg == "--name")
        {
            if (!expect_value(options.command_args, i, "--name", name, err))
            {
                return 1;
            }
        }
        else if (arg == "--point")
        {
            if (!expect_value(options.command_args, i, "--point", point_text, err))
            {
                return 1;
            }
        }
        else if (arg == "--rect")
        {
            if (!expect_value(options.command_args, i, "--rect", rect_text, err))
            {
                return 1;
            }
        }
        else if (arg == "--points")
        {
            if (!expect_value(options.command_args, i, "--points", points_text, err))
            {
                return 1;
            }
        }
        else if (arg == "--local-params")
        {
            if (!expect_value(options.command_args, i, "--local-params", value, err))
            {
                return 1;
            }
            local_params = parse_bool_value(value);
            if (!local_params.has_value())
            {
                err << "Invalid boolean for --local-params: " << value << "\n";
                return 1;
            }
        }
        else if (arg == "--refl-temp")
        {
            if (!expect_value(options.command_args, i, "--refl-temp", value, err))
            {
                return 1;
            }
            refl_temp = parse_float_value(value);
            if (!refl_temp.has_value())
            {
                err << "Invalid float for --refl-temp: " << value << "\n";
                return 1;
            }
        }
        else if (arg == "--distance")
        {
            if (!expect_value(options.command_args, i, "--distance", value, err))
            {
                return 1;
            }
            distance = parse_float_value(value);
            if (!distance.has_value())
            {
                err << "Invalid float for --distance: " << value << "\n";
                return 1;
            }
        }
        else if (arg == "--emissivity")
        {
            if (!expect_value(options.command_args, i, "--emissivity", value, err))
            {
                return 1;
            }
            emissivity = parse_float_value(value);
            if (!emissivity.has_value())
            {
                err << "Invalid float for --emissivity: " << value << "\n";
                return 1;
            }
        }
        else if (arg == "--persist")
        {
            persist = true;
        }
        else
        {
            err << "Unknown marker-add option: " << arg << "\n";
            return 1;
        }
    }

    const std::optional<ir_file_enum_marker_type> type = parse_marker_type(type_text);
    if (!type.has_value())
    {
        err << "marker-add requires --type spot|rectangle|ellipse|polyline|polygon\n";
        return 1;
    }

    MarkerData marker;
    marker.type = *type;
    marker.local_params = local_params.value_or(false) ? 1 : 0;
    if (refl_temp.has_value())
    {
        marker.refl_temp = *refl_temp;
    }
    if (distance.has_value())
    {
        marker.distance = *distance;
    }
    if (emissivity.has_value())
    {
        marker.emissivity = *emissivity;
    }

    bool shape_ok = false;
    if (*type == IR_FILE_SPOT)
    {
        ir_file_point point{};
        shape_ok = parse_point_text(point_text, point);
        if (shape_ok)
        {
            marker.points = {point};
        }
    }
    else if (*type == IR_FILE_RECTANGLE || *type == IR_FILE_ELLIPSE)
    {
        shape_ok = parse_rect_text(rect_text, marker.points);
    }
    else
    {
        shape_ok = parse_points_text(points_text, marker.points);
    }

    if (!shape_ok)
    {
        err << "Invalid geometry for marker-add\n";
        return 1;
    }

    HandleCloser handle_guard;
    if (!open_input_handle(options, handle_guard, out, err, 1))
    {
        return 1;
    }

    print_section(out, 2, "Add or update marker");
    if (name.empty())
    {
        std::vector<std::string> existing_names;
        INT32 result = IR_FILE_EC_FAIL;
        if (!read_marker_names(handle_guard.handle, existing_names, result))
        {
            print_call(out, "ir_file_get_marker_names", result, "");
            return 1;
        }

        const char* prefix = (*type == IR_FILE_SPOT) ? kSpotPrefix
            : ((*type == IR_FILE_POLYLINE) ? kPolylinePrefix : kAreaPrefix);
        name = next_prefixed_name(existing_names, prefix);
        print_call(out, "name mapping", IR_FILE_EC_OK, "Windows auto-name -> " + name);
    }
    marker.name = name;

    INT32 result = set_marker(handle_guard.handle, marker);
    print_call(out, "ir_file_set_marker", result, marker_summary(marker));
    if (result != IR_FILE_EC_OK)
    {
        return 1;
    }

    MarkerData verify;
    const bool verify_ok = fetch_marker_by_name(handle_guard.handle, marker.name, verify, result);
    print_call(out, "ir_file_get_marker(verify)", result, verify_ok ? marker_summary(verify) : "");
    if (!verify_ok)
    {
        return 1;
    }

    return maybe_persist(handle_guard.handle, persist, out) ? 0 : 1;
}

int handle_marker_rename_command(const ThermalDemoOptions& options, std::ostream& out, std::ostream& err)
{
    std::string name;
    std::string new_name;
    bool persist = false;
    for (size_t i = 0; i < options.command_args.size(); ++i)
    {
        const std::string& arg = options.command_args[i];
        if (arg == "--name")
        {
            if (!expect_value(options.command_args, i, "--name", name, err))
            {
                return 1;
            }
        }
        else if (arg == "--to")
        {
            if (!expect_value(options.command_args, i, "--to", new_name, err))
            {
                return 1;
            }
        }
        else if (arg == "--persist")
        {
            persist = true;
        }
        else
        {
            err << "Unknown marker-rename option: " << arg << "\n";
            return 1;
        }
    }

    if (name.empty() || new_name.empty())
    {
        err << "marker-rename requires --name <old> --to <new>\n";
        return 1;
    }

    HandleCloser handle_guard;
    if (!open_input_handle(options, handle_guard, out, err, 1))
    {
        return 1;
    }

    print_section(out, 2, "Rename marker");
    ir_file_marker_name from{};
    ir_file_marker_name to{};
    std::memcpy(from, name.data(), std::min(name.size(), sizeof(from)));
    std::memcpy(to, new_name.data(), std::min(new_name.size(), sizeof(to)));
    const INT32 result = ir_file_rename_marker(handle_guard.handle, from, to);
    print_call(out, "ir_file_rename_marker", result, name + " -> " + new_name);
    if (result != IR_FILE_EC_OK)
    {
        return 1;
    }

    MarkerData verify;
    INT32 verify_result = IR_FILE_EC_FAIL;
    const bool ok = fetch_marker_by_name(handle_guard.handle, new_name, verify, verify_result);
    print_call(out, "ir_file_get_marker(verify)", verify_result, ok ? marker_summary(verify) : "");
    if (!ok)
    {
        return 1;
    }

    return maybe_persist(handle_guard.handle, persist, out) ? 0 : 1;
}

int handle_marker_local_params_command(const ThermalDemoOptions& options, std::ostream& out, std::ostream& err)
{
    std::string name;
    std::optional<bool> enabled;
    std::optional<float> refl_temp;
    std::optional<float> distance;
    std::optional<float> emissivity;
    bool persist = false;

    for (size_t i = 0; i < options.command_args.size(); ++i)
    {
        const std::string& arg = options.command_args[i];
        std::string value;
        if (arg == "--name")
        {
            if (!expect_value(options.command_args, i, "--name", name, err))
            {
                return 1;
            }
        }
        else if (arg == "--enabled")
        {
            if (!expect_value(options.command_args, i, "--enabled", value, err))
            {
                return 1;
            }
            enabled = parse_bool_value(value);
            if (!enabled.has_value())
            {
                err << "Invalid boolean for --enabled: " << value << "\n";
                return 1;
            }
        }
        else if (arg == "--refl-temp")
        {
            if (!expect_value(options.command_args, i, "--refl-temp", value, err))
            {
                return 1;
            }
            refl_temp = parse_float_value(value);
            if (!refl_temp.has_value())
            {
                err << "Invalid float for --refl-temp: " << value << "\n";
                return 1;
            }
        }
        else if (arg == "--distance")
        {
            if (!expect_value(options.command_args, i, "--distance", value, err))
            {
                return 1;
            }
            distance = parse_float_value(value);
            if (!distance.has_value())
            {
                err << "Invalid float for --distance: " << value << "\n";
                return 1;
            }
        }
        else if (arg == "--emissivity")
        {
            if (!expect_value(options.command_args, i, "--emissivity", value, err))
            {
                return 1;
            }
            emissivity = parse_float_value(value);
            if (!emissivity.has_value())
            {
                err << "Invalid float for --emissivity: " << value << "\n";
                return 1;
            }
        }
        else if (arg == "--persist")
        {
            persist = true;
        }
        else
        {
            err << "Unknown marker-local-params option: " << arg << "\n";
            return 1;
        }
    }

    if (name.empty())
    {
        err << "marker-local-params requires --name <marker-name>\n";
        return 1;
    }

    HandleCloser handle_guard;
    if (!open_input_handle(options, handle_guard, out, err, 1))
    {
        return 1;
    }

    print_section(out, 2, "Toggle or update local params");
    MarkerData marker;
    INT32 result = IR_FILE_EC_FAIL;
    if (!fetch_marker_by_name(handle_guard.handle, name, marker, result))
    {
        print_call(out, "ir_file_get_marker", result, name);
        return 1;
    }
    print_call(out, "ir_file_get_marker", result, marker_summary(marker));

    marker.local_params = enabled.has_value() ? (*enabled ? 1 : 0) : (marker.local_params == 0 ? 1 : 0);
    if (refl_temp.has_value())
    {
        marker.refl_temp = *refl_temp;
    }
    if (distance.has_value())
    {
        marker.distance = *distance;
    }
    if (emissivity.has_value())
    {
        marker.emissivity = *emissivity;
    }

    result = set_marker(handle_guard.handle, marker);
    print_call(out, "ir_file_set_marker", result, marker_summary(marker));
    if (result != IR_FILE_EC_OK)
    {
        return 1;
    }

    return maybe_persist(handle_guard.handle, persist, out) ? 0 : 1;
}

int handle_marker_remove_command(const ThermalDemoOptions& options, std::ostream& out, std::ostream& err)
{
    std::string name;
    bool persist = false;
    for (size_t i = 0; i < options.command_args.size(); ++i)
    {
        const std::string& arg = options.command_args[i];
        if (arg == "--name")
        {
            if (!expect_value(options.command_args, i, "--name", name, err))
            {
                return 1;
            }
        }
        else if (arg == "--persist")
        {
            persist = true;
        }
        else
        {
            err << "Unknown marker-remove option: " << arg << "\n";
            return 1;
        }
    }

    if (name.empty())
    {
        err << "marker-remove requires --name <marker-name>\n";
        return 1;
    }

    HandleCloser handle_guard;
    if (!open_input_handle(options, handle_guard, out, err, 1))
    {
        return 1;
    }

    print_section(out, 2, "Remove marker");
    ir_file_marker_name marker_name{};
    std::memcpy(marker_name, name.data(), std::min(name.size(), sizeof(marker_name)));
    const INT32 result = ir_file_remove_marker(handle_guard.handle, marker_name);
    print_call(out, "ir_file_remove_marker", result, name);
    if (result != IR_FILE_EC_OK && result != IR_FILE_EC_INVALID_KEY)
    {
        return 1;
    }

    return maybe_persist(handle_guard.handle, persist, out) ? 0 : 1;
}

int handle_delta_list_command(const ThermalDemoOptions& options, std::ostream& out, std::ostream& err)
{
    HandleCloser handle_guard;
    if (!open_input_handle(options, handle_guard, out, err, 1))
    {
        return 1;
    }

    print_section(out, 2, "List deltas");
    std::vector<std::string> names;
    INT32 result = IR_FILE_EC_FAIL;
    if (!read_delta_names(handle_guard.handle, names, result))
    {
        print_call(out, "ir_file_get_delta_names", result, "");
        return 1;
    }

    print_call(out, "ir_file_get_delta_names", result, summarize_names(names));
    for (const std::string& name : names)
    {
        if (name.empty())
        {
            continue;
        }
        DeltaData delta;
        const bool ok = fetch_delta_by_name(handle_guard.handle, name, delta, result);
        print_call(out, "ir_file_get_delta(" + name + ")", result, ok ? delta_summary(delta) : "");
    }
    return 0;
}

int handle_delta_get_command(const ThermalDemoOptions& options, std::ostream& out, std::ostream& err)
{
    std::string name;
    for (size_t i = 0; i < options.command_args.size(); ++i)
    {
        const std::string& arg = options.command_args[i];
        if (arg == "--name")
        {
            if (!expect_value(options.command_args, i, "--name", name, err))
            {
                return 1;
            }
        }
        else
        {
            err << "Unknown delta-get option: " << arg << "\n";
            return 1;
        }
    }

    if (name.empty())
    {
        err << "delta-get requires --name <delta-name>\n";
        return 1;
    }

    HandleCloser handle_guard;
    if (!open_input_handle(options, handle_guard, out, err, 1))
    {
        return 1;
    }

    print_section(out, 2, "Read delta");
    DeltaData delta;
    INT32 result = IR_FILE_EC_FAIL;
    const bool ok = fetch_delta_by_name(handle_guard.handle, name, delta, result);
    print_call(out, "ir_file_get_delta", result, ok ? delta_summary(delta) : name);
    return ok ? 0 : 1;
}

int handle_delta_add_command(const ThermalDemoOptions& options, std::ostream& out, std::ostream& err)
{
    DeltaData delta;
    bool persist = false;
    for (size_t i = 0; i < options.command_args.size(); ++i)
    {
        const std::string& arg = options.command_args[i];
        std::string value;
        if (arg == "--name")
        {
            if (!expect_value(options.command_args, i, "--name", delta.name, err))
            {
                return 1;
            }
        }
        else if (arg == "--src-name")
        {
            if (!expect_value(options.command_args, i, "--src-name", delta.src_name, err))
            {
                return 1;
            }
        }
        else if (arg == "--src-type")
        {
            if (!expect_value(options.command_args, i, "--src-type", value, err))
            {
                return 1;
            }
            const auto parsed = parse_temp_value_type(value);
            if (!parsed.has_value())
            {
                err << "Invalid --src-type: " << value << "\n";
                return 1;
            }
            delta.src_type = *parsed;
        }
        else if (arg == "--src-temp")
        {
            if (!expect_value(options.command_args, i, "--src-temp", value, err))
            {
                return 1;
            }
            const auto parsed = parse_float_value(value);
            if (!parsed.has_value())
            {
                err << "Invalid --src-temp: " << value << "\n";
                return 1;
            }
            delta.src_temp = *parsed;
        }
        else if (arg == "--dest-name")
        {
            if (!expect_value(options.command_args, i, "--dest-name", delta.dest_name, err))
            {
                return 1;
            }
        }
        else if (arg == "--dest-type")
        {
            if (!expect_value(options.command_args, i, "--dest-type", value, err))
            {
                return 1;
            }
            const auto parsed = parse_temp_value_type(value);
            if (!parsed.has_value())
            {
                err << "Invalid --dest-type: " << value << "\n";
                return 1;
            }
            delta.dest_type = *parsed;
        }
        else if (arg == "--dest-temp")
        {
            if (!expect_value(options.command_args, i, "--dest-temp", value, err))
            {
                return 1;
            }
            const auto parsed = parse_float_value(value);
            if (!parsed.has_value())
            {
                err << "Invalid --dest-temp: " << value << "\n";
                return 1;
            }
            delta.dest_temp = *parsed;
        }
        else if (arg == "--persist")
        {
            persist = true;
        }
        else
        {
            err << "Unknown delta-add option: " << arg << "\n";
            return 1;
        }
    }

    HandleCloser handle_guard;
    if (!open_input_handle(options, handle_guard, out, err, 1))
    {
        return 1;
    }

    print_section(out, 2, "Add or update delta");
    if (delta.name.empty())
    {
        std::vector<std::string> names;
        INT32 result = IR_FILE_EC_FAIL;
        if (!read_delta_names(handle_guard.handle, names, result))
        {
            print_call(out, "ir_file_get_delta_names", result, "");
            return 1;
        }
        delta.name = next_prefixed_name(names, kDeltaPrefix);
        print_call(out, "name mapping", IR_FILE_EC_OK, "Windows auto-name -> " + delta.name);
    }

    INT32 result = set_delta(handle_guard.handle, delta);
    print_call(out, "ir_file_set_delta", result, delta_summary(delta));
    if (result != IR_FILE_EC_OK)
    {
        return 1;
    }

    DeltaData verify;
    const bool verify_ok = fetch_delta_by_name(handle_guard.handle, delta.name, verify, result);
    print_call(out, "ir_file_get_delta(verify)", result, verify_ok ? delta_summary(verify) : "");
    if (!verify_ok)
    {
        return 1;
    }

    return maybe_persist(handle_guard.handle, persist, out) ? 0 : 1;
}

int handle_delta_remove_command(const ThermalDemoOptions& options, std::ostream& out, std::ostream& err)
{
    std::string name;
    bool persist = false;
    for (size_t i = 0; i < options.command_args.size(); ++i)
    {
        const std::string& arg = options.command_args[i];
        if (arg == "--name")
        {
            if (!expect_value(options.command_args, i, "--name", name, err))
            {
                return 1;
            }
        }
        else if (arg == "--persist")
        {
            persist = true;
        }
        else
        {
            err << "Unknown delta-remove option: " << arg << "\n";
            return 1;
        }
    }

    if (name.empty())
    {
        err << "delta-remove requires --name <delta-name>\n";
        return 1;
    }

    HandleCloser handle_guard;
    if (!open_input_handle(options, handle_guard, out, err, 1))
    {
        return 1;
    }

    print_section(out, 2, "Remove delta");
    ir_file_delta_name delta_name{};
    std::memcpy(delta_name, name.data(), std::min(name.size(), sizeof(delta_name)));
    const INT32 result = ir_file_remove_delta(handle_guard.handle, delta_name);
    print_call(out, "ir_file_remove_delta", result, name);
    if (result != IR_FILE_EC_OK && result != IR_FILE_EC_INVALID_KEY)
    {
        return 1;
    }

    return maybe_persist(handle_guard.handle, persist, out) ? 0 : 1;
}

int handle_frame_move_command(const ThermalDemoOptions& options, std::ostream& out, std::ostream& err, const std::string& mode)
{
    int target_index = -1;
    for (size_t i = 0; i < options.command_args.size(); ++i)
    {
        const std::string& arg = options.command_args[i];
        if (arg == "--index" && mode == "goto")
        {
            std::string value;
            if (!expect_value(options.command_args, i, "--index", value, err))
            {
                return 1;
            }
            target_index = std::atoi(value.c_str());
        }
        else
        {
            err << "Unknown frame-" << mode << " option: " << arg << "\n";
            return 1;
        }
    }

    if (mode == "goto" && target_index < 0)
    {
        err << "frame-goto requires --index <n>\n";
        return 1;
    }

    HandleCloser handle_guard;
    if (!open_input_handle(options, handle_guard, out, err, 1))
    {
        return 1;
    }

    print_section(out, 2, "Move current frame");
    UINT32 frame_count = 0;
    INT32 result = ir_file_get_frame_count(handle_guard.handle, &frame_count);
    print_call(out, "ir_file_get_frame_count", result, std::to_string(frame_count));
    if (result != IR_FILE_EC_OK)
    {
        return 1;
    }

    UINT32 current_index = 0;
    result = ir_file_get_frame_index(handle_guard.handle, &current_index);
    print_call(out, "ir_file_get_frame_index", result, std::to_string(current_index));
    if (result != IR_FILE_EC_OK)
    {
        return 1;
    }

    UINT32 new_index = current_index;
    if (mode == "prev")
    {
        if (current_index > 0)
        {
            new_index = current_index - 1;
        }
    }
    else if (mode == "next")
    {
        if (frame_count > 0 && current_index + 1 < frame_count)
        {
            new_index = current_index + 1;
        }
    }
    else if (mode == "stop")
    {
        new_index = 0;
    }
    else
    {
        new_index = static_cast<UINT32>(target_index);
    }

    result = ir_file_set_frame_index(handle_guard.handle, new_index);
    print_call(out, "ir_file_set_frame_index", result, std::to_string(new_index));
    if (result != IR_FILE_EC_OK)
    {
        return 1;
    }

    result = ir_file_get_frame_index(handle_guard.handle, &current_index);
    print_call(out, "ir_file_get_frame_index(verify)", result, std::to_string(current_index));
    return result == IR_FILE_EC_OK ? 0 : 1;
}

int handle_play_steps_command(const ThermalDemoOptions& options, std::ostream& out, std::ostream& err)
{
    int steps = 1;
    int from_index = -1;
    for (size_t i = 0; i < options.command_args.size(); ++i)
    {
        const std::string& arg = options.command_args[i];
        std::string value;
        if (arg == "--steps")
        {
            if (!expect_value(options.command_args, i, "--steps", value, err))
            {
                return 1;
            }
            steps = std::atoi(value.c_str());
        }
        else if (arg == "--from-index")
        {
            if (!expect_value(options.command_args, i, "--from-index", value, err))
            {
                return 1;
            }
            from_index = std::atoi(value.c_str());
        }
        else
        {
            err << "Unknown play-steps option: " << arg << "\n";
            return 1;
        }
    }

    if (steps <= 0)
    {
        err << "--steps must be > 0\n";
        return 1;
    }

    HandleCloser handle_guard;
    if (!open_input_handle(options, handle_guard, out, err, 1))
    {
        return 1;
    }

    print_section(out, 2, "Simulate playback ticks");
    UINT32 frame_count = 0;
    INT32 result = ir_file_get_frame_count(handle_guard.handle, &frame_count);
    print_call(out, "ir_file_get_frame_count", result, std::to_string(frame_count));
    if (result != IR_FILE_EC_OK || frame_count == 0)
    {
        return 1;
    }

    UINT32 current_index = 0;
    if (!maybe_select_frame_index(handle_guard.handle, from_index, current_index, out))
    {
        return 1;
    }

    for (int i = 0; i < steps; ++i)
    {
        current_index = (current_index + 1 < frame_count) ? (current_index + 1) : 0;
        result = ir_file_set_frame_index(handle_guard.handle, current_index);
        print_call(out, "ir_file_set_frame_index(tick " + std::to_string(i + 1) + ")", result, std::to_string(current_index));
        if (result != IR_FILE_EC_OK)
        {
            return 1;
        }
    }

    return 0;
}

int handle_clip_stream_command(const ThermalDemoOptions& options, std::ostream& out, std::ostream& err)
{
    std::string output_path;
    int from_index = -1;
    int to_index = -1;
    for (size_t i = 0; i < options.command_args.size(); ++i)
    {
        const std::string& arg = options.command_args[i];
        std::string value;
        if (arg == "--output")
        {
            if (!expect_value(options.command_args, i, "--output", output_path, err))
            {
                return 1;
            }
        }
        else if (arg == "--from-index")
        {
            if (!expect_value(options.command_args, i, "--from-index", value, err))
            {
                return 1;
            }
            from_index = std::atoi(value.c_str());
        }
        else if (arg == "--to-index")
        {
            if (!expect_value(options.command_args, i, "--to-index", value, err))
            {
                return 1;
            }
            to_index = std::atoi(value.c_str());
        }
        else
        {
            err << "Unknown clip-stream option: " << arg << "\n";
            return 1;
        }
    }

    if (output_path.empty() || from_index < 0 || to_index < from_index)
    {
        err << "clip-stream requires --output <path> --from-index <n> --to-index <n>\n";
        return 1;
    }

    HandleCloser source_guard;
    if (!open_input_handle(options, source_guard, out, err, 1))
    {
        return 1;
    }

    print_section(out, 2, "Clip frame range");
    ir_file_size thermal_size{};
    INT32 result = ir_file_get_thermal_size(source_guard.handle, &thermal_size);
    print_call(out, "ir_file_get_thermal_size", result,
        std::to_string(thermal_size.width) + "x" + std::to_string(thermal_size.height));
    if (result != IR_FILE_EC_OK)
    {
        return 1;
    }

    HandleCloser clip_guard;
    result = ir_file_save_as_stream(source_guard.handle, output_path.c_str(), &clip_guard.handle);
    print_call(out, "ir_file_save_as_stream", result, output_path);
    if (result != IR_FILE_EC_OK || clip_guard.handle == nullptr)
    {
        return 1;
    }

    for (int index = from_index; index <= to_index; ++index)
    {
        FrameData frame;
        if (!read_frame(source_guard.handle, static_cast<UINT32>(index), thermal_size.width, thermal_size.height, frame, result))
        {
            print_call(out, "ir_file_get_frame(" + std::to_string(index) + ")", result, "");
            return 1;
        }
        print_call(out, "ir_file_get_frame(" + std::to_string(index) + ")", result,
            "samples=" + std::to_string(frame.samples.size()));

        ir_file_frame raw_frame{};
        raw_frame.timeStamp = frame.timestamp;
        raw_frame.length = static_cast<UINT32>(frame.samples.size());
        raw_frame.buffer = frame.samples.data();
        result = ir_file_append_frame(clip_guard.handle, &raw_frame);
        print_call(out, "ir_file_append_frame(" + std::to_string(index) + ")", result,
            format_timestamp(frame.timestamp));
        if (result != IR_FILE_EC_OK)
        {
            return 1;
        }
    }

    result = ir_file_save(clip_guard.handle);
    print_call(out, "ir_file_save", result, output_path);
    return result == IR_FILE_EC_OK ? 0 : 1;
}

int handle_rotate_command(const ThermalDemoOptions& options, std::ostream& out, std::ostream& err)
{
    std::string value;
    bool persist = false;
    for (size_t i = 0; i < options.command_args.size(); ++i)
    {
        const std::string& arg = options.command_args[i];
        if (arg == "--degrees")
        {
            if (!expect_value(options.command_args, i, "--degrees", value, err))
            {
                return 1;
            }
        }
        else if (arg == "--persist")
        {
            persist = true;
        }
        else
        {
            err << "Unknown rotate option: " << arg << "\n";
            return 1;
        }
    }

    const auto rotation = parse_rotation_type(value);
    if (!rotation.has_value())
    {
        err << "rotate requires --degrees 90|180|270\n";
        return 1;
    }

    HandleCloser handle_guard;
    if (!open_input_handle(options, handle_guard, out, err, 1))
    {
        return 1;
    }

    print_section(out, 2, "Rotate image");
    const INT32 result = ir_file_set_rotation(handle_guard.handle, *rotation);
    print_call(out, "ir_file_set_rotation", result, value);
    if (result != IR_FILE_EC_OK)
    {
        return 1;
    }

    return maybe_persist(handle_guard.handle, persist, out) ? 0 : 1;
}

int handle_mirror_command(const ThermalDemoOptions& options, std::ostream& out, std::ostream& err)
{
    std::string value;
    bool persist = false;
    for (size_t i = 0; i < options.command_args.size(); ++i)
    {
        const std::string& arg = options.command_args[i];
        if (arg == "--direction")
        {
            if (!expect_value(options.command_args, i, "--direction", value, err))
            {
                return 1;
            }
        }
        else if (arg == "--persist")
        {
            persist = true;
        }
        else
        {
            err << "Unknown mirror option: " << arg << "\n";
            return 1;
        }
    }

    const auto mirror = parse_mirror_type(value);
    if (!mirror.has_value())
    {
        err << "mirror requires --direction horizontal|vertical\n";
        return 1;
    }

    HandleCloser handle_guard;
    if (!open_input_handle(options, handle_guard, out, err, 1))
    {
        return 1;
    }

    print_section(out, 2, "Mirror image");
    const INT32 result = ir_file_set_mirror(handle_guard.handle, *mirror);
    print_call(out, "ir_file_set_mirror", result, value);
    if (result != IR_FILE_EC_OK)
    {
        return 1;
    }

    return maybe_persist(handle_guard.handle, persist, out) ? 0 : 1;
}

int handle_auto_adjust_command(const ThermalDemoOptions& options, std::ostream& out, std::ostream& err)
{
    bool persist = false;
    for (const std::string& arg : options.command_args)
    {
        if (arg == "--persist")
        {
            persist = true;
        }
        else
        {
            err << "Unknown auto-adjust option: " << arg << "\n";
            return 1;
        }
    }

    HandleCloser handle_guard;
    if (!open_input_handle(options, handle_guard, out, err, 1))
    {
        return 1;
    }

    print_section(out, 2, "Set gain to auto");
    ir_file_gain gain{};
    INT32 result = ir_file_get_gain(handle_guard.handle, &gain);
    if (result == IR_FILE_EC_OK)
    {
        std::ostringstream oss;
        oss << "mode=" << gain_mode_name(gain.mode)
            << ", min=" << format_temperature(gain.min)
            << ", max=" << format_temperature(gain.max)
            << ", span=" << gain.span;
        print_call(out, "ir_file_get_gain", result, oss.str());
    }
    else
    {
        print_call(out, "ir_file_get_gain", result, "");
        return 1;
    }

    gain.mode = IR_FILE_AUTO;
    result = ir_file_set_gain(handle_guard.handle, &gain);
    {
        std::ostringstream oss;
        oss << "mode=" << gain_mode_name(gain.mode)
            << ", min=" << format_temperature(gain.min)
            << ", max=" << format_temperature(gain.max);
        print_call(out, "ir_file_set_gain", result, oss.str());
    }
    if (result != IR_FILE_EC_OK)
    {
        return 1;
    }

    return maybe_persist(handle_guard.handle, persist, out) ? 0 : 1;
}

int handle_palette_invert_command(const ThermalDemoOptions& options, std::ostream& out, std::ostream& err)
{
    bool persist = false;
    for (const std::string& arg : options.command_args)
    {
        if (arg == "--persist")
        {
            persist = true;
        }
        else
        {
            err << "Unknown palette-invert option: " << arg << "\n";
            return 1;
        }
    }

    HandleCloser handle_guard;
    if (!open_input_handle(options, handle_guard, out, err, 1))
    {
        return 1;
    }

    print_section(out, 2, "Toggle palette inversion");
    ir_file_palette palette{};
    INT32 result = ir_file_get_palette(handle_guard.handle, &palette);
    if (result == IR_FILE_EC_OK)
    {
        std::ostringstream oss;
        oss << "refNo=" << static_cast<int>(palette.refNo)
            << ", inverted=" << yes_no(palette.inverted)
            << ", firstColor=" << format_argb(palette.colors[0]);
        print_call(out, "ir_file_get_palette", result, oss.str());
    }
    else
    {
        print_call(out, "ir_file_get_palette", result, "");
        return 1;
    }

    palette.inverted = palette.inverted == 0 ? 1 : 0;
    result = ir_file_set_palette(handle_guard.handle, &palette);
    print_call(out, "ir_file_set_palette", result, "temporary inverted=" + yes_no(palette.inverted));
    if (result != IR_FILE_EC_OK)
    {
        return 1;
    }

    return maybe_persist(handle_guard.handle, persist, out) ? 0 : 1;
}

int handle_export_marker_temperature_command(const ThermalDemoOptions& options, std::ostream& out, std::ostream& err)
{
    std::string name;
    std::string output_path;
    for (size_t i = 0; i < options.command_args.size(); ++i)
    {
        const std::string& arg = options.command_args[i];
        if (arg == "--name")
        {
            if (!expect_value(options.command_args, i, "--name", name, err))
            {
                return 1;
            }
        }
        else if (arg == "--output")
        {
            if (!expect_value(options.command_args, i, "--output", output_path, err))
            {
                return 1;
            }
        }
        else
        {
            err << "Unknown export-marker-temperature option: " << arg << "\n";
            return 1;
        }
    }

    if (name.empty() || output_path.empty())
    {
        err << "export-marker-temperature requires --name <marker-name> --output <csv>\n";
        return 1;
    }

    HandleCloser handle_guard;
    if (!open_input_handle(options, handle_guard, out, err, 1))
    {
        return 1;
    }

    print_section(out, 2, "Export marker temperature");
    MarkerData marker;
    INT32 result = IR_FILE_EC_FAIL;
    if (!fetch_marker_by_name(handle_guard.handle, name, marker, result))
    {
        print_call(out, "ir_file_get_marker", result, name);
        return 1;
    }
    print_call(out, "ir_file_get_marker", result, marker_summary(marker));

    ir_file_reference reference{};
    result = ir_file_get_reference(handle_guard.handle, &reference);
    print_call(out, "ir_file_get_reference", result,
        result == IR_FILE_EC_OK ? ("enabled=" + yes_no(reference.enabled)) : "");
    if (result != IR_FILE_EC_OK)
    {
        return 1;
    }

    const bool subtract_kelvin = reference.enabled == 0;
    std::string csv;
    if (marker.type == IR_FILE_SPOT)
    {
        ir_file_temp_point point{};
        point.point = marker.points.front();
        point.localParams = 0;
        point.reflTemp = 293.15f;
        point.distance = 1.0f;
        point.emissivity = 1.0f;
        result = ir_file_get_temp_spot(handle_guard.handle, &point);
        print_call(out, "ir_file_get_temp_spot", result, format_point(point.point));
        if (result != IR_FILE_EC_OK)
        {
            return 1;
        }

        std::ostringstream oss;
        oss << std::fixed << std::setprecision(4) << (subtract_kelvin ? point.temp - kKelvinOffset : point.temp);
        csv = oss.str();
    }
    else if (marker.type == IR_FILE_RECTANGLE || marker.type == IR_FILE_ELLIPSE)
    {
        const auto rect = marker_to_rect(marker);
        if (!rect.has_value())
        {
            err << "Marker geometry is not a valid rectangle/ellipse.\n";
            return 1;
        }

        std::vector<ir_file_point_value> values;
        const bool ok = query_temp_rect_like(handle_guard.handle, *rect, marker.type == IR_FILE_ELLIPSE, values, result);
        print_call(out,
            marker.type == IR_FILE_ELLIPSE ? "ir_file_get_temp_ellipse" : "ir_file_get_temp_rect",
            result,
            ok ? ("count=" + std::to_string(values.size())) : "");
        if (!ok)
        {
            return 1;
        }
        csv = sparse_values_to_csv(values, rect->x, rect->y, false, subtract_kelvin);
    }
    else
    {
        std::vector<ir_file_point_value> values;
        const bool polygon = marker.type == IR_FILE_POLYGON;
        const bool ok = query_temp_points_like(handle_guard.handle, marker.points, polygon, values, result);
        print_call(out,
            polygon ? "ir_file_get_temp_polygon" : "ir_file_get_temp_polyline",
            result,
            ok ? ("count=" + std::to_string(values.size())) : "");
        if (!ok)
        {
            return 1;
        }

        int left = std::numeric_limits<int>::max();
        int top = std::numeric_limits<int>::max();
        for (const ir_file_point& point : marker.points)
        {
            left = std::min(left, static_cast<int>(point.x));
            top = std::min(top, static_cast<int>(point.y));
        }
        csv = sparse_values_to_csv(values, left, top, marker.type == IR_FILE_POLYLINE, subtract_kelvin);
    }

    if (!write_text_file(output_path, csv))
    {
        err << "Failed to write CSV to " << output_path << "\n";
        return 1;
    }
    out << "  - export csv: OK -> " << output_path << "\n";
    return 0;
}

int handle_export_marker_curve_command(const ThermalDemoOptions& options, std::ostream& out, std::ostream& err)
{
    std::string name;
    std::string output_path;
    for (size_t i = 0; i < options.command_args.size(); ++i)
    {
        const std::string& arg = options.command_args[i];
        if (arg == "--name")
        {
            if (!expect_value(options.command_args, i, "--name", name, err))
            {
                return 1;
            }
        }
        else if (arg == "--output")
        {
            if (!expect_value(options.command_args, i, "--output", output_path, err))
            {
                return 1;
            }
        }
        else
        {
            err << "Unknown export-marker-curve option: " << arg << "\n";
            return 1;
        }
    }

    if (name.empty() || output_path.empty())
    {
        err << "export-marker-curve requires --name <marker-name> --output <csv>\n";
        return 1;
    }

    HandleCloser handle_guard;
    if (!open_input_handle(options, handle_guard, out, err, 1))
    {
        return 1;
    }

    print_section(out, 2, "Export marker curve");
    UINT32 frame_count = 0;
    INT32 result = ir_file_get_frame_count(handle_guard.handle, &frame_count);
    print_call(out, "ir_file_get_frame_count", result, std::to_string(frame_count));
    if (result != IR_FILE_EC_OK)
    {
        return 1;
    }

    ir_file_reference reference{};
    result = ir_file_get_reference(handle_guard.handle, &reference);
    print_call(out, "ir_file_get_reference", result,
        result == IR_FILE_EC_OK ? ("enabled=" + yes_no(reference.enabled)) : "");
    if (result != IR_FILE_EC_OK)
    {
        return 1;
    }

    const bool subtract_kelvin = reference.enabled == 0;
    std::ostringstream csv;
    for (UINT32 index = 0; index < frame_count; ++index)
    {
        UINT64 shot_time = 0;
        result = ir_file_get_index_shot_time(handle_guard.handle, index, &shot_time);
        print_call(out, "ir_file_get_index_shot_time(" + std::to_string(index) + ")", result, format_timestamp(shot_time));
        if (result != IR_FILE_EC_OK)
        {
            return 1;
        }

        MarkerData marker;
        if (!fetch_marker_by_index(handle_guard.handle, index, name, marker, result))
        {
            print_call(out, "ir_file_get_index_marker(" + std::to_string(index) + ")", result, name);
            return 1;
        }
        print_call(out, "ir_file_get_index_marker(" + std::to_string(index) + ")", result, marker_summary(marker));

        csv << index << "," << format_timestamp(shot_time);
        if (marker.type == IR_FILE_SPOT)
        {
            const float value = subtract_kelvin ? (marker.temp_values[0] - kKelvinOffset) : marker.temp_values[0];
            csv << "," << std::fixed << std::setprecision(4) << value
                << "," << value
                << "," << value;
        }
        else
        {
            const float max_value = subtract_kelvin ? (marker.temp_values[0] - kKelvinOffset) : marker.temp_values[0];
            const float min_value = subtract_kelvin ? (marker.temp_values[1] - kKelvinOffset) : marker.temp_values[1];
            const float avg_value = subtract_kelvin ? (marker.temp_values[2] - kKelvinOffset) : marker.temp_values[2];
            csv << "," << std::fixed << std::setprecision(4) << max_value
                << "," << min_value
                << "," << avg_value;
        }
        csv << "\n";
    }

    if (!write_text_file(output_path, csv.str()))
    {
        err << "Failed to write CSV to " << output_path << "\n";
        return 1;
    }
    out << "  - export csv: OK -> " << output_path << "\n";
    return 0;
}

int handle_export_delta_curve_command(const ThermalDemoOptions& options, std::ostream& out, std::ostream& err)
{
    std::string name;
    std::string output_path;
    for (size_t i = 0; i < options.command_args.size(); ++i)
    {
        const std::string& arg = options.command_args[i];
        if (arg == "--name")
        {
            if (!expect_value(options.command_args, i, "--name", name, err))
            {
                return 1;
            }
        }
        else if (arg == "--output")
        {
            if (!expect_value(options.command_args, i, "--output", output_path, err))
            {
                return 1;
            }
        }
        else
        {
            err << "Unknown export-delta-curve option: " << arg << "\n";
            return 1;
        }
    }

    if (name.empty() || output_path.empty())
    {
        err << "export-delta-curve requires --name <delta-name> --output <csv>\n";
        return 1;
    }

    HandleCloser handle_guard;
    if (!open_input_handle(options, handle_guard, out, err, 1))
    {
        return 1;
    }

    print_section(out, 2, "Export delta curve");
    UINT32 frame_count = 0;
    INT32 result = ir_file_get_frame_count(handle_guard.handle, &frame_count);
    print_call(out, "ir_file_get_frame_count", result, std::to_string(frame_count));
    if (result != IR_FILE_EC_OK)
    {
        return 1;
    }

    std::ostringstream csv;
    for (UINT32 index = 0; index < frame_count; ++index)
    {
        UINT64 shot_time = 0;
        result = ir_file_get_index_shot_time(handle_guard.handle, index, &shot_time);
        print_call(out, "ir_file_get_index_shot_time(" + std::to_string(index) + ")", result, format_timestamp(shot_time));
        if (result != IR_FILE_EC_OK)
        {
            return 1;
        }

        DeltaData delta;
        if (!fetch_delta_by_index(handle_guard.handle, index, name, delta, result))
        {
            print_call(out, "ir_file_get_index_delta(" + std::to_string(index) + ")", result, name);
            return 1;
        }
        print_call(out, "ir_file_get_index_delta(" + std::to_string(index) + ")", result, delta_summary(delta));
        csv << index << "," << format_timestamp(shot_time) << ","
            << std::fixed << std::setprecision(4) << delta.value << "\n";
    }

    if (!write_text_file(output_path, csv.str()))
    {
        err << "Failed to write CSV to " << output_path << "\n";
        return 1;
    }
    out << "  - export csv: OK -> " << output_path << "\n";
    return 0;
}

int handle_export_frame_ad_command(const ThermalDemoOptions& options, std::ostream& out, std::ostream& err)
{
    std::string output_path;
    int requested_frame_index = -1;
    for (size_t i = 0; i < options.command_args.size(); ++i)
    {
        const std::string& arg = options.command_args[i];
        if (arg == "--output")
        {
            if (!expect_value(options.command_args, i, "--output", output_path, err))
            {
                return 1;
            }
        }
        else if (arg == "--frame-index")
        {
            std::string value;
            if (!expect_value(options.command_args, i, "--frame-index", value, err))
            {
                return 1;
            }
            requested_frame_index = std::atoi(value.c_str());
        }
        else
        {
            err << "Unknown export-frame-ad option: " << arg << "\n";
            return 1;
        }
    }

    if (output_path.empty())
    {
        err << "export-frame-ad requires --output <csv>\n";
        return 1;
    }

    HandleCloser handle_guard;
    if (!open_input_handle(options, handle_guard, out, err, 1))
    {
        return 1;
    }

    print_section(out, 2, "Export current frame AD");
    UINT32 selected_index = 0;
    if (!maybe_select_frame_index(handle_guard.handle, requested_frame_index, selected_index, out))
    {
        return 1;
    }

    ir_file_size thermal_size{};
    INT32 result = ir_file_get_thermal_size(handle_guard.handle, &thermal_size);
    print_call(out, "ir_file_get_thermal_size", result,
        std::to_string(thermal_size.width) + "x" + std::to_string(thermal_size.height));
    if (result != IR_FILE_EC_OK)
    {
        return 1;
    }

    FrameData frame;
    if (!read_frame(handle_guard.handle, selected_index, thermal_size.width, thermal_size.height, frame, result))
    {
        print_call(out, "ir_file_get_frame", result, "");
        return 1;
    }
    print_call(out, "ir_file_get_frame", result, "index=" + std::to_string(selected_index));

    std::ostringstream csv;
    for (int y = 0; y < frame.height; ++y)
    {
        for (int x = 0; x < frame.width; ++x)
        {
            if (x != 0)
            {
                csv << ",";
            }
            csv << frame.samples[static_cast<size_t>(y) * frame.width + x];
        }
        csv << "\n";
    }

    if (!write_text_file(output_path, csv.str()))
    {
        err << "Failed to write CSV to " << output_path << "\n";
        return 1;
    }
    out << "  - export csv: OK -> " << output_path << "\n";
    return 0;
}

int handle_export_frame_temperature_command(const ThermalDemoOptions& options, std::ostream& out, std::ostream& err)
{
    std::string output_path;
    int requested_frame_index = -1;
    for (size_t i = 0; i < options.command_args.size(); ++i)
    {
        const std::string& arg = options.command_args[i];
        if (arg == "--output")
        {
            if (!expect_value(options.command_args, i, "--output", output_path, err))
            {
                return 1;
            }
        }
        else if (arg == "--frame-index")
        {
            std::string value;
            if (!expect_value(options.command_args, i, "--frame-index", value, err))
            {
                return 1;
            }
            requested_frame_index = std::atoi(value.c_str());
        }
        else
        {
            err << "Unknown export-frame-temperature option: " << arg << "\n";
            return 1;
        }
    }

    if (output_path.empty())
    {
        err << "export-frame-temperature requires --output <csv>\n";
        return 1;
    }

    HandleCloser handle_guard;
    if (!open_input_handle(options, handle_guard, out, err, 1))
    {
        return 1;
    }

    print_section(out, 2, "Export current frame temperature");
    UINT32 selected_index = 0;
    if (!maybe_select_frame_index(handle_guard.handle, requested_frame_index, selected_index, out))
    {
        return 1;
    }

    std::vector<USHORT> lut_ads;
    std::vector<float> lut_temps;
    BYTE rad_method = 0;
    INT32 result = IR_FILE_EC_FAIL;
    if (!read_calculated_lut(handle_guard.handle, lut_ads, lut_temps, rad_method, result))
    {
        print_call(out, "ir_file_get_calculated_lut", result, "");
        return 1;
    }
    print_call(out, "ir_file_get_calculated_lut", result,
        "count=" + std::to_string(lut_ads.size()) + ", radMethod=" + std::to_string(rad_method));

    ir_file_size thermal_size{};
    result = ir_file_get_thermal_size(handle_guard.handle, &thermal_size);
    print_call(out, "ir_file_get_thermal_size", result,
        std::to_string(thermal_size.width) + "x" + std::to_string(thermal_size.height));
    if (result != IR_FILE_EC_OK)
    {
        return 1;
    }

    FrameData frame;
    if (!read_frame(handle_guard.handle, selected_index, thermal_size.width, thermal_size.height, frame, result))
    {
        print_call(out, "ir_file_get_frame", result, "");
        return 1;
    }
    print_call(out, "ir_file_get_frame", result, "index=" + std::to_string(selected_index));

    std::vector<float> temp_by_ad(static_cast<size_t>(std::numeric_limits<USHORT>::max()) + 1, 0.0f);
    int from = lut_ads.empty() ? 0 : lut_ads.front();
    int to = from;
    for (size_t i = 0; i + 1 < lut_ads.size(); ++i)
    {
        const int tmp_from = lut_ads[i];
        const int tmp_to = lut_ads[i + 1];
        to = tmp_to;
        const float from_temp = lut_temps[i];
        const float to_temp = lut_temps[i + 1];
        if (tmp_to == tmp_from)
        {
            temp_by_ad[tmp_from] = from_temp;
            continue;
        }
        for (int ad = tmp_from; ad <= tmp_to; ++ad)
        {
            temp_by_ad[ad] = from_temp + (to_temp - from_temp) * static_cast<float>(ad - tmp_from) / static_cast<float>(tmp_to - tmp_from);
        }
    }
    for (int ad = 0; ad < from; ++ad)
    {
        temp_by_ad[ad] = temp_by_ad[from];
    }
    for (int ad = to + 1; ad <= std::numeric_limits<USHORT>::max(); ++ad)
    {
        temp_by_ad[ad] = temp_by_ad[to];
    }

    std::ostringstream csv;
    for (int y = 0; y < frame.height; ++y)
    {
        for (int x = 0; x < frame.width; ++x)
        {
            if (x != 0)
            {
                csv << ",";
            }
            const USHORT ad = frame.samples[static_cast<size_t>(y) * frame.width + x];
            csv << std::fixed << std::setprecision(4) << (temp_by_ad[ad] - kKelvinOffset);
        }
        csv << "\n";
    }

    if (!write_text_file(output_path, csv.str()))
    {
        err << "Failed to write CSV to " << output_path << "\n";
        return 1;
    }
    out << "  - export csv: OK -> " << output_path << "\n";
    return 0;
}

} // namespace

int run_thermal_file_demo(const ThermalDemoOptions& options, std::ostream& out, std::ostream& err)
{
    if (options.command == "demo")
    {
        return run_thermal_file_demo_aggregate(options, out, err);
    }
    if (options.command == "open-read")
    {
        return handle_open_read_command(options, out, err);
    }
    if (options.command == "save")
    {
        return handle_save_like_command(options, out, err, false);
    }
    if (options.command == "save-as")
    {
        return handle_save_like_command(options, out, err, true);
    }
    if (options.command == "save-frame")
    {
        return handle_save_frame_command(options, out, err);
    }
    if (options.command == "marker-list")
    {
        return handle_marker_list_command(options, out, err);
    }
    if (options.command == "marker-get")
    {
        return handle_marker_get_command(options, out, err);
    }
    if (options.command == "marker-add")
    {
        return handle_marker_add_command(options, out, err);
    }
    if (options.command == "marker-rename")
    {
        return handle_marker_rename_command(options, out, err);
    }
    if (options.command == "marker-local-params")
    {
        return handle_marker_local_params_command(options, out, err);
    }
    if (options.command == "marker-remove")
    {
        return handle_marker_remove_command(options, out, err);
    }
    if (options.command == "delta-list")
    {
        return handle_delta_list_command(options, out, err);
    }
    if (options.command == "delta-get")
    {
        return handle_delta_get_command(options, out, err);
    }
    if (options.command == "delta-add" || options.command == "delta-update")
    {
        return handle_delta_add_command(options, out, err);
    }
    if (options.command == "delta-remove")
    {
        return handle_delta_remove_command(options, out, err);
    }
    if (options.command == "frame-goto")
    {
        return handle_frame_move_command(options, out, err, "goto");
    }
    if (options.command == "frame-next")
    {
        return handle_frame_move_command(options, out, err, "next");
    }
    if (options.command == "frame-prev")
    {
        return handle_frame_move_command(options, out, err, "prev");
    }
    if (options.command == "frame-stop")
    {
        return handle_frame_move_command(options, out, err, "stop");
    }
    if (options.command == "play-steps")
    {
        return handle_play_steps_command(options, out, err);
    }
    if (options.command == "clip-stream")
    {
        return handle_clip_stream_command(options, out, err);
    }
    if (options.command == "rotate")
    {
        return handle_rotate_command(options, out, err);
    }
    if (options.command == "mirror")
    {
        return handle_mirror_command(options, out, err);
    }
    if (options.command == "auto-adjust")
    {
        return handle_auto_adjust_command(options, out, err);
    }
    if (options.command == "palette-invert")
    {
        return handle_palette_invert_command(options, out, err);
    }
    if (options.command == "export-marker-temperature")
    {
        return handle_export_marker_temperature_command(options, out, err);
    }
    if (options.command == "export-marker-curve")
    {
        return handle_export_marker_curve_command(options, out, err);
    }
    if (options.command == "export-delta-curve")
    {
        return handle_export_delta_curve_command(options, out, err);
    }
    if (options.command == "export-frame-ad")
    {
        return handle_export_frame_ad_command(options, out, err);
    }
    if (options.command == "export-frame-temperature")
    {
        return handle_export_frame_temperature_command(options, out, err);
    }

    err << "Unknown command: " << options.command << "\n";
    print_thermal_demo_usage("thermal_file_reader_demo", err);
    return 1;
}
