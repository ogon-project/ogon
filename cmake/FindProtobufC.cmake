# ogon - Free Remote Desktop Services
# FindProtobufC.cmake
# Loosely based on CMake's FindProtobuf.cmake
#
# Authors:
# Bernhard Miklautz <bernhard.miklautz@thincast.com>
#
# Copyright 2013-2018 Thincast Technologies GmbH
# Copyright 2009 Kitware, Inc.
# Copyright 2009-2011 Philip Lowman <philip@yhbt.com>
# Copyright 2008 Esben Mose Hansen, Ange Optimization ApS
#
# Distributed under the OSI-approved BSD License (the "License"):
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# * Redistributions of source code must retain the above copyright
#   notice, this list of conditions and the following disclaimer.
#
# * Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in the
#   documentation and/or other materials provided with the distribution.
#
# * Neither the names of Thincast Technologies GmbH, Kitware, Inc.,
#   the Insight Software Consortium, nor the names of their contributors
#   may be used to endorse or promote products derived from this software
#   without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.



#=============================================================================
#
# Find protobuf c implementation libraries, includes and the protoc-c compiler
#
# Module defines:
#   PROTOBUFC_FOUND        - library, includes and compiler where found
#   PROTOBUFC_INCLUDE_DIRS - include directories
#   PROTOBUFC_LIBRARIES    - protobuf-c libraries
#   PROTOBUFC_EXECUTEABLE  - protobuf-c compiler
#
# Environment variables:
#   PROTOBUFC_ROOTDIR      - optional - rootdir of protobuf-c installation
#
# Cache entries:
#   PROTOBUFC_LIBRARY      - detected protobuf-c library
#   PROTOBUFC_INCLUDE_DIR   - detected protobuf-c include dir(s)
#   PROTOBUFC_COMPILER      - detected protobuf-c compiler
#
####
#
# Example:
#
#   find_package(ProtobufC REQUIRED)
#   include_directories(${PROTOBUFC_INCLUDE_DIRS})
#   include_directories(${CMAKE_CURRENT_BINARY_DIR})
#   PROTOBUFC_GENERATE_C(PROTO_SOURCES PROTO_HEADERS protobuf.proto)
#   add_executable(bar bar.c ${PROTO_SRCn} ${PROTO_HDRS})
#   target_link_libraries(bar ${PROTOBUFC_LIBRARIES})
#
# NOTE: You may need to link against pthreads, depending
#       on the platform.
#
# NOTE: The PROTOBUFC_GENERATE_C macro & add_executable() or add_library()
#       calls only work properly within the same directory.
#
#=============================================================================

set(PROTBUFC_SOURCE_EXTENSION "pb-c.c")
set(PROTBUFC_HEADER_EXTENSION "pb-c.h")
function(PROTOBUFC_GENERATE_C SOURCES HEADERS)
	if(NOT ARGN)
		message(SEND_ERROR "Error: PROTOBUFC_GENERATE_C() called without any proto files")
		return()
	endif(NOT ARGN)

	foreach(FIL ${ARGN})
		get_filename_component(ABS_FIL ${FIL} ABSOLUTE)
		get_filename_component(FIL_WE ${FIL} NAME_WE)
		get_filename_component(FIL_PATH ${ABS_FIL} PATH)

		list(APPEND ${SOURCES} "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}.${PROTBUFC_SOURCE_EXTENSION}")
		list(APPEND ${HEADERS} "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}.${PROTBUFC_HEADER_EXTENSION}")

		add_custom_command(
			OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}.${PROTBUFC_SOURCE_EXTENSION}"
			"${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}.${PROTBUFC_HEADER_EXTENSION}"
			COMMAND  ${PROTOBUFC_COMPILER}
			ARGS --c_out ${CMAKE_CURRENT_BINARY_DIR} -I ${CMAKE_CURRENT_SOURCE_DIR} -I ${FIL_PATH} ${ABS_FIL}
			DEPENDS ${ABS_FIL}
			COMMENT "Running protobuf-c compiler on ${FIL}"
			VERBATIM )
	endforeach()
	set_source_files_properties(${${SOURCES}} ${${HEADERS}} PROPERTIES GENERATED TRUE)
	set(${SOURCES} ${${SOURCES}} PARENT_SCOPE)
	set(${HEADERS} ${${HEADERS}} PARENT_SCOPE)
endfunction()

find_library(PROTOBUFC_LIBRARY
	NAMES "protobuf-c"
	PATHS "/usr" "/usr/local" "/opt" ENV PROTOBUFC_ROOTDIR
	PATH_SUFFIXES "lib")
mark_as_advanced(PROTOBUFC_LIBRARY)

find_path(PROTOBUFC_INCLUDE_DIR
	NAMES "google/protobuf-c/protobuf-c.h"
	PATHS "/usr" "/usr/local" "/opt" ENV PROTOBUFC_ROOTDIR
	PATH_SUFFIXES "include")
mark_as_advanced(PROTOBUFC_INCLUDE_DIR)

find_program(PROTOBUFC_COMPILER
	NAMES "protoc-c"
	PATHS "/usr" "/usr/local" "/opt" ENV PROTOBUFC_ROOTDIR
	PATH_SUFFIXES "bin")
mark_as_advanced(PROTOBUFC_COMPILER)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(ProtobufC DEFAULT_MSG PROTOBUFC_LIBRARY PROTOBUFC_COMPILER PROTOBUFC_INCLUDE_DIR )

if (PROTOBUFC_FOUND)
	set(PROTOBUFC_LIBRARIES ${PROTOBUFC_LIBRARY})
	set(PROTOBUFC_INCLUDE_DIRS ${PROTOBUFC_INCLUDE_DIR})
	set(PROTOBUFC_EXECUTEABLE ${PROTOBUFC_COMPILER})
endif(PROTOBUFC_FOUND)
