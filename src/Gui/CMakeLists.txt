file(READ ${CMAKE_CURRENT_SOURCE_DIR}/public/index.html GUI_PUBLIC_INDEX_HTML_CONTENT)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/public/index.h.in ${CMAKE_BINARY_DIR}/include/index.h)