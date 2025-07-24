vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_download_distfile(
  ARCHIVE
  URLS
  "http://live555.com/liveMedia/public/live555-latest.tar.gz"
  FILENAME
  "live555-latest.tar.gz"
  SHA512
  6acc02b22a65c24c07e744f9d3069b78199677d903567649a034408ce785a2253c789a2868e2af30eda0fadb9e9a224fddbbefe5e626277f7df40a26c9230a1d
)

vcpkg_extract_source_archive(
  SOURCE_PATH ARCHIVE "${ARCHIVE}"
  # PATCHES fix-RTSPClient.patch fix_operator_overload.patch
)

file(COPY "${CMAKE_CURRENT_LIST_DIR}/CMakeLists.txt"
     DESTINATION "${SOURCE_PATH}")

vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")

vcpkg_cmake_install()
vcpkg_copy_pdbs()

vcpkg_cmake_config_fixup(PACKAGE_NAME live555)

file(
  GLOB
  HEADERS
  "${SOURCE_PATH}/BasicUsageEnvironment/include/*.h*"
  "${SOURCE_PATH}/groupsock/include/*.h*"
  "${SOURCE_PATH}/liveMedia/include/*.h*"
  "${SOURCE_PATH}/UsageEnvironment/include/*.h*")

file(COPY ${HEADERS} DESTINATION "${CURRENT_PACKAGES_DIR}/include")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/COPYING")
