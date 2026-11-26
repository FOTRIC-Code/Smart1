#include "uvc_demo_params.h"

#include <iostream>

bool run_thermal_get(const DemoOptions& options)
{
    return with_logged_in_device(options, [&](HANDLE handle) {
        uvc_thermal_params params {};
        return query_thermal_params(handle, params);
    });
}

bool run_thermal_set(const DemoOptions& options)
{
    return with_logged_in_device(options, [&](HANDLE handle) {
        uvc_thermal_params params {
            options.emissivity,
            options.distance,
            options.refl_temp,
            options.atm_temp,
            options.rel_humidity
        };
        print_thermal_params(params);
        return check_rc("uvc_set_thermal_params", uvc_set_thermal_params(handle, &params)) == UVC_EC_OK;
    });
}

bool run_shutter_mode_get(const DemoOptions& options)
{
    return with_logged_in_device(options, [&](HANDLE handle) {
        uvc_enum_shutter_mode mode {};
        return query_shutter_mode(handle, mode);
    });
}

bool run_shutter_mode_set(const DemoOptions& options)
{
    return with_logged_in_device(options, [&](HANDLE handle) {
        std::cout << "  shutter_mode: " << shutter_mode_to_string(options.shutter_mode) << "\n";
        uvc_enum_shutter_mode mode = options.shutter_mode;
        return check_rc("uvc_set_thermal_shutter_mode", uvc_set_thermal_shutter_mode(handle, &mode)) == UVC_EC_OK;
    });
}

bool run_shutter_time_get(const DemoOptions& options)
{
    return with_logged_in_device(options, [&](HANDLE handle) {
        USHORT minutes = 0;
        return query_shutter_time(handle, minutes);
    });
}

bool run_shutter_time_set(const DemoOptions& options)
{
    return with_logged_in_device(options, [&](HANDLE handle) {
        std::cout << "  shutter_time_minutes: " << options.shutter_time << "\n";
        USHORT minutes = options.shutter_time;
        return check_rc("uvc_set_thermal_shutter_time", uvc_set_thermal_shutter_time(handle, &minutes)) == UVC_EC_OK;
    });
}

bool run_range_list(const DemoOptions& options)
{
    return with_logged_in_device(options, [&](HANDLE handle) {
        uvc_lens_info lens_info {};
        if (!query_lens_info(handle, lens_info))
        {
            return false;
        }
        print_range_list(lens_info, nullptr);
        return true;
    });
}

bool run_range_get(const DemoOptions& options)
{
    return with_logged_in_device(options, [&](HANDLE handle) {
        uvc_lens_info lens_info {};
        USHORT current_range = 0;
        const bool lens_ok = query_lens_info(handle, lens_info);
        const bool range_ok = query_current_range(handle, current_range);
        if (lens_ok)
        {
            print_range_list(lens_info, range_ok ? &current_range : nullptr);
        }
        return lens_ok && range_ok;
    });
}

bool run_range_set(const DemoOptions& options)
{
    return with_logged_in_device(options, [&](HANDLE handle) {
        std::cout << "  target_range_id: " << options.range_id << "\n";
        USHORT range_id = options.range_id;
        return check_rc("uvc_set_thermal_range", uvc_set_thermal_range(handle, &range_id)) == UVC_EC_OK;
    });
}

bool run_display_get(const DemoOptions& options)
{
    return with_logged_in_device(options, [&](HANDLE handle) {
        uvc_image_params params {};
        return query_display_params(handle, params);
    });
}

bool run_display_set(const DemoOptions& options)
{
    return with_logged_in_device(options, [&](HANDLE handle) {
        uvc_image_params params {
            options.contrast,
            options.brightness,
            options.dde_enable,
            options.dde_value
        };
        print_image_params(params);
        return check_rc("uvc_set_image_params", uvc_set_image_params(handle, &params)) == UVC_EC_OK;
    });
}
