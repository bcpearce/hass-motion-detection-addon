# * Try to find the OpenH264 library Once done this will define
#
# OPENH264_ROOT - A list of search hints
#
# OPENH264_FOUND - system has OpenH264 OPENH264_INCLUDE_DIR - the OpenH264
# include directory OPENH264_LIBRARIES - libopenh264 library

if(UNIX AND NOT ANDROID)
  find_package(PkgConfig QUIET)
  pkg_check_modules(PC_OPENH264 QUIET openh264)
endif(UNIX AND NOT ANDROID)

if(OPENH264_INCLUDE_DIR AND OPENH264_LIBRARY)
  set(OPENH264_FIND_QUIETLY TRUE)
endif(OPENH264_INCLUDE_DIR AND OPENH264_LIBRARY)

find_path(
  OPENH264_INCLUDE_DIR
  NAMES wels/codec_api.h wels/codec_app_def.h wels/codec_def.h
  PATH_SUFFIXES include
  HINTS ${OPENH264_ROOT} ${PC_OPENH264_INCLUDE_DIRS})
find_library(
  OPENH264_LIBRARY
  NAMES openh264_dll openh264 welsdec
  PATH_SUFFIXES lib
  HINTS ${OPENH264_ROOT} ${PC_OPENH264_LIBRARY_DIRS})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OpenH264 DEFAULT_MSG OPENH264_LIBRARY
                                  OPENH264_INCLUDE_DIR)

if(OPENH264_INCLUDE_DIR AND OPENH264_LIBRARY)
  set(OPENH264_FOUND TRUE)
  set(OPENH264_LIBRARIES ${OPENH264_LIBRARY})
endif(OPENH264_INCLUDE_DIR AND OPENH264_LIBRARY)

if(OPENH264_FOUND)
  if(NOT OPENH264_FIND_QUIETLY)
    message(STATUS "Found OpenH264: ${OPENH264_LIBRARIES}")
  endif(NOT OPENH264_FIND_QUIETLY)
else(OPENH264_FOUND)
  if(OPENH264_FIND_REQUIRED)
    message(FATAL_ERROR "OpenH264 was not found")
  endif(OPENH264_FIND_REQUIRED)
endif(OPENH264_FOUND)

mark_as_advanced(OPENH264_INCLUDE_DIR OPENH264_LIBRARY)
