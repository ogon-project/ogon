set(MODULE_NAME "ogon-otsapi")
set(MODULE_PREFIX "OGON_OTSAPI")

set(${MODULE_PREFIX}_SRCS otsapi_thrift.cpp)

add_library(${MODULE_NAME} SHARED ${${MODULE_PREFIX}_SRCS})
set(${MODULE_PREFIX}_LIBS otsapi-thrift)
list(APPEND ${MODULE_PREFIX}_LIBS winpr)

target_link_libraries(${MODULE_NAME} ${${MODULE_PREFIX}_LIBS})
set_target_properties(${MODULE_NAME} PROPERTIES PREFIX "lib")

install(TARGETS ${MODULE_NAME} DESTINATION ${OGON_APP_LIB_PATH})
