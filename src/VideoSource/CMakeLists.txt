add_library(VideoSource SHARED Http.cxx Live555.cxx VideoSource.cxx)

target_link_libraries(
  VideoSource
  PUBLIC Util
         Boost::url
         Live555::groupsock
         Live555::liveMedia
         Live555::UsageEnvironment
         Live555::BasicUsageEnvironment
         ${OPENH264_LIBRARIES}
         opencv_core
         opencv_imgproc
         opencv_imgcodecs
         spdlog::spdlog)

target_include_directories(VideoSource
                           PRIVATE ${CMAKE_SOURCE_DIR}/include/VideoSource)
