set(MODULE_NAME "ogon-icp")
set(MODULE_PREFIX "OGON_ICP")
set(PBRPC_PREFIX "pbrpc")

PROTOBUFC_GENERATE_C(ICP_PROTOC_SRC ICP_PROTOC_HDRS ${CMAKE_CURRENT_SOURCE_DIR}/../../protocols/protobuf/ICP.proto)
PROTOBUFC_GENERATE_C(PBRPC_PROTOC_SRC PBRPC_PROTOC_HDRS ${CMAKE_CURRENT_SOURCE_DIR}/../../protocols/protobuf/pbRPC.proto)

set(PBRPC_SRC
  ${PBRPC_PREFIX}/pbrpc.c
  ${PBRPC_PREFIX}/pbrpc.h
	${PBRPC_PREFIX}/pbrpc_utils.cpp
	${PBRPC_PREFIX}/pbrpc_utils.h
	${PBRPC_PREFIX}/pbrpc_transport.h
	${PBRPC_PREFIX}/pipe_transport.cpp
	${PBRPC_PREFIX}/pipe_transport.h
	${PBRPC_PROTOC_HDRS}
	${PBRPC_PROTOC_SRC}
)

include_directories(${PBRPC_PREFIX})
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_BINARY_DIR}/backend)

set(${MODULE_PREFIX}_SRCS
	icp.cpp
	icp_client_stubs.c
	icp_client_stubs.h
	icp_server_stubs.c
	icp_server_stubs.h
	${ICP_PROTOC_SRC}
	${ICP_PROTOC_HDRS}
	${PBRPC_SRC}
)

add_library(${MODULE_NAME} STATIC ${${MODULE_PREFIX}_SRCS})


target_link_libraries(${MODULE_NAME} winpr ${PROTOBUFC_LIBRARIES})
