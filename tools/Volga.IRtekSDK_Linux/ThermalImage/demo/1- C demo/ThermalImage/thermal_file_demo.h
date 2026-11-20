#pragma once

#include <iosfwd>
#include <string>
#include <vector>

struct ThermalDemoOptions
{
    std::string input_path;
    std::string output_path = "thermal_preview.ppm";
    int frame_index = -1;
    bool export_image = true;
    std::string command = "demo";
    std::vector<std::string> command_args;
};

void print_thermal_demo_usage(const char* argv0, std::ostream& os);
bool parse_thermal_demo_args(int argc, char** argv, ThermalDemoOptions& options, bool& help_requested, std::ostream& err);
int run_thermal_file_demo_aggregate(const ThermalDemoOptions& options, std::ostream& out, std::ostream& err);
int run_thermal_file_demo(const ThermalDemoOptions& options, std::ostream& out, std::ostream& err);
