#include "net_demo_params.h"

#include <algorithm>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

// ---------------------------------------------------------------------------
// Thermal parameter display
// ---------------------------------------------------------------------------

void print_thermal_params_core(NET_HANDLE handle, std::ostream& out)
{
    net_thermal_params params{};
    INT32 rc = net_get_thermal_params(handle, &params);
    log_call(out, "net_get_thermal_params", rc);
    if (rc == NET_EC_OK)
    {
        out << "thermal-params"
            << " emissivity=" << params.emissivity
            << " distance=" << params.distance
            << " refl_temp=" << params.refl_temp
            << " atm_temp=" << params.atm_temp
            << " rel_humidity=" << params.rel_humidity
            << "\n";
    }

    net_enum_shutter_mode shutter_mode = NET_SHUTTER_AUTOMATIC;
    rc = net_get_thermal_shutter_mode(handle, &shutter_mode);
    log_call(out, "net_get_thermal_shutter_mode", rc);
    if (rc == NET_EC_OK)
    {
        out << "shutter-mode=" << shutter_mode_text(shutter_mode) << "\n";
    }

    USHORT shutter_time = 0;
    rc = net_get_thermal_shutter_time(handle, &shutter_time);
    log_call(out, "net_get_thermal_shutter_time", rc);
    if (rc == NET_EC_OK)
    {
        out << "shutter-time-minutes=" << shutter_time << "\n";
    }

    net_unit_info units{};
    rc = net_get_thermal_unit(handle, &units);
    log_call(out, "net_get_thermal_unit", rc);
    if (rc == NET_EC_OK)
    {
        out << "thermal-unit"
            << " temp=" << temp_unit_text(units.temp_unit)
            << " distance=" << distance_unit_text(units.distance_unit)
            << "\n";
    }
}

void print_thermal_ranges(NET_HANDLE handle, std::ostream& out)
{
    net_thermal_lens_info lens{};
    INT32 rc = net_get_thermal_lens_info(handle, &lens);
    log_call(out, "net_get_thermal_lens_info", rc);
    if (rc == NET_EC_OK)
    {
        for (USHORT i = 0; i < lens.ranges_count; ++i)
        {
            out << "range[" << i << "]"
                << " id=" << lens.range_map[i].id
                << " min=" << lens.range_map[i].range.min
                << " max=" << lens.range_map[i].range.max
                << "\n";
        }
    }

    USHORT range_id = 0;
    rc = net_get_thermal_range(handle, &range_id);
    log_call(out, "net_get_thermal_range", rc);
    if (rc == NET_EC_OK)
    {
        out << "active-range-id=" << range_id << "\n";
    }
}

void print_thermal_color_dist(NET_HANDLE handle, std::ostream& out)
{
    net_enum_color_dist_type type = NET_THERMAL_T_TWB;
    const INT32 rc = net_get_thermal_color_dist(handle, &type);
    log_call(out, "net_get_thermal_color_dist", rc);
    if (rc == NET_EC_OK)
    {
        out << "color-dist=" << color_dist_text(type) << "\n";
    }
}

void print_thermal_gain(NET_HANDLE handle, std::ostream& out)
{
    net_gain gain{};
    const INT32 rc = net_get_thermal_gain(handle, &gain);
    log_call(out, "net_get_thermal_gain", rc);
    if (rc == NET_EC_OK)
    {
        out << "gain"
            << " mode=" << gain_mode_text(gain.mode)
            << " min=" << gain.min
            << " max=" << gain.max
            << " fixed_span=" << format_bool(gain.fixedSpan)
            << " span=" << gain.span
            << "\n";
    }
}

void print_thermal_palette(NET_HANDLE handle, std::ostream& out)
{
    std::vector<net_preset_palette> names(32);
    USHORT count = 0;
    INT32 rc = net_get_thermal_preset_palettes(handle, names.data(), static_cast<USHORT>(names.size()), &count);
    log_call(out, "net_get_thermal_preset_palettes", rc);
    if (rc == NET_EC_OK)
    {
        for (USHORT i = 0; i < count; ++i)
        {
            out << "palette[" << i << "]"
                << " ref_no=" << names[i].ref_no
                << " name=" << as_cstr(names[i].name)
                << "\n";
        }
    }

    net_palette palette{};
    rc = net_get_thermal_palette(handle, &palette);
    log_call(out, "net_get_thermal_palette", rc);
    if (rc == NET_EC_OK)
    {
        out << "active-palette"
            << " ref_no=" << palette.ref_no
            << " color_count=" << palette.color_count
            << " inverted=" << format_bool(palette.inverted)
            << "\n";
    }
}

void print_thermal_overlay(const net_thermal_overlay_info& info, std::ostream& out)
{
    const auto print_block = [&out](const char* label, const net_overlay_block& block) {
        out << label
            << " enabled=" << format_bool(block.enabled)
            << " align=" << overlay_align_text(block.align)
            << " mode=" << overlay_mode_text(block.mode)
            << " x=" << block.x
            << " y=" << block.y
            << "\n";
    };
    const auto print_follow = [&out](const char* label, const net_overlay_follow_block& block) {
        out << label
            << " enabled=" << format_bool(block.enabled)
            << " align=" << overlay_align_text(block.align)
            << " follow=" << format_bool(block.follow)
            << " max=" << format_bool(block.max_enabled)
            << " min=" << format_bool(block.min_enabled)
            << " avg=" << format_bool(block.avg_enabled)
            << " emissivity=" << format_bool(block.emissivity_enabled)
            << " x=" << block.x
            << " y=" << block.y
            << "\n";
    };

    print_block("thermal-osd-unit", info.unit_overlay);
    print_block("thermal-osd-palette", info.plt_overlay);
    print_block("thermal-osd-params", info.thermal_params_overlay);
    print_follow("thermal-osd-global", info.global_overlay);
    print_follow("thermal-osd-marker", info.marker_overlay);
    print_block("thermal-osd-alarm", info.alarm_overlay);
}

