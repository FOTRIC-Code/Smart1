#pragma once

#include "uvc_demo_common.h"
#include "uvc_frame_utils.h"

bool wait_for_frame(HANDLE handle, const uvc_frame_info& frame_info, int wait_ms, UvcParsedFrame& out_frame);
bool capture_single_frame(HANDLE handle, int wait_ms, UvcParsedFrame& captured);
