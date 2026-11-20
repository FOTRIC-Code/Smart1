#include "net_demo_alarm.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>

// ---------------------------------------------------------------------------
// Alarm callback
// ---------------------------------------------------------------------------

namespace
{

struct AlarmContext
{
    std::ostream* out = nullptr;
    unsigned long long count = 0;
};

AlarmContext g_alarm_context{};

void IRTEK_NETSDK_CALL on_alarm(const net_alarm_info* alarm_info)
{
    if (!alarm_info || !g_alarm_context.out)
    {
        return;
    }

    ++g_alarm_context.count;
    (*g_alarm_context.out)
        << "[alarm] type=" << static_cast<int>(alarm_info->type)
        << " payload_length=" << alarm_info->alarm_info_length;

    if (alarm_info->type == NET_ALARM_THERMAL_TEMP &&
        alarm_info->alarm_info &&
        alarm_info->alarm_info_length >= sizeof(net_alarm_thermal_temp_info))
    {
        const auto* payload = reinterpret_cast<const net_alarm_thermal_temp_info*>(alarm_info->alarm_info);
        (*g_alarm_context.out)
            << " ip=" << as_cstr(payload->dev_info.ip)
            << " channel=" << payload->dev_info.channel_id
            << " marker_alarms=" << payload->marker_alarms_count
            << " markers=" << payload->markers_count;
    }

    (*g_alarm_context.out) << "\n";
}

} // namespace

// ---------------------------------------------------------------------------
// Alarm helpers
// ---------------------------------------------------------------------------

net_thermal_temp_alarm make_default_alarm(const std::string& marker_name)
{
    net_thermal_temp_alarm alarm{};
    copy_text(alarm.marker_name, sizeof(alarm.marker_name), marker_name);
    alarm.enabled = 0;
    alarm.alarm_type = NET_ALARM_HIGHESTGREATER;
    alarm.alarm_temp = 50.0f;
    alarm.alarm_delay_time = 1;
    alarm.threshold_temp = 30.0f;
    alarm.threshold_delay_time = 1;
    alarm.trigger_temp = -50.0f;
    alarm.trigger_delay_time = 1;
    return alarm;
}

std::vector<net_thermal_temp_alarm> merge_alarms_with_markers(
    const std::vector<net_marker_info>& markers,
    USHORT marker_count,
    const std::vector<net_thermal_temp_alarm>& alarms,
    USHORT alarm_count)
{
    std::vector<net_thermal_temp_alarm> merged;
    merged.reserve(marker_count);
    for (USHORT i = 0; i < marker_count; ++i)
    {
        const std::string marker_name = as_cstr(markers[i].name);
        bool matched = false;
        for (USHORT j = 0; j < alarm_count; ++j)
        {
            if (marker_name == as_cstr(alarms[j].marker_name))
            {
                merged.push_back(alarms[j]);
                matched = true;
                break;
            }
        }
        if (!matched)
        {
            merged.push_back(make_default_alarm(marker_name));
        }
    }
    return merged;
}

void print_alarm_bundle(NET_HANDLE handle, std::ostream& out)
{
    std::vector<net_marker_info> markers;
    USHORT marker_count = 0;
    get_markers(handle, markers, marker_count, out);

    std::vector<net_thermal_temp_alarm> alarms;
    USHORT alarm_count = 0;
    if (!get_alarm_rules(handle, alarms, alarm_count, out))
    {
        return;
    }

    const std::vector<net_thermal_temp_alarm> merged = merge_alarms_with_markers(markers, marker_count, alarms, alarm_count);
    for (size_t i = 0; i < merged.size(); ++i)
    {
        const net_thermal_temp_alarm& alarm = merged[i];
        out << "alarm[" << i << "]"
            << " marker=" << as_cstr(alarm.marker_name)
            << " enabled=" << format_bool(alarm.enabled)
            << " type=" << alarm_type_text(alarm.alarm_type)
            << " alarm_temp=" << alarm.alarm_temp
            << " alarm_delay=" << alarm.alarm_delay_time
            << " warning_temp=" << alarm.threshold_temp
            << " warning_delay=" << alarm.threshold_delay_time
            << " alert_temp=" << alarm.trigger_temp
            << " alert_delay=" << alarm.trigger_delay_time
            << "\n";
    }

    net_enum_alarm_type alarm_type = NET_ALARM_THERMAL_TEMP;
    net_alarm_trigger trigger{};
    INT32 rc = net_get_alarm_trigger(handle, &alarm_type, &trigger);
    log_call(out, "net_get_alarm_trigger", rc);
    if (rc == NET_EC_OK)
    {
        out << "alarm-trigger"
            << " ftp=" << format_bool(trigger.trigger_ftp_enabled)
            << " webhook=" << format_bool(trigger.trigger_webhook_enabled)
            << "\n";
    }

    net_thermal_temp_alarm_interval interval{};
    rc = net_get_thermal_temp_alarm_interval(handle, &interval);
    log_call(out, "net_get_thermal_temp_alarm_interval", rc);
    if (rc == NET_EC_OK)
    {
        out << "alarm-interval=" << interval.interval << "\n";
    }
}

INT32 start_alarm_listener_and_wait(NET_HANDLE handle, const NetDemoOptions& options, std::ostream& out)
{
    g_alarm_context.out = &out;
    g_alarm_context.count = 0;
    const INT32 alarm_id = net_start_alarm_listener(handle, on_alarm);
    log_call(out, "net_start_alarm_listener", alarm_id);
    if (alarm_id > 0)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(options.alarm_wait_ms));
        const INT32 stop_rc = net_stop_alarm_listener(reinterpret_cast<NET_HANDLE>(static_cast<intptr_t>(alarm_id)));
        log_call(out, "net_stop_alarm_listener", stop_rc);
        out << "alarm callbacks received: " << g_alarm_context.count << "\n";
    }
    g_alarm_context.out = nullptr;
    return alarm_id;
}

