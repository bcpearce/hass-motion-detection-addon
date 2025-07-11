cmake_minimum_required(VERSION 3.5)
project(live555 C CXX)

set(CMAKE_CXX_STANDARD 20)
include_directories(BasicUsageEnvironment/include groupsock/include
                    liveMedia/include UsageEnvironment/include)

if(WIN32)
  add_definitions(-DNO_GETIFADDRS)
endif(WIN32)

if(NOT MSVC)
  add_compile_options(-DSOCKLEN_T=socklen_t)
endif()

if(CMAKE_SYSTEM_NAME MATCHES "Darwin")
  add_compile_options(-DBSD=1)
  add_compile_options(-DHAVE_SOCKADDR_LEN=1)
  add_compile_options(-DTIME_BASE=int)
  add_compile_options(-DNEED_XLOCALE_H=1)
endif()

file(GLOB BASIC_USAGE_ENVIRONMENT_SRCS BasicUsageEnvironment/*.c
     BasicUsageEnvironment/*.cpp)
add_library(BasicUsageEnvironment ${BASIC_USAGE_ENVIRONMENT_SRCS})
target_include_directories(BasicUsageEnvironment
                           PUBLIC $<INSTALL_INTERFACE:include>)

file(GLOB GROUPSOCK_SRCS groupsock/*.c groupsock/*.cpp)
add_library(groupsock ${GROUPSOCK_SRCS})
target_include_directories(groupsock PUBLIC $<INSTALL_INTERFACE:include>)

file(GLOB LIVEMEDIA_SRCS liveMedia/*.c liveMedia/*.cpp)
add_library(liveMedia ${LIVEMEDIA_SRCS})
find_package(OpenSSL REQUIRED)
target_include_directories(
  liveMedia
  PRIVATE "${OPENSSL_INCLUDE_DIR}"
  PUBLIC $<INSTALL_INTERFACE:include>)

file(GLOB USAGE_ENVIRONMENT_SRCS UsageEnvironment/*.c UsageEnvironment/*.cpp)
add_library(UsageEnvironment ${USAGE_ENVIRONMENT_SRCS})
target_include_directories(UsageEnvironment PUBLIC $<INSTALL_INTERFACE:include>)

file(
  WRITE "${CMAKE_BINARY_DIR}/live555-config.cmake"
  [[
include(CMakeFindDependencyMacro)

find_dependency(OpenSSL)

include("${CMAKE_CURRENT_LIST_DIR}/live555.cmake")
]])

install(FILES "${CMAKE_BINARY_DIR}/live555-config.cmake"
        DESTINATION share/live555)

install(
  TARGETS groupsock BasicUsageEnvironment liveMedia UsageEnvironment
  EXPORT Live555
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib)

install(
  EXPORT Live555
  FILE live555.cmake
  NAMESPACE Live555::
  DESTINATION share/live555)
