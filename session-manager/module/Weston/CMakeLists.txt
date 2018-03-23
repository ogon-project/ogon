set(MODULE_NAME "ogon-mod-weston")
set(MODULE_PREFIX "OGON_MODULE_WESTON")

set(${MODULE_PREFIX}_SRCS
	weston_module.cpp
	weston_module.h
	../common/module_helper.cpp
	../common/module_helper.h)

add_library(${MODULE_NAME} SHARED ${${MODULE_PREFIX}_SRCS})

list(APPEND ${MODULE_PREFIX}_LIBS ogon-backend)
list(APPEND ${MODULE_PREFIX}_LIBS winpr)

target_link_libraries(${MODULE_NAME} ${${MODULE_PREFIX}_LIBS})

install(TARGETS ${MODULE_NAME} DESTINATION ${OGON_MODULE_LIB_PATH})
