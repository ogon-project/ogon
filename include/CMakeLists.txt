configure_file(${CMAKE_CURRENT_SOURCE_DIR}/ogon/version.h.in ${CMAKE_CURRENT_BINARY_DIR}/ogon/version.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/ogon/build-config.h.in ${CMAKE_CURRENT_BINARY_DIR}/ogon/build-config.h)

file(GLOB OGON_HEADERS "ogon/*.h")
install(FILES ${OGON_HEADERS} DESTINATION include/ogon${OGON_VERSION_MAJOR}/ogon COMPONENT headers)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ogon/version.h DESTINATION include/ogon${OGON_VERSION_MAJOR}/ogon COMPONENT headers)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ogon/build-config.h DESTINATION include/ogon${OGON_VERSION_MAJOR}/ogon COMPONENT headers)
