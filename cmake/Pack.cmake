set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_SOURCE_DIR}/LICENSE)
set(CPACK_PACKAGE_CONTACT "Benjamin Pearce (gitlab@bcpearce.com)")

set(DO_PACK TRUE)
set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  # Read the contents of /etc/os-release into a variable
  file(READ "/etc/os-release" OS_RELEASE_CONTENT)

  # Check if the content contains "ID=debian" or "ID_LIKE=debian"
  if("${OS_RELEASE_CONTENT}" MATCHES "ID=debian" OR "${OS_RELEASE_CONTENT}"
                                                    MATCHES "ID_LIKE=debian")
    message(STATUS "Detected Debian or Debian-based distribution.")
    # Set a variable or define a preprocessor macro for use in your project
    set(IS_DEBIAN_BASED TRUE)
    add_compile_definitions(DEBIAN_BASED)
    set(CPACK_GENERATOR "DEB")
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    set(CPACK_DEBIAN_PACKAGE_DESCRIPTION ${PROJECT_DESCRIPTION})

    if(USER_GRAPHICAL_USER_INTERFACE)
      message(FATAL_ERROR "Packaging .deb file with GUI is not supported")
    endif()

    list(APPEND CPACK_DEBIAN_PACKAGE_DEPENDS)

  endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  file(COPY ${CPACK_RESOURCE_FILE_LICENSE}
       DESTINATION ${CMAKE_BINARY_DIR}/LICENCE.TXT)
  set(CPACK_WIX_LICENSE_RTF ${CMAKE_BINARY_DIR}/LICENCE.TXT)
  set(CPACK_WIX_VERSION 4)
  set(CPACK_WIX_UPGRADE_GUID 5F153490-758F-4B54-BBA6-DFCF96F0E32F)
endif()

include(CPack)
