set(MODULE_NAME "ogon-mod-x11")
set(MODULE_PREFIX "OGON_MODULE_X11")

set(${MODULE_PREFIX}_SRCS
	x11_module.cpp
	x11_module.h
	../common/module_helper.cpp
	../common/module_helper.h)

add_library(${MODULE_NAME} SHARED ${${MODULE_PREFIX}_SRCS})

list(APPEND ${MODULE_PREFIX}_LIBS ogon-backend)
list(APPEND ${MODULE_PREFIX}_LIBS winpr)
list(APPEND ${MODULE_PREFIX}_LIBS ${X11_Xau_LIB})

target_link_libraries(${MODULE_NAME} ${${MODULE_PREFIX}_LIBS})
include_directories(${X11_Xau_INCLUDE_PATH})

install(TARGETS ${MODULE_NAME} DESTINATION ${OGON_MODULE_LIB_PATH})
