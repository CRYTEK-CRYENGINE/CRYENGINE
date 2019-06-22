# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#.rst:
# FindLAPACK
# ----------
#
# Find LAPACK library
#
# This module finds an installed fortran library that implements the
# LAPACK linear-algebra interface (see http://www.netlib.org/lapack/).
#
# The approach follows that taken for the autoconf macro file,
# acx_lapack.m4 (distributed at
# http://ac-archive.sourceforge.net/ac-archive/acx_lapack.html).
#
# This module sets the following variables:
#
# ::
#
#   LAPACK_FOUND - set to true if a library implementing the LAPACK interface
#     is found
#   LAPACK_LINKER_FLAGS - uncached list of required linker flags (excluding -l
#     and -L).
#   LAPACK_LIBRARIES - uncached list of libraries (using full path name) to
#     link against to use LAPACK
#   LAPACK95_LIBRARIES - uncached list of libraries (using full path name) to
#     link against to use LAPACK95
#   LAPACK95_FOUND - set to true if a library implementing the LAPACK f95
#     interface is found
#   BLA_STATIC  if set on this determines what kind of linkage we do (static)
#   BLA_VENDOR  if set checks only the specified vendor, if not set checks
#      all the possibilities
#   BLA_F95     if set on tries to find the f95 interfaces for BLAS/LAPACK
#
# List of vendors (BLA_VENDOR) valid in this module:
#
# * Intel(mkl)
# * OpenBLAS
# * FLAME
# * ACML
# * Apple
# * NAS
# * Generic
#

set(_lapack_ORIG_CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_FIND_LIBRARY_SUFFIXES})

# Check the language being used
if( NOT (CMAKE_C_COMPILER_LOADED OR CMAKE_CXX_COMPILER_LOADED OR CMAKE_Fortran_COMPILER_LOADED) )
  if(LAPACK_FIND_REQUIRED)
    message(FATAL_ERROR "FindLAPACK requires Fortran, C, or C++ to be enabled.")
  else()
    message(STATUS "Looking for LAPACK... - NOT found (Unsupported languages)")
    return()
  endif()
endif()

if (CMAKE_Fortran_COMPILER_LOADED)
include(${CMAKE_CURRENT_LIST_DIR}/CheckFortranFunctionExists.cmake)
else ()
include(${CMAKE_CURRENT_LIST_DIR}/CheckFunctionExists.cmake)
endif ()
include(${CMAKE_CURRENT_LIST_DIR}/CMakePushCheckState.cmake)

cmake_push_check_state()
set(CMAKE_REQUIRED_QUIET ${LAPACK_FIND_QUIETLY})

set(LAPACK_FOUND FALSE)
set(LAPACK95_FOUND FALSE)

# TODO: move this stuff to separate module

macro(Check_Lapack_Libraries LIBRARIES _prefix _name _flags _list _blas _threads)
# This macro checks for the existence of the combination of fortran libraries
# given by _list.  If the combination is found, this macro checks (using the
# Check_Fortran_Function_Exists macro) whether can link against that library
# combination using the name of a routine given by _name using the linker
# flags given by _flags.  If the combination of libraries is found and passes
# the link test, LIBRARIES is set to the list of complete library paths that
# have been found.  Otherwise, LIBRARIES is set to FALSE.

# N.B. _prefix is the prefix applied to the names of all cached variables that
# are generated internally and marked advanced by this macro.

set(_libraries_work TRUE)
set(${LIBRARIES})
set(_combined_name)
if (NOT _libdir)
  if (WIN32)
    set(_libdir ENV LIB)
  elseif (APPLE)
    set(_libdir ENV DYLD_LIBRARY_PATH)
  else ()
    set(_libdir ENV LD_LIBRARY_PATH)
  endif ()
endif ()
foreach(_library ${_list})
  set(_combined_name ${_combined_name}_${_library})

  if(_libraries_work)
    if (BLA_STATIC)
      if (WIN32)
        set(CMAKE_FIND_LIBRARY_SUFFIXES .lib ${CMAKE_FIND_LIBRARY_SUFFIXES})
      endif ()
      if (APPLE)
        set(CMAKE_FIND_LIBRARY_SUFFIXES .lib ${CMAKE_FIND_LIBRARY_SUFFIXES})
      else ()
        set(CMAKE_FIND_LIBRARY_SUFFIXES .a ${CMAKE_FIND_LIBRARY_SUFFIXES})
      endif ()
    else ()
			if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
        # for ubuntu's libblas3gf and liblapack3gf packages
        set(CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_FIND_LIBRARY_SUFFIXES} .so.3gf)
      endif ()
    endif ()
    find_library(${_prefix}_${_library}_LIBRARY
      NAMES ${_library}
      PATHS ${_libdir}
      )
    mark_as_advanced(${_prefix}_${_library}_LIBRARY)
    set(${LIBRARIES} ${${LIBRARIES}} ${${_prefix}_${_library}_LIBRARY})
    set(_libraries_work ${${_prefix}_${_library}_LIBRARY})
  endif()
