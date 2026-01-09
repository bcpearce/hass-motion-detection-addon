vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_download_distfile(
  ARCHIVE
  URLS
  "http://live555.com/liveMedia/public/live555-latest.tar.gz"
  FILENAME
  "live555-latest.tar.gz"
  SHA512
  da8ffae35881c8a801d5ee14b83fdbf240bfa6c600ed03f9064cfe465a5d07237bb4c845a6a4d0f6e4b0b4bf6a5c420b3af0153cbdb3f40ad34891a138b0e797
)

vcpkg_extract_source_archive(
  SOURCE_PATH ARCHIVE "${ARCHIVE}" PATCHES fix_testOnDemandRTSPServer.patch
  # PATCHES fix-RTSPClient.patch fix_operator_overload.patch
)

file(COPY "${CMAKE_CURRENT_LIST_DIR}/CMakeLists.txt"
     DESTINATION "${SOURCE_PATH}")

vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS FEATURES "test-progs"
                     ENABLE_TEST_PROGS)

vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}" OPTIONS ${FEATURE_OPTIONS})

vcpkg_cmake_install()
vcpkg_copy_pdbs()

vcpkg_cmake_config_fixup(PACKAGE_NAME live555)
if("test-progs" IN_LIST FEATURES)
  vcpkg_copy_tools(TOOL_NAMES testOnDemandRTSPServer testRTSPClient)
endif()

file(
  GLOB
  HEADERS
  "${SOURCE_PATH}/BasicUsageEnvironment/include/*.h*"
  "${SOURCE_PATH}/groupsock/include/*.h*"
  "${SOURCE_PATH}/liveMedia/include/*.h*"
  "${SOURCE_PATH}/UsageEnvironment/include/*.h*")

file(COPY ${HEADERS} DESTINATION "${CURRENT_PACKAGES_DIR}/include")

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/bin"
     "${CURRENT_PACKAGES_DIR}/bin")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/COPYING")
