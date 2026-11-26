#include "net_demo_capture.h"

#include <iostream>

int run_capture_commands(const std::string& command, NET_HANDLE handle,
                         const NetDemoOptions& options, std::ostream& out, std::ostream& err)
{
    int exit_code = 0;

    if (command == "capture")
    {
        std::string output_path;
        if (!require_option_text(options, "output", err, output_path))
        {
            exit_code = 1;
        }
        else
        {
            const auto capture_mode = parse_capture_mode(get_value(options, "capture-mode").value_or("irtek"));
            if (!capture_mode.has_value())
            {
                err << "Invalid --capture-mode. Use irtek or pure.\n";
                exit_code = 1;
            }
            else
            {
                net_capture_file file{};
                copy_text(file.local_file, sizeof(file.local_file), output_path);
                const INT32 rc = net_get_thermal_capture(handle, *capture_mode, &file);
                log_call(out, "net_get_thermal_capture", rc, "output=" + output_path);
                exit_code = rc == NET_EC_OK ? 0 : 1;
            }
        }
    }

    return exit_code;
}
