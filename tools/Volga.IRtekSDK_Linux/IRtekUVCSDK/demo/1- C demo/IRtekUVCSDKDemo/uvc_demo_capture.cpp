#include "uvc_demo_capture.h"
#include "uvc_demo_live_stream.h"
#include "irtek_file_sdk_loader.h"
#include "uvc_frame_utils.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>

namespace
{
template <size_t DestSize>
void copy_fixed_string(const BYTE* src, size_t src_size, IrFileByte (&dest)[DestSize])
{
    std::memset(dest, 0, DestSize);
    const size_t count = std::min(src_size, DestSize - 1);
    std::memcpy(dest, src, count);
}

IrFileULongLong now_utc_millis()
{
    const auto now = std::chrono::system_clock::now();
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
    return static_cast<IrFileULongLong>(millis.count());
}

bool set_file_lut(IrtekFileSdkLoader& loader, HANDLE handle, IrFileHandle file_handle, const UvcParsedFrame& frame)
{
    std::vector<USHORT> ad_values;
    std::vector<float> temp_values;

    if (frame.has_lut && frame.lut_ad.size() == frame.lut_temp.size() && !frame.lut_ad.empty())
    {
        ad_values = frame.lut_ad;
        temp_values = frame.lut_temp;
        std::cout << "capture_lut_source => frame_metadata\n";
    }
    else
    {
        std::cout << "capture_lut_source => fallback_uvc_get_thermal_lut\n";
        uvc_lut_table lut {};
        if (!query_lut(handle, lut) || lut.lut_count == 0)
        {
            return false;
        }
        ad_values.assign(lut.ad, lut.ad + lut.lut_count);
        temp_values.assign(lut.t, lut.t + lut.lut_count);
    }

    IrFileLut file_lut {};
    file_lut.radMethod = 0x01;
    file_lut.count = static_cast<IrFileUInt>(ad_values.size());
    file_lut.adArray = ad_values.data();
    file_lut.tempArray = temp_values.data();
    return check_file_rc("ir_file_set_lut", loader.ir_file_set_lut(file_handle, &file_lut)) == IR_FILE_EC_OK;
}

bool set_file_temp_range(IrtekFileSdkLoader& loader, HANDLE handle, IrFileHandle file_handle)
{
    USHORT range_id = 0;
    if (!query_current_range(handle, range_id))
    {
        return false;
    }

    uvc_lens_info lens_info {};
    if (!query_lens_info(handle, lens_info))
    {
        return false;
    }

    for (USHORT i = 0; i < lens_info.ranges_count && i < 4; ++i)
    {
        if (lens_info.range_map[i].id == range_id)
        {
            IrFileTempRange temp_range {
                lens_info.range_map[i].range.max,
                lens_info.range_map[i].range.min
            };
            return check_file_rc("ir_file_set_temp_range", loader.ir_file_set_temp_range(file_handle, &temp_range)) == IR_FILE_EC_OK;
        }
    }

    std::cerr << "Current range id was not found in lens info.\n";
    return false;
}

bool set_file_camera_info(IrtekFileSdkLoader& loader, HANDLE handle, IrFileHandle file_handle)
{
    uvc_device_info device_info {};
    if (!query_device_info(handle, device_info))
    {
        return false;
    }

    IrFileCameraInfo camera_info {};
    copy_fixed_string(device_info.model, sizeof(device_info.model), camera_info.model);
    copy_fixed_string(device_info.serial_no, sizeof(device_info.serial_no), camera_info.serialNo);
    copy_fixed_string(device_info.brand, sizeof(device_info.brand), camera_info.brand);
    copy_fixed_string(device_info.company, sizeof(device_info.company), camera_info.company);
    camera_info.logoLength = 0;
    camera_info.logo = nullptr;
    return check_file_rc("ir_file_set_camera_info", loader.ir_file_set_camera_info(file_handle, &camera_info)) == IR_FILE_EC_OK;
}

bool set_file_lens_info(IrtekFileSdkLoader& loader, HANDLE handle, IrFileHandle file_handle)
{
    uvc_lens_info lens_info {};
    if (!query_lens_info(handle, lens_info))
    {
        return false;
    }

    IrFileLensInfoV2 file_lens_info {};
    copy_fixed_string(lens_info.name, sizeof(lens_info.name), file_lens_info.name);
    copy_fixed_string(lens_info.band, sizeof(lens_info.band), file_lens_info.band);
    file_lens_info.focalLength = lens_info.focal;
    file_lens_info.hfov = lens_info.hfov;
    return check_file_rc("ir_file_set_lens_info_v2", loader.ir_file_set_lens_info_v2(file_handle, &file_lens_info)) == IR_FILE_EC_OK;
}

bool set_file_thermal_params(IrtekFileSdkLoader& loader, HANDLE handle, IrFileHandle file_handle)
{
    uvc_thermal_params thermal_params {};
    if (!query_thermal_params(handle, thermal_params))
    {
        return false;
    }

    IrFileThermalParams file_thermal_params {
        thermal_params.refl_temp,
        thermal_params.atm_temp,
        thermal_params.distance,
        thermal_params.rel_humidity,
        thermal_params.emissivity,
        293.15f,
        1.0f
    };
    return check_file_rc("ir_file_set_thermal_params", loader.ir_file_set_thermal_params(file_handle, &file_thermal_params)) == IR_FILE_EC_OK;
}

bool render_with_irtek_file_sdk(
    const UvcParsedFrame& frame,
    const uvc_thermal_params& thermal_params,
    const uvc_lens_info& lens_info,
    USHORT range_id,
    const std::string& output_path,
    float& out_center_kelvin)
{
    if (!frame.has_lut || frame.raw_pixels.empty())
    {
        return false;
    }

    IrtekFileSdkLoader loader;
    std::string loader_error;
    if (!loader.load(&loader_error))
    {
        std::cerr << "IRtekFileSDK runtime not available: " << loader_error << "\n";
        return false;
    }

    std::cout << "Loaded IRtekFileSDK from " << loader.loaded_path() << "\n";

    IrFileHandle file_handle = nullptr;
    if (loader.ir_file_create("__preview__.irs", IR_FILE_THERMAL_PICTURE, &file_handle) != IR_FILE_EC_OK || !file_handle)
    {
        std::cerr << "ir_file_create failed\n";
        return false;
    }

    bool ok = false;
    do
    {
        IrFileSize size {
            static_cast<IrFileShort>(frame.width),
            static_cast<IrFileShort>(frame.image_height)
        };
        if (loader.ir_file_set_isp_type(file_handle, IR_FILE_HOLLY) != IR_FILE_EC_OK ||
            loader.ir_file_set_thermal_size(file_handle, &size) != IR_FILE_EC_OK)
        {
            break;
        }

        for (USHORT i = 0; i < lens_info.ranges_count && i < 4; ++i)
        {
            if (lens_info.range_map[i].id == range_id)
            {
                IrFileTempRange temp_range {
                    lens_info.range_map[i].range.max,
                    lens_info.range_map[i].range.min
                };
                loader.ir_file_set_temp_range(file_handle, &temp_range);
                break;
            }
        }

        IrFileThermalParams file_params {
            thermal_params.refl_temp,
            thermal_params.atm_temp,
            thermal_params.distance,
            thermal_params.rel_humidity,
            thermal_params.emissivity,
            293.15f,
            1.0f
        };
        loader.ir_file_set_thermal_params(file_handle, &file_params);

        IrFileLut lut {};
        lut.radMethod = 0x01;
        lut.count = static_cast<IrFileUInt>(frame.lut_ad.size());
        lut.adArray = const_cast<IrFileUShort*>(frame.lut_ad.data());
        lut.tempArray = const_cast<float*>(frame.lut_temp.data());
        if (loader.ir_file_set_lut(file_handle, &lut) != IR_FILE_EC_OK)
        {
            break;
        }

        IrFileFrame input_frame {};
        input_frame.timeStamp = frame.pts / 1000ULL;
        input_frame.length = static_cast<IrFileUInt>(frame.raw_pixels.size());
        input_frame.buffer = const_cast<IrFileUShort*>(frame.raw_pixels.data());

        std::vector<uint8_t> bgr(static_cast<size_t>(frame.width) * frame.image_height * 3);
        IrFileImage image {};
        image.offset = 0;
        image.data = bgr.data();
        if (loader.ir_file_get_thermal_image_from_frame(file_handle, &input_frame, IR_FILE_BGR, &image) != IR_FILE_EC_OK)
        {
            break;
        }

        IrFileTempPoint center {};
        center.point.x = static_cast<IrFileShort>(frame.width / 2);
        center.point.y = static_cast<IrFileShort>(frame.image_height / 2);
        center.localParams = 0;
        center.reflTemp = thermal_params.refl_temp;
        center.distance = thermal_params.distance;
        center.emissivity = thermal_params.emissivity;
        if (loader.ir_file_get_temp_spot(file_handle, &center) == IR_FILE_EC_OK)
        {
            out_center_kelvin = center.temp;
        }

        ok = save_ppm(output_path, frame.width, frame.image_height, bgr);
    } while (false);

    loader.ir_file_close(file_handle);
    std::remove("__preview__.irs");
    return ok;
}

bool save_thermal_image_as_ir_file(
    HANDLE handle,
    const UvcParsedFrame& frame,
    const std::string& output_path)
{
    IrtekFileSdkLoader loader;
    std::string loader_error;
    if (!loader.load(&loader_error))
    {
        std::cerr << "IRtekFileSDK runtime not available: " << loader_error << "\n";
        return false;
    }

    std::cout << "Loaded IRtekFileSDK from " << loader.loaded_path() << "\n";

    IrFileHandle file_handle = nullptr;
    if (check_file_rc("ir_file_create", loader.ir_file_create(output_path.c_str(), IR_FILE_THERMAL_PICTURE, &file_handle)) != IR_FILE_EC_OK ||
        !file_handle)
    {
        return false;
    }

    bool ok = false;
    do
    {
        IrFileSize size {
            static_cast<IrFileShort>(frame.width),
            static_cast<IrFileShort>(frame.image_height)
        };
        if (check_file_rc("ir_file_set_thermal_size", loader.ir_file_set_thermal_size(file_handle, &size)) != IR_FILE_EC_OK)
        {
            break;
        }

        if (check_file_rc("ir_file_set_isp_type", loader.ir_file_set_isp_type(file_handle, IR_FILE_HOLLY)) != IR_FILE_EC_OK)
        {
            break;
        }

        if (check_file_rc("ir_file_set_color_dist", loader.ir_file_set_color_dist(file_handle, IR_FILE_AD_LINEAR)) != IR_FILE_EC_OK)
        {
            break;
        }

        if (!set_file_lut(loader, handle, file_handle, frame))
        {
            break;
        }

        if (!set_file_temp_range(loader, handle, file_handle))
        {
            break;
        }

        if (!set_file_camera_info(loader, handle, file_handle))
        {
            break;
        }

        if (!set_file_lens_info(loader, handle, file_handle))
        {
            break;
        }

        if (!set_file_thermal_params(loader, handle, file_handle))
        {
            break;
        }

        IrFilePalette palette {};
        palette.refNo = kDefaultPaletteRefNo;
        palette.inverted = kDefaultPaletteInverted;
        if (check_file_rc("ir_file_set_palette", loader.ir_file_set_palette(file_handle, &palette)) != IR_FILE_EC_OK)
        {
            break;
        }

        IrFileAlarm alarm {};
        alarm.type = IR_FILE_NORMAL;
        alarm.custom = 0;
        alarm.color = 0x7FFF0000;
        alarm.max = kDefaultAlarmMaxKelvin;
        alarm.min = kDefaultAlarmMinKelvin;
        alarm.rhLimit = 1.0f;
        alarm.indoorTemp = 293.15f;
        alarm.outdoorTemp = 283.15f;
        alarm.heatIndex = 0.8f;
        if (check_file_rc("ir_file_set_alarm", loader.ir_file_set_alarm(file_handle, &alarm)) != IR_FILE_EC_OK)
        {
            break;
        }

        IrFileGain gain {};
        gain.mode = IR_FILE_GAIN_AUTO;
        gain.min = kDefaultGainMinKelvin;
        gain.max = kDefaultGainMaxKelvin;
        gain.fixedSpan = 0;
        gain.span = 2.0f;
        if (check_file_rc("ir_file_set_gain", loader.ir_file_set_gain(file_handle, &gain)) != IR_FILE_EC_OK)
        {
            break;
        }

        IrFileFrame input_frame {};
        input_frame.timeStamp = now_utc_millis();
        input_frame.length = static_cast<IrFileUInt>(frame.raw_pixels.size());
        input_frame.buffer = const_cast<IrFileUShort*>(frame.raw_pixels.data());
        if (check_file_rc("ir_file_append_frame", loader.ir_file_append_frame(file_handle, &input_frame)) != IR_FILE_EC_OK)
        {
            break;
        }

        std::cout << "ir_file_set_cover_image => skipped (GUI-only bitmap source)\n";
        if (check_file_rc("ir_file_save", loader.ir_file_save(file_handle)) != IR_FILE_EC_OK)
        {
            break;
        }

        ok = true;
    } while (false);

    check_file_rc("ir_file_close", loader.ir_file_close(file_handle));
    if (ok)
    {
        std::cout << "Saved thermal image to " << output_path << "\n";
    }
    return ok;
}
} // namespace