endforeach()

if(_libraries_work)
  # Test this combination of libraries.
  if(UNIX AND BLA_STATIC)
    set(CMAKE_REQUIRED_LIBRARIES ${_flags} "-Wl,--start-group" ${${LIBRARIES}} ${_blas} "-Wl,--end-group" ${_threads})
  else()
    set(CMAKE_REQUIRED_LIBRARIES ${_flags} ${${LIBRARIES}} ${_blas} ${_threads})
  endif()
#  message("DEBUG: CMAKE_REQUIRED_LIBRARIES = ${CMAKE_REQUIRED_LIBRARIES}")
  if (NOT CMAKE_Fortran_COMPILER_LOADED)
    check_function_exists("${_name}_" ${_prefix}${_combined_name}_WORKS)
  else ()
    check_fortran_function_exists(${_name} ${_prefix}${_combined_name}_WORKS)
  endif ()
  set(CMAKE_REQUIRED_LIBRARIES)
  mark_as_advanced(${_prefix}${_combined_name}_WORKS)
  set(_libraries_work ${${_prefix}${_combined_name}_WORKS})
  #message("DEBUG: ${LIBRARIES} = ${${LIBRARIES}}")
endif()

 if(_libraries_work)
   set(${LIBRARIES} ${${LIBRARIES}} ${_blas} ${_threads})
 else()
    set(${LIBRARIES} FALSE)
 endif()

endmacro()


set(LAPACK_LINKER_FLAGS)
set(LAPACK_LIBRARIES)
set(LAPACK95_LIBRARIES)


if(LAPACK_FIND_QUIETLY OR NOT LAPACK_FIND_REQUIRED)
  find_package(BLAS)
else()
  find_package(BLAS REQUIRED)
endif()


if(BLAS_FOUND)
  set(LAPACK_LINKER_FLAGS ${BLAS_LINKER_FLAGS})
  if (NOT $ENV{BLA_VENDOR} STREQUAL "")
    set(BLA_VENDOR $ENV{BLA_VENDOR})
  else ()
    if(NOT BLA_VENDOR)
      set(BLA_VENDOR "All")
    endif()
  endif ()

if (BLA_VENDOR STREQUAL "Goto" OR BLA_VENDOR STREQUAL "All")
 if(NOT LAPACK_LIBRARIES)
  check_lapack_libraries(
  LAPACK_LIBRARIES
  LAPACK
  cheev
  ""
  "goto2"
  "${BLAS_LIBRARIES}"
  ""
  )
 endif()
endif ()

if (BLA_VENDOR STREQUAL "OpenBLAS" OR BLA_VENDOR STREQUAL "All")
 if(NOT LAPACK_LIBRARIES)
  check_lapack_libraries(
  LAPACK_LIBRARIES
  LAPACK
  cheev
  ""
  "openblas"
  "${BLAS_LIBRARIES}"
  ""
  )
 endif()
endif ()

if (BLA_VENDOR STREQUAL "FLAME" OR BLA_VENDOR STREQUAL "All")
 if(NOT LAPACK_LIBRARIES)
  check_lapack_libraries(
  LAPACK_LIBRARIES
  LAPACK
  cheev
  ""
  "flame"
  "${BLAS_LIBRARIES}"
  ""
  )
 endif()
endif ()

#acml lapack
 if (BLA_VENDOR MATCHES "ACML" OR BLA_VENDOR STREQUAL "All")
   if (BLAS_LIBRARIES MATCHES ".+acml.+")
     set (LAPACK_LIBRARIES ${BLAS_LIBRARIES})
   endif ()
 endif ()

# Apple LAPACK library?
if (BLA_VENDOR STREQUAL "Apple" OR BLA_VENDOR STREQUAL "All")
 if(NOT LAPACK_LIBRARIES)
  check_lapack_libraries(
  LAPACK_LIBRARIES
  LAPACK
  cheev
  ""
  "Accelerate"
  "${BLAS_LIBRARIES}"
  ""
  )
 endif()
endif ()
if (BLA_VENDOR STREQUAL "NAS" OR BLA_VENDOR STREQUAL "All")
  if ( NOT LAPACK_LIBRARIES )
    check_lapack_libraries(
    LAPACK_LIBRARIES
    LAPACK
    cheev
    ""
    "vecLib"
    "${BLAS_LIBRARIES}"
    ""
    )
  endif ()
