find_package(GTest REQUIRED)
find_package(unofficial-mongoose REQUIRED)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

add_executable(MotionDetectionTests
  main.cxx
  SimServer.cxx
  SimServer.h
  TestCurlWrapper.cxx
  TestHassHandler.cxx
  TestProgramOptions.cxx
  TestMotionDetector.cxx
  TestVideoSource.cxx
)

target_link_libraries(
  MotionDetectionTests PRIVATE
  Detector
  HomeAssistant
  nlohmann_json::nlohmann_json
  unofficial::mongoose::mongoose
  Util
  VideoSource
  GTest::gtest
)

set(SIM_SERVER_PORT "25689")
target_compile_definitions(MotionDetectionTests PRIVATE
  SIM_SERVER_PORT=${SIM_SERVER_PORT}
)

include(GoogleTest)
gtest_discover_tests(
	MotionDetectionTests
)