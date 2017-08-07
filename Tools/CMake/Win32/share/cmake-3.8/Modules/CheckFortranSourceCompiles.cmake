# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#.rst:
# CheckFortranSourceCompiles
# --------------------------
#
# Check if given Fortran source compiles and links into an executable::
#
#   CHECK_Fortran_SOURCE_COMPILES(<code> <var> [FAIL_REGEX <fail-regex>]
#                                 [SRC_EXT <ext>])
#
# The arguments are:
#
# ``<code>``
#   Source code to try to compile.  It must define a PROGRAM entry point.
# ``<var>``
#   Variable to store whether the source code compiled.
#   Will be created as an internal cache variable.
# ``FAIL_REGEX <fail-regex>``
#   Fail if test output matches this regex.
# ``SRC_EXT <ext>``
#   Use source extension ``.<ext>`` instead of the default ``.F``.
#
# The following variables may be set before calling this macro to modify
# the way the check is run::
#
#   CMAKE_REQUIRED_FLAGS = string of compile command line flags
#   CMAKE_REQUIRED_DEFINITIONS = list of macros to define (-DFOO=bar)
#   CMAKE_REQUIRED_INCLUDES = list of include directories
#   CMAKE_REQUIRED_LIBRARIES = list of libraries to link
#   CMAKE_REQUIRED_QUIET = execute quietly without messages

macro(CHECK_Fortran_SOURCE_COMPILES SOURCE VAR)
  if(NOT DEFINED "${VAR}")
    set(_FAIL_REGEX)
    set(_SRC_EXT)
    set(_key)
    foreach(arg ${ARGN})
      if("${arg}" MATCHES "^(FAIL_REGEX|SRC_EXT)$")
        set(_key "${arg}")
      elseif(_key)
        list(APPEND _${_key} "${arg}")
      else()
        message(FATAL_ERROR "Unknown argument:\n  ${arg}\n")
      endif()
    endforeach()
    if(NOT _SRC_EXT)
      set(_SRC_EXT F)
    endif()
    set(MACRO_CHECK_FUNCTION_DEFINITIONS
      "-D${VAR} ${CMAKE_REQUIRED_FLAGS}")
    if(CMAKE_REQUIRED_LIBRARIES)
      set(CHECK_Fortran_SOURCE_COMPILES_ADD_LIBRARIES
        LINK_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES})
    else()
      set(CHECK_Fortran_SOURCE_COMPILES_ADD_LIBRARIES)
    endif()
    if(CMAKE_REQUIRED_INCLUDES)
      set(CHECK_Fortran_SOURCE_COMPILES_ADD_INCLUDES
        "-DINCLUDE_DIRECTORIES:STRING=${CMAKE_REQUIRED_INCLUDES}")
    else()
      set(CHECK_Fortran_SOURCE_COMPILES_ADD_INCLUDES)
    endif()
    file(WRITE "${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp/src.${_SRC_EXT}"
      "${SOURCE}\n")

    if(NOT CMAKE_REQUIRED_QUIET)
      message(STATUS "Performing Test ${VAR}")
    endif()
    try_compile(${VAR}
      ${CMAKE_BINARY_DIR}
      ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp/src.${_SRC_EXT}
      COMPILE_DEFINITIONS ${CMAKE_REQUIRED_DEFINITIONS}
      ${CHECK_Fortran_SOURCE_COMPILES_ADD_LIBRARIES}
      CMAKE_FLAGS -DCOMPILE_DEFINITIONS:STRING=${MACRO_CHECK_FUNCTION_DEFINITIONS}
      "${CHECK_Fortran_SOURCE_COMPILES_ADD_INCLUDES}"
      OUTPUT_VARIABLE OUTPUT)

    foreach(_regex ${_FAIL_REGEX})
      if("${OUTPUT}" MATCHES "${_regex}")
        set(${VAR} 0)
      endif()
    endforeach()

    if(${VAR})
      set(${VAR} 1 CACHE INTERNAL "Test ${VAR}")
      if(NOT CMAKE_REQUIRED_QUIET)
        message(STATUS "Performing Test ${VAR} - Success")
      endif()
      file(APPEND ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeOutput.log
        "Performing Fortran SOURCE FILE Test ${VAR} succeeded with the following output:\n"
        "${OUTPUT}\n"
        "Source file was:\n${SOURCE}\n")
    else()
      if(NOT CMAKE_REQUIRED_QUIET)
        message(STATUS "Performing Test ${VAR} - Failed")
      endif()
      set(${VAR} "" CACHE INTERNAL "Test ${VAR}")
      file(APPEND ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeError.log
        "Performing Fortran SOURCE FILE Test ${VAR} failed with the following output:\n"
        "${OUTPUT}\n"
        "Source file was:\n${SOURCE}\n")
    endif()
  endif()
endmacro()
