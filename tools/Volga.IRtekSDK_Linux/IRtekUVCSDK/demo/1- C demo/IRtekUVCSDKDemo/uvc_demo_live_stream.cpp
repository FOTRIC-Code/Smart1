#include "uvc_demo_live_stream.h"

#include <chrono>
#include <cmath>
#include <condition_variable>
#include <iostream>
#include <mutex>

namespace
{
struct CaptureContext
{
    std::mutex mutex;
    std::condition_variable cv;
    bool has_frame = false;
    UvcParsedFrame frame;
};

CaptureContext* g_capture_context = nullptr;

void UVC_CALLBACK on_frame_received(const uvc_frame_data* frame)
{
    if (!g_capture_context || !frame || !frame->data || frame->length == 0)
    {
        return;
    }

    UvcParsedFrame parsed;
    if (!parse_uvc_frame_payload(frame->data, frame->length, frame->width, frame->height, parsed))
    {
        return;
    }

    if (parsed.has_metadata && parsed.shutter_state == 1)
    {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(g_capture_context->mutex);
        g_capture_context->frame = std::move(parsed);
        g_capture_context->has_frame = true;
    }
    g_capture_context->cv.notify_all();
}
} // namespace

bool wait_for_frame(HANDLE handle, const uvc_frame_info& frame_info, int wait_ms, UvcParsedFrame& out_frame)
{
    CaptureContext context;
    g_capture_context = &context;

    const int start_rc = check_rc(
        "uvc_set_stream_callback(start)",
        uvc_set_stream_callback(handle, const_cast<uvc_frame_info*>(&frame_info), on_frame_received));
    if (!is_ok(start_rc))
    {
        g_capture_context = nullptr;
        return false;
    }

    bool ready = false;
    {
        std::unique_lock<std::mutex> lock(context.mutex);
        ready = context.cv.wait_for(lock, std::chrono::milliseconds(wait_ms), [&context] {
            return context.has_frame;
        });
        if (ready)
        {
            out_frame = context.frame;
        }
    }

    check_rc(
        "uvc_set_stream_callback(stop)",
        uvc_set_stream_callback(handle, const_cast<uvc_frame_info*>(&frame_info), nullptr));
    g_capture_context = nullptr;
    return ready;
}

bool capture_single_frame(HANDLE handle, int wait_ms, UvcParsedFrame& captured)
{
    std::vector<uvc_frame_info> frames;
    if (!query_frame_list(handle, frames))
    {
        return false;
    }

    const uvc_frame_info* selected_frame = select_best_frame(frames);
    if (!selected_frame)
    {
        std::cerr << "No suitable frame selected.\n";
        return false;
    }

    if (!wait_for_frame(handle, *selected_frame, wait_ms, captured))
    {
        std::cerr << "Timed out waiting for a stable thermal frame.\n";
        return false;
    }

    if (captured.has_lut)
    {
        const float lut_center_kelvin = get_center_temperature_kelvin(captured);
        const float lut_max_kelvin = get_max_temperature_kelvin(captured);
        if (!std::isnan(lut_center_kelvin))
        {
            std::cout << "  center_temperature_lut: " << kelvin_to_celsius(lut_center_kelvin) << " C\n";
        }
        if (!std::isnan(lut_max_kelvin))
        {
            std::cout << "  max_temperature_lut: " << kelvin_to_celsius(lut_max_kelvin) << " C\n";
        }
    }

    return true;
}
