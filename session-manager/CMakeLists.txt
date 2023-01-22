set(MODULE_PREFIX "OGON_SESSION_MANAGER")
set(MODULE_NAME "ogon-session-manager")

PROTOBUF_GENERATE_CPP(PBRPC_PROTOBUF_SRC PBRPC_PROTOBUF_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/../protocols/protobuf/pbRPC.proto)
PROTOBUF_GENERATE_CPP(ICP_PROTOBUF_SRC ICP_PROTOBUF_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/../protocols/protobuf/ICP.proto)
PROTOBUF_GENERATE_CPP(SBP_PROTOBUF_SRC SBP_PROTOBUF_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/../protocols/protobuf/SBP.proto)
PROTOBUF_GENERATE_CPP(MODULE_PROTOBUF_SRC MODULE_PROTOBUF_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/../protocols/protobuf/Module.proto)

find_package(Thrift)
if (NOT THRIFT_FOUND)
	include(ThriftExternalProject)
else()
	set(THRIFT_EXTERNAL FALSE)
    include_directories(${THRIFT_INCLUDE_DIR})
endif()

# required for latest versions of boost
include_directories(SYSTEM ${THRIFT_INCLUDE_DIR})
set(THRIFT_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/gen/otsapi)
file(MAKE_DIRECTORY ${THRIFT_OUTPUT_DIR})
include(ThriftUtils)
set_source_files_properties("${THRIFT_FILES}" PROPERTIES GENERATED TRUE)


THRIFT_GEN(THRIFT_FILES ${CMAKE_CURRENT_SOURCE_DIR}/../protocols/thrift/otsapi.thrift)
set_source_files_properties(${THRIFT_FILES} PROPERTIES GENERATED TRUE)
add_library(otsapi-thrift STATIC ${THRIFT_FILES})
if (THRIFT_EXTERNAL)
	add_dependencies(otsapi-thrift thrift)
endif()
target_link_libraries(otsapi-thrift ${THRIFT_STATIC_LIB} ${OPENSSL_LIBRARIES})
set_property(TARGET otsapi-thrift PROPERTY COMPILE_FLAGS "-fPIC")

set(OTSAPI_SRC
	common/otsapi/OTSApiHandler.cpp common/otsapi/OTSApiServer.cpp
	common/otsapi/TaskDisconnect.cpp
	common/otsapi/TaskLogoff.cpp
	common/otsapi/TaskStartRemoteControl.cpp
	common/otsapi/TaskStopRemoteControl.cpp
	)

include_directories(${DBUS_INCLUDE_DIR})

find_package(Boost REQUIRED)
include_directories( ${Boost_INCLUDE_DIR} ${DBUS_ARCH_INCLUDE_DIR})

include_directories(${PROTOBUF_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/common)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/gen)
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(ICP_SOURCES
	common/call/Call.cpp
	common/call/CallIn.cpp
	common/call/CallOut.cpp
	common/call/CallFactory.cpp
	common/call/CallInPing.cpp
	common/call/CallOutPing.cpp
	common/call/CallInLogonUser.cpp
	common/call/CallInDisconnectUserSession.cpp
	common/call/CallOutOtsApiVirtualChannelOpen.cpp
	common/call/CallOutOtsApiVirtualChannelClose.cpp
	common/call/CallInAuthenticateUser.cpp
	common/call/CallOutSwitchTo.cpp
	common/call/CallOutLogOffUserSession.cpp
	common/call/CallInEndSession.cpp
	common/call/CallInPropertyBool.cpp
	common/call/CallInPropertyNumber.cpp
	common/call/CallInPropertyString.cpp
	common/call/CallInPropertyBulk.cpp
	common/call/CallOutDisconnectUserSession.cpp
	common/call/CallInSBPVersion.cpp
	common/call/TaskCallInLogonUser.cpp
	common/call/TaskAuthenticateUser.cpp
	common/call/TaskEndRemoteControl.cpp
	common/call/CallOutOtsApiStartRemoteControl.cpp
	common/call/CallOutOtsApiStopRemoteControl.cpp
	common/call/CallInRemoteControlEnded.cpp
	common/call/CallOutMessage.cpp
	common/pbRPC/RpcEngine.cpp
)

set(${MODULE_PREFIX}_SRCS
	main.cpp
	common/appcontext/ApplicationContext.cpp
	common/session/SessionStore.cpp
	common/session/Session.cpp
	common/session/ConnectionStore.cpp
	common/session/Connection.cpp
	common/session/TaskSessionTimeout.cpp
	common/session/TaskLogonUser.cpp
	common/session/TaskDisconnect.cpp
	common/session/TaskCallIn.cpp
	common/session/TaskSwitchTo.cpp
	common/session/TaskShutdown.cpp
	common/session/TaskEnd.cpp
	common/module/ModuleManager.cpp
	common/module/LocalModule.cpp
	common/module/RemoteModule.cpp
	common/module/RemoteModuleTransport.cpp
	common/module/AuthModule.cpp
	common/module/CallBacks.cpp
	common/config/PropertyManager.cpp
	common/config/PropertyCWrapper.cpp
	common/utils/StringHelpers.cpp
	common/utils/TimeHelpers.cpp
	common/utils/MakeCert.cpp
	common/task/Executor.cpp
	common/permission/PermissionManager.cpp
	common/permission/LogonPermission.cpp
	common/session/SessionNotifier.cpp
	common/process/ProcessMonitor.cpp
	../common/security.cpp
	../common/procutils.cpp
	${ICP_SOURCES}
	${OTSAPI_SRC}
	${PBRPC_PROTOBUF_SRC}
	${PBRPC_PROTOBUF_HEADERS}
	${ICP_PROTOBUF_SRC}
	${ICP_PROTOBUF_HEADERS}
	${SBP_PROTOBUF_SRC}
	${SBP_PROTOBUF_HEADERS}
	${MODULE_PROTOBUF_SRC}
	${MODULE_PROTOBUF_HEADERS}
	)

add_executable(${MODULE_NAME} ${${MODULE_PREFIX}_SRCS})
if (THRIFT_EXTERNAL)
	add_dependencies(${MODULE_NAME} thrift)
endif()

set(${MODULE_PREFIX}_LIBS "otsapi-thrift")

list(APPEND ${MODULE_PREFIX}_LIBS winpr)
list(APPEND ${MODULE_PREFIX}_LIBS ${PROTOBUF_LIBRARY})
list(APPEND ${MODULE_PREFIX}_LIBS winpr-tools)
list(APPEND ${MODULE_PREFIX}_LIBS ${DBUS_LIBRARIES})
list(APPEND ${MODULE_PREFIX}_LIBS ${CMAKE_THREAD_LIBS_INIT})

target_link_libraries(${MODULE_NAME} ${${MODULE_PREFIX}_LIBS})

install(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_SBINDIR})

add_subdirectory(auth)
add_subdirectory(module)
add_subdirectory(otsapi)

install(FILES config/ogon.SessionManager.conf DESTINATION ${SYSCONFDIR}/dbus-1/system.d)
