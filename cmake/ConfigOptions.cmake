include (CMakeDependentOption)

# Build unit tests
option(BUILD_TESTING "Build unit tests" OFF)

# FreeRDP major version
option(FREERDP_LINK_VERSION "FreeRDP major version to compile for" 2)

# Build with OpenH264 support
option(USE_FREERDP_H264 "use freerdp backend for h264 encoding" OFF)
cmake_dependent_option(WITH_OPENH264 "Add support for H.264 encoding" ON "NOT USE_FREERDP_H264" OFF)

# Debugging options
option(WITH_DEBUG_STATE "Enable frame state machine debugging." OFF)
option(WITH_ENCODER_STATS "Enable encoding stats" OFF)
