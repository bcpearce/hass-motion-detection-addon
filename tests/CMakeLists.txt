find_package(GTest REQUIRED)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

add_executable(MotionDetectionTests
  TestCurlWrapper.cxx
  TestProgramOptions.cxx
  TestMotionDetector.cxx
  TestVideoSource.cxx)

target_link_libraries(
  MotionDetectionTests PUBLIC
  Detector
  Util
  VideoSource
  GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(
	MotionDetectionTests
)