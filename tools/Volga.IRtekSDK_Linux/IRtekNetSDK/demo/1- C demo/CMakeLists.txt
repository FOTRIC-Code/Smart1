cmake_minimum_required(VERSION 3.16)
project(IRtekNetSDKDemo CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(IRTEK_LINUX_VARIANT "ubuntu22" CACHE STRING "Packaged library variant")
set_property(CACHE IRTEK_LINUX_VARIANT PROPERTY STRINGS rk3506 ubuntu22)

get_filename_component(IRTEKNETSDK_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(IRTEKNETSDK_PACKAGE_ROOT "${IRTEKNETSDK_ROOT}/library/${IRTEK_LINUX_VARIANT}" CACHE PATH
    "IRtekNetSDK packaged library root")
set(IRTEKNETSDK_INCLUDE_DIR "${IRTEKNETSDK_ROOT}/header")
set(IRTEKNETSDK_LIBRARY "${IRTEKNETSDK_PACKAGE_ROOT}/library/libIRtekNetSDK.so")

if(NOT EXISTS "${IRTEKNETSDK_INCLUDE_DIR}/IRtekNetSDK.h")
    message(FATAL_ERROR "Missing IRtekNetSDK.h under ${IRTEKNETSDK_INCLUDE_DIR}")
endif()

if(NOT EXISTS "${IRTEKNETSDK_LIBRARY}")
    message(FATAL_ERROR "Missing libIRtekNetSDK.so under ${IRTEKNETSDK_PACKAGE_ROOT}/library")
endif()

find_package(Threads REQUIRED)

add_executable(irtek_net_sdk_demo
    "${CMAKE_CURRENT_SOURCE_DIR}/IRtekNetSDKDemo/main.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/IRtekNetSDK/net_demo_common.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/IRtekNetSDK/net_demo_connection.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/IRtekNetSDK/net_demo_stream_info.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/IRtekNetSDK/net_demo_params.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/IRtekNetSDK/net_demo_capture.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/IRtekNetSDK/net_demo_record.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/IRtekNetSDK/net_demo_live_stream.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/IRtekNetSDK/net_demo_alarm.cpp"
)

target_include_directories(irtek_net_sdk_demo
    PRIVATE
        "${IRTEKNETSDK_INCLUDE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/IRtekNetSDK"
)

target_link_libraries(irtek_net_sdk_demo
    PRIVATE
        "${IRTEKNETSDK_LIBRARY}"
        Threads::Threads
        ${CMAKE_DL_LIBS}
)
