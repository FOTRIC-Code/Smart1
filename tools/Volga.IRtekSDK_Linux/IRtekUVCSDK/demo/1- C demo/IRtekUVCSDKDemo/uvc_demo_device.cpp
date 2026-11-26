#include "uvc_demo_device.h"

#include <chrono>
#include <cstring>
#include <iostream>
#include <thread>

namespace
{
struct UpgradePollResult
{
    std::string message;
    bool is_success = false;
};

UpgradePollResult likely_completed_result()
{
    return {
        "Upgrade likely completed. Device may have restarted and disconnected. Please reconnect and verify the firmware version.",
        true
    };
}

UpgradePollResult* infer_success_after_disconnect(
    uvc_enum_upgrade_state last_state,
    bool saw_upgrade_progress,
    UpgradePollResult& storage)
{
    if (last_state == uvc_enum_upgrade_state::DONE)
    {
        storage = {
            "Upgrade completed. Device is restarting.",
            true
        };
        return &storage;
    }

    if (last_state == uvc_enum_upgrade_state::FLASHING ||
        last_state == uvc_enum_upgrade_state::STAGED ||
        saw_upgrade_progress)
    {
        storage = likely_completed_result();
        return &storage;
    }

    return nullptr;
}

bool try_recover_upgrade_state(
    HANDLE handle,
    int retry_count,
    int poll_interval_ms,
    int& elapsed_ms,
    uvc_enum_upgrade_state& state)
{
    for (int i = 0; i < retry_count; ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(poll_interval_ms));
        elapsed_ms += poll_interval_ms;
        const int rc = check_rc("uvc_get_upgrade_state(retry)", uvc_get_upgrade_state(handle, &state));
        if (is_ok(rc))
        {
            std::cout << "  upgrade_state: " << upgrade_state_to_string(state) << "\n";
            return true;
        }
    }

    state = uvc_enum_upgrade_state::IDLE;
    return false;
}

bool confirm_upgrade_failed(
    HANDLE handle,
    int retry_count,
    int poll_interval_ms,
    int& elapsed_ms,
    uvc_enum_upgrade_state& last_state,
    bool& saw_upgrade_progress,
    uvc_enum_upgrade_state& state)
{
    state = uvc_enum_upgrade_state::FAILED;
    for (int i = 0; i < retry_count; ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(poll_interval_ms));
        elapsed_ms += poll_interval_ms;
        const int rc = check_rc("uvc_get_upgrade_state(confirm)", uvc_get_upgrade_state(handle, &state));
        if (!is_ok(rc))
        {
            return false;
        }

        std::cout << "  upgrade_state: " << upgrade_state_to_string(state) << "\n";
        last_state = state;
        if (state != uvc_enum_upgrade_state::IDLE)
        {
            saw_upgrade_progress = true;
        }
        if (state == uvc_enum_upgrade_state::DONE || state != uvc_enum_upgrade_state::FAILED)
        {
            return false;
        }
    }

    return state == uvc_enum_upgrade_state::FAILED;
}

UpgradePollResult wait_for_upgrade_complete(HANDLE handle, int timeout_ms, int poll_interval_ms)
{
    const int retry_count = 6;
    int elapsed_ms = 0;
    uvc_enum_upgrade_state last_state = uvc_enum_upgrade_state::IDLE;
    bool saw_upgrade_progress = false;
    UpgradePollResult fallback_result;

    while (elapsed_ms < timeout_ms)
    {
        uvc_enum_upgrade_state state = uvc_enum_upgrade_state::IDLE;
        const int rc = check_rc("uvc_get_upgrade_state", uvc_get_upgrade_state(handle, &state));
        if (!is_ok(rc))
        {
            if (UpgradePollResult* inferred = infer_success_after_disconnect(last_state, saw_upgrade_progress, fallback_result))
            {
                return *inferred;
            }

            if (saw_upgrade_progress && try_recover_upgrade_state(handle, retry_count, poll_interval_ms, elapsed_ms, state))
            {
                last_state = state;
                if (state != uvc_enum_upgrade_state::IDLE)
                {
                    saw_upgrade_progress = true;
                }
            }
            else
            {
                if (saw_upgrade_progress)
                {
                    return likely_completed_result();
                }
                return { "Failed to query upgrade state after starting upgrade.", false };
            }
        }
        else
        {
            std::cout << "  upgrade_state: " << upgrade_state_to_string(state) << "\n";
        }

        last_state = state;
        if (state != uvc_enum_upgrade_state::IDLE)
        {
            saw_upgrade_progress = true;
        }

        switch (state)
        {
        case uvc_enum_upgrade_state::DONE:
            return { "Upgrade completed. Device will restart automatically.", true };

        case uvc_enum_upgrade_state::FAILED:
            if (confirm_upgrade_failed(handle, retry_count, poll_interval_ms, elapsed_ms, last_state, saw_upgrade_progress, state))
            {
                return { "Upgrade failed on device.", false };
            }
            if (state == uvc_enum_upgrade_state::DONE)
            {
                return { "Upgrade completed. Device will restart automatically.", true };
            }
            if (UpgradePollResult* inferred = infer_success_after_disconnect(last_state, saw_upgrade_progress, fallback_result))
            {
                return *inferred;
            }
            if (state != uvc_enum_upgrade_state::FAILED)
            {
                continue;
            }
            return likely_completed_result();

        case uvc_enum_upgrade_state::IDLE:
            if (elapsed_ms > 0)
            {
                if (saw_upgrade_progress)
                {
                    return likely_completed_result();
                }
                return { "Upgrade ended (device idle).", false };
            }
            break;

        default:
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(poll_interval_ms));
        elapsed_ms += poll_interval_ms;
    }

    return { "Upgrade timeout. Check device status manually.", false };
}

bool copy_path_to_fixed_bytes(const std::string& path, BYTE* buffer, size_t buffer_size)
{
    if (path.size() >= buffer_size)
    {
        std::cerr << "Firmware path is too long for SDK fixed buffer (" << buffer_size << " bytes).\n";
        return false;
    }

    std::memset(buffer, 0, buffer_size);
    std::memcpy(buffer, path.data(), path.size());
    return true;
}
} // namespace

bool run_calibrate(const DemoOptions& options)
{
    return with_logged_in_device(options, [&](HANDLE handle) {
        return check_rc("uvc_thermal_calibrate", uvc_thermal_calibrate(handle)) == UVC_EC_OK;
    });
}

bool run_upgrade(const DemoOptions& options)
{
    return with_logged_in_device(options, [&](HANDLE handle) {
        BYTE path_bytes[UVC_LEN_128] {};
        if (!copy_path_to_fixed_bytes(options.firmware_path, path_bytes, sizeof(path_bytes)))
        {
            return false;
        }

        const int rc = check_rc("uvc_device_upgrade", uvc_device_upgrade(handle, path_bytes, static_cast<USHORT>(sizeof(path_bytes))));
        if (!is_ok(rc))
        {
            return false;
        }

        const UpgradePollResult result = wait_for_upgrade_complete(
            handle,
            options.upgrade_timeout_ms,
            options.upgrade_poll_interval_ms);
        std::cout << result.message << "\n";
        return result.is_success;
    });
}

bool run_restart(const DemoOptions& options)
{
    return with_logged_in_device(options, [&](HANDLE handle) {
        return check_rc("uvc_device_restart", uvc_device_restart(handle)) == UVC_EC_OK;
    });
}

bool run_reset(const DemoOptions& options)
{
    return with_logged_in_device(options, [&](HANDLE handle) {
        return check_rc("uvc_device_reset", uvc_device_reset(handle)) == UVC_EC_OK;
    });
}
