# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.


# This module is shared by multiple languages; use include blocker.
if(__WINDOWS_OPENWATCOM)
  return()
endif()
set(__WINDOWS_OPENWATCOM 1)

set(CMAKE_LIBRARY_PATH_FLAG "libpath ")
set(CMAKE_LINK_LIBRARY_FLAG "library ")
set(CMAKE_LINK_LIBRARY_FILE_FLAG "library")

if(CMAKE_VERBOSE_MAKEFILE)
  set(CMAKE_WCL_QUIET)
  set(CMAKE_WLINK_QUIET)
  set(CMAKE_LIB_QUIET)
else()
  set(CMAKE_WCL_QUIET "-zq")
  set(CMAKE_WLINK_QUIET "option quiet")
  set(CMAKE_LIB_QUIET "-q")
endif()

string(APPEND CMAKE_EXE_LINKER_FLAGS_INIT " ")
set(CMAKE_CREATE_WIN32_EXE "system nt_win" )
set(CMAKE_CREATE_CONSOLE_EXE "system nt" )
string(APPEND CMAKE_SHARED_LINKER_FLAGS_INIT " system nt_dll")
string(APPEND CMAKE_MODULE_LINKER_FLAGS_INIT " system nt_dll")
foreach(type SHARED MODULE EXE)
  string(APPEND CMAKE_${type}_LINKER_FLAGS_DEBUG_INIT " debug all opt map")
  string(APPEND CMAKE_${type}_LINKER_FLAGS_RELWITHDEBINFO_INIT " debug all opt map")
endforeach()

set(CMAKE_C_COMPILE_OPTIONS_DLL "-bd") # Note: This variable is a ';' separated list
set(CMAKE_SHARED_LIBRARY_C_FLAGS "-bd") # ... while this is a space separated string.

set(CMAKE_RC_COMPILER "rc" )

set(CMAKE_BUILD_TYPE_INIT Debug)

# single/multi-threaded                 /-bm
# static/DLL run-time libraries         /-br
# default is setup for multi-threaded + DLL run-time libraries
string(APPEND CMAKE_C_FLAGS_INIT " -bt=nt -w3 -dWIN32 -br -bm")
string(APPEND CMAKE_CXX_FLAGS_INIT " -bt=nt -xs -w3 -dWIN32 -br -bm")
foreach(lang C CXX)
  string(APPEND CMAKE_${lang}_FLAGS_DEBUG_INIT " -d2")
  string(APPEND CMAKE_${lang}_FLAGS_MINSIZEREL_INIT " -s -os -d0 -dNDEBUG")
  string(APPEND CMAKE_${lang}_FLAGS_RELEASE_INIT " -s -ot -d0 -dNDEBUG")
  string(APPEND CMAKE_${lang}_FLAGS_RELWITHDEBINFO_INIT " -s -ot -d1 -dNDEBUG")
endforeach()

foreach(type CREATE_SHARED_LIBRARY CREATE_SHARED_MODULE LINK_EXECUTABLE)
  set(CMAKE_C_${type}_USE_WATCOM_QUOTE 1)
  set(CMAKE_CXX_${type}_USE_WATCOM_QUOTE 1)
endforeach()

set(CMAKE_C_CREATE_IMPORT_LIBRARY
  "wlib -c -q -n -b <TARGET_IMPLIB> +<TARGET_QUOTED>")
set(CMAKE_CXX_CREATE_IMPORT_LIBRARY ${CMAKE_C_CREATE_IMPORT_LIBRARY})

set(CMAKE_C_LINK_EXECUTABLE
    "wlink ${CMAKE_START_TEMP_FILE} ${CMAKE_WLINK_QUIET} name <TARGET> <LINK_FLAGS> file {<OBJECTS>} <LINK_LIBRARIES> ${CMAKE_END_TEMP_FILE}")


set(CMAKE_CXX_LINK_EXECUTABLE ${CMAKE_C_LINK_EXECUTABLE})

