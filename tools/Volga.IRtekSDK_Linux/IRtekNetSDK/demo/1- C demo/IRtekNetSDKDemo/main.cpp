#include "net_demo_common.h"
#include "net_demo_connection.h"
#include "net_demo_stream_info.h"
#include "net_demo_params.h"
#include "net_demo_capture.h"
#include "net_demo_record.h"
#include "net_demo_live_stream.h"
#include "net_demo_alarm.h"

#include <iostream>
#include <unordered_set>

// ---------------------------------------------------------------------------
// CLI: usage and argument parsing
// ---------------------------------------------------------------------------

static void print_net_demo_usage(const char* argv0, std::ostream& os)
{
    os
        << "Usage: " << argv0 << " <command> [options]\n"
        << "\n"
        << "Core commands:\n"
        << "  search\n"
        << "  connect-flow\n"
        << "  properties-get\n"
        << "  capture\n"
        << "  record-stream\n"
        << "  thermal-data-get\n"
        << "  thermal-lut-get\n"
        << "  thermal-params-get | thermal-params-set\n"
        << "  thermal-osd-get | thermal-osd-set\n"
        << "  thermal-calibrate\n"
        << "  time-get | time-sync | time-apply | ntp-apply\n"
        << "  network-refresh | network-apply | ftp-apply | ftp-test | port-apply\n"
        << "  webhook-get | webhook-apply\n"
        << "  users-get | user-add | user-update | user-delete\n"
        << "  display-get | display-set | channel-set\n"
        << "  device-name-set | device-upgrade | device-restart | device-reset\n"
        << "  markers-get | marker-add-spot | marker-add-rectangle | marker-add-line | marker-delete\n"
        << "  alarm-get | alarm-set | alarm-listen\n"
        << "\n"
        << "Common options:\n"
        << "  --ip <address>          Login target IP\n"
        << "  --port <port>           Login port, default 80\n"
        << "  --user <name>           Login user, default admin\n"
        << "  --password <pwd>        Login password, default admin\n"
        << "  --search-timeout-ms <n> Discovery timeout, default 2000\n"
        << "  --stream-id <id>        Selected stream id for connect/record/probe\n"
        << "  --stream-type <type>    bgra | rgba | yuv422 | h264\n"
        << "  --enable-log            Enable SDK logging\n"
        << "  --help                  Show this help\n"
        << "\n"
        << "Legacy compatibility:\n"
        << "  no command + no --ip            => search\n"
        << "  no command + --ip               => properties-get\n"
        << "  no command + --thermal-snapshot => thermal-data-get\n"
        << "  no command + --listen-alarm     => alarm-listen\n";
}

