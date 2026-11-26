#pragma once

#include <cstdint>
#include <string>
#include <vector>

using IrFileHandle = void*;
using IrFileByte = unsigned char;
using IrFileShort = short;
using IrFileUShort = unsigned short;
using IrFileInt = int;
using IrFileUInt = unsigned int;
using IrFileULongLong = unsigned long long;

enum : IrFileInt
{
    IR_FILE_EC_OK = 0,
    IR_FILE_EC_FAIL = -1
};

enum : IrFileInt
{
    IR_FILE_THERMAL_PICTURE = 1,
    IR_FILE_BGR = 1,
    IR_FILE_RGBA = 2,
    IR_FILE_HOLLY = 1,
    IR_FILE_GAIN_AUTO = 0,
    IR_FILE_NORMAL = 0,
    IR_FILE_AD_LINEAR = 3
};

#pragma pack(push, 8)

struct IrFileSize
{
    IrFileShort width;
    IrFileShort height;
};

struct IrFileTempRange
{
    float max;
    float min;
};

struct IrFileThermalParams
{
    float reflTemp;
    float atmTemp;
    float distance;
    float relHumidity;
    float emissivity;
    float extOptTemp;
    float extOptTrans;
};

struct IrFileLut
{
    IrFileByte radMethod;
    IrFileUInt count;
    IrFileUShort* adArray;
    float* tempArray;
};

struct IrFileFrame
{
    IrFileULongLong timeStamp;
    IrFileUInt length;
    IrFileUShort* buffer;
};

struct IrFilePoint
{
    IrFileShort x;
    IrFileShort y;
};

struct IrFileTempPoint
{
    IrFilePoint point;
    IrFileByte localParams;
    float reflTemp;
    float distance;
    float emissivity;
    float temp;
};

struct IrFileImage
{
    IrFileUInt offset;
    IrFileByte* data;
};

struct IrFileCameraInfo
{
    IrFileByte model[32];
    IrFileByte serialNo[48];
    IrFileByte brand[32];
    IrFileByte company[64];
    IrFileUInt logoLength;
    IrFileByte* logo;
};

struct IrFileLensInfoV2
{
    IrFileByte name[32];
    float focalLength;
    float hfov;
    IrFileByte band[32];
};

struct IrFilePalette
{
    IrFileByte refNo;
    IrFileUInt colors[256];
    IrFileByte inverted;
};

struct IrFileGain
{
    IrFileInt mode;
    float max;
    float min;
    IrFileByte fixedSpan;
    float span;
};

struct IrFileAlarm
{
    IrFileInt type;
    IrFileByte custom;
    IrFileUInt color;
    float max;
    float min;
    float rhLimit;
    float indoorTemp;
    float outdoorTemp;
    float heatIndex;
};

#pragma pack(pop)

class IrtekFileSdkLoader
{
public:
    using FnIrFileCreate = IrFileInt (*)(const char*, IrFileInt, IrFileHandle*);
    using FnIrFileClose = IrFileInt (*)(IrFileHandle);
    using FnIrFileSave = IrFileInt (*)(IrFileHandle);
    using FnIrFileSetIspType = IrFileInt (*)(IrFileHandle, IrFileInt);
    using FnIrFileSetThermalSize = IrFileInt (*)(IrFileHandle, const IrFileSize*);
    using FnIrFileSetTempRange = IrFileInt (*)(IrFileHandle, const IrFileTempRange*);
    using FnIrFileSetThermalParams = IrFileInt (*)(IrFileHandle, const IrFileThermalParams*);
    using FnIrFileSetLut = IrFileInt (*)(IrFileHandle, const IrFileLut*);
    using FnIrFileSetCameraInfo = IrFileInt (*)(IrFileHandle, const IrFileCameraInfo*);
    using FnIrFileSetLensInfoV2 = IrFileInt (*)(IrFileHandle, const IrFileLensInfoV2*);
    using FnIrFileSetPalette = IrFileInt (*)(IrFileHandle, const IrFilePalette*);
    using FnIrFileSetGain = IrFileInt (*)(IrFileHandle, IrFileGain*);
    using FnIrFileSetAlarm = IrFileInt (*)(IrFileHandle, const IrFileAlarm*);
    using FnIrFileSetColorDist = IrFileInt (*)(IrFileHandle, IrFileInt);
    using FnIrFileAppendFrame = IrFileInt (*)(IrFileHandle, const IrFileFrame*);
    using FnIrFileGetThermalImageFromFrame =
        IrFileInt (*)(IrFileHandle, const IrFileFrame*, IrFileInt, IrFileImage*);
    using FnIrFileGetTempSpot = IrFileInt (*)(IrFileHandle, IrFileTempPoint*);

    ~IrtekFileSdkLoader();

    bool load(std::string* error_message = nullptr);
    const std::string& loaded_path() const { return _loaded_path; }

    FnIrFileCreate ir_file_create = nullptr;
    FnIrFileClose ir_file_close = nullptr;
    FnIrFileSave ir_file_save = nullptr;
    FnIrFileSetIspType ir_file_set_isp_type = nullptr;
    FnIrFileSetThermalSize ir_file_set_thermal_size = nullptr;
    FnIrFileSetTempRange ir_file_set_temp_range = nullptr;
    FnIrFileSetThermalParams ir_file_set_thermal_params = nullptr;
    FnIrFileSetLut ir_file_set_lut = nullptr;
    FnIrFileSetCameraInfo ir_file_set_camera_info = nullptr;
    FnIrFileSetLensInfoV2 ir_file_set_lens_info_v2 = nullptr;
    FnIrFileSetPalette ir_file_set_palette = nullptr;
    FnIrFileSetGain ir_file_set_gain = nullptr;
    FnIrFileSetAlarm ir_file_set_alarm = nullptr;
    FnIrFileSetColorDist ir_file_set_color_dist = nullptr;
    FnIrFileAppendFrame ir_file_append_frame = nullptr;
    FnIrFileGetThermalImageFromFrame ir_file_get_thermal_image_from_frame = nullptr;
    FnIrFileGetTempSpot ir_file_get_temp_spot = nullptr;

private:
    void* _handle = nullptr;
    std::string _loaded_path;

    bool bind_symbols(std::string* error_message);
    static std::vector<std::string> candidate_paths();
};
