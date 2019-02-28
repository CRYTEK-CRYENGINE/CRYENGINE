# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#.rst:
# CheckFortranFunctionExists
# --------------------------
#
# macro which checks if the Fortran function exists
#
# CHECK_FORTRAN_FUNCTION_EXISTS(FUNCTION VARIABLE)
#
# ::
#
#   FUNCTION - the name of the Fortran function
#   VARIABLE - variable to store the result
#              Will be created as an internal cache variable.
#
#
#
# The following variables may be set before calling this macro to modify
# the way the check is run:
#
# ::
#
#   CMAKE_REQUIRED_LIBRARIES = list of libraries to link

macro(CHECK_FORTRAN_FUNCTION_EXISTS FUNCTION VARIABLE)
  if(NOT DEFINED ${VARIABLE})
    message(STATUS "Looking for Fortran ${FUNCTION}")
    if(CMAKE_REQUIRED_LIBRARIES)
      set(CHECK_FUNCTION_EXISTS_ADD_LIBRARIES
        LINK_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES})
    else()
      set(CHECK_FUNCTION_EXISTS_ADD_LIBRARIES)
    endif()
    file(WRITE
    ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp/testFortranCompiler.f
    "
      program TESTFortran
      external ${FUNCTION}
      call ${FUNCTION}()
      end program TESTFortran
    "
    )
    try_compile(${VARIABLE}
    ${CMAKE_BINARY_DIR}
    ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp/testFortranCompiler.f
    ${CHECK_FUNCTION_EXISTS_ADD_LIBRARIES}
    OUTPUT_VARIABLE OUTPUT
    )
#    message(STATUS "${OUTPUT}")
    if(${VARIABLE})
      set(${VARIABLE} 1 CACHE INTERNAL "Have Fortran function ${FUNCTION}")
      message(STATUS "Looking for Fortran ${FUNCTION} - found")
      file(APPEND ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeOutput.log
        "Determining if the Fortran ${FUNCTION} exists passed with the following output:\n"
        "${OUTPUT}\n\n")
    else()
      message(STATUS "Looking for Fortran ${FUNCTION} - not found")
      set(${VARIABLE} "" CACHE INTERNAL "Have Fortran function ${FUNCTION}")
      file(APPEND ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeError.log
        "Determining if the Fortran ${FUNCTION} exists failed with the following output:\n"
        "${OUTPUT}\n\n")
    endif()
  endif()
endmacro()
