#include "uvc_demo_stream_info.h"

bool run_frame_list(const DemoOptions& options)
{
    return with_logged_in_device(options, [&](HANDLE handle) {
        std::vector<uvc_frame_info> frames;
        return query_frame_list(handle, frames);
    });
}