void print_thermal_osd_get(NET_HANDLE handle, std::ostream& out)
{
    net_thermal_overlay_info info{};
    const INT32 rc = net_get_thermal_overlay(handle, &info);
    log_call(out, "net_get_thermal_overlay", rc);
    if (rc == NET_EC_OK)
    {
        print_thermal_overlay(info, out);
    }
}

// ---------------------------------------------------------------------------
// Time / Network / Users / Display display helpers
// ---------------------------------------------------------------------------

void print_time_bundle(NET_HANDLE handle, std::ostream& out)
{
    net_time_info time_info{};
    INT32 rc = net_get_time(handle, &time_info);
    log_call(out, "net_get_time", rc);
    if (rc == NET_EC_OK)
    {
        out << "time"
            << " mode=" << time_mode_text(time_info.time_mode)
            << " local_time=" << as_cstr(time_info.local_time)
            << " time_zone=" << as_cstr(time_info.time_zone)
            << "\n";
    }

    net_ntp_info ntp{};
    rc = net_get_ntp_server(handle, &ntp);
    log_call(out, "net_get_ntp_server", rc);
    if (rc == NET_EC_OK)
    {
        out << "ntp"
            << " host=" << as_cstr(ntp.host_name)
            << " port=" << ntp.port_no
            << " interval=" << ntp.syn_interval
            << "\n";
    }
}

void print_network_bundle(NET_HANDLE handle, std::ostream& out)
{
    std::vector<net_network_info> networks;
    USHORT count = 0;
    if (get_networks(handle, networks, count, out))
    {
        for (USHORT i = 0; i < count; ++i)
        {
            out << "network[" << i << "]"
                << " id=" << networks[i].id
                << " name=" << as_cstr(networks[i].name)
                << " dhcp=" << format_bool(networks[i].dhcp)
                << " ip=" << as_cstr(networks[i].ip)
                << " netmask=" << as_cstr(networks[i].netmask);
            if (networks[i].gateway_count > 0)
            {
                out << " gateway=" << as_cstr(networks[i].gateway[0]);
            }
            if (networks[i].dns_count > 0)
            {
                out << " dns=" << as_cstr(networks[i].dns[0]);
            }
            out << "\n";
        }
    }

    net_ftp_info ftp{};
    INT32 rc = net_get_ftp(handle, &ftp);
    log_call(out, "net_get_ftp", rc);
    if (rc == NET_EC_OK)
    {
        out << "ftp"
            << " host=" << as_cstr(ftp.ip)
            << " port=" << ftp.port_no
            << " user=" << as_cstr(ftp.user_name)
            << " anonymous=" << format_bool(ftp.enable_annoy)
            << " ssl=" << format_bool(ftp.use_ssl)
            << " path=" << as_cstr(ftp.path_template)
            << "\n";
    }

    net_port_info ports{};
    rc = net_get_port(handle, &ports);
    log_call(out, "net_get_port", rc);
    if (rc == NET_EC_OK)
    {
        out << "ports"
            << " http=" << ports.http_port_no
            << " rtsp=" << ports.rtsp_port_no
            << " rtp=" << ports.rtp_port_no
            << "\n";
    }

    net_webhook_info webhook{};
    rc = net_get_webhook(handle, &webhook);
    log_call(out, "net_get_webhook", rc);
    if (rc == NET_EC_OK)
    {
        out << "webhook url=" << as_cstr(webhook.url) << "\n";
    }
}

void print_users(NET_HANDLE handle, std::ostream& out)
{
    std::vector<net_user_info> users(kMaxListEntries);
    USHORT count = 0;
    const INT32 rc = net_get_users(handle, users.data(), static_cast<USHORT>(users.size()), &count);
    log_call(out, "net_get_users", rc);
    if (rc != NET_EC_OK)
    {
        return;
    }

    for (USHORT i = 0; i < count; ++i)
    {
        out << "user[" << i << "]"
            << " id=" << users[i].id
            << " level=" << user_level_text(users[i].user_level)
            << " name=" << as_cstr(users[i].user_name)
            << "\n";
    }
}

