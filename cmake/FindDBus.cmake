# - Try to find the low-level D-Bus library
# Once done this will define
#
#  DBUS_FOUND - system has D-Bus
#  DBUS_INCLUDE_DIR - the D-Bus include directory
#  DBUS_ARCH_INCLUDE_DIR - the D-Bus architecture-specific include directory
#  DBUS_LIBRARIES - the libraries needed to use D-Bus

# Copyright (c) 2008, Kevin Kofler, <kevin.kofler@chello.at>
# modeled after FindLibArt.cmake:
# Copyright (c) 2006, Alexander Neundorf, <neundorf@kde.org>
#
# Redistribution and use is allowed according to the terms of the
# BSD license as follows:
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

if (DBUS_INCLUDE_DIR AND DBUS_ARCH_INCLUDE_DIR AND DBUS_LIBRARIES)

  # in cache already
  SET(DBUS_FOUND TRUE)

else (DBUS_INCLUDE_DIR AND DBUS_ARCH_INCLUDE_DIR AND DBUS_LIBRARIES)

  IF (NOT WIN32)
    FIND_PACKAGE(PkgConfig)
    IF (PKG_CONFIG_FOUND)
      # use pkg-config to get the directories and then use these values
      # in the FIND_PATH() and FIND_LIBRARY() calls
      pkg_check_modules(_DBUS_PC QUIET dbus-1)
    ENDIF (PKG_CONFIG_FOUND)
  ENDIF (NOT WIN32)

  FIND_PATH(DBUS_INCLUDE_DIR dbus/dbus.h
    ${_DBUS_PC_INCLUDE_DIRS}
    /usr/include
    /usr/include/dbus-1.0
    /usr/local/include
  )

  FIND_PATH(DBUS_ARCH_INCLUDE_DIR dbus/dbus-arch-deps.h
    ${_DBUS_PC_INCLUDE_DIRS}
    /usr/lib${LIB_SUFFIX}/include
    /usr/lib${LIB_SUFFIX}/dbus-1.0/include
    /usr/lib64/include
    /usr/lib64/dbus-1.0/include
    /usr/lib/include
    /usr/lib/dbus-1.0/include
  )

  FIND_LIBRARY(DBUS_LIBRARIES NAMES dbus-1 dbus
    PATHS
     ${_DBUS_PC_LIBDIR}
  )


  if (DBUS_INCLUDE_DIR AND DBUS_ARCH_INCLUDE_DIR AND DBUS_LIBRARIES)
     set(DBUS_FOUND TRUE)
  endif (DBUS_INCLUDE_DIR AND DBUS_ARCH_INCLUDE_DIR AND DBUS_LIBRARIES)


  if (DBUS_FOUND)
     if (NOT DBus_FIND_QUIETLY)
        message(STATUS "Found D-Bus: ${DBUS_LIBRARIES}")
     endif (NOT DBus_FIND_QUIETLY)
  else (DBUS_FOUND)
     if (DBus_FIND_REQUIRED)
        message(FATAL_ERROR "Could NOT find D-Bus")
     endif (DBus_FIND_REQUIRED)
  endif (DBUS_FOUND)

  MARK_AS_ADVANCED(DBUS_INCLUDE_DIR DBUS_ARCH_INCLUDE_DIR DBUS_LIBRARIES)

endif (DBUS_INCLUDE_DIR AND DBUS_ARCH_INCLUDE_DIR AND DBUS_LIBRARIES)
