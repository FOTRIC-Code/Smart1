#include "thermal_file_demo.h"

#include <iostream>

int main(int argc, char** argv)
{
    ThermalDemoOptions options;
    bool help_requested = false;
    if (!parse_thermal_demo_args(argc, argv, options, help_requested, std::cerr))
    {
        print_thermal_demo_usage(argv[0], help_requested ? std::cout : std::cerr);
        return help_requested ? 0 : 1;
    }

    return run_thermal_file_demo(options, std::cout, std::cerr);
}
