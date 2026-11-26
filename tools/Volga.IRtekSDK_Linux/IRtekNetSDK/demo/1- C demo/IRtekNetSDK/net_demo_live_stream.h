#pragma once

#include "net_demo_common.h"

void print_thermal_data_snapshot(NET_HANDLE handle, std::ostream& out);
void print_thermal_lut(NET_HANDLE handle, std::ostream& out);
bool probe_thermal_stream(NET_HANDLE handle, const NetDemoOptions& options, std::ostream& out);

int run_live_stream_commands(const std::string& command, NET_HANDLE handle,
                             const NetDemoOptions& options, std::ostream& out, std::ostream& err);
