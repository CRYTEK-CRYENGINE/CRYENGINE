# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#.rst:
# CheckIncludeFile
# ----------------
#
# Provides a macro to check if a header file can be included in ``C``.
#
# .. command:: CHECK_INCLUDE_FILE
#
#   ::
#
#     CHECK_INCLUDE_FILE(<include> <variable> [<flags>])
#
#   Check if the given ``<include>`` file may be included in a ``C``
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
# ``CMAKE_REQUIRED_QUIET``
#   execute quietly without messages
#
# See the :module:`CheckIncludeFiles` module to check for multiple headers
# at once.  See the :module:`CheckIncludeFileCXX` module to check for headers
# using the ``CXX`` language.

macro(CHECK_INCLUDE_FILE INCLUDE VARIABLE)
  if(NOT DEFINED "${VARIABLE}")
    if(CMAKE_REQUIRED_INCLUDES)
      set(CHECK_INCLUDE_FILE_C_INCLUDE_DIRS "-DINCLUDE_DIRECTORIES=${CMAKE_REQUIRED_INCLUDES}")
    else()
      set(CHECK_INCLUDE_FILE_C_INCLUDE_DIRS)
    endif()
    set(MACRO_CHECK_INCLUDE_FILE_FLAGS ${CMAKE_REQUIRED_FLAGS})
    set(CHECK_INCLUDE_FILE_VAR ${INCLUDE})
    configure_file(${CMAKE_ROOT}/Modules/CheckIncludeFile.c.in
      ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp/CheckIncludeFile.c)
    if(NOT CMAKE_REQUIRED_QUIET)
      message(STATUS "Looking for ${INCLUDE}")
    endif()
    if(${ARGC} EQUAL 3)
      set(CMAKE_C_FLAGS_SAVE ${CMAKE_C_FLAGS})
      string(APPEND CMAKE_C_FLAGS " ${ARGV2}")
    endif()

    try_compile(${VARIABLE}
      ${CMAKE_BINARY_DIR}
      ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp/CheckIncludeFile.c
      COMPILE_DEFINITIONS ${CMAKE_REQUIRED_DEFINITIONS}
      CMAKE_FLAGS
      -DCOMPILE_DEFINITIONS:STRING=${MACRO_CHECK_INCLUDE_FILE_FLAGS}
      "${CHECK_INCLUDE_FILE_C_INCLUDE_DIRS}"
      OUTPUT_VARIABLE OUTPUT)

    if(${ARGC} EQUAL 3)
      set(CMAKE_C_FLAGS ${CMAKE_C_FLAGS_SAVE})
    endif()

    if(${VARIABLE})
      if(NOT CMAKE_REQUIRED_QUIET)
        message(STATUS "Looking for ${INCLUDE} - found")
      endif()
      set(${VARIABLE} 1 CACHE INTERNAL "Have include ${INCLUDE}")
      file(APPEND ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeOutput.log
        "Determining if the include file ${INCLUDE} "
        "exists passed with the following output:\n"
        "${OUTPUT}\n\n")
    else()
      if(NOT CMAKE_REQUIRED_QUIET)
        message(STATUS "Looking for ${INCLUDE} - not found")
      endif()
      set(${VARIABLE} "" CACHE INTERNAL "Have include ${INCLUDE}")
      file(APPEND ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeError.log
        "Determining if the include file ${INCLUDE} "
        "exists failed with the following output:\n"
        "${OUTPUT}\n\n")
    endif()
  endif()
endmacro()
