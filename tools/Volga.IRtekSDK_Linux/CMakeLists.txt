cmake_minimum_required(VERSION 3.16)
project(IRtekSDKLinuxDemos NONE)

option(BUILD_THERMALIMAGE_DEMO "Configure ThermalImage Linux demo" OFF)
option(BUILD_IRTEKUVCSDK_DEMO "Configure IRtekUVCSDK Linux demo" OFF)
option(BUILD_IRTEKNETSDK_DEMO "Configure IRtekNetSDK Linux demo" OFF)

if(BUILD_THERMALIMAGE_DEMO OR BUILD_IRTEKUVCSDK_DEMO OR BUILD_IRTEKNETSDK_DEMO)
    enable_language(CXX)
endif()

if(BUILD_THERMALIMAGE_DEMO)
    add_subdirectory("ThermalImage/demo/1- C demo")
endif()

if(BUILD_IRTEKUVCSDK_DEMO)
    add_subdirectory("IRtekUVCSDK/demo/1- C++ demo")
endif()

if(BUILD_IRTEKNETSDK_DEMO)
    add_subdirectory("IRtekNetSDK/demo/1- C++ demo")
endif()