// ---------------------------------------------------------------------------
// Command handler
// ---------------------------------------------------------------------------

int run_alarm_commands(const std::string& command, NET_HANDLE handle,
                       const NetDemoOptions& options, std::ostream& out, std::ostream& err)
{
    int exit_code = 0;

    if (command == "alarm-get")
    {
        print_alarm_bundle(handle, out);
    }
    else if (command == "alarm-set")
    {
        std::vector<net_marker_info> markers;
        USHORT marker_count = 0;
        get_markers(handle, markers, marker_count, out);

        std::vector<net_thermal_temp_alarm> alarms;
        USHORT alarm_count = 0;
        if (!get_alarm_rules(handle, alarms, alarm_count, out))
        {
            exit_code = 1;
        }
        else
        {
            std::vector<net_thermal_temp_alarm> merged = merge_alarms_with_markers(markers, marker_count, alarms, alarm_count);
            bool updated_rules = false;

            if (const auto marker_name = get_value(options, "marker-name"); marker_name.has_value())
            {
                auto it = std::find_if(merged.begin(), merged.end(), [&](const net_thermal_temp_alarm& alarm) {
                    return *marker_name == as_cstr(alarm.marker_name);
                });
                if (it == merged.end())
                {
                    err << "Alarm marker not found: " << *marker_name << "\n";
                    exit_code = 1;
                }
                else
                {
                    if (const auto value = get_bool_option(options, "alarm-enabled"); value.has_value())
                    {
                        it->enabled = *value ? 1 : 0;
                    }
                    if (const auto value = get_value(options, "alarm-type"); value.has_value())
                    {
                        const auto parsed = parse_alarm_type(*value);
                        if (!parsed.has_value())
                        {
                            err << "Invalid --alarm-type\n";
                            exit_code = 1;
                        }
                        else
                        {
                            it->alarm_type = *parsed;
                        }
                    }
                    if (const auto value = get_float_option(options, "alarm-temp"); value.has_value()) it->alarm_temp = *value;
                    if (const auto value = get_integral_option<UINT32>(options, "alarm-delay"); value.has_value()) it->alarm_delay_time = *value;
                    if (const auto value = get_float_option(options, "warning-temp"); value.has_value()) it->threshold_temp = *value;
                    if (const auto value = get_integral_option<UINT32>(options, "warning-delay"); value.has_value()) it->threshold_delay_time = *value;
                    if (const auto value = get_float_option(options, "alert-temp"); value.has_value()) it->trigger_temp = *value;
                    if (const auto value = get_integral_option<UINT32>(options, "alert-delay"); value.has_value()) it->trigger_delay_time = *value;
                    updated_rules = true;
                }
            }

            if (exit_code == 0 && updated_rules)
            {
                const INT32 rc = net_set_thermal_temp_alarms(
                    handle,
                    merged.data(),
                    static_cast<USHORT>(merged.size()));
                log_call(out, "net_set_thermal_temp_alarms", rc);
                if (rc != NET_EC_OK)
                {
                    exit_code = 1;
                }
            }

            if (exit_code == 0)
            {
                if (const auto interval = get_integral_option<USHORT>(options, "alarm-interval"); interval.has_value())
                {
                    net_thermal_temp_alarm_interval value{};
                    value.interval = *interval;
                    const INT32 rc = net_set_thermal_temp_alarm_interval(handle, &value);
                    log_call(out, "net_set_thermal_temp_alarm_interval", rc);
                    if (rc != NET_EC_OK)
                    {
                        exit_code = 1;
                    }
                }
            }

            if (exit_code == 0)
            {
                if (get_bool_option(options, "alarm-trigger-ftp").has_value() ||
                    get_bool_option(options, "alarm-trigger-webhook").has_value())
                {
                    net_enum_alarm_type alarm_type = NET_ALARM_THERMAL_TEMP;
                    net_alarm_trigger trigger{};
                    INT32 rc = net_get_alarm_trigger(handle, &alarm_type, &trigger);
                    log_call(out, "net_get_alarm_trigger", rc);
                    if (rc != NET_EC_OK)
                    {
                        exit_code = 1;
                    }
                    else
                    {
                        if (const auto value = get_bool_option(options, "alarm-trigger-ftp"); value.has_value())
                        {
                            trigger.trigger_ftp_enabled = *value ? 1 : 0;
                        }
                        if (const auto value = get_bool_option(options, "alarm-trigger-webhook"); value.has_value())
                        {
                            trigger.trigger_webhook_enabled = *value ? 1 : 0;
                        }
                        rc = net_set_alarm_trigger(handle, &alarm_type, &trigger);
                        log_call(out, "net_set_alarm_trigger", rc);
                        if (rc != NET_EC_OK)
                        {
                            exit_code = 1;
                        }
                    }
                }
            }

            if (!updated_rules &&
                !get_integral_option<USHORT>(options, "alarm-interval").has_value() &&
                !get_bool_option(options, "alarm-trigger-ftp").has_value() &&
                !get_bool_option(options, "alarm-trigger-webhook").has_value() &&
                exit_code == 0)
            {
                err << "No alarm update option was provided.\n";
                exit_code = 1;
            }
        }
    }
    else if (command == "alarm-listen")
    {
        if (start_alarm_listener_and_wait(handle, options, out) <= 0)
        {
            exit_code = 1;
        }
    }

    return exit_code;
}