# compile a C++ file into an object file
set(CMAKE_CXX_COMPILE_OBJECT
    "<CMAKE_CXX_COMPILER> ${CMAKE_START_TEMP_FILE} ${CMAKE_WCL_QUIET} -d+ <DEFINES> <INCLUDES> <FLAGS> -fo<OBJECT> -c -cc++ <SOURCE>${CMAKE_END_TEMP_FILE}")

# compile a C file into an object file
set(CMAKE_C_COMPILE_OBJECT
    "<CMAKE_C_COMPILER> ${CMAKE_START_TEMP_FILE} ${CMAKE_WCL_QUIET} -d+ <DEFINES> <INCLUDES> <FLAGS> -fo<OBJECT> -c -cc <SOURCE>${CMAKE_END_TEMP_FILE}")

# preprocess a C source file
set(CMAKE_C_CREATE_PREPROCESSED_SOURCE
    "<CMAKE_C_COMPILER> ${CMAKE_START_TEMP_FILE} ${CMAKE_WCL_QUIET} -d+ <DEFINES> <INCLUDES> <FLAGS> -fo<PREPROCESSED_SOURCE> -pl -cc <SOURCE>${CMAKE_END_TEMP_FILE}")

# preprocess a C++ source file
set(CMAKE_CXX_CREATE_PREPROCESSED_SOURCE
    "<CMAKE_CXX_COMPILER> ${CMAKE_START_TEMP_FILE} ${CMAKE_WCL_QUIET} -d+ <DEFINES> <INCLUDES> <FLAGS> -fo<PREPROCESSED_SOURCE> -pl -cc++ <SOURCE>${CMAKE_END_TEMP_FILE}")

set(CMAKE_CXX_CREATE_SHARED_LIBRARY
 "wlink ${CMAKE_START_TEMP_FILE} ${CMAKE_WLINK_QUIET} name <TARGET> <LINK_FLAGS> option implib=<TARGET_IMPLIB> file {<OBJECTS>} <LINK_LIBRARIES> ${CMAKE_END_TEMP_FILE}")
string(REPLACE " option implib=<TARGET_IMPLIB>" ""
  CMAKE_CXX_CREATE_SHARED_MODULE "${CMAKE_CXX_CREATE_SHARED_LIBRARY}")

# create a C shared library
set(CMAKE_C_CREATE_SHARED_LIBRARY ${CMAKE_CXX_CREATE_SHARED_LIBRARY})

# create a C shared module
set(CMAKE_C_CREATE_SHARED_MODULE ${CMAKE_CXX_CREATE_SHARED_MODULE})

# create a C++ static library
set(CMAKE_CXX_CREATE_STATIC_LIBRARY  "wlib ${CMAKE_LIB_QUIET} -c -n -b <TARGET_QUOTED> <LINK_FLAGS> <OBJECTS> ")

# create a C static library
set(CMAKE_C_CREATE_STATIC_LIBRARY ${CMAKE_CXX_CREATE_STATIC_LIBRARY})

if(NOT _CMAKE_WATCOM_VERSION)
  set(_CMAKE_WATCOM_VERSION 1)
  if(CMAKE_C_COMPILER_VERSION)
    set(_compiler_version ${CMAKE_C_COMPILER_VERSION})
    set(_compiler_id ${CMAKE_C_COMPILER_ID})
  else()
    set(_compiler_version ${CMAKE_CXX_COMPILER_VERSION})
    set(_compiler_id ${CMAKE_CXX_COMPILER_ID})
  endif()
  set(WATCOM16)
  set(WATCOM17)
  set(WATCOM18)
  set(WATCOM19)
  if("${_compiler_id}" STREQUAL "OpenWatcom")
    if("${_compiler_version}" VERSION_LESS 1.7)
      set(WATCOM16 1)
    endif()
    if("${_compiler_version}" VERSION_EQUAL 1.7)
      set(WATCOM17 1)
    endif()
    if("${_compiler_version}" VERSION_EQUAL 1.8)
      set(WATCOM18 1)
    endif()
    if("${_compiler_version}" VERSION_EQUAL 1.9)
      set(WATCOM19 1)
    endif()
  endif()
endif()
