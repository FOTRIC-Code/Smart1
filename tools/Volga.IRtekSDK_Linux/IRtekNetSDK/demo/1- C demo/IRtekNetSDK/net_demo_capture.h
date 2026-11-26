#pragma once

#include "net_demo_common.h"

int run_capture_commands(const std::string& command, NET_HANDLE handle,
                         const NetDemoOptions& options, std::ostream& out, std::ostream& err);
