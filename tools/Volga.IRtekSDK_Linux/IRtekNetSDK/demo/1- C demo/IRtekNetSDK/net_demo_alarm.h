#pragma once

#include "net_demo_common.h"

void print_alarm_bundle(NET_HANDLE handle, std::ostream& out);
net_thermal_temp_alarm make_default_alarm(const std::string& marker_name);
std::vector<net_thermal_temp_alarm> merge_alarms_with_markers(
    const std::vector<net_marker_info>& markers, USHORT marker_count,
    const std::vector<net_thermal_temp_alarm>& alarms, USHORT alarm_count);
INT32 start_alarm_listener_and_wait(NET_HANDLE handle, const NetDemoOptions& options, std::ostream& out);

int run_alarm_commands(const std::string& command, NET_HANDLE handle,
                       const NetDemoOptions& options, std::ostream& out, std::ostream& err);
