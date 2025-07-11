vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_download_distfile(
  ARCHIVE
  URLS
  "http://live555.com/liveMedia/public/live555-latest.tar.gz"
  FILENAME
  "live555-latest.tar.gz"
  SHA512
  1f2c970de72d0d4b7a8f26b9cc0acfecacc39629d1bd02b1731ceeb97ca51d58288c230342c19488e5faf082aa1c52143e4dd4d2f4b58d7ad37808b9b32c0546
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
