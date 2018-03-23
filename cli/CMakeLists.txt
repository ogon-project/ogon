
set(MODULE_NAME "ogon-cli")
set(MODULE_PREFIX "OGON_CLI")

set(${MODULE_PREFIX}_SRCS
	cli.cpp)

add_executable(${MODULE_NAME} ${${MODULE_PREFIX}_SRCS})

target_link_libraries(${MODULE_NAME} winpr)

install(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})

