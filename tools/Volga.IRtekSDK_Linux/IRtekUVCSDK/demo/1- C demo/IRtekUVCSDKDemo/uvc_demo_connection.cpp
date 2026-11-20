#include "uvc_demo_connection.h"
#include "uvc_demo_live_stream.h"

#include <iostream>

namespace
{
bool run_login_chain(HANDLE handle, const DemoOptions& options)
{
    bool ok = true;

    std::cout << "\n== LoadProperties ==\n";
    uvc_device_info dev_info {};
    uvc_lens_info lens_info {};
    ok = query_device_info(handle, dev_info) && ok;
    ok = query_lens_info(handle, lens_info) && ok;
    ok = query_thermal_resolution(handle) && ok;

    std::cout << "\n== LoadThermalParams ==\n";
    uvc_thermal_params thermal_params {};
    uvc_enum_shutter_mode shutter_mode {};
    USHORT shutter_time = 0;
    ok = query_thermal_params(handle, thermal_params) && ok;
    ok = query_shutter_mode(handle, shutter_mode) && ok;
    ok = query_shutter_time(handle, shutter_time) && ok;

    std::cout << "\n== LoadFrameList ==\n";
    std::vector<uvc_frame_info> frames;
    const bool have_frames = query_frame_list(handle, frames);
    ok = have_frames && ok;

    std::cout << "\n== LoadRanges ==\n";
    uvc_lens_info range_lens_info {};
    USHORT current_range = 0;
    const bool have_range_lens = query_lens_info(handle, range_lens_info);
    const bool have_current_range = query_current_range(handle, current_range);
    if (have_range_lens)
    {
        print_range_list(range_lens_info, have_current_range ? &current_range : nullptr);
    }
    ok = have_range_lens && ok;
    ok = have_current_range && ok;

    std::cout << "\n== LoadImageFlip ==\n";
    std::cout << "  Windows MainForm stores image flip as client-side bitmap state; no uvc_* call in login chain.\n";

    std::cout << "\n== UpdateRendererParams ==\n";
    uvc_thermal_params renderer_thermal {};
    uvc_image_params renderer_image {};
    uvc_lut_table renderer_lut {};
    uvc_lens_info renderer_lens_info {};
    USHORT renderer_range = 0;
    ok = query_thermal_params(handle, renderer_thermal) && ok;
    ok = query_display_params(handle, renderer_image) && ok;
    ok = query_current_range(handle, renderer_range) && ok;
    ok = query_lens_info(handle, renderer_lens_info) && ok;
    ok = query_lut(handle, renderer_lut) && ok;

    if (options.stream_smoke_test)
    {
        std::cout << "\n== StartStream (CLI smoke test) ==\n";
        if (!have_frames)
        {
            std::cerr << "Skipping stream smoke test because no frame list is available.\n";
            ok = false;
        }
        else
        {
            const uvc_frame_info* selected = select_best_frame(frames);
            if (!selected)
            {
                std::cerr << "Skipping stream smoke test because no frame was selected.\n";
                ok = false;
            }
            else
            {
                UvcParsedFrame frame;
                if (wait_for_frame(handle, *selected, options.wait_ms, frame))
                {
                    std::cout << "  captured_frame: " << frame.width << "x" << frame.image_height
                              << " frame_no=" << frame.frame_no
                              << " shutter_state=" << frame.shutter_state
                              << "\n";
                }
                else
                {
                    std::cerr << "Timed out waiting for a stable thermal frame.\n";
                    ok = false;
                }
            }
        }
    }
    else
    {
        std::cout << "\n== StartStream ==\n";
        std::cout << "  skipped by --no-stream-smoke\n";
    }

    return ok;
}
} // namespace

bool run_refresh_devices(const DemoOptions& options)
{
    ScopedSdk sdk(options.enable_log);
    if (!sdk.ok())
    {
        return false;
    }

    std::vector<uvc_device> devices;
    return list_devices(devices);
}

bool run_login_command(const DemoOptions& options)
{
    return with_logged_in_device(options, [&](HANDLE handle) {
        return run_login_chain(handle, options);
    });
}
