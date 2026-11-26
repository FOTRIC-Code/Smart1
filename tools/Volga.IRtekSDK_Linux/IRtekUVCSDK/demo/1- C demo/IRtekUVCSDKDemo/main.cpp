#include "uvc_demo_common.h"
#include "uvc_demo_capture.h"
#include "uvc_demo_connection.h"
#include "uvc_demo_device.h"
#include "uvc_demo_params.h"
#include "uvc_demo_stream_info.h"

#include <iostream>

int main(int argc, char** argv)
{
    DemoOptions options;
    if (!parse_args(argc, argv, options))
    {
        return 1;
    }

    if (options.command == DemoCommand::Help)
    {
        print_usage(argv[0]);
        return 0;
    }

    uvc_sdk_version version {};
    check_rc("uvc_get_sdk_version", uvc_get_sdk_version(&version));
    std::cout << "IRtekUVCSDK version: "
              << version.major << "."
              << version.minor << "."
              << version.patch << "."
              << version.build << "\n";

    const bool ok =
        options.command == DemoCommand::RefreshDevices ? run_refresh_devices(options) :
        options.command == DemoCommand::Login ? run_login_command(options) :
        options.command == DemoCommand::ThermalGet ? run_thermal_get(options) :
        options.command == DemoCommand::ThermalSet ? run_thermal_set(options) :
        options.command == DemoCommand::ShutterModeGet ? run_shutter_mode_get(options) :
        options.command == DemoCommand::ShutterModeSet ? run_shutter_mode_set(options) :
        options.command == DemoCommand::ShutterTimeGet ? run_shutter_time_get(options) :
        options.command == DemoCommand::ShutterTimeSet ? run_shutter_time_set(options) :
        options.command == DemoCommand::FrameList ? run_frame_list(options) :
        options.command == DemoCommand::RangeList ? run_range_list(options) :
        options.command == DemoCommand::RangeGet ? run_range_get(options) :
        options.command == DemoCommand::RangeSet ? run_range_set(options) :
        options.command == DemoCommand::DisplayGet ? run_display_get(options) :
        options.command == DemoCommand::DisplaySet ? run_display_set(options) :
        options.command == DemoCommand::Capture ? run_capture(options) :
        options.command == DemoCommand::Calibrate ? run_calibrate(options) :
        options.command == DemoCommand::Upgrade ? run_upgrade(options) :
        options.command == DemoCommand::Restart ? run_restart(options) :
        options.command == DemoCommand::Reset ? run_reset(options) :
        false;

    return ok ? 0 : 1;
}
