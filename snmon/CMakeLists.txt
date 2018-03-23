
set(MODULE_NAME "ogon-snmon")
set(MODULE_PREFIX "OGON_SNMON")

set(${MODULE_PREFIX}_SRCS
	snmon.cpp)

include_directories(${DBUS_INCLUDE_DIR})
include_directories(${DBUS_ARCH_INCLUDE_DIR})

add_executable(${MODULE_NAME} ${${MODULE_PREFIX}_SRCS})

target_link_libraries(${MODULE_NAME} winpr ${DBUS_LIBRARIES})

install(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})

