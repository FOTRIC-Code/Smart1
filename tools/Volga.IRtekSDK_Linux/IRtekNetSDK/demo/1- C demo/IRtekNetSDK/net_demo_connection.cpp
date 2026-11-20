#include "net_demo_connection.h"
#include "net_demo_stream_info.h"
#include "net_demo_params.h"
#include "net_demo_live_stream.h"
#include "net_demo_record.h"
#include "net_demo_alarm.h"

#include <cstring>
#include <iostream>

INT32 discover_devices(const NetDemoOptions& options, std::vector<net_discovered_device>& devices,
                       USHORT& found, std::ostream& out)
{
    devices.assign(kMaxListEntries, net_discovered_device{});
    found = 0;
    const INT32 rc = net_search_devices(devices.data(), static_cast<USHORT>(devices.size()), &found, options.search_timeout_ms);
    log_call(out, "net_search_devices", rc, "timeout_ms=" + std::to_string(options.search_timeout_ms));
    if (rc == NET_EC_OK)
    {
        for (USHORT i = 0; i < found; ++i)
        {
            const net_discovered_device& dev = devices[i];
            out << "device[" << i << "]"
                << " ip=" << as_cstr(dev.ip)
                << " netmask=" << as_cstr(dev.netmask)
                << " name=" << as_cstr(dev.name)
                << " model=" << as_cstr(dev.model)
                << " serial=" << as_cstr(dev.serial_no)
                << " firmware=" << as_cstr(dev.firmware_name) << " " << as_cstr(dev.firmware_version)
                << " wandering=" << static_cast<unsigned>(dev.wandering)
                << "\n";
        }
    }
    return rc;
}

void print_device_properties(NET_HANDLE handle, std::ostream& out)
{
    net_device_info dev_info{};
    INT32 rc = net_get_device_info(handle, &dev_info);
    log_call(out, "net_get_device_info", rc);
    if (rc == NET_EC_OK)
    {
        out << "device"
            << " name=" << as_cstr(dev_info.name)
            << " model=" << as_cstr(dev_info.model)
            << " spec=" << as_cstr(dev_info.spec)
            << " serial=" << as_cstr(dev_info.serial_no)
            << " brand=" << as_cstr(dev_info.brand)
            << " company=" << as_cstr(dev_info.company)
            << " firmware=" << as_cstr(dev_info.firmware_name)
            << " version=" << as_cstr(dev_info.firmware_version)
            << "\n";
    }

    net_thermal_lens_info lens{};
    rc = net_get_thermal_lens_info(handle, &lens);
    log_call(out, "net_get_thermal_lens_info", rc);
    if (rc == NET_EC_OK)
    {
        out << "lens"
            << " name=" << as_cstr(lens.name)
            << " focal=" << lens.focal
            << " hfov=" << lens.hfov
            << " vfov=" << lens.vfov
            << " band=" << as_cstr(lens.band)
            << " ranges=" << lens.ranges_count
            << "\n";
        for (USHORT i = 0; i < lens.ranges_count; ++i)
        {
            out << "range-map[" << i << "]"
                << " id=" << lens.range_map[i].id
                << " min=" << lens.range_map[i].range.min
                << " max=" << lens.range_map[i].range.max
                << "\n";
        }
    }

    net_size size{};
    rc = net_get_thermal_resolution(handle, &size);
    log_call(out, "net_get_thermal_resolution", rc);
    if (rc == NET_EC_OK)
    {
        out << "thermal-size=" << size.width << "x" << size.height << "\n";
    }

    std::vector<net_stream_info> streams;
    USHORT stream_count = 0;
    if (!get_streams(handle, streams, stream_count, out))
    {
        return;
    }

    for (USHORT i = 0; i < stream_count; ++i)
    {
        net_stream_info stream{};
        rc = net_get_stream(handle, streams[i].id, &stream);
        log_call(out, "net_get_stream", rc, "stream_id=" + std::to_string(streams[i].id));
        if (rc != NET_EC_OK)
        {
            stream = streams[i];
        }

        net_stream_status status{};
        const INT32 status_rc = net_get_stream_status(handle, stream.id, &status);
        log_call(out, "net_get_stream_status", status_rc, "stream_id=" + std::to_string(stream.id));

        out << "stream"
            << " id=" << stream.id
            << " codec=" << as_cstr(stream.codec_type)
            << " bitrate=" << stream.bit_rate
            << " fps=" << stream.max_frame_rate
            << " resolution=" << stream.resolution_width << "x" << stream.resolution_height
            << " connections=" << (status_rc == NET_EC_OK ? status.connection_count : 0)
            << "\n";
    }
}

int run_connection_commands(const std::string& command, NET_HANDLE handle,
                            const NetDemoOptions& options, std::ostream& out, std::ostream& err)
{
    int exit_code = 0;

    if (command == "connect-flow")
    {
        print_thermal_params_core(handle, out);
        print_thermal_ranges(handle, out);
        print_thermal_palette(handle, out);
        print_device_properties(handle, out);
        print_time_bundle(handle, out);
        print_network_bundle(handle, out);
        print_users(handle, out);
        print_display_get(handle, options, out);
        print_thermal_color_dist(handle, out);
        print_thermal_gain(handle, out);
        print_markers(handle, out);
        probe_thermal_stream(handle, options, out);
        probe_video_stream(handle, options, out);
    }
    else if (command == "properties-get")
    {
        print_device_properties(handle, out);
        print_channels(handle, out);
    }
    else if (command == "device-restart")
    {
        const INT32 rc = net_device_restart(handle);
        log_call(out, "net_device_restart", rc);
        exit_code = rc == NET_EC_OK ? 0 : 1;
    }
    else if (command == "device-reset")
    {
        const INT32 rc = net_device_reset(handle);
        log_call(out, "net_device_reset", rc);
        exit_code = rc == NET_EC_OK ? 0 : 1;
    }
    else if (command == "device-name-set")
    {
        std::string name;
        if (!require_option_text(options, "device-name", err, name))
        {
            exit_code = 1;
        }
        else
        {
            net_device_info info{};
            INT32 rc = net_get_device_info(handle, &info);
            log_call(out, "net_get_device_info", rc);
            if (rc != NET_EC_OK)
            {
                exit_code = 1;
            }
            else
            {
                copy_text(info.name, sizeof(info.name), name);
                rc = net_set_device_info(handle, &info);
                log_call(out, "net_set_device_info", rc);
                exit_code = rc == NET_EC_OK ? 0 : 1;
            }
        }
    }
    else if (command == "device-upgrade")
    {
        std::string file;
        if (!require_option_text(options, "upgrade-file", err, file))
        {
            exit_code = 1;
        }
        else
        {
            net_upgrade_info info{};
            copy_text(info.path, sizeof(info.path), file);
            const INT32 rc = net_device_upgrade(handle, &info);
            log_call(out, "net_device_upgrade", rc, "file=" + file);
            exit_code = rc == NET_EC_OK ? 0 : 1;
        }
    }

    return exit_code;
}
