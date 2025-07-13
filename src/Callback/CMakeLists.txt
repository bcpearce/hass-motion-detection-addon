set(PROJECT_NAME Callback)
add_library(
  ${PROJECT_NAME} SHARED
  AsyncFileSave.cxx AsyncHassHandler.cxx BaseHassHandler.cxx
  SyncHassHandler.cxx ThreadedHassHandler.cxx)

target_link_libraries(
  ${PROJECT_NAME} PUBLIC Boost::url Detector nlohmann_json::nlohmann_json
                         spdlog::spdlog Live555::UsageEnvironment Util)

target_include_directories(${PROJECT_NAME}
                           PRIVATE ${CMAKE_SOURCE_DIR}/include/${PROJECT_NAME})
