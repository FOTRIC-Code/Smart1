#pragma once

#include "net_demo_common.h"

INT32 discover_devices(const NetDemoOptions& options, std::vector<net_discovered_device>& devices,
                       USHORT& found, std::ostream& out);

void print_device_properties(NET_HANDLE handle, std::ostream& out);

int run_connection_commands(const std::string& command, NET_HANDLE handle,
                            const NetDemoOptions& options, std::ostream& out, std::ostream& err);
