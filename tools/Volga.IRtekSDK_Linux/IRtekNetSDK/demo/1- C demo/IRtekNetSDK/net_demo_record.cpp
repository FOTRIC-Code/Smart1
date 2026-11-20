#include "net_demo_record.h"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>

// ---------------------------------------------------------------------------
// Video stream callback
// ---------------------------------------------------------------------------

namespace
{

struct VideoStreamContext
{
    std::ostream* out = nullptr;
    unsigned long long count = 0;
    unsigned int samples_to_log = 2;
};

VideoStreamContext g_video_context{};

void IRTEK_NETSDK_CALL on_stream_data(const net_stream_data* stream_data)
{
    if (!stream_data || !g_video_context.out)
    {
        return;
    }

    ++g_video_context.count;
    if (g_video_context.samples_to_log == 0)
    {
        return;
    }

    --g_video_context.samples_to_log;
    (*g_video_context.out)
        << "[video-stream] stream_id=" << stream_data->stream_id
        << " type=" << stream_type_text(stream_data->stream_type)
        << " width=" << stream_data->width
        << " height=" << stream_data->height
        << " data_length=" << stream_data->data_length
        << " timestamp_us=" << stream_data->timestamp_us
        << " key_frame=" << format_bool(stream_data->key_frame)
        << "\n";
}

} // namespace

// ---------------------------------------------------------------------------
// Probe helper (also used by connect-flow)
// ---------------------------------------------------------------------------

bool probe_video_stream(NET_HANDLE handle, const NetDemoOptions& options, std::ostream& out)
{
    net_stream_info stream{};
    if (!load_selected_stream(handle, options, stream, out))
    {
        return false;
    }

    const auto stream_type = parse_stream_type(get_value(options, "stream-type").value_or("bgra"));
    if (!stream_type.has_value())
    {
        log_note(out, "invalid --stream-type");
        return false;
    }

    g_video_context.out = &out;
    g_video_context.count = 0;
    g_video_context.samples_to_log = 2;

    const INT32 stream_handle = net_start_stream_play(handle, &stream, *stream_type, on_stream_data);
    log_call(
        out,
        "net_start_stream_play",
        stream_handle,
        "stream_id=" + std::to_string(stream.id) + " type=" + stream_type_text(*stream_type));
    if (stream_handle <= 0)
    {
        g_video_context.out = nullptr;
        return false;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(options.stream_wait_ms));
    const INT32 stop_rc = net_stop_stream_play(reinterpret_cast<NET_HANDLE>(static_cast<intptr_t>(stream_handle)));
    log_call(out, "net_stop_stream_play", stop_rc);
    out << "video stream callbacks received: " << g_video_context.count << "\n";
    g_video_context.out = nullptr;
    return true;
}

// ---------------------------------------------------------------------------
// Command handler
// ---------------------------------------------------------------------------

int run_record_commands(const std::string& command, NET_HANDLE handle,
                        const NetDemoOptions& options, std::ostream& out, std::ostream& err)
{
    int exit_code = 0;

    if (command == "record-stream")
    {
        std::string output_path;
        if (!require_option_text(options, "output", err, output_path))
        {
            exit_code = 1;
        }
        else
        {
            net_stream_info stream{};
            if (!load_selected_stream(handle, options, stream, out))
            {
                exit_code = 1;
            }
            else
            {
                const auto stream_type = parse_stream_type(get_value(options, "stream-type").value_or("bgra"));
                if (!stream_type.has_value())
                {
                    err << "Invalid --stream-type. Use bgra, rgba, yuv422, or h264.\n";
                    exit_code = 1;
                }
                else
                {
                    const INT32 stream_handle = net_start_stream_play(handle, &stream, *stream_type, on_stream_data);
                    log_call(out, "net_start_stream_play", stream_handle, "stream_id=" + std::to_string(stream.id));
                    if (stream_handle <= 0)
                    {
                        exit_code = 1;
                    }
                    else
                    {
                        const NET_HANDLE play_handle = reinterpret_cast<NET_HANDLE>(static_cast<intptr_t>(stream_handle));
                        const INT32 record_rc = net_start_stream_record(
                            play_handle,
                            reinterpret_cast<const BYTE*>(output_path.c_str()),
                            static_cast<USHORT>(output_path.size()));
                        log_call(out, "net_start_stream_record", record_rc, "output=" + output_path);
                        if (record_rc != NET_EC_OK)
                        {
                            exit_code = 1;
                        }
                        else
                        {
                            std::this_thread::sleep_for(std::chrono::milliseconds(options.record_wait_ms));
                            const INT32 stop_record_rc = net_stop_stream_record(play_handle);
                            log_call(out, "net_stop_stream_record", stop_record_rc);
                            if (stop_record_rc != NET_EC_OK)
                            {
                                exit_code = 1;
                            }
                        }
                        const INT32 stop_stream_rc = net_stop_stream_play(play_handle);
                        log_call(out, "net_stop_stream_play", stop_stream_rc);
                        if (stop_stream_rc != NET_EC_OK)
                        {
                            exit_code = 1;
                        }
                    }
                }
            }
        }
    }

    return exit_code;
}