endif ()
# Generic LAPACK library?
if (BLA_VENDOR STREQUAL "Generic" OR
    BLA_VENDOR STREQUAL "ATLAS" OR
    BLA_VENDOR STREQUAL "All")
  if ( NOT LAPACK_LIBRARIES )
    check_lapack_libraries(
    LAPACK_LIBRARIES
    LAPACK
    cheev
    ""
    "lapack"
    "${BLAS_LIBRARIES}"
    ""
    )
  endif ()
endif ()
#intel lapack
if (BLA_VENDOR MATCHES "Intel" OR BLA_VENDOR STREQUAL "All")
  if (NOT WIN32)
    set(LM "-lm")
  endif ()
  if (CMAKE_C_COMPILER_LOADED OR CMAKE_CXX_COMPILER_LOADED)
    if(LAPACK_FIND_QUIETLY OR NOT LAPACK_FIND_REQUIRED)
      find_PACKAGE(Threads)
    else()
      find_package(Threads REQUIRED)
    endif()

    set(LAPACK_SEARCH_LIBS "")

    if (BLA_F95)
      set(LAPACK_mkl_SEARCH_SYMBOL "CHEEV")
      set(_LIBRARIES LAPACK95_LIBRARIES)
      set(_BLAS_LIBRARIES ${BLAS95_LIBRARIES})

      # old
      list(APPEND LAPACK_SEARCH_LIBS
        "mkl_lapack95")
      # new >= 10.3
      list(APPEND LAPACK_SEARCH_LIBS
        "mkl_intel_c")
      list(APPEND LAPACK_SEARCH_LIBS
        "mkl_intel_lp64")
    else()
      set(LAPACK_mkl_SEARCH_SYMBOL "cheev")
      set(_LIBRARIES LAPACK_LIBRARIES)
      set(_BLAS_LIBRARIES ${BLAS_LIBRARIES})

      # old
      list(APPEND LAPACK_SEARCH_LIBS
        "mkl_lapack")
      # new >= 10.3
      list(APPEND LAPACK_SEARCH_LIBS
        "mkl_gf_lp64")
    endif()

    # First try empty lapack libs
    if (NOT ${_LIBRARIES})
      check_lapack_libraries(
        ${_LIBRARIES}
        BLAS
        ${LAPACK_mkl_SEARCH_SYMBOL}
        ""
        ""
        "${_BLAS_LIBRARIES}"
        "${CMAKE_THREAD_LIBS_INIT};${LM}"
        )
    endif ()
    # Then try the search libs
    foreach (IT ${LAPACK_SEARCH_LIBS})
      if (NOT ${_LIBRARIES})
        check_lapack_libraries(
          ${_LIBRARIES}
          BLAS
          ${LAPACK_mkl_SEARCH_SYMBOL}
          ""
          "${IT}"
          "${_BLAS_LIBRARIES}"
          "${CMAKE_THREAD_LIBS_INIT};${LM}"
          )
      endif ()
    endforeach ()
  endif ()
endif()
else()
  message(STATUS "LAPACK requires BLAS")
endif()

if(BLA_F95)
 if(LAPACK95_LIBRARIES)
  set(LAPACK95_FOUND TRUE)
 else()
  set(LAPACK95_FOUND FALSE)
 endif()
 if(NOT LAPACK_FIND_QUIETLY)
  if(LAPACK95_FOUND)
    message(STATUS "A library with LAPACK95 API found.")
  else()
    if(LAPACK_FIND_REQUIRED)
      message(FATAL_ERROR
      "A required library with LAPACK95 API not found. Please specify library location."
      )
    else()
      message(STATUS
      "A library with LAPACK95 API not found. Please specify library location."
      )
    endif()
  endif()
 endif()
 set(LAPACK_FOUND "${LAPACK95_FOUND}")
 set(LAPACK_LIBRARIES "${LAPACK95_LIBRARIES}")
else()
 if(LAPACK_LIBRARIES)
  set(LAPACK_FOUND TRUE)
 else()
  set(LAPACK_FOUND FALSE)
 endif()

 if(NOT LAPACK_FIND_QUIETLY)
  if(LAPACK_FOUND)
    message(STATUS "A library with LAPACK API found.")
  else()
    if(LAPACK_FIND_REQUIRED)
      message(FATAL_ERROR
      "A required library with LAPACK API not found. Please specify library location."
      )
    else()
      message(STATUS
      "A library with LAPACK API not found. Please specify library location."
      )
    endif()
  endif()
 endif()
endif()

cmake_pop_check_state()
set(CMAKE_FIND_LIBRARY_SUFFIXES ${_lapack_ORIG_CMAKE_FIND_LIBRARY_SUFFIXES})
