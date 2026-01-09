find_package(GTest REQUIRED)
find_package(unofficial-mongoose REQUIRED)

set(gtest_force_shared_crt
    ON
    CACHE BOOL "" FORCE)

set(UNIT_TEST_PROJECT MotionDetectionTests)

add_executable(
  ${UNIT_TEST_PROJECT}
  main.cxx
  SimServer.cxx
  SimServer.h
  TestCallback.cxx
  TestCurlWrapper.cxx
  TestProgramOptions.cxx
  TestMotionDetector.cxx
  TestUtil.cxx
  TestVideoSource.cxx
  TestWebHandler.cxx
  ServerEnv.h
  LogEnv.h)
target_link_libraries(
  ${UNIT_TEST_PROJECT}
  PRIVATE Callback
          Detector
          Gui
          nlohmann_json::nlohmann_json
          unofficial::mongoose::mongoose
          Util
          VideoSource
          GTest::gtest)

set(SIM_SERVER_PORT "25689")
target_compile_definitions(${UNIT_TEST_PROJECT}
                           PRIVATE SIM_SERVER_PORT=${SIM_SERVER_PORT})

include(GoogleTest)
gtest_discover_tests(${UNIT_TEST_PROJECT})

add_executable(
  TestLive555_Integraton
  TestLive555_Integration.cxx
  LogEnv.h)
target_link_libraries(
  TestLive555_Integraton
  PRIVATE Callback
          Detector
          Gui
          nlohmann_json::nlohmann_json
          unofficial::mongoose::mongoose
          Util
          VideoSource
          GTest::gtest)

set(TEST_FILE_URL "http://www.live555.com/liveMedia/public/264/test.264")
add_test(NAME TestLive555_Integration 
  COMMAND 
    python3 
    "${CMAKE_SOURCE_DIR}/tests/TestLive555_Integration.py"
    "${TEST_FILE_URL}"
    $<TARGET_FILE:Live555::testOnDemandRTSPServer> 
    $<TARGET_FILE:TestLive555_Integraton>
  )

add_test(NAME MotionDetection.CliHelp COMMAND MotionDetection --help)
add_test(NAME MotionDetection.CliVersion COMMAND MotionDetection --version)
