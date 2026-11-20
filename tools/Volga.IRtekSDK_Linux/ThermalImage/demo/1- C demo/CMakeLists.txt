cmake_minimum_required(VERSION 3.16)
project(ThermalFileReaderDemo CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(IRTEK_LINUX_VARIANT "ubuntu22" CACHE STRING "Packaged library variant")
set_property(CACHE IRTEK_LINUX_VARIANT PROPERTY STRINGS rk3506 ubuntu20 ubuntu22)

get_filename_component(THERMALIMAGE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(THERMALIMAGE_PACKAGE_ROOT "${THERMALIMAGE_ROOT}/library/${IRTEK_LINUX_VARIANT}" CACHE PATH
    "ThermalImage packaged library root")
set(IRTEKFILESDK_INCLUDE_DIR "${THERMALIMAGE_ROOT}/header")
set(IRTEKFILESDK_LIBRARY "${THERMALIMAGE_PACKAGE_ROOT}/library/libIRtekFileSDK.so")

if(NOT EXISTS "${IRTEKFILESDK_INCLUDE_DIR}/IRtekFileSDK.h")
    message(FATAL_ERROR "Missing IRtekFileSDK.h under ${IRTEKFILESDK_INCLUDE_DIR}")
endif()

if(NOT EXISTS "${IRTEKFILESDK_LIBRARY}")
    message(FATAL_ERROR "Missing libIRtekFileSDK.so under ${THERMALIMAGE_PACKAGE_ROOT}/library")
endif()

add_executable(thermal_file_reader_demo
    "${CMAKE_CURRENT_SOURCE_DIR}/ThermalFileReaderDemo/main.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/ThermalImage/thermal_file_commands.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/ThermalImage/thermal_file_demo.cpp"
)

target_include_directories(thermal_file_reader_demo
    PRIVATE
        "${IRTEKFILESDK_INCLUDE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/ThermalImage"
)

target_link_libraries(thermal_file_reader_demo PRIVATE "${IRTEKFILESDK_LIBRARY}")
