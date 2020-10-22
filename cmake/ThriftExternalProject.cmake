include(ExternalProject)

set(EXTERNAL_PROJECT "thrift")

set(${EXTERNAL_PROJECT}_VERSION "0.13.0")
set(${EXTERNAL_PROJECT}_NAME "${EXTERNAL_PROJECT}-${${EXTERNAL_PROJECT}_VERSION}")
set(${EXTERNAL_PROJECT}_EXT ".tar.gz")
set(${EXTERNAL_PROJECT}_FILE "${${EXTERNAL_PROJECT}_NAME}${${EXTERNAL_PROJECT}_EXT}")
set(${EXTERNAL_PROJECT}_URL "http://archive.apache.org/dist/thrift/${${EXTERNAL_PROJECT}_VERSION}/${${EXTERNAL_PROJECT}_FILE}")
set(${EXTERNAL_PROJECT}_HASH "SHA1=0cbb06d047a8212c6ac1240492bc569609fecd33")
set(${EXTERNAL_PROJECT}_CONFIGURE --without-python --without-py3 --without-java --without-c_glib --with-pic --without-csharp --without-haskell --without-go --without-d --without-qt4 --without-qt5 --without-perl --without-erlang --without-php --without-ruby --without-haxe --without-nodejs --disable-tests --disable-tutorial --prefix=${EXTERNAL_PROJECTS_BASE}/Install/${EXTERNAL_PROJECT} CPPFLAGS=-DFORCE_BOOST_SMART_PTR CXXFLAGS=-Wno-unused-variable)
set(${EXTERNAL_PROJECT}_UPDATE "")
set(${EXTERNAL_PROJECT}_MAKE "")

ExternalProject_Add(${EXTERNAL_PROJECT}
		URL ${${EXTERNAL_PROJECT}_URL}
		UPDATE_COMMAND ${${EXTERNAL_PROJECT}_UPDATE}
		CONFIGURE_COMMAND "${EXTERNAL_PROJECTS_BASE}/Source/${EXTERNAL_PROJECT}/configure" ${${EXTERNAL_PROJECT}_CONFIGURE}
		BUILD_IN_SOURCE 1
		URL_HASH "${${EXTERNAL_PROJECT}_HASH}"
		INSTALL_COMMAND make install)
		#PATCH_COMMAND patch -p1 < ${CMAKE_SOURCE_DIR}/patches/THRIFT-3420.patch

set(THRIFT_FOUND TRUE)
set(THRIFT_EXTERNAL TRUE)
set(THRIFT_INSTALL ${EXTERNAL_PROJECTS_BASE}/Install/thrift)
set(THRIFT_INCLUDE_DIR ${THRIFT_INSTALL}/include CACHE PATH "thrift include path" FORCE)
set(THRIFT_LIBS ${THRIFT_INSTALL}/lib/libthrift.so CACHE FILEPATH "thrift library" FORCE)
set(THRIFT_STATIC_LIB ${THRIFT_INSTALL}/lib/libthrift.a CACHE FILEPATH "thrift static lib" FORCE)
set(THRIFT_NB_STATIC_LIB ${THRIFT_INSTALL}/lib/libthriftnb.a CACHE FILEPATH "thrift non blocking static lib" FORCE)
set(THRIFT_COMPILER ${THRIFT_INSTALL}/bin/thrift CACHE FILEPATH "thrift compiler" FORCE)

message(STATUS "Thrift include dir: ${THRIFT_INCLUDE_DIR}")
message(STATUS "Thrift library path: ${THRIFT_LIBS}")
message(STATUS "Thrift static library: ${THRIFT_STATIC_LIB}")
message(STATUS "Thrift static non-blocking library: ${THRIFT_NB_STATIC_LIB}")
message(STATUS "Thrift compiler: ${THRIFT_COMPILER}")
