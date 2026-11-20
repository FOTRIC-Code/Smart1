
####### Expanded from @PACKAGE_INIT@ by configure_package_config_file() #######
####### Any changes to this file will be overwritten by the next CMake run ####
####### The input file was IRtekUVCSDKConfig.cmake.in                            ########

get_filename_component(PACKAGE_PREFIX_DIR "${CMAKE_CURRENT_LIST_DIR}/../../" ABSOLUTE)

macro(set_and_check _var _file)
  set(${_var} "${_file}")
  if(NOT EXISTS "${_file}")
    message(FATAL_ERROR "File or directory ${_file} referenced by variable ${_var} does not exist !")
  endif()
endmacro()

macro(check_required_components _NAME)
  foreach(comp ${${_NAME}_FIND_COMPONENTS})
    if(NOT ${_NAME}_${comp}_FOUND)
      if(${_NAME}_FIND_REQUIRED_${comp})
        set(${_NAME}_FOUND FALSE)
      endif()
    endif()
  endforeach()
endmacro()

####################################################################################

# Public headers are not part of the Linux release. Downstream projects should
# add the shared Windows/Android header tree to their own include path.

set_and_check(_IRtekUVCSDK_library
    "${PACKAGE_PREFIX_DIR}/library/libIRtekUVCSDK.so")

if(NOT TARGET IRtekUVCSDK::IRtekUVCSDK)
    add_library(IRtekUVCSDK::IRtekUVCSDK SHARED IMPORTED)
    set_target_properties(IRtekUVCSDK::IRtekUVCSDK PROPERTIES
        IMPORTED_LOCATION "${_IRtekUVCSDK_library}"
    )
endif()

unset(_IRtekUVCSDK_library)

check_required_components(IRtekUVCSDK)
