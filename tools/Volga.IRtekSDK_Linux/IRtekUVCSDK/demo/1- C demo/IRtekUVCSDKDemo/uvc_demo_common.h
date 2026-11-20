#pragma once

#include "IRtekUVCSDK.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
constexpr char kDefaultCaptureOutput[] = "thermal_capture.jpg";
constexpr float kKelvinOffset = 273.15f;
constexpr BYTE kDefaultPaletteRefNo = 1;
constexpr BYTE kDefaultPaletteInverted = 0;
constexpr float kDefaultAlarmMaxKelvin = 40.0f + kKelvinOffset;
constexpr float kDefaultAlarmMinKelvin = 20.0f + kKelvinOffset;
constexpr float kDefaultGainMinKelvin = -20.0f + kKelvinOffset;
constexpr float kDefaultGainMaxKelvin = 150.0f + kKelvinOffset;

// ---------------------------------------------------------------------------
// Enums & Structs
// ---------------------------------------------------------------------------
enum class DemoCommand
{
    Help,
    RefreshDevices,
    Login,
    ThermalGet,
    ThermalSet,
    ShutterModeGet,
    ShutterModeSet,
    ShutterTimeGet,
    ShutterTimeSet,
    FrameList,
    RangeList,
    RangeGet,
    RangeSet,
    DisplayGet,
    DisplaySet,
    Capture,
    Calibrate,
    Upgrade,
    Restart,
    Reset
};

struct DemoOptions
{
    DemoCommand command = DemoCommand::Help;
    int device_index = 0;
    bool enable_log = false;
    bool stream_smoke_test = true;
    int wait_ms = 3000;
    int upgrade_timeout_ms = 120000;
    int upgrade_poll_interval_ms = 500;
    std::string output_path = kDefaultCaptureOutput;
    std::string preview_output_path;
    std::string firmware_path;

    bool has_emissivity = false;
    bool has_refl_temp = false;
    bool has_distance = false;
    bool has_atm_temp = false;
    bool has_rel_humidity = false;
    float emissivity = 0.0f;
    float refl_temp = 0.0f;
    float distance = 0.0f;
    float atm_temp = 0.0f;
    float rel_humidity = 0.0f;

    bool has_shutter_mode = false;
    uvc_enum_shutter_mode shutter_mode = uvc_enum_shutter_mode::AUTOMATIC;

    bool has_shutter_time = false;
    USHORT shutter_time = 0;

    bool has_range_id = false;
    USHORT range_id = 0;

    bool has_contrast = false;
    bool has_brightness = false;
    bool has_dde_enable = false;
    bool has_dde_value = false;
    USHORT contrast = 0;
    USHORT brightness = 0;
    BYTE dde_enable = 0;
    USHORT dde_value = 0;
};

// ---------------------------------------------------------------------------
// Low-level helpers
// ---------------------------------------------------------------------------
bool is_ok(int rc);
int check_rc(const char* label, int rc);
int check_file_rc(const char* label, int rc);
float kelvin_to_celsius(float kelvin);
std::string bytes_to_string(const BYTE* data, size_t size);

// ---------------------------------------------------------------------------
// Parse helpers
// ---------------------------------------------------------------------------
bool parse_int_value(const std::string& text, int& out);
bool parse_ushort_value(const std::string& text, USHORT& out);
bool parse_float_value(const std::string& text, float& out);
bool parse_bool_value(const std::string& text, BYTE& out);
bool parse_shutter_mode_value(const std::string& text, uvc_enum_shutter_mode& out);
bool parse_command(const std::string& text, DemoCommand& out);
bool parse_args(int argc, char** argv, DemoOptions& options);

// ---------------------------------------------------------------------------
// Print / format helpers
// ---------------------------------------------------------------------------
void print_usage(const char* argv0);
std::string shutter_mode_to_string(uvc_enum_shutter_mode mode);
std::string upgrade_state_to_string(uvc_enum_upgrade_state state);
void print_device_brief(size_t index, const uvc_device& device);
void print_device_info(const uvc_device_info& info);
void print_lens_info(const uvc_lens_info& lens_info);
void print_thermal_resolution(const uvc_size& size);
void print_thermal_params(const uvc_thermal_params& params);
void print_image_params(const uvc_image_params& params);
void print_frame_list(const std::vector<uvc_frame_info>& frames);
void print_range_list(const uvc_lens_info& lens_info, const USHORT* current_range_id);
void print_lut_summary(const uvc_lut_table& lut);

// ---------------------------------------------------------------------------
// Frame / utility
// ---------------------------------------------------------------------------
const uvc_frame_info* select_best_frame(const std::vector<uvc_frame_info>& frames);
bool save_ppm(const std::string& path, int width, int height, const std::vector<uint8_t>& bgr);

// ---------------------------------------------------------------------------
// Device list
// ---------------------------------------------------------------------------
bool list_devices(std::vector<uvc_device>& devices);

// ---------------------------------------------------------------------------
// Query helpers (each issues one SDK call, prints result)
// ---------------------------------------------------------------------------
bool query_device_info(HANDLE handle, uvc_device_info& info);
bool query_lens_info(HANDLE handle, uvc_lens_info& lens_info);
bool query_thermal_resolution(HANDLE handle);
bool query_thermal_params(HANDLE handle, uvc_thermal_params& params);
bool query_shutter_mode(HANDLE handle, uvc_enum_shutter_mode& mode);
bool query_shutter_time(HANDLE handle, USHORT& minutes);
bool query_frame_list(HANDLE handle, std::vector<uvc_frame_info>& frames);
bool query_current_range(HANDLE handle, USHORT& range_id);
bool query_display_params(HANDLE handle, uvc_image_params& params);
bool query_lut(HANDLE handle, uvc_lut_table& lut);

// ---------------------------------------------------------------------------
// RAII helpers
// ---------------------------------------------------------------------------
class ScopedSdk
{
public:
    explicit ScopedSdk(bool enable_log);
    ~ScopedSdk();
    bool ok() const;

private:
    bool initialized_ = false;
};

class ScopedLogin
{
public:
    ~ScopedLogin();
    bool login(const uvc_device& device);
    HANDLE handle() const;

private:
    HANDLE handle_ = nullptr;
    uvc_device device_ {};
};

// ---------------------------------------------------------------------------
// Login helpers
// ---------------------------------------------------------------------------
bool open_logged_in_device(const DemoOptions& options, ScopedLogin& login, uvc_device& out_device);
bool with_logged_in_device(const DemoOptions& options, const std::function<bool(HANDLE)>& fn);
