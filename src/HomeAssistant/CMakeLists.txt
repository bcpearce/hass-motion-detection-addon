add_library(HomeAssistant SHARED
  AsyncHassHandler.cxx
  BaseHassHandler.cxx
  ThreadedHassHandler.cxx
)

target_link_libraries(HomeAssistant PUBLIC
  Boost::url
  Detector
  nlohmann_json::nlohmann_json
  spdlog::spdlog
  Live555::UsageEnvironment
  Util
)

target_include_directories(HomeAssistant PRIVATE
  ${CMAKE_SOURCE_DIR}/include/HomeAssistant
)