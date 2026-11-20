#pragma once

#include "net_demo_common.h"

void print_thermal_params_core(NET_HANDLE handle, std::ostream& out);
void print_thermal_ranges(NET_HANDLE handle, std::ostream& out);
void print_thermal_color_dist(NET_HANDLE handle, std::ostream& out);
void print_thermal_gain(NET_HANDLE handle, std::ostream& out);
void print_thermal_palette(NET_HANDLE handle, std::ostream& out);
void print_thermal_overlay(const net_thermal_overlay_info& info, std::ostream& out);
void print_thermal_osd_get(NET_HANDLE handle, std::ostream& out);

void print_time_bundle(NET_HANDLE handle, std::ostream& out);
void print_network_bundle(NET_HANDLE handle, std::ostream& out);
void print_users(NET_HANDLE handle, std::ostream& out);

void print_display_get(NET_HANDLE handle, const NetDemoOptions& options, std::ostream& out);
void print_markers(NET_HANDLE handle, std::ostream& out);

std::string next_marker_name(const std::vector<net_marker_info>& markers, USHORT count, char prefix);
bool push_markers(NET_HANDLE handle, std::vector<net_marker_info>& markers, USHORT count, std::ostream& out);

int run_params_commands(const std::string& command, NET_HANDLE handle,
                        const NetDemoOptions& options, std::ostream& out, std::ostream& err);
