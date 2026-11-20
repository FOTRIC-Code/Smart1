#include "irtek_file_sdk_loader.h"

#include <cstdlib>
#include <dlfcn.h>

namespace
{
template <typename T>
bool bind_symbol(void* handle, const char* name, T& target, std::string* error_message)
{
    dlerror();
    target = reinterpret_cast<T>(dlsym(handle, name));
    const char* error = dlerror();
    if (error != nullptr || !target)
    {
        if (error_message)
        {
            *error_message = std::string("Missing symbol: ") + name;
        }
        return false;
    }
    return true;
}
}

IrtekFileSdkLoader::~IrtekFileSdkLoader()
{
    if (_handle)
    {
        dlclose(_handle);
        _handle = nullptr;
    }
}

bool IrtekFileSdkLoader::load(std::string* error_message)
{
    if (_handle)
    {
        return true;
    }

    std::string last_error;
    for (const std::string& candidate : candidate_paths())
    {
        void* handle = dlopen(candidate.c_str(), RTLD_LAZY | RTLD_LOCAL);
        if (!handle)
        {
            const char* dl_error = dlerror();
            if (dl_error)
            {
                last_error = dl_error;
            }
            continue;
        }

        _handle = handle;
        _loaded_path = candidate;
        if (bind_symbols(error_message))
        {
            return true;
        }

        dlclose(_handle);
        _handle = nullptr;
        _loaded_path.clear();
    }

    if (error_message)
    {
        *error_message = last_error.empty() ? "Unable to locate libIRtekFileSDK.so" : last_error;
    }
    return false;
}

bool IrtekFileSdkLoader::bind_symbols(std::string* error_message)
{
    return bind_symbol(_handle, "ir_file_create", ir_file_create, error_message)
        && bind_symbol(_handle, "ir_file_close", ir_file_close, error_message)
        && bind_symbol(_handle, "ir_file_save", ir_file_save, error_message)
        && bind_symbol(_handle, "ir_file_set_isp_type", ir_file_set_isp_type, error_message)
        && bind_symbol(_handle, "ir_file_set_thermal_size", ir_file_set_thermal_size, error_message)
        && bind_symbol(_handle, "ir_file_set_temp_range", ir_file_set_temp_range, error_message)
        && bind_symbol(_handle, "ir_file_set_thermal_params", ir_file_set_thermal_params, error_message)
        && bind_symbol(_handle, "ir_file_set_lut", ir_file_set_lut, error_message)
        && bind_symbol(_handle, "ir_file_set_camera_info", ir_file_set_camera_info, error_message)
        && bind_symbol(_handle, "ir_file_set_lens_info_v2", ir_file_set_lens_info_v2, error_message)
        && bind_symbol(_handle, "ir_file_set_palette", ir_file_set_palette, error_message)
        && bind_symbol(_handle, "ir_file_set_gain", ir_file_set_gain, error_message)
        && bind_symbol(_handle, "ir_file_set_alarm", ir_file_set_alarm, error_message)
        && bind_symbol(_handle, "ir_file_set_color_dist", ir_file_set_color_dist, error_message)
        && bind_symbol(_handle, "ir_file_append_frame", ir_file_append_frame, error_message)
        && bind_symbol(_handle, "ir_file_get_thermal_image_from_frame", ir_file_get_thermal_image_from_frame, error_message)
        && bind_symbol(_handle, "ir_file_get_temp_spot", ir_file_get_temp_spot, error_message);
}

std::vector<std::string> IrtekFileSdkLoader::candidate_paths()
{
    std::vector<std::string> paths;
    if (const char* env_path = std::getenv("IRTEKFILESDK_LIBRARY"))
    {
        if (*env_path)
        {
            paths.emplace_back(env_path);
        }
    }

    paths.emplace_back("libIRtekFileSDK.so");
    return paths;
}
