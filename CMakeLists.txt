cmake_minimum_required(VERSION 3.30)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_definitions(
    $<$<CONFIG:Debug>:_DEBUG>
)

include_directories(AFTER "${CMAKE_BINARY_DIR}/include")

set(OPENCV_MODULES "core;bgsegm;imgproc")

option(USE_GRAPHICAL_USER_INTERFACE "Enable GUI backed by OpenCV's highgui module" OFF)
if (USE_GRAPHICAL_USER_INTERFACE)
  message(STATUS "Building with graphical user interface.")
  list(APPEND VCPKG_MANIFEST_FEATURES "graphical-user-interface")
  list(APPEND OPENCV_MODULES "highgui")
  add_compile_definitions(USE_GRAPHICAL_USER_INTERFACE=1)
endif()

option(BUILD_TESTS "Build Tests" ON)
if (BUILD_TESTS)
  list(APPEND VCPKG_MANIFEST_FEATURES "tests")
endif()

if(WIN32)
    add_compile_definitions("_WIN32_WINNT=0x0601" "_HAS_CXX17=1") # Windows 7
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
endif()

if (MSVC)
    add_compile_options("/external:W0 /external:anglebrackets /utf-8")
endif()

project(MotionDetection
    DESCRIPTION "Motion detection for video feeds"
    VERSION "0.5.0"
)
add_compile_definitions(MOTION_DETECTION_SEMVER="${PROJECT_VERSION}")
add_compile_definitions(PROJECT_NAME="${PROJECT_NAME}")

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

find_package(Boost REQUIRED COMPONENTS
  program_options
  url
)
find_package(CURL REQUIRED)
find_package(Live555 CONFIG REQUIRED)
find_package(nlohmann_json REQUIRED)

find_package(OpenCV REQUIRED COMPONENTS
  ${OPENCV_MODULES}
)
find_package(OpenSSL CONFIG REQUIRED)
find_package(OpenH264 REQUIRED)
find_package(spdlog CONFIG REQUIRED)

add_executable(
  MotionDetection include/Logger.h include/WindowsWrapper.h src/main.cxx)

add_library(Detector SHARED
  src/Detector/Detector.cxx
  src/Detector/MotionDetector.cxx
  include/Detector/Detector.h
  include/Detector/MotionDetector.h
)
add_library(HomeAssistant SHARED
  src/HomeAssistant/HassHandler.cxx
  include/HomeAssistant/HassHandler.h
)
add_library(Util SHARED
  src/Util/CurlWrapper.cxx
  src/Util/BufferOperations.cxx
  src/Util/ProgramOptions.cxx
  include/Util/CurlWrapper.h
  include/Util/EventHandler.h
  include/Util/BufferOperations.h
  include/Util/ProgramOptions.h
)
add_library(VideoSource SHARED
  src/VideoSource/Http.cxx
  src/VideoSource/Live555.cxx
  src/VideoSource/VideoSource.cxx
  include/VideoSource/Http.h
  include/VideoSource/Live555.h
  include/VideoSource/VideoSource.h
)

if (USE_GRAPHICAL_USER_INTERFACE)
  add_library(Gui SHARED
    src/Gui/GuiHandler.cxx
    include/Gui/GuiHandler.h
  )
  target_link_libraries(Gui PUBLIC
    opencv_highgui
  )
else()
  add_library(Gui SHARED
    src/Gui/WebHandler.cxx
    include/Gui/WebHandler.h
    "${CMAKE_CURRENT_BINARY_DIR}/packed_fs.c"
  )

  option(HOT_RELOAD_WEB_UI "Enable hot-reload of the Web UI" OFF)
  if (HOT_RELOAD_WEB_UI)
    target_compile_definitions(Gui PRIVATE SERVE_UNPACKED)
  endif()

  file(GLOB_RECURSE GUI_PUBLIC_FILES RELATIVE "${CMAKE_SOURCE_DIR}/src/Gui/" "src/Gui/public/*" )
  file(GLOB_RECURSE GUI_PUBLIC_FILES_ABS "${CMAKE_SOURCE_DIR}/src/Gui/public/*" )
  add_custom_command(OUTPUT packed_fs.c
    COMMAND unofficial::mongoose::pack ${GUI_PUBLIC_FILES} > "${CMAKE_CURRENT_BINARY_DIR}/packed_fs.c"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/src/Gui"
    DEPENDS ${GUI_PUBLIC_FILES_ABS}
  )

  find_package(unofficial-mongoose REQUIRED)
  target_link_libraries(Gui PUBLIC
    Detector
    unofficial::mongoose::mongoose
    VideoSource
  )
endif()

if (EXISTS ${CMAKE_SOURCE_DIR}/.env)
  add_custom_command(
    TARGET MotionDetection POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_SOURCE_DIR}/.env
    $<TARGET_FILE_DIR:MotionDetection>/.env
)
endif()

include_directories(${CMAKE_SOURCE_DIR}/include)

target_link_libraries(Detector PUBLIC
  opencv_core
  opencv_imgproc
  opencv_bgsegm
  spdlog::spdlog
)
target_link_libraries(HomeAssistant PUBLIC
  Boost::url
  Detector
  nlohmann_json::nlohmann_json
  spdlog::spdlog
  Util
)
target_link_libraries(Util PUBLIC
  CURL::libcurl
  Boost::program_options
  Boost::url
  OpenSSL::SSL
  OpenSSL::Crypto
  spdlog::spdlog
)
target_link_libraries(VideoSource PUBLIC
  Util
  Boost::url
  Live555::groupsock
  Live555::liveMedia
  Live555::UsageEnvironment
  Live555::BasicUsageEnvironment
  ${OPENH264_LIBRARIES}
  opencv_core
  opencv_imgproc
  opencv_imgcodecs
  spdlog::spdlog
)
target_link_libraries(MotionDetection PRIVATE
  Detector
  Gui
  HomeAssistant
  Util
  VideoSource
  opencv_core
  opencv_imgproc
  spdlog::spdlog
)

if (WIN32)
  target_link_libraries(Util PUBLIC ws2_32 mswsock)
  target_link_libraries(VideoSource PUBLIC ws2_32 mswsock)
endif()

if(BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

install(TARGETS MotionDetection
  EXPORT ${PROJECT_NAME}
  DESTINATION bin
)
install(TARGETS Detector Gui HomeAssistant Util VideoSource
  EXPORT ${PROJECT_NAME}
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib/${PROJECT_NAME}
)
install(FILES LICENSE DESTINATION share/licenses/${PROJECT_NAME})

option(PACK_PROJECT "Package the project for distribution" ON)
if (PACK_PROJECT)
  include(cmake/Pack.cmake)
endif()

option(BUILD_DOCKERFILE "Build the deployment dockerfile" OFF)
if (BUILD_DOCKERFILE)
  message(STATUS "Will build .deb package and Dockerfile for deployment")
  include(cmake/FindDocker.cmake)
  add_subdirectory(docker/deploy)
endif()