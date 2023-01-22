include_directories(${CMAKE_CURRENT_BINARY_DIR}/icp)
add_subdirectory(icp)

set(MODULE_NAME "ogon-rdp-server")
set(MODULE_PREFIX "OGON_CORE")

include_directories(".")
include_directories("${CMAKE_BINARY_DIR}/backend")

set(${MODULE_PREFIX}_SRCS
	app_context.cpp
	app_context.h
	backend.cpp
	channels.c
	channels.h
	encoder.c
	encoder.h
	eventloop.c
	eventloop.h
	ogon.c
	ogon.h
	frontend.c
	graphics.c
	peer.c
	peer.h
	PMurHash.c
	PMurHash.h
	state.cpp
	state.h
	rdpgfx.c
	rdpgfx.h
	font8x8.h
	openh264.c
	openh264.h
	bandwidth_mgmt.c
	bandwidth_mgmt.h
	../common/procutils.cpp
	../common/procutils.h
	)


add_executable(${MODULE_NAME} ${${MODULE_PREFIX}_SRCS})

list(APPEND ${MODULE_PREFIX}_LIBS freerdp)
list(APPEND ${MODULE_PREFIX}_LIBS ogon-backend)
list(APPEND ${MODULE_PREFIX}_LIBS ogon-icp)
list(APPEND ${MODULE_PREFIX}_LIBS freerdp-server)
list(APPEND ${MODULE_PREFIX}_LIBS ${OPENSSL_LIBRARIES})
list(APPEND ${MODULE_PREFIX}_LIBS winpr)
list(APPEND ${MODULE_PREFIX}_LIBS ${CMAKE_DL_LIBS})
target_link_libraries(${MODULE_NAME} ${${MODULE_PREFIX}_LIBS} ${CMAKE_THREAD_LIBS_INIT})

install(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_SBINDIR})

if(BUILD_TESTING)
	add_subdirectory(test)
endif()