static bool parse_net_demo_args(int argc, char** argv, NetDemoOptions& options,
                                bool& help_requested, std::ostream& err)
{
    help_requested = false;
    options = NetDemoOptions{};

    static const std::unordered_set<std::string> flag_only = {
        "enable-log",
        "help",
        "h",
        "thermal-snapshot",
        "listen-alarm",
        "thermal-stream-all"
    };

    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (!arg.empty() && arg[0] != '-')
        {
            if (options.command.empty())
            {
                if (!is_known_command(arg))
                {
                    err << "Unknown command: " << arg << "\n";
                    return false;
                }
                options.command = arg;
                continue;
            }

            err << "Unexpected positional argument: " << arg << "\n";
            return false;
        }

        const std::string key = flag_key(arg);
        if (flag_only.find(key) != flag_only.end())
        {
            options.flags.insert(key);
            if (key == "enable-log")
            {
                options.enable_log = true;
            }
            else if (key == "help" || key == "h")
            {
                help_requested = true;
                return false;
            }
            else if (key == "thermal-snapshot")
            {
                options.thermal_snapshot = true;
            }
            else if (key == "listen-alarm")
            {
                options.listen_alarm = true;
            }
            else if (key == "thermal-stream-all")
            {
                options.thermal_stream_all = true;
            }
            continue;
        }

        if (i + 1 >= argc)
        {
            err << "Missing value for argument: " << arg << "\n";
            return false;
        }

        options.values[key] = argv[++i];
        if (key == "ip")
        {
            options.ip = options.values[key];
        }
        else if (key == "port")
        {
            const auto parsed = parse_integral<unsigned short>(options.values[key]);
            if (!parsed.has_value())
            {
                err << "Invalid value for --port\n";
                return false;
            }
            options.port = *parsed;
        }
        else if (key == "user")
        {
            options.user_name = options.values[key];
        }
        else if (key == "password")
        {
            options.password = options.values[key];
        }
        else if (key == "search-timeout-ms")
        {
            const auto parsed = parse_integral<unsigned int>(options.values[key]);
            if (!parsed.has_value())
            {
                err << "Invalid value for --search-timeout-ms\n";
                return false;
            }
            options.search_timeout_ms = *parsed;
        }
        else if (key == "alarm-wait-ms")
        {
            const auto parsed = parse_integral<unsigned int>(options.values[key]);
            if (!parsed.has_value())
            {
                err << "Invalid value for --alarm-wait-ms\n";
                return false;
            }
            options.alarm_wait_ms = *parsed;
        }
        else if (key == "stream-wait-ms")
        {
            const auto parsed = parse_integral<unsigned int>(options.values[key]);
            if (!parsed.has_value())
            {
                err << "Invalid value for --stream-wait-ms\n";
                return false;
            }
            options.stream_wait_ms = *parsed;
        }
        else if (key == "thermal-wait-ms")
        {
            const auto parsed = parse_integral<unsigned int>(options.values[key]);
            if (!parsed.has_value())
            {
                err << "Invalid value for --thermal-wait-ms\n";
                return false;
            }
            options.thermal_wait_ms = *parsed;
        }
        else if (key == "record-wait-ms")
        {
            const auto parsed = parse_integral<unsigned int>(options.values[key]);
            if (!parsed.has_value())
            {
                err << "Invalid value for --record-wait-ms\n";
                return false;
            }
            options.record_wait_ms = *parsed;
        }
    }

    if (options.command.empty())
    {
        if (options.thermal_snapshot)
        {
            options.command = "thermal-data-get";
        }
        else if (options.listen_alarm)
        {
            options.command = "alarm-listen";
        }
        else if (options.ip.empty())
        {
            options.command = "search";
        }
        else
        {
            options.command = "properties-get";
        }
    }

    return true;
}

// ---------------------------------------------------------------------------
// Command dispatch
// ---------------------------------------------------------------------------

static int run_irtek_net_demo(const NetDemoOptions& options, std::ostream& out, std::ostream& err)
{
    if (options.command == "search")
    {
        DemoSession session(out);
        if (!session.init(options))
        {
            return 1;
        }

        std::vector<net_discovered_device> devices;
        USHORT found = 0;
        const INT32 search_rc = discover_devices(options, devices, found, out);
        session.close(true);
        return search_rc == NET_EC_OK ? 0 : 1;
    }

    if (command_requires_login(options.command) && options.ip.empty())
    {
        err << "Command '" << options.command << "' requires --ip <address>\n";
        return 1;
    }

    DemoSession session(out);
    if (!session.init(options))
    {
        return 1;
    }
    if (!session.login(options))
    {
        session.close(true);
        return 1;
    }
    session.set_exception_callback();

    const std::string& command = options.command;
    int exit_code = 0;

    if (command == "connect-flow" || command == "properties-get" ||
        command == "device-restart" || command == "device-reset" ||
        command == "device-name-set" || command == "device-upgrade")
    {
        exit_code = run_connection_commands(command, session.handle(), options, out, err);
    }
    else if (command == "capture")
    {
        exit_code = run_capture_commands(command, session.handle(), options, out, err);
    }
    else if (command == "record-stream")
    {
        exit_code = run_record_commands(command, session.handle(), options, out, err);
    }
    else if (command == "thermal-data-get" || command == "thermal-lut-get")
    {
        exit_code = run_live_stream_commands(command, session.handle(), options, out, err);
    }
    else if (command == "alarm-get" || command == "alarm-set" || command == "alarm-listen")
    {
        exit_code = run_alarm_commands(command, session.handle(), options, out, err);
    }
    else
    {
        exit_code = run_params_commands(command, session.handle(), options, out, err);
    }

    session.close(true);
    return exit_code;
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

int main(int argc, char** argv)
{
    NetDemoOptions options;
    bool help_requested = false;
    if (!parse_net_demo_args(argc, argv, options, help_requested, std::cerr))
    {
        print_net_demo_usage(argv[0], help_requested ? std::cout : std::cerr);
        return help_requested ? 0 : 1;
    }

    return run_irtek_net_demo(options, std::cout, std::cerr);
}
