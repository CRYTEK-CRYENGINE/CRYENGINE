# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.


# This file sets the basic flags for the Windows Resource Compiler.
# It also loads the available platform file for the system-compiler
# if it exists.

# make sure we don't use CMAKE_BASE_NAME from somewhere else
set(CMAKE_BASE_NAME)
if(CMAKE_RC_COMPILER MATCHES "windres[^/]*$")
 set(CMAKE_BASE_NAME "windres")
else()
 get_filename_component(CMAKE_BASE_NAME ${CMAKE_RC_COMPILER} NAME_WE)
endif()
set(CMAKE_SYSTEM_AND_RC_COMPILER_INFO_FILE
  ${CMAKE_ROOT}/Modules/Platform/${CMAKE_SYSTEM_NAME}-${CMAKE_BASE_NAME}.cmake)
include(Platform/${CMAKE_SYSTEM_NAME}-${CMAKE_BASE_NAME} OPTIONAL)

set(CMAKE_RC_FLAGS_INIT "$ENV{RCFLAGS} ${CMAKE_RC_FLAGS_INIT}")

foreach(c "" _DEBUG _RELEASE _MINSIZEREL _RELWITHDEBINFO)
  string(STRIP "${CMAKE_RC_FLAGS${c}_INIT}" CMAKE_RC_FLAGS${c}_INIT)
endforeach()

set (CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS_INIT}" CACHE STRING
     "Flags for Windows Resource Compiler.")

if(NOT CMAKE_NOT_USING_CONFIG_FLAGS)
  set (CMAKE_RC_FLAGS_DEBUG "${CMAKE_RC_FLAGS_DEBUG_INIT}" CACHE STRING
    "Flags for Windows Resource Compiler during debug builds.")
  set (CMAKE_RC_FLAGS_MINSIZEREL "${CMAKE_RC_FLAGS_MINSIZEREL_INIT}" CACHE STRING
    "Flags for Windows Resource Compiler during release builds for minimum size.")
  set (CMAKE_RC_FLAGS_RELEASE "${CMAKE_RC_FLAGS_RELEASE_INIT}" CACHE STRING
    "Flags for Windows Resource Compiler during release builds.")
  set (CMAKE_RC_FLAGS_RELWITHDEBINFO "${CMAKE_RC_FLAGS_RELWITHDEBINFO_INIT}" CACHE STRING
    "Flags for Windows Resource Compiler during release builds with debug info.")
endif()

# These are the only types of flags that should be passed to the rc
# command, if COMPILE_FLAGS is used on a target this will be used
# to filter out any other flags
set(CMAKE_RC_FLAG_REGEX "^[-/](D|I)")

# now define the following rule variables
# CMAKE_RC_COMPILE_OBJECT
set(CMAKE_INCLUDE_FLAG_RC "-I")
# compile a Resource file into an object file
if(NOT CMAKE_RC_COMPILE_OBJECT)
  set(CMAKE_RC_COMPILE_OBJECT
    "<CMAKE_RC_COMPILER> <DEFINES> <INCLUDES> <FLAGS> /fo<OBJECT> <SOURCE>")
endif()

mark_as_advanced(
CMAKE_RC_FLAGS
CMAKE_RC_FLAGS_DEBUG
CMAKE_RC_FLAGS_MINSIZEREL
CMAKE_RC_FLAGS_RELEASE
CMAKE_RC_FLAGS_RELWITHDEBINFO
)
# set this variable so we can avoid loading this more than once.
set(CMAKE_RC_INFORMATION_LOADED 1)
