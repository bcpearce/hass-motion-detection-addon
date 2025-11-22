vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_download_distfile(
  ARCHIVE
  URLS
  "http://live555.com/liveMedia/public/live555-latest.tar.gz"
  FILENAME
  "live555-latest.tar.gz"
  SHA512
  cbe689f904d7c16cb7926a41a1e4768adcd66cc17b950e4a4b08288a3f443c6bbc1ddd9ec7a08290bdfa66021348f8953c6e1f1253d73fe755fa6d846d104587
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
