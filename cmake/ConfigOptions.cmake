# Build unit tests
option(BUILD_TESTING "Build unit tests" OFF)

# FreeRDP major version
option(FREERDP_LINK_VERSION "FreeRDP major version to compile for" 2)

# Build with OpenH264 support
option(WITH_OPENH264 "Add support for H.264 encoding" ON)

# Debugging options
option(WITH_DEBUG_STATE "Enable frame state machine debugging." OFF)
option(WITH_ENCODER_STATS "Enable encoding stats" OFF)
