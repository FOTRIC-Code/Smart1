#include "net_demo_live_stream.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <thread>

// ---------------------------------------------------------------------------
// Thermal stream callback
// ---------------------------------------------------------------------------

namespace
{

struct ThermalStreamContext
{
    std::ostream* out = nullptr;
    unsigned long long count = 0;
    unsigned int samples_to_log = 2;
};

ThermalStreamContext g_thermal_context{};

void IRTEK_NETSDK_CALL on_thermal_data(const net_thermal_data* frame)
{
    if (!frame || !g_thermal_context.out)
    {
        return;
    }

    ++g_thermal_context.count;
    if (g_thermal_context.samples_to_log == 0)
    {
        return;
    }

    --g_thermal_context.samples_to_log;
    (*g_thermal_context.out)
        << "[thermal-stream] data_types=0x" << std::hex << static_cast<unsigned>(frame->data_types) << std::dec
        << " timestamp_us=" << frame->timestamp_us
        << " markers=" << frame->markers_count;
    if (frame->data_types & NET_THERMAL_DATA_GLOBAL)
    {
        (*g_thermal_context.out)
            << " max=" << frame->global_temp_info.max.temperature
            << " min=" << frame->global_temp_info.min.temperature
            << " avg=" << frame->global_temp_info.avg_temp;
    }
    (*g_thermal_context.out) << "\n";
}

} // namespace

// ---------------------------------------------------------------------------
// Display helpers
// ---------------------------------------------------------------------------

void print_thermal_data_snapshot(NET_HANDLE handle, std::ostream& out)
{
    net_size resolution{};
    INT32 rc = net_get_thermal_resolution(handle, &resolution);
    log_call(out, "net_get_thermal_resolution", rc);
    if (rc == NET_EC_OK)
    {
        out << "thermal-resolution=" << resolution.width << "x" << resolution.height << "\n";
    }

    net_thermal_data frame{};
    rc = net_get_thermal_data(
        handle,
        static_cast<UINT32>(NET_THERMAL_DATA_RAW | NET_THERMAL_DATA_GLOBAL | NET_THERMAL_DATA_MARKER),
        &frame);
    log_call(out, "net_get_thermal_data", rc);
    if (rc != NET_EC_OK)
    {
        return;
    }

    out << "thermal-snapshot"
        << " data_types=0x" << std::hex << static_cast<unsigned>(frame.data_types) << std::dec
        << " timestamp_us=" << frame.timestamp_us
        << " markers=" << frame.markers_count
        << "\n";

    if (frame.data_types & NET_THERMAL_DATA_RAW)
    {
        out << "raw"
            << " width=" << frame.raw_frame.width
            << " height=" << frame.raw_frame.height
            << " points=" << frame.raw_frame.points_count
            << " range_id=" << frame.raw_frame.range_id
            << " frame_no=" << frame.raw_frame.frame_no
            << "\n";
    }

    if (frame.data_types & NET_THERMAL_DATA_GLOBAL)
    {
        out << "global"
            << " max=" << frame.global_temp_info.max.temperature
            << " min=" << frame.global_temp_info.min.temperature
            << " avg=" << frame.global_temp_info.avg_temp
            << "\n";
    }

    for (USHORT i = 0; i < frame.markers_count; ++i)
    {
        const net_marker_temp_info& marker = frame.marker_temps[i];
        out << "marker-temp[" << i << "]"
            << " id=" << marker.marker_id
            << " name=" << as_cstr(marker.marker_name)
            << " type=" << marker_type_text(marker.type)
            << " max=" << marker.temp_point_info.max.temperature
            << " min=" << marker.temp_point_info.min.temperature
            << " avg=" << marker.temp_point_info.avg_temp
            << "\n";
    }
}

void print_thermal_lut(NET_HANDLE handle, std::ostream& out)
{
    net_lut_table lut{};
    const INT32 rc = net_get_thermal_lut(handle, &lut);
    log_call(out, "net_get_thermal_lut", rc);
    if (rc != NET_EC_OK)
    {
        return;
    }

    out << "thermal-lut"
        << " range_id=" << lut.range_id
        << " length=" << lut.length
        << "\n";

    const USHORT preview = std::min<USHORT>(lut.length, 8);
    for (USHORT i = 0; i < preview; ++i)
    {
        out << "lut[" << i << "] ad=" << lut.ad[i] << " temp=" << lut.t[i] << "\n";
    }
}

// ---------------------------------------------------------------------------
// Probe helper (also used by connect-flow)
// ---------------------------------------------------------------------------

bool probe_thermal_stream(NET_HANDLE handle, const NetDemoOptions& options, std::ostream& out)
{
    net_thermal_data_config config{};
    config.data_types = thermal_stream_mask(options);
    config.sample_interval = 1000;

    g_thermal_context.out = &out;
    g_thermal_context.count = 0;
    g_thermal_context.samples_to_log = 2;

    const INT32 stream_handle = net_start_thermal_data_stream(handle, &config, on_thermal_data);
    log_call(
        out,
        "net_start_thermal_data_stream",
        stream_handle,
        "data_types=" + std::to_string(static_cast<unsigned>(config.data_types)));
    if (stream_handle <= 0)
    {
        g_thermal_context.out = nullptr;
        return false;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(options.thermal_wait_ms));
    const INT32 stop_rc = net_stop_thermal_data_stream(reinterpret_cast<NET_HANDLE>(static_cast<intptr_t>(stream_handle)));
    log_call(out, "net_stop_thermal_data_stream", stop_rc);
    out << "thermal stream callbacks received: " << g_thermal_context.count << "\n";
    g_thermal_context.out = nullptr;
    return true;
}

// ---------------------------------------------------------------------------
// Command handler
// ---------------------------------------------------------------------------

int run_live_stream_commands(const std::string& command, NET_HANDLE handle,
                             const NetDemoOptions& options, std::ostream& out, std::ostream& err)
{
    (void)err;
    int exit_code = 0;

    if (command == "thermal-data-get")
    {
        print_thermal_data_snapshot(handle, out);
    }
    else if (command == "thermal-lut-get")
    {
        print_thermal_lut(handle, out);
    }

    return exit_code;
}
