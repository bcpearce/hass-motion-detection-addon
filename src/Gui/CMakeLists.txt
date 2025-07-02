add_library(Gui SHARED
  WebHandler.cxx
  "${CMAKE_CURRENT_BINARY_DIR}/packed_fs.c"
)

option(HOT_RELOAD_WEB_UI "Enable hot-reload of the Web UI" OFF)
if (HOT_RELOAD_WEB_UI)
  target_compile_definitions(Gui PRIVATE SERVE_UNPACKED)
endif()

file(GLOB_RECURSE GUI_PUBLIC_FILES RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "public/*" )
file(GLOB_RECURSE GUI_PUBLIC_FILES_ABS "${CMAKE_CURRENT_SOURCE_DIR}/public/*" )
add_custom_command(OUTPUT packed_fs.c
  COMMAND unofficial::mongoose::pack ${GUI_PUBLIC_FILES} > "${CMAKE_CURRENT_BINARY_DIR}/packed_fs.c"
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  DEPENDS ${GUI_PUBLIC_FILES_ABS}
)

target_link_libraries(Gui PUBLIC
  Detector
  unofficial::mongoose::mongoose
  nlohmann_json::nlohmann_json
  VideoSource
)

target_include_directories(Gui PRIVATE
  ${CMAKE_SOURCE_DIR}/include/Gui
)