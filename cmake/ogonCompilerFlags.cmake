include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)

macro (checkCFlag FLAG)
	CHECK_C_COMPILER_FLAG("${FLAG}" CFLAG${FLAG})
	if(CFLAG${FLAG})
		  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${FLAG}")
	endif()
endmacro()

macro (checkCXXFlag FLAG)
	CHECK_CXX_COMPILER_FLAG("${FLAG}" CXXFLAG${FLAG})
	if(CXXFLAG${FLAG})
		  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${FLAG}")
	endif()
endmacro()

if(NOT EXPORT_ALL_SYMBOLS)
	CheckCFlag("-fvisibility=hidden")
	CheckCXXFlag("-fvisibility=hidden")
endif()

set(C_WARNING_FLAGS "-Waddress -Warray-bounds -Wchar-subscripts -Wformat -Wreturn-type -Wunused -Wunused-function  -Wunused-label -Wunused-parameter -Wunused-value  -Wunused-variable -Wunreachable-code -Wmissing-field-initializers -Wmissing-format-attribute -Wmissing-braces -Wnonnull -Wvolatile-register-var -Wstrict-aliasing -Wclobbered -Wempty-body -Wignored-qualifiers -Wmissing-parameter-type -Woverride-init -Wsign-compare -Wtype-limits -Wuninitialized -Wcast-qual -Wformat-security -Wimplicit-function-declaration -Wimplicit-int -Wmissing-noreturn -Wredundant-decls -Winline")
set(CXX_WARNING_FLAGS "-Waddress -Warray-bounds -Wchar-subscripts -Wformat -Wreturn-type -Wunused -Wunused-function  -Wunused-label -Wunused-parameter -Wunused-value  -Wunused-variable -Wunreachable-code -Wmissing-field-initializers -Wmissing-format-attribute -Wmissing-braces -Wnonnull -Wvolatile-register-var -Wstrict-aliasing -Wclobbered -Wempty-body -Wignored-qualifiers -Wsign-compare -Wtype-limits -Wuninitialized -Wcast-qual -Wformat-security -Wmissing-noreturn -Wredundant-decls -Winline")
#disabled
#-Wvariadic-macros - to much noise because of wlog ;(
#-Wshadow thrift/protobuf generated files generate a log of noise
#-Wunknown-pragmas - use clang pragmas where required to remove warnings where
#                    where gcc creates warnings
string(REPLACE " " ";" C_WARNING_FLAGS ${C_WARNING_FLAGS})
string(REPLACE " " ";" CXX_WARNING_FLAGS ${CXX_WARNING_FLAGS})

foreach(FLAG ${C_WARNING_FLAGS})
	CheckCFlag(${FLAG})
endforeach()

foreach(FLAG ${CXX_WARNING_FLAGS})
	CheckCXXFlag(${FLAG})
endforeach()

message("Using CFLAGS ${CMAKE_C_FLAGS}")
message("Using CXXFLAGS ${CMAKE_CXX_FLAGS}")

if (ENABLE_WARNING_ERROR)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
endif()
