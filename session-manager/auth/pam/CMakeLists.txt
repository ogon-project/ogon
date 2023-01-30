set(MODULE_NAME "ogon-auth-pam")
set(MODULE_PREFIX "OGON_AUTH_PAM")

include_directories(${PAM_INCLUDE_DIR})

set(${MODULE_PREFIX}_SRCS
	pam_auth.cpp
	pam_auth.h)

add_library(${MODULE_NAME} SHARED ${${MODULE_PREFIX}_SRCS})

set(${MODULE_PREFIX}_LIBS ${PAM_LIBRARY})
list(APPEND ${MODULE_PREFIX}_LIBS winpr)

target_link_libraries(${MODULE_NAME} ${${MODULE_PREFIX}_LIBS})

install(TARGETS ${MODULE_NAME} DESTINATION ${OGON_AUTH_MODULE_LIB_PATH})
