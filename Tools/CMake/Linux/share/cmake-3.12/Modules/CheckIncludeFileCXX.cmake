# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#.rst:
# CheckIncludeFileCXX
# -------------------
#
# Provides a macro to check if a header file can be included in ``CXX``.
#
# .. command:: CHECK_INCLUDE_FILE_CXX
#
#   ::
#
#     CHECK_INCLUDE_FILE_CXX(<include> <variable> [<flags>])
#
#   Check if the given ``<include>`` file may be included in a ``CXX``
#   source file and store the result in an internal cache entry named
#   ``<variable>``.  The optional third argument may be used to add
#   compilation flags to the check (or use ``CMAKE_REQUIRED_FLAGS`` below).
#
# The following variables may be set before calling this macro to modify
# the way the check is run:
#
# ``CMAKE_REQUIRED_FLAGS``
#   string of compile command line flags
# ``CMAKE_REQUIRED_DEFINITIONS``
#   list of macros to define (-DFOO=bar)
# ``CMAKE_REQUIRED_INCLUDES``
#   list of include directories
# ``CMAKE_REQUIRED_LIBRARIES``
#   A list of libraries to link.  See policy :policy:`CMP0075`.
# ``CMAKE_REQUIRED_QUIET``
#   execute quietly without messages
#
# See modules :module:`CheckIncludeFile` and :module:`CheckIncludeFiles`
# to check for one or more ``C`` headers.

include_guard(GLOBAL)

macro(CHECK_INCLUDE_FILE_CXX INCLUDE VARIABLE)
  if(NOT DEFINED "${VARIABLE}" OR "x${${VARIABLE}}" STREQUAL "x${VARIABLE}")
    if(CMAKE_REQUIRED_INCLUDES)
      set(CHECK_INCLUDE_FILE_CXX_INCLUDE_DIRS "-DINCLUDE_DIRECTORIES=${CMAKE_REQUIRED_INCLUDES}")
    else()
      set(CHECK_INCLUDE_FILE_CXX_INCLUDE_DIRS)
    endif()
    set(MACRO_CHECK_INCLUDE_FILE_FLAGS ${CMAKE_REQUIRED_FLAGS})
    set(CHECK_INCLUDE_FILE_VAR ${INCLUDE})
    configure_file(${CMAKE_ROOT}/Modules/CheckIncludeFile.cxx.in
      ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp/CheckIncludeFile.cxx)
    if(NOT CMAKE_REQUIRED_QUIET)
      message(STATUS "Looking for C++ include ${INCLUDE}")
    endif()
    if(${ARGC} EQUAL 3)
      set(CMAKE_CXX_FLAGS_SAVE ${CMAKE_CXX_FLAGS})
      string(APPEND CMAKE_CXX_FLAGS " ${ARGV2}")
    endif()

    set(_CIF_LINK_LIBRARIES "")
    if(CMAKE_REQUIRED_LIBRARIES)
      cmake_policy(GET CMP0075 _CIF_CMP0075
        PARENT_SCOPE # undocumented, do not use outside of CMake
        )
      if("x${_CIF_CMP0075}x" STREQUAL "xNEWx")
        set(_CIF_LINK_LIBRARIES LINK_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES})
      elseif("x${_CIF_CMP0075}x" STREQUAL "xOLDx")
      elseif(NOT _CIF_CMP0075_WARNED)
        set(_CIF_CMP0075_WARNED 1)
        message(AUTHOR_WARNING
          "Policy CMP0075 is not set: Include file check macros honor CMAKE_REQUIRED_LIBRARIES.  "
          "Run \"cmake --help-policy CMP0075\" for policy details.  "
          "Use the cmake_policy command to set the policy and suppress this warning."
          "\n"
          "CMAKE_REQUIRED_LIBRARIES is set to:\n"
          "  ${CMAKE_REQUIRED_LIBRARIES}\n"
          "For compatibility with CMake 3.11 and below this check is ignoring it."
          )
      endif()
      unset(_CIF_CMP0075)
    endif()

    try_compile(${VARIABLE}
      ${CMAKE_BINARY_DIR}
      ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp/CheckIncludeFile.cxx
      COMPILE_DEFINITIONS ${CMAKE_REQUIRED_DEFINITIONS}
      ${_CIF_LINK_LIBRARIES}
      CMAKE_FLAGS
      -DCOMPILE_DEFINITIONS:STRING=${MACRO_CHECK_INCLUDE_FILE_FLAGS}
      "${CHECK_INCLUDE_FILE_CXX_INCLUDE_DIRS}"
      OUTPUT_VARIABLE OUTPUT)
    unset(_CIF_LINK_LIBRARIES)

    if(${ARGC} EQUAL 3)
      set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS_SAVE})
    endif()

    if(${VARIABLE})
      if(NOT CMAKE_REQUIRED_QUIET)
        message(STATUS "Looking for C++ include ${INCLUDE} - found")
      endif()
      set(${VARIABLE} 1 CACHE INTERNAL "Have include ${INCLUDE}")
      file(APPEND ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeOutput.log
        "Determining if the include file ${INCLUDE} "
        "exists passed with the following output:\n"
        "${OUTPUT}\n\n")
    else()
      if(NOT CMAKE_REQUIRED_QUIET)
        message(STATUS "Looking for C++ include ${INCLUDE} - not found")
      endif()
      set(${VARIABLE} "" CACHE INTERNAL "Have include ${INCLUDE}")
      file(APPEND ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeError.log
        "Determining if the include file ${INCLUDE} "
        "exists failed with the following output:\n"
        "${OUTPUT}\n\n")
    endif()
  endif()
endmacro()
