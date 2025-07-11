add_library(Detector SHARED Detector.cxx MotionDetector.cxx)

target_link_libraries(Detector PUBLIC opencv_core opencv_imgproc opencv_bgsegm
                                      spdlog::spdlog)

target_include_directories(Detector
                           PRIVATE ${CMAKE_SOURCE_DIR}/include/Detector)