void print_display_get(NET_HANDLE handle, const NetDemoOptions& options, std::ostream& out)
{
    const auto channel_id = resolve_channel_id(handle, options, out);
    if (!channel_id.has_value())
    {
        return;
    }

    net_size size{};
    INT32 rc = net_get_thermal_resolution(handle, &size);
    log_call(out, "net_get_thermal_resolution", rc);
    if (rc == NET_EC_OK)
    {
        out << "display-resolution=" << size.width << "x" << size.height << "\n";
    }

    net_channel_info channel{};
    rc = net_get_channel(handle, *channel_id, &channel);
    log_call(out, "net_get_channel", rc, "channel_id=" + std::to_string(*channel_id));
    if (rc == NET_EC_OK)
    {
        out << "channel"
            << " id=" << channel.id
            << " name=" << as_cstr(channel.name)
            << "\n";
    }

    net_overlay_info overlay{};
    rc = net_get_overlay(handle, *channel_id, &overlay);
    log_call(out, "net_get_overlay", rc, "channel_id=" + std::to_string(*channel_id));
    if (rc == NET_EC_OK)
    {
        out << "display-channel-name-overlay"
            << " enabled=" << format_bool(overlay.channel_name_overlay.enabled)
            << " align=" << overlay_align_text(overlay.channel_name_overlay.align)
            << " x=" << overlay.channel_name_overlay.x
            << " y=" << overlay.channel_name_overlay.y
            << "\n";
        out << "display-time-overlay"
            << " enabled=" << format_bool(overlay.time_overlay.enabled)
            << " align=" << overlay_align_text(overlay.time_overlay.align)
            << " x=" << overlay.time_overlay.x
            << " y=" << overlay.time_overlay.y
            << "\n";
        out << "display-custom-overlay"
            << " enabled=" << format_bool(overlay.custom_overlay.enabled)
            << " align=" << overlay_align_text(overlay.custom_overlay.align)
            << " mode=" << overlay_mode_text(overlay.custom_overlay.mode)
            << " x=" << overlay.custom_overlay.x
            << " y=" << overlay.custom_overlay.y
            << " text=" << as_cstr(overlay.custom_overlay.text)
            << "\n";
    }

    net_image_params image_params{};
    rc = net_get_image_params(handle, *channel_id, &image_params);
    log_call(out, "net_get_image_params", rc, "channel_id=" + std::to_string(*channel_id));
    if (rc == NET_EC_OK)
    {
        out << "image-params"
            << " contrast=" << image_params.contrast
            << " brightness=" << image_params.brightness
            << " dde_enable=" << format_bool(image_params.dde_enable)
            << " dde_value=" << image_params.dde_value
            << "\n";
    }

    net_enum_image_flip_mode flip_mode = NET_IMAGE_FLIP_NONE;
    rc = net_get_image_flip(handle, *channel_id, &flip_mode);
    log_call(out, "net_get_image_flip", rc, "channel_id=" + std::to_string(*channel_id));
    if (rc == NET_EC_OK)
    {
        out << "image-flip=" << image_flip_text(flip_mode) << "\n";
    }
}

void print_markers(NET_HANDLE handle, std::ostream& out)
{
    net_size size{};
    const INT32 size_rc = net_get_thermal_resolution(handle, &size);
    log_call(out, "net_get_thermal_resolution", size_rc);
    if (size_rc == NET_EC_OK)
    {
        out << "marker-space=" << size.width << "x" << size.height << "\n";
    }

    std::vector<net_marker_info> markers;
    USHORT count = 0;
    if (!get_markers(handle, markers, count, out))
    {
        return;
    }

    for (USHORT i = 0; i < count; ++i)
    {
        out << "marker[" << i << "]"
            << " id=" << markers[i].id
            << " name=" << as_cstr(markers[i].name)
            << " type=" << marker_type_text(markers[i].type)
            << " local_params=" << markers[i].local_params
            << " emissivity=" << markers[i].emissivity
            << " distance=" << markers[i].distance
            << " refl_temp=" << markers[i].refl_temp
            << " points=" << markers[i].points_count;
        for (USHORT p = 0; p < markers[i].points_count; ++p)
        {
            out << " (" << markers[i].points[p].x << "," << markers[i].points[p].y << ")";
        }
        out << "\n";
    }
}

// ---------------------------------------------------------------------------
// Marker helpers
// ---------------------------------------------------------------------------

std::string next_marker_name(const std::vector<net_marker_info>& markers, USHORT count, char prefix)
{
    unsigned int next = 1;
    const std::string prefix_text(1, prefix);
    for (USHORT i = 0; i < count; ++i)
    {
        const std::string name = as_cstr(markers[i].name);
        if (name.rfind(prefix_text, 0) != 0 || name.size() < 2)
        {
            continue;
        }
        const std::string suffix = name.substr(1);
        if (!std::all_of(suffix.begin(), suffix.end(), [](unsigned char ch) { return std::isdigit(ch) != 0; }))
        {
            continue;
        }
        next = std::max(next, static_cast<unsigned int>(std::strtoul(suffix.c_str(), nullptr, 10) + 1));
    }

    std::ostringstream oss;
    oss << prefix << std::setw(2) << std::setfill('0') << next;
    return oss.str();
}

bool push_markers(NET_HANDLE handle, std::vector<net_marker_info>& markers, USHORT count, std::ostream& out)
{
    const INT32 rc = net_set_thermal_markers(handle, markers.data(), count);
    log_call(out, "net_set_thermal_markers", rc);
    return rc == NET_EC_OK;
}

// ---------------------------------------------------------------------------
// Command handlers
// ---------------------------------------------------------------------------

