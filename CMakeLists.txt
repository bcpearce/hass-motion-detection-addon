cmake_minimum_required(VERSION 3.30)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_definitions($<$<CONFIG:Debug>:_DEBUG>)

include_directories(AFTER "${CMAKE_BINARY_DIR}/include")

option(BUILD_TESTS "Build Tests" ON)
if(BUILD_TESTS)
  list(APPEND VCPKG_MANIFEST_FEATURES "tests")
endif()

if(WIN32)
  add_compile_definitions("_WIN32_WINNT=0x0601" "_HAS_CXX17=1") # Windows 7
  set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
endif()

if(MSVC)
  add_compile_options("/external:W0 /external:anglebrackets /utf-8")
endif()

file(READ "${CMAKE_SOURCE_DIR}/VERSION" PROJECT_VERSION)
string(STRIP ${PROJECT_VERSION} PROJECT_VERSION)
project(
  MotionDetection
  DESCRIPTION "Motion detection for video feeds"
  VERSION ${PROJECT_VERSION})
add_compile_definitions(MOTION_DETECTION_SEMVER="${PROJECT_VERSION}")
add_compile_definitions(PROJECT_NAME="${PROJECT_NAME}")

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

find_package(Boost REQUIRED COMPONENTS program_options url)
find_package(CURL REQUIRED)
find_package(Live555 CONFIG REQUIRED)
find_package(unofficial-mongoose REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(OpenCV REQUIRED COMPONENTS core bgsegm imgproc)
find_package(OpenSSL CONFIG REQUIRED)
find_package(OpenH264 REQUIRED)
find_package(spdlog CONFIG REQUIRED)

add_executable(
  MotionDetection include/Logger.h include/WindowsWrapper.h src/main.cxx
                  "include/HomeAssistant/SyncHassHandler.h")

if(EXISTS ${CMAKE_SOURCE_DIR}/.env)
  add_custom_command(
    TARGET MotionDetection
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/.env
            $<TARGET_FILE_DIR:MotionDetection>/.env)
endif()

include_directories(${CMAKE_SOURCE_DIR}/include)

add_subdirectory(src/Detector)
add_subdirectory(src/Gui)
add_subdirectory(src/HomeAssistant)
add_subdirectory(src/Util)
add_subdirectory(src/VideoSource)

set(PROJECT_SHARED_LIBS "Detector;Gui;HomeAssistant;Util;VideoSource")

target_link_libraries(
  MotionDetection PRIVATE ${PROJECT_SHARED_LIBS} opencv_core opencv_imgproc
                          spdlog::spdlog)

if(WIN32)
  target_link_libraries(Util PUBLIC ws2_32 mswsock)
  target_link_libraries(VideoSource PUBLIC ws2_32 mswsock)
endif()

if(CMAKE_C_COMPILER_ID MATCHES "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Enabling AddressSanitizer for GCC")
    add_compile_options(-fsanitize=address)
    add_link_options(-fsanitize=address)
  endif()
endif()

if(BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

install(
  TARGETS MotionDetection
  EXPORT ${PROJECT_NAME}
  DESTINATION bin)
install(
  TARGETS ${PROJECT_SHARED_LIBS}
  EXPORT ${PROJECT_NAME}
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib/${PROJECT_NAME})
install(FILES LICENSE DESTINATION share/licenses/${PROJECT_NAME})

option(PACK_PROJECT "Package the project for distribution" ON)
if(PACK_PROJECT)
  include(cmake/Pack.cmake)
endif()

option(BUILD_DOCKERFILE "Build the deployment dockerfile" OFF)
if(BUILD_DOCKERFILE)
  message(STATUS "Will build .deb package and Dockerfile for deployment")
  include(cmake/FindDocker.cmake)
  add_subdirectory(docker/deploy)
endif()
