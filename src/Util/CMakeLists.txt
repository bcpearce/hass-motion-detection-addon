add_library(Util SHARED
  CurlMultiWrapper.cxx
  CurlWrapper.cxx
  BufferOperations.cxx
  ProgramOptions.cxx
)

target_link_libraries(Util PUBLIC
  CURL::libcurl
  Boost::program_options
  Boost::url
  OpenSSL::SSL
  OpenSSL::Crypto
  spdlog::spdlog
)

target_include_directories(Util PRIVATE
  ${CMAKE_SOURCE_DIR}/include/Util
)