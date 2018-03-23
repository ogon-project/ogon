set(MODULE_NAME "ogon-backend")
set(MODULE_PREFIX "OGON_MODULE")

PROTOBUFC_GENERATE_C(MODULE_PROTOC_SRC MODULE_PROTOC_HDRS ${CMAKE_SOURCE_DIR}/protocols/protobuf/backend.proto)

set(${MODULE_PREFIX}_SRCS
	dmgbuf.c
	protocol.c
	protocol.h
	transport.c
	service.c
	keycodes.c
	${MODULE_PROTOC_SRC}
	${MODULE_PROTOC_HDRS}
	../common/security.c
	)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
add_library(${MODULE_NAME} ${${MODULE_PREFIX}_SRCS})
target_link_libraries(${MODULE_NAME} ${PRIVATE_KEYWORD} winpr ${PROTOBUFC_LIBRARIES})

set_target_properties(${MODULE_NAME} PROPERTIES PREFIX "lib")
set_target_properties(${MODULE_NAME} PROPERTIES VERSION ${OGON_VERSION_FULL} SOVERSION ${OGON_VERSION_MAJOR})

install(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR} EXPORT ogon-backend)

if(${CMAKE_VERSION} VERSION_GREATER "2.8.10")

	export(PACKAGE ogon-backend)

	set(OGON_CMAKE_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/ogon-backend${OGON_VERSION_MAJOR}")
	set(OGON_INCLUDE_DIR "include/ogon${OGON_VERSION_MAJOR}")
	set(OGON_PROTOCOL_DIR "share/ogon/${OGON_VERSION_MAJOR}")

	configure_package_config_file(ogon-backendConfig.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/ogon-backendConfig.cmake
		INSTALL_DESTINATION ${OGON_CMAKE_INSTALL_DIR}
		PATH_VARS OGON_INCLUDE_DIR OGON_PROTOCOL_DIR)

	write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/ogon-backendConfigVersion.cmake
		VERSION ${OGON_VERSION} COMPATIBILITY SameMajorVersion)

	install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ogon-backendConfig.cmake ${CMAKE_CURRENT_BINARY_DIR}/ogon-backendConfigVersion.cmake
		DESTINATION ${OGON_CMAKE_INSTALL_DIR})

	install(EXPORT ogon-backend DESTINATION ${OGON_CMAKE_INSTALL_DIR})

endif()

if(NOT MSVC)
	configure_file(${CMAKE_CURRENT_SOURCE_DIR}/ogon-backend.pc.in ${CMAKE_CURRENT_BINARY_DIR}/ogon-backend${OGON_VERSION_MAJOR}.pc @ONLY)
	install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ogon-backend${OGON_VERSION_MAJOR}.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
endif()
