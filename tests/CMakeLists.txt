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
          Boost::process
          GTest::gtest)

set(SIM_SERVER_PORT "25689")
target_compile_definitions(${UNIT_TEST_PROJECT}
                           PRIVATE SIM_SERVER_PORT=${SIM_SERVER_PORT})

include(GoogleTest)
# gtest_discover_tests(${UNIT_TEST_PROJECT} TEST_LIST UNIT_TESTS)

add_executable(Live555_IntegrationTest Live555_IntegrationTest.cxx LogEnv.h)
find_package(Boost REQUIRED COMPONENTS asio process)
target_link_libraries(
  Live555_IntegrationTest
  PRIVATE Boost::asio
          Boost::process
          Callback
          Detector
          Gui
          nlohmann_json::nlohmann_json
          unofficial::mongoose::mongoose
          Util
          VideoSource
          GTest::gtest)

add_custom_command(
  TARGET Live555_IntegrationTest
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/res/test.264"
          "$<TARGET_FILE_DIR:Live555_IntegrationTest>/test.264"
  COMMENT "Copying Live555 Integration test data")
add_test(
  NAME Live555_IntegrationTest.H264
  COMMAND $<TARGET_FILE:Live555_IntegrationTest> --duration=10
          --rtspServerExec=$<TARGET_FILE:Live555::testOnDemandRTSPServer>
  WORKING_DIRECTORY $<TARGET_FILE_DIR:Live555_IntegrationTest>)

add_test(NAME MotionDetection.CliHelp COMMAND MotionDetection --help)
add_test(NAME MotionDetection.CliVersion COMMAND MotionDetection --version)