int run_params_commands(const std::string& command, NET_HANDLE handle,
                        const NetDemoOptions& options, std::ostream& out, std::ostream& err)
{
    int exit_code = 0;

    if (command == "thermal-params-get")
    {
        print_thermal_params_core(handle, out);
        print_thermal_ranges(handle, out);
        print_thermal_color_dist(handle, out);
        print_thermal_gain(handle, out);
        print_thermal_palette(handle, out);
        print_thermal_osd_get(handle, out);
    }
    else if (command == "thermal-params-set")
    {
        bool any_change = false;

        if (get_float_option(options, "emissivity").has_value() ||
            get_float_option(options, "distance").has_value() ||
            get_float_option(options, "refl-temp").has_value() ||
            get_float_option(options, "atm-temp").has_value() ||
            get_float_option(options, "rel-humidity").has_value())
        {
            net_thermal_params params{};
            INT32 rc = net_get_thermal_params(handle, &params);
            log_call(out, "net_get_thermal_params", rc);
            if (rc != NET_EC_OK)
            {
                exit_code = 1;
            }
            else
            {
                if (const auto value = get_float_option(options, "emissivity"); value.has_value()) params.emissivity = *value;
                if (const auto value = get_float_option(options, "distance"); value.has_value()) params.distance = *value;
                if (const auto value = get_float_option(options, "refl-temp"); value.has_value()) params.refl_temp = *value;
                if (const auto value = get_float_option(options, "atm-temp"); value.has_value()) params.atm_temp = *value;
                if (const auto value = get_float_option(options, "rel-humidity"); value.has_value()) params.rel_humidity = *value;
                rc = net_set_thermal_params(handle, &params);
                log_call(out, "net_set_thermal_params", rc);
                any_change = true;
                if (rc != NET_EC_OK)
                {
                    exit_code = 1;
                }
            }
        }

        if (exit_code == 0)
        {
            if (const auto value = get_value(options, "shutter-mode"); value.has_value())
            {
                const auto mode = parse_shutter_mode(*value);
                if (!mode.has_value())
                {
                    err << "Invalid --shutter-mode. Use automatic or manual.\n";
                    exit_code = 1;
                }
                else
                {
                    const INT32 rc = net_set_thermal_shutter_mode(handle, &*mode);
                    log_call(out, "net_set_thermal_shutter_mode", rc);
                    any_change = true;
                    if (rc != NET_EC_OK)
                    {
                        exit_code = 1;
                    }
                }
            }
        }

        if (exit_code == 0)
        {
            if (const auto shutter_time = get_integral_option<USHORT>(options, "shutter-time"); shutter_time.has_value())
            {
                const INT32 rc = net_set_thermal_shutter_time(handle, &*shutter_time);
                log_call(out, "net_set_thermal_shutter_time", rc);
                any_change = true;
                if (rc != NET_EC_OK)
                {
                    exit_code = 1;
                }
            }
        }

        if (exit_code == 0)
        {
            if (get_value(options, "temp-unit").has_value() || get_value(options, "distance-unit").has_value())
            {
                net_unit_info info{};
                INT32 rc = net_get_thermal_unit(handle, &info);
                log_call(out, "net_get_thermal_unit", rc);
                if (rc != NET_EC_OK)
                {
                    exit_code = 1;
                }
                else
                {
                    if (const auto value = get_value(options, "temp-unit"); value.has_value())
                    {
                        const auto parsed = parse_temp_unit(*value);
                        if (!parsed.has_value())
                        {
                            err << "Invalid --temp-unit. Use c, k, or f.\n";
                            exit_code = 1;
                        }
                        else
                        {
                            info.temp_unit = *parsed;
                        }
                    }
                    if (exit_code == 0)
                    {
                        if (const auto value = get_value(options, "distance-unit"); value.has_value())
                        {
                            const auto parsed = parse_distance_unit(*value);
                            if (!parsed.has_value())
                            {
                                err << "Invalid --distance-unit. Use m or ft.\n";
                                exit_code = 1;
                            }
                            else
                            {
                                info.distance_unit = *parsed;
                            }
                        }
                    }
                    if (exit_code == 0)
                    {
                        rc = net_set_thermal_unit(handle, &info);
                        log_call(out, "net_set_thermal_unit", rc);
                        any_change = true;
                        if (rc != NET_EC_OK)
                        {
                            exit_code = 1;
                        }
                    }
                }
            }
        }

        if (exit_code == 0)
        {
            if (const auto range_id = get_integral_option<USHORT>(options, "range-id"); range_id.has_value())
            {
                const INT32 rc = net_set_thermal_range(handle, &*range_id);
                log_call(out, "net_set_thermal_range", rc);
                any_change = true;
                if (rc != NET_EC_OK)
                {
                    exit_code = 1;
                }
            }
        }

        if (exit_code == 0)
        {
            if (const auto value = get_value(options, "color-dist"); value.has_value())
            {
                const auto type = parse_color_dist_type(*value);
                if (!type.has_value())
                {
                    err << "Invalid --color-dist. Use twb, temp-linear, or ad-linear.\n";
                    exit_code = 1;
                }
                else
                {
                    const INT32 rc = net_set_thermal_color_dist(handle, &*type);
                    log_call(out, "net_set_thermal_color_dist", rc);
                    any_change = true;
                    if (rc != NET_EC_OK)
                    {
                        exit_code = 1;
                    }
                }
            }
        }

        if (exit_code == 0)
        {
            if (get_value(options, "gain-mode").has_value() ||
                get_float_option(options, "gain-min").has_value() ||
                get_float_option(options, "gain-max").has_value() ||
                get_float_option(options, "gain-span").has_value() ||
                get_bool_option(options, "gain-fixed-span").has_value())
            {
                net_gain gain{};
                INT32 rc = net_get_thermal_gain(handle, &gain);
                log_call(out, "net_get_thermal_gain", rc);
                if (rc != NET_EC_OK)
                {
                    exit_code = 1;
                }
                else
                {
                    if (const auto value = get_value(options, "gain-mode"); value.has_value())
                    {
                        const auto parsed = parse_gain_mode(*value);
                        if (!parsed.has_value())
                        {
                            err << "Invalid --gain-mode. Use auto or fixed.\n";
                            exit_code = 1;
                        }
                        else
                        {
                            gain.mode = *parsed;
                        }
                    }
                    if (const auto value = get_float_option(options, "gain-min"); value.has_value()) gain.min = *value;
                    if (const auto value = get_float_option(options, "gain-max"); value.has_value()) gain.max = *value;
                    if (const auto value = get_float_option(options, "gain-span"); value.has_value()) gain.span = *value;
                    if (const auto value = get_bool_option(options, "gain-fixed-span"); value.has_value())
                    {
                        gain.fixedSpan = *value ? 1 : 0;
                    }
                    if (exit_code == 0)
                    {
                        rc = net_set_thermal_gain(handle, &gain);
                        log_call(out, "net_set_thermal_gain", rc);
                        any_change = true;
                        if (rc != NET_EC_OK)
                        {
                            exit_code = 1;
                        }
                    }
                }
            }
        }

        if (exit_code == 0)
        {
            if (get_integral_option<UINT32>(options, "palette-ref-no").has_value() ||
                get_bool_option(options, "palette-inverted").has_value())
            {
                net_palette palette{};
                INT32 rc = net_get_thermal_palette(handle, &palette);
                log_call(out, "net_get_thermal_palette", rc);
                if (rc != NET_EC_OK)
                {
                    exit_code = 1;
                }
                else
                {
                    if (const auto value = get_integral_option<UINT32>(options, "palette-ref-no"); value.has_value())
                    {
                        palette.ref_no = *value;
                    }
                    if (const auto value = get_bool_option(options, "palette-inverted"); value.has_value())
                    {
                        palette.inverted = *value ? 1 : 0;
                    }
                    rc = net_set_thermal_palette(handle, &palette);
                    log_call(out, "net_set_thermal_palette", rc);
                    any_change = true;
                    if (rc != NET_EC_OK)
                    {
                        exit_code = 1;
                    }
                }
            }
        }

        if (!any_change && exit_code == 0)
        {
            err << "No thermal parameter update option was provided.\n";
            exit_code = 1;
        }
    }
    else if (command == "thermal-osd-get")
    {
        print_thermal_osd_get(handle, out);
    }
    else if (command == "thermal-osd-set")
    {
        net_thermal_overlay_info info{};
        INT32 rc = net_get_thermal_overlay(handle, &info);
        log_call(out, "net_get_thermal_overlay", rc);
        if (rc != NET_EC_OK)
        {
            exit_code = 1;
        }
        else
        {
            bool changed = false;
            bool ok = true;
            ok = patch_overlay_block(options, "unit", info.unit_overlay, out, changed) && ok;
            ok = patch_overlay_block(options, "palette", info.plt_overlay, out, changed) && ok;
            ok = patch_overlay_block(options, "params", info.thermal_params_overlay, out, changed) && ok;
            ok = patch_overlay_block(options, "alarm", info.alarm_overlay, out, changed) && ok;
            ok = patch_follow_overlay(options, "global", info.global_overlay, out, changed) && ok;
            ok = patch_follow_overlay(options, "marker", info.marker_overlay, out, changed) && ok;
            if (!ok)
            {
                exit_code = 1;
            }
            else if (!changed)
            {
                err << "No thermal OSD update option was provided.\n";
                exit_code = 1;
            }
            else
            {
                rc = net_set_thermal_overlay(handle, &info);
                log_call(out, "net_set_thermal_overlay", rc);
                exit_code = rc == NET_EC_OK ? 0 : 1;
            }
        }
    }
    else if (command == "thermal-calibrate")
    {
        const INT32 rc = net_thermal_calibrate(handle);
        log_call(out, "net_thermal_calibrate", rc);
        exit_code = rc == NET_EC_OK ? 0 : 1;
    }
    else if (command == "time-get")
    {
        print_time_bundle(handle, out);
    }
    else if (command == "time-sync" || command == "time-apply")
    {
        net_time_info info{};
        INT32 rc = net_get_time(handle, &info);
        log_call(out, "net_get_time", rc);
        if (rc != NET_EC_OK)
        {
            exit_code = 1;
        }
        else
        {
            net_enum_time_mode mode = info.time_mode;
            if (command == "time-sync")
            {
                mode = NET_TIME_MODE_LOCAL;
            }
            else if (const auto value = get_value(options, "time-mode"); value.has_value())
            {
                const auto parsed = parse_time_mode(*value);
                if (!parsed.has_value())
                {
                    err << "Invalid --time-mode. Use local or ntp.\n";
                    exit_code = 1;
                }
                else
                {
                    mode = *parsed;
                }
            }

            if (exit_code == 0)
            {
                info.time_mode = mode;
                copy_text(info.time_zone, sizeof(info.time_zone), get_value(options, "time-zone").value_or(as_cstr(info.time_zone)));

                if (mode == NET_TIME_MODE_LOCAL)
                {
                    std::string local_time = get_value(options, "local-time").value_or("");
                    if (command == "time-sync")
                    {
                        const std::time_t now_time = std::time(nullptr);
                        std::tm local_tm = *std::localtime(&now_time);
                        char buffer[32] = {};
                        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &local_tm);
                        local_time = buffer;
                    }
                    if (local_time.empty())
                    {
                        err << "Missing --local-time for local time apply.\n";
                        exit_code = 1;
                    }
                    else
                    {
                        copy_text(info.local_time, sizeof(info.local_time), local_time);
                    }
                }
                else
                {
                    std::memset(info.local_time, 0, sizeof(info.local_time));
                }
            }

            if (exit_code == 0)
            {
                rc = net_set_time(handle, &info);
                log_call(out, "net_set_time", rc);
                exit_code = rc == NET_EC_OK ? 0 : 1;
                if (exit_code == 0)
                {
                    print_time_bundle(handle, out);
                }
            }
        }
    }
    else if (command == "ntp-apply")
    {
        net_ntp_info ntp{};
        INT32 rc = net_get_ntp_server(handle, &ntp);
        log_call(out, "net_get_ntp_server", rc);
        if (rc != NET_EC_OK)
        {
            exit_code = 1;
        }
        else
        {
            if (const auto value = get_value(options, "ntp-host"); value.has_value()) copy_text(ntp.host_name, sizeof(ntp.host_name), *value);
            if (const auto value = get_integral_option<USHORT>(options, "ntp-port"); value.has_value()) ntp.port_no = *value;
            if (const auto value = get_integral_option<UINT32>(options, "ntp-interval"); value.has_value()) ntp.syn_interval = *value;
            rc = net_set_ntp_server(handle, &ntp);
            log_call(out, "net_set_ntp_server", rc);
            exit_code = rc == NET_EC_OK ? 0 : 1;
            if (exit_code == 0)
            {
                print_time_bundle(handle, out);
            }
        }
    }
    else if (command == "network-refresh")
    {
        print_network_bundle(handle, out);
    }
    else if (command == "network-apply")
    {
        const auto network_id = resolve_network_id(handle, options, out);
        if (!network_id.has_value())
        {
            exit_code = 1;
        }
        else
        {
            net_network_info info{};
            INT32 rc = net_get_network(handle, *network_id, &info);
            log_call(out, "net_get_network", rc, "network_id=" + std::to_string(*network_id));
            if (rc != NET_EC_OK)
            {
                exit_code = 1;
            }
            else
            {
                if (const auto value = get_bool_option(options, "dhcp"); value.has_value()) info.dhcp = *value ? 1 : 0;
                if (const auto value = get_value(options, "net-ip"); value.has_value()) copy_text(info.ip, sizeof(info.ip), *value);
                if (const auto value = get_value(options, "netmask"); value.has_value()) copy_text(info.netmask, sizeof(info.netmask), *value);
                if (const auto value = get_value(options, "gateway"); value.has_value())
                {
                    if (!value->empty())
                    {
                        info.gateway_count = 1;
                        copy_text(info.gateway[0], sizeof(info.gateway[0]), *value);
                    }
                    else
                    {
                        info.gateway_count = 0;
                        std::memset(info.gateway, 0, sizeof(info.gateway));
                    }
                }
                if (const auto value = get_value(options, "dns"); value.has_value())
                {
                    if (!value->empty())
                    {
                        info.dns_count = 1;
                        copy_text(info.dns[0], sizeof(info.dns[0]), *value);
                    }
                    else
                    {
                        info.dns_count = 0;
                        std::memset(info.dns, 0, sizeof(info.dns));
                    }
                }
                rc = net_set_network(handle, info.id, &info);
                log_call(out, "net_set_network", rc, "network_id=" + std::to_string(info.id));
                exit_code = rc == NET_EC_OK ? 0 : 1;
            }
        }
    }
    else if (command == "ftp-apply" || command == "ftp-test")
    {
        net_ftp_info ftp{};
        INT32 rc = net_get_ftp(handle, &ftp);
        log_call(out, "net_get_ftp", rc);
        if (rc != NET_EC_OK)
        {
            exit_code = 1;
        }
        else
        {
            if (const auto value = get_value(options, "ftp-host"); value.has_value()) copy_text(ftp.ip, sizeof(ftp.ip), *value);
            if (const auto value = get_integral_option<USHORT>(options, "ftp-port"); value.has_value()) ftp.port_no = *value;
            if (const auto value = get_value(options, "ftp-user"); value.has_value()) copy_text(ftp.user_name, sizeof(ftp.user_name), *value);
            if (const auto value = get_value(options, "ftp-password"); value.has_value()) copy_text(ftp.pwd, sizeof(ftp.pwd), *value);
            if (const auto value = get_value(options, "ftp-path"); value.has_value()) copy_text(ftp.path_template, sizeof(ftp.path_template), *value);
            if (const auto value = get_bool_option(options, "ftp-ssl"); value.has_value()) ftp.use_ssl = *value ? 1 : 0;
            if (const auto value = get_bool_option(options, "ftp-anonymous"); value.has_value()) ftp.enable_annoy = *value ? 1 : 0;

            if (command == "ftp-apply")
            {
                rc = net_set_ftp(handle, &ftp);
                log_call(out, "net_set_ftp", rc);
            }
            else
            {
                rc = net_test_ftp(handle, &ftp);
                log_call(out, "net_test_ftp", rc);
            }
            exit_code = rc == NET_EC_OK ? 0 : 1;
        }
    }
    else if (command == "port-apply")
    {
        net_port_info ports{};
        INT32 rc = net_get_port(handle, &ports);
        log_call(out, "net_get_port", rc);
        if (rc != NET_EC_OK)
        {
            exit_code = 1;
        }
        else
        {
            if (const auto value = get_integral_option<USHORT>(options, "http-port"); value.has_value()) ports.http_port_no = *value;
            if (const auto value = get_integral_option<USHORT>(options, "rtsp-port"); value.has_value()) ports.rtsp_port_no = *value;
            if (const auto value = get_integral_option<USHORT>(options, "rtp-port"); value.has_value()) ports.rtp_port_no = *value;
            rc = net_set_port(handle, &ports);
            log_call(out, "net_set_port", rc);
            exit_code = rc == NET_EC_OK ? 0 : 1;
        }
    }
    else if (command == "webhook-get")
    {
        net_webhook_info webhook{};
        const INT32 rc = net_get_webhook(handle, &webhook);
        log_call(out, "net_get_webhook", rc);
        if (rc == NET_EC_OK)
        {
            out << "webhook url=" << as_cstr(webhook.url) << "\n";
        }
        else
        {
            exit_code = 1;
        }
    }
    else if (command == "webhook-apply")
    {
        std::string url;
        if (!require_option_text(options, "webhook-url", err, url))
        {
            exit_code = 1;
        }
        else
        {
            net_webhook_info webhook{};
            copy_text(webhook.url, sizeof(webhook.url), url);
            const INT32 rc = net_set_webhook(handle, &webhook);
            log_call(out, "net_set_webhook", rc);
            exit_code = rc == NET_EC_OK ? 0 : 1;
        }
    }
    else if (command == "users-get")
    {
        print_users(handle, out);
    }
    else if (command == "user-add" || command == "user-update")
    {
        std::string user_name;
        if (!require_option_text(options, "target-user-name", err, user_name))
        {
            exit_code = 1;
        }
        else
        {
            net_user_pwd_info user{};
            if (command == "user-update")
            {
                const auto user_id = get_integral_option<UINT32>(options, "user-id");
                if (!user_id.has_value())
                {
                    err << "Missing required option --user-id\n";
                    exit_code = 1;
                }
                else
                {
                    user.id = *user_id;
                }
            }

            const auto user_level = parse_user_level(get_value(options, "user-level").value_or("viewer"));
            if (!user_level.has_value())
            {
                err << "Invalid --user-level. Use root, operator, or viewer.\n";
                exit_code = 1;
            }
            else
            {
                user.user_level = *user_level;
                copy_text(user.user_name, sizeof(user.user_name), user_name);
                if (const auto value = get_value(options, "login-password"); value.has_value())
                {
                    copy_text(user.login_pwd, sizeof(user.login_pwd), *value);
                }
                if (const auto value = get_value(options, "new-password"); value.has_value())
                {
                    copy_text(user.pwd, sizeof(user.pwd), *value);
                }
                const INT32 rc = net_update_user(handle, &user);
                log_call(out, "net_update_user", rc, command == "user-add" ? "mode=add" : "mode=update");
                exit_code = rc == NET_EC_OK ? 0 : 1;
            }
        }
    }
    else if (command == "user-delete")
    {
        const auto user_id = get_integral_option<UINT32>(options, "user-id");
        if (!user_id.has_value())
        {
            err << "Missing required option --user-id\n";
            exit_code = 1;
        }
        else
        {
            const INT32 rc = net_delete_user(handle, *user_id);
            log_call(out, "net_delete_user", rc, "user_id=" + std::to_string(*user_id));
            exit_code = rc == NET_EC_OK ? 0 : 1;
        }
    }
    else if (command == "display-get")
    {
        print_display_get(handle, options, out);
    }
    else if (command == "display-set")
    {
        const auto channel_id = resolve_channel_id(handle, options, out);
        if (!channel_id.has_value())
        {
            exit_code = 1;
        }
        else
        {
            bool changed = false;
            net_overlay_info overlay{};
            INT32 rc = net_get_overlay(handle, *channel_id, &overlay);
            log_call(out, "net_get_overlay", rc, "channel_id=" + std::to_string(*channel_id));
            if (rc != NET_EC_OK)
            {
                exit_code = 1;
            }
            else
            {
                if (!patch_overlay_block(options, "channel-name", overlay.channel_name_overlay, out, changed) ||
                    !patch_overlay_block(options, "time", overlay.time_overlay, out, changed) ||
                    !patch_custom_overlay(options, overlay.custom_overlay, out, changed))
                {
                    exit_code = 1;
                }
                else if (changed)
                {
                    rc = net_set_overlay(handle, *channel_id, &overlay);
                    log_call(out, "net_set_overlay", rc, "channel_id=" + std::to_string(*channel_id));
                    if (rc != NET_EC_OK)
                    {
                        exit_code = 1;
                    }
                }
            }

            if (exit_code == 0 &&
                (get_integral_option<USHORT>(options, "contrast").has_value() ||
                 get_integral_option<USHORT>(options, "brightness").has_value() ||
                 get_integral_option<USHORT>(options, "dde-value").has_value() ||
                 get_bool_option(options, "dde-enabled").has_value()))
            {
                net_image_params params{};
                rc = net_get_image_params(handle, *channel_id, &params);
                log_call(out, "net_get_image_params", rc, "channel_id=" + std::to_string(*channel_id));
                if (rc != NET_EC_OK)
                {
                    exit_code = 1;
                }
                else
                {
                    if (const auto value = get_integral_option<USHORT>(options, "contrast"); value.has_value()) params.contrast = *value;
                    if (const auto value = get_integral_option<USHORT>(options, "brightness"); value.has_value()) params.brightness = *value;
                    if (const auto value = get_integral_option<USHORT>(options, "dde-value"); value.has_value()) params.dde_value = *value;
                    if (const auto value = get_bool_option(options, "dde-enabled"); value.has_value()) params.dde_enable = *value ? 1 : 0;
                    rc = net_set_image_params(handle, *channel_id, &params);
                    log_call(out, "net_set_image_params", rc, "channel_id=" + std::to_string(*channel_id));
                    changed = true;
                    if (rc != NET_EC_OK)
                    {
                        exit_code = 1;
                    }
                }
            }

            if (exit_code == 0)
            {
                if (const auto value = get_value(options, "image-flip"); value.has_value())
                {
                    const auto mode = parse_image_flip_mode(*value);
                    if (!mode.has_value())
                    {
                        err << "Invalid --image-flip. Use none, up-down, left-right, or center.\n";
                        exit_code = 1;
                    }
                    else
                    {
                        rc = net_set_image_flip(handle, *channel_id, &*mode);
                        log_call(out, "net_set_image_flip", rc, "channel_id=" + std::to_string(*channel_id));
                        changed = true;
                        if (rc != NET_EC_OK)
                        {
                            exit_code = 1;
                        }
                    }
                }
            }

            if (!changed && exit_code == 0)
            {
                err << "No display update option was provided.\n";
                exit_code = 1;
            }
        }
    }
    else if (command == "channel-set")
    {
        const auto channel_id = resolve_channel_id(handle, options, out);
        std::string name;
        if (!channel_id.has_value() || !require_option_text(options, "channel-name", err, name))
        {
            exit_code = 1;
        }
        else
        {
            net_channel_info channel{};
            channel.id = *channel_id;
            copy_text(channel.name, sizeof(channel.name), name);
            const INT32 rc = net_set_channel(handle, *channel_id, &channel);
            log_call(out, "net_set_channel", rc, "channel_id=" + std::to_string(*channel_id));
            exit_code = rc == NET_EC_OK ? 0 : 1;
        }
    }
    else if (command == "markers-get")
    {
        print_markers(handle, out);
    }
    else if (command == "marker-add-spot" || command == "marker-add-rectangle" || command == "marker-add-line")
    {
        std::vector<net_marker_info> markers;
        USHORT count = 0;
        if (!get_markers(handle, markers, count, out))
        {
            exit_code = 1;
        }
        else if (count >= NET_MAX_MARKERS)
        {
            err << "Device marker table is already full.\n";
            exit_code = 1;
        }
        else
        {
            net_marker_info marker{};
            marker.id = 0;
            marker.local_params = 0;
            marker.emissivity = 0.95f;
            marker.distance = 1.0f;
            marker.refl_temp = 20.0f;

            const std::string default_name =
                command == "marker-add-spot" ? next_marker_name(markers, count, 'S') :
                command == "marker-add-rectangle" ? next_marker_name(markers, count, 'A') :
                next_marker_name(markers, count, 'L');
            copy_text(marker.name, sizeof(marker.name), get_value(options, "marker-name").value_or(default_name));

            if (command == "marker-add-spot")
            {
                const auto p0 = build_point(options, "x", "y");
                if (!p0.has_value())
                {
                    err << "marker-add-spot requires --x and --y\n";
                    exit_code = 1;
                }
                else
                {
                    marker.type = IR_FILE_SPOT;
                    marker.points[0] = *p0;
                    marker.points_count = 1;
                }
            }
            else
            {
                const auto p0 = build_point(options, "x", "y");
                const auto p1 = build_point(options, "x2", "y2");
                if (!p0.has_value() || !p1.has_value())
                {
                    err << command << " requires --x --y --x2 --y2\n";
                    exit_code = 1;
                }
                else
                {
                    marker.type = command == "marker-add-rectangle" ? IR_FILE_RECTANGLE : IR_FILE_POLYLINE;
                    marker.points[0] = *p0;
                    marker.points[1] = *p1;
                    marker.points_count = 2;
                }
            }

            if (exit_code == 0)
            {
                markers[count] = marker;
                if (!push_markers(handle, markers, static_cast<USHORT>(count + 1), out))
                {
                    exit_code = 1;
                }
            }
        }
    }
    else if (command == "marker-delete")
    {
        std::string marker_name;
        if (!require_option_text(options, "marker-name", err, marker_name))
        {
            exit_code = 1;
        }
        else
        {
            std::vector<net_marker_info> markers;
            USHORT count = 0;
            if (!get_markers(handle, markers, count, out))
            {
                exit_code = 1;
            }
            else
            {
                bool removed = false;
                std::vector<net_marker_info> filtered;
                filtered.reserve(count);
                for (USHORT i = 0; i < count; ++i)
                {
                    if (!removed && marker_name == as_cstr(markers[i].name))
                    {
                        removed = true;
                        continue;
                    }
                    filtered.push_back(markers[i]);
                }

                if (!removed)
                {
                    err << "Marker not found: " << marker_name << "\n";
                    exit_code = 1;
                }
                else
                {
                    filtered.resize(NET_MAX_MARKERS);
                    if (!push_markers(handle, filtered, static_cast<USHORT>(count - 1), out))
                    {
                        exit_code = 1;
                    }
                }
            }
        }
    }

    return exit_code;
}
