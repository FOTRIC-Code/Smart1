cmake_minimum_required(VERSION 3.16)
project(IRtekUVCSDKDemo CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(IRTEK_LINUX_VARIANT "ubuntu22" CACHE STRING "Packaged library variant")
set_property(CACHE IRTEK_LINUX_VARIANT PROPERTY STRINGS rk3506 ubuntu22)

get_filename_component(IRTEKUVCSDK_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(IRTEKUVCSDK_PACKAGE_ROOT "${IRTEKUVCSDK_ROOT}/library/${IRTEK_LINUX_VARIANT}" CACHE PATH
    "IRtekUVCSDK packaged library root")
set(IRTEKUVCSDK_INCLUDE_DIR "${IRTEKUVCSDK_ROOT}/header")
set(IRTEKUVCSDK_LIBRARY "${IRTEKUVCSDK_PACKAGE_ROOT}/library/libIRtekUVCSDK.so")

if(NOT EXISTS "${IRTEKUVCSDK_INCLUDE_DIR}/IRtekUVCSDK.h")
    message(FATAL_ERROR "Missing IRtekUVCSDK.h under ${IRTEKUVCSDK_INCLUDE_DIR}")
endif()

if(NOT EXISTS "${IRTEKUVCSDK_LIBRARY}")
    message(FATAL_ERROR "Missing libIRtekUVCSDK.so under ${IRTEKUVCSDK_PACKAGE_ROOT}/library")
endif()

add_executable(irtek_uvc_sdk_demo
    "${CMAKE_CURRENT_SOURCE_DIR}/IRtekUVCSDKDemo/main.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/IRtekUVCSDKDemo/uvc_demo_common.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/IRtekUVCSDKDemo/uvc_demo_connection.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/IRtekUVCSDKDemo/uvc_demo_stream_info.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/IRtekUVCSDKDemo/uvc_demo_params.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/IRtekUVCSDKDemo/uvc_demo_capture.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/IRtekUVCSDKDemo/uvc_demo_live_stream.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/IRtekUVCSDKDemo/uvc_demo_device.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/IRtekUVCSDK/irtek_file_sdk_loader.cpp"
)

target_include_directories(irtek_uvc_sdk_demo
    PRIVATE
        "${IRTEKUVCSDK_INCLUDE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/IRtekUVCSDK"
        "${CMAKE_CURRENT_SOURCE_DIR}/IRtekUVCSDKDemo"
)

target_link_libraries(irtek_uvc_sdk_demo PRIVATE "${IRTEKUVCSDK_LIBRARY}" dl)