bool run_capture(const DemoOptions& options)
{
    return with_logged_in_device(options, [&](HANDLE handle) {
        std::cout << "\n== Capture frame ==\n";
        UvcParsedFrame captured;
        if (!capture_single_frame(handle, options.wait_ms, captured))
        {
            return false;
        }

        std::cout << "\n== SaveThermalImageAsIrFile ==\n";
        if (!save_thermal_image_as_ir_file(handle, captured, options.output_path))
        {
            return false;
        }

        if (!options.preview_output_path.empty())
        {
            std::cout << "\n== Render preview ==\n";
            uvc_thermal_params thermal_params {};
            uvc_lens_info lens_info {};
            USHORT range_id = 0;
            if (!query_thermal_params(handle, thermal_params) ||
                !query_lens_info(handle, lens_info) ||
                !query_current_range(handle, range_id))
            {
                return false;
            }

            float rendered_center_kelvin = std::numeric_limits<float>::quiet_NaN();
            if (!render_with_irtek_file_sdk(captured, thermal_params, lens_info, range_id, options.preview_output_path, rendered_center_kelvin))
            {
                std::cerr << "Preview render failed.\n";
                return false;
            }

            std::cout << "Saved rendered preview to " << options.preview_output_path << "\n";
            if (!std::isnan(rendered_center_kelvin))
            {
                std::cout << "  center_temperature_filesdk: " << kelvin_to_celsius(rendered_center_kelvin) << " C\n";
            }
        }

        return true;
    });
}
