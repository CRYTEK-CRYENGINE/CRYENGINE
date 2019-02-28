# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#.rst:
# FindSWIG
# --------
#
# Find SWIG
#
# This module finds an installed SWIG.  It sets the following variables:
#
# ::
#
#   SWIG_FOUND - set to true if SWIG is found
#   SWIG_DIR - the directory where swig is installed
#   SWIG_EXECUTABLE - the path to the swig executable
#   SWIG_VERSION   - the version number of the swig executable
#
#
#
# The minimum required version of SWIG can be specified using the
# standard syntax, e.g.  find_package(SWIG 1.1)
#
# All information is collected from the SWIG_EXECUTABLE so the version
# to be found can be changed from the command line by means of setting
# SWIG_EXECUTABLE

find_program(SWIG_EXECUTABLE NAMES swig3.0 swig2.0 swig)

if(SWIG_EXECUTABLE)
  execute_process(COMMAND ${SWIG_EXECUTABLE} -swiglib
    OUTPUT_VARIABLE SWIG_swiglib_output
    ERROR_VARIABLE SWIG_swiglib_error
    RESULT_VARIABLE SWIG_swiglib_result)

  if(SWIG_swiglib_result)
    if(SWIG_FIND_REQUIRED)
      message(SEND_ERROR "Command \"${SWIG_EXECUTABLE} -swiglib\" failed with output:\n${SWIG_swiglib_error}")
    else()
      message(STATUS "Command \"${SWIG_EXECUTABLE} -swiglib\" failed with output:\n${SWIG_swiglib_error}")
    endif()
  else()
    string(REGEX REPLACE "[\n\r]+" ";" SWIG_swiglib_output ${SWIG_swiglib_output})
    find_path(SWIG_DIR swig.swg PATHS ${SWIG_swiglib_output} NO_CMAKE_FIND_ROOT_PATH)
    if(SWIG_DIR)
      set(SWIG_USE_FILE ${CMAKE_CURRENT_LIST_DIR}/UseSWIG.cmake)
      execute_process(COMMAND ${SWIG_EXECUTABLE} -version
        OUTPUT_VARIABLE SWIG_version_output
        ERROR_VARIABLE SWIG_version_output
        RESULT_VARIABLE SWIG_version_result)
      if(SWIG_version_result)
        message(SEND_ERROR "Command \"${SWIG_EXECUTABLE} -version\" failed with output:\n${SWIG_version_output}")
      else()
        string(REGEX REPLACE ".*SWIG Version[^0-9.]*\([0-9.]+\).*" "\\1"
          SWIG_version_output "${SWIG_version_output}")
        set(SWIG_VERSION ${SWIG_version_output} CACHE STRING "Swig version" FORCE)
      endif()
    endif()
  endif()
endif()

include(${CMAKE_CURRENT_LIST_DIR}/FindPackageHandleStandardArgs.cmake)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(SWIG  REQUIRED_VARS SWIG_EXECUTABLE SWIG_DIR
                                        VERSION_VAR SWIG_VERSION )

mark_as_advanced(SWIG_DIR SWIG_VERSION)
