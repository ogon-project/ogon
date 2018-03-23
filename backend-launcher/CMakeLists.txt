set(MODULE_PREFIX "OGON_SESSION_MANAGER")
set(MODULE_NAME "ogon-backend-launcher")

PROTOBUF_GENERATE_CPP(PBRPC_PROTOBUF_SRC PBRPC_PROTOBUF_HEADERS ${CMAKE_SOURCE_DIR}/protocols/protobuf/pbRPC.proto)
PROTOBUF_GENERATE_CPP(MODULE_PROTOBUF_SRC MODULE_PROTOBUF_HEADERS ${CMAKE_SOURCE_DIR}/protocols/protobuf/Module.proto)

include_directories(${PROTOBUF_INCLUDE_DIRS})

set(${MODULE_PREFIX}_SRCS
		SystemSession.cpp
		../session-manager/common/module/RemoteModuleTransport.cpp
		ModuleCommunication.cpp
		main.cpp
		${PBRPC_PROTOBUF_SRC}
		${PBRPC_PROTOBUF_HEADERS}
		${MODULE_PROTOBUF_SRC}
		${MODULE_PROTOBUF_HEADERS}
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})
add_executable(${MODULE_NAME} ${${MODULE_PREFIX}_SRCS})
target_link_libraries(${MODULE_NAME} ${PAM_LIBRARY} ${PROTOBUF_LIBRARY} winpr ${CMAKE_THREAD_LIBS_INIT})
if(WITH_LIBSYSTEMD)
	target_link_libraries(${MODULE_NAME} ${LIBSYSTEMD_LIBRARIES})
	include_directories(${LIBSYSTEMD_INCLUDE_DIRS})
endif(WITH_LIBSYSTEMD)

install(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_SBINDIR})
