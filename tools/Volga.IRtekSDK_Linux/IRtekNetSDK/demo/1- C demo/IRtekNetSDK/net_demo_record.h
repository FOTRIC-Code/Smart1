#pragma once

#include "net_demo_common.h"

bool probe_video_stream(NET_HANDLE handle, const NetDemoOptions& options, std::ostream& out);

int run_record_commands(const std::string& command, NET_HANDLE handle,
                        const NetDemoOptions& options, std::ostream& out, std::ostream& err);
