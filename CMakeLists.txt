cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

project(MotionDetection)

find_package(CURL REQUIRED)
find_package(OpenCV REQUIRED core highgui imgproc video)
find_package(OpenSSL CONFIG REQUIRED)

add_executable(MotionDetection main.cpp)

target_link_libraries(MotionDetection PUBLIC 
    OpenSSL::SSL 
    OpenSSL::Crypto
)
target_link_libraries(MotionDetection PRIVATE
    CURL::libcurl
    opencv_core
    opencv_highgui
    opencv_imgproc
    opencv_video
)