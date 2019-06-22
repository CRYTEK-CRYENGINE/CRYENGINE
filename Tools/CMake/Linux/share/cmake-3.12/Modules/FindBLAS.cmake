# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#.rst:
# FindBLAS
# --------
#
# Find BLAS library
#
# This module finds an installed fortran library that implements the
# BLAS linear-algebra interface (see http://www.netlib.org/blas/).  The
# list of libraries searched for is taken from the autoconf macro file,
# acx_blas.m4 (distributed at
# http://ac-archive.sourceforge.net/ac-archive/acx_blas.html).
#
# This module sets the following variables:
#
# ::
#
#   BLAS_FOUND - set to true if a library implementing the BLAS interface
#     is found
#   BLAS_LINKER_FLAGS - uncached list of required linker flags (excluding -l
#     and -L).
#   BLAS_LIBRARIES - uncached list of libraries (using full path name) to
#     link against to use BLAS
#   BLAS95_LIBRARIES - uncached list of libraries (using full path name)
#     to link against to use BLAS95 interface
#   BLAS95_FOUND - set to true if a library implementing the BLAS f95 interface
#     is found
#
# The following variables can be used to control this module:
#
# ::
#
#   BLA_STATIC  if set on this determines what kind of linkage we do (static)
#   BLA_VENDOR  if set checks only the specified vendor, if not set checks
#      all the possibilities
#   BLA_F95     if set on tries to find the f95 interfaces for BLAS/LAPACK
#   BLA_PREFER_PKGCONFIG  if set pkg-config will be used to search for a BLAS
#      library first and if one is found that is preferred
#
# List of vendors (BLA_VENDOR) valid in this module:
#
# * Goto
# * OpenBLAS
# * FLAME
# * ATLAS PhiPACK
# * CXML
# * DXML
# * SunPerf
# * SCSL
# * SGIMATH
# * IBMESSL
# * Intel10_32 (intel mkl v10 32 bit)
# * Intel10_64lp (intel mkl v10 64 bit, lp thread model, lp64 model)
# * Intel10_64lp_seq (intel mkl v10 64 bit, sequential code, lp64 model)
# * Intel (older versions of mkl 32 and 64 bit)
# * ACML
# * ACML_MP
# * ACML_GPU
# * Apple
# * NAS
# * Generic
#
# .. note::
#
#   C/CXX should be enabled to use Intel mkl
#

include(${CMAKE_CURRENT_LIST_DIR}/CheckFunctionExists.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/CheckFortranFunctionExists.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/CMakePushCheckState.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/FindPackageHandleStandardArgs.cmake)
cmake_push_check_state()
set(CMAKE_REQUIRED_QUIET ${BLAS_FIND_QUIETLY})

set(_blas_ORIG_CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_FIND_LIBRARY_SUFFIXES})

# Check the language being used
if( NOT (CMAKE_C_COMPILER_LOADED OR CMAKE_CXX_COMPILER_LOADED OR CMAKE_Fortran_COMPILER_LOADED) )
  if(BLAS_FIND_REQUIRED)
    message(FATAL_ERROR "FindBLAS requires Fortran, C, or C++ to be enabled.")
  else()
    message(STATUS "Looking for BLAS... - NOT found (Unsupported languages)")
    return()
  endif()
endif()

if(BLA_PREFER_PKGCONFIG)
  find_package(PkgConfig)
  pkg_check_modules(PKGC_BLAS blas)
  if(PKGC_BLAS_FOUND)
    set(BLAS_LIBRARIES "${PKGC_BLAS_LINK_LIBRARIES}")
    return()
  endif()
endif()

macro(Check_Fortran_Libraries LIBRARIES _prefix _name _flags _list _thread)
# This macro checks for the existence of the combination of fortran libraries
# given by _list.  If the combination is found, this macro checks (using the
# Check_Fortran_Function_Exists macro) whether can link against that library
# combination using the name of a routine given by _name using the linker
# flags given by _flags.  If the combination of libraries is found and passes
# the link test, LIBRARIES is set to the list of complete library paths that
# have been found.  Otherwise, LIBRARIES is set to FALSE.

# N.B. _prefix is the prefix applied to the names of all cached variables that
# are generated internally and marked advanced by this macro.

set(_libdir ${ARGN})

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
  set(CMAKE_REQUIRED_LIBRARIES ${_flags} ${${LIBRARIES}} ${_thread})
#  message("DEBUG: CMAKE_REQUIRED_LIBRARIES = ${CMAKE_REQUIRED_LIBRARIES}")
  if (CMAKE_Fortran_COMPILER_LOADED)
    check_fortran_function_exists("${_name}" ${_prefix}${_combined_name}_WORKS)
  else()
    check_function_exists("${_name}_" ${_prefix}${_combined_name}_WORKS)
  endif()
  set(CMAKE_REQUIRED_LIBRARIES)
  mark_as_advanced(${_prefix}${_combined_name}_WORKS)
  set(_libraries_work ${${_prefix}${_combined_name}_WORKS})
endif()
if(NOT _libraries_work)
  set(${LIBRARIES} FALSE)
endif()
#message("DEBUG: ${LIBRARIES} = ${${LIBRARIES}}")
endmacro()

set(BLAS_LINKER_FLAGS)
set(BLAS_LIBRARIES)
set(BLAS95_LIBRARIES)
if (NOT $ENV{BLA_VENDOR} STREQUAL "")
  set(BLA_VENDOR $ENV{BLA_VENDOR})
else ()
  if(NOT BLA_VENDOR)
    set(BLA_VENDOR "All")
  endif()
endif ()

if (BLA_VENDOR STREQUAL "Goto" OR BLA_VENDOR STREQUAL "All")
 if(NOT BLAS_LIBRARIES)
  # gotoblas (http://www.tacc.utexas.edu/tacc-projects/gotoblas2)
  check_fortran_libraries(
  BLAS_LIBRARIES
  BLAS
  sgemm
  ""
  "goto2"
  ""
  )
 endif()
endif ()

if (BLA_VENDOR STREQUAL "OpenBLAS" OR BLA_VENDOR STREQUAL "All")
 if(NOT BLAS_LIBRARIES)
  # OpenBLAS (http://www.openblas.net)
  check_fortran_libraries(
  BLAS_LIBRARIES
  BLAS
  sgemm
  ""
  "openblas"
  ""
  )
 endif()
endif ()

if (BLA_VENDOR STREQUAL "FLAME" OR BLA_VENDOR STREQUAL "All")
 if(NOT BLAS_LIBRARIES)
  # FLAME's blis library (https://github.com/flame/blis)
  check_fortran_libraries(
  BLAS_LIBRARIES
  BLAS
  sgemm
  ""
  "blis"
  ""
  )
 endif()
endif ()

if (BLA_VENDOR STREQUAL "ATLAS" OR BLA_VENDOR STREQUAL "All")
 if(NOT BLAS_LIBRARIES)
  # BLAS in ATLAS library? (http://math-atlas.sourceforge.net/)
  check_fortran_libraries(
  BLAS_LIBRARIES
  BLAS
  dgemm
  ""
  "f77blas;atlas"
  ""
  )
 endif()
endif ()

# BLAS in PhiPACK libraries? (requires generic BLAS lib, too)
if (BLA_VENDOR STREQUAL "PhiPACK" OR BLA_VENDOR STREQUAL "All")
 if(NOT BLAS_LIBRARIES)
  check_fortran_libraries(
  BLAS_LIBRARIES
  BLAS
  sgemm
  ""
  "sgemm;dgemm;blas"
  ""
  )
 endif()
endif ()

# BLAS in Alpha CXML library?
if (BLA_VENDOR STREQUAL "CXML" OR BLA_VENDOR STREQUAL "All")
 if(NOT BLAS_LIBRARIES)
  check_fortran_libraries(
  BLAS_LIBRARIES
  BLAS
  sgemm
  ""
  "cxml"
  ""
  )
 endif()
endif ()

# BLAS in Alpha DXML library? (now called CXML, see above)
if (BLA_VENDOR STREQUAL "DXML" OR BLA_VENDOR STREQUAL "All")
 if(NOT BLAS_LIBRARIES)
  check_fortran_libraries(
  BLAS_LIBRARIES
  BLAS
  sgemm
  ""
  "dxml"
  ""
  )
 endif()
endif ()

# BLAS in Sun Performance library?
if (BLA_VENDOR STREQUAL "SunPerf" OR BLA_VENDOR STREQUAL "All")
 if(NOT BLAS_LIBRARIES)
  check_fortran_libraries(
  BLAS_LIBRARIES
  BLAS
  sgemm
  "-xlic_lib=sunperf"
  "sunperf;sunmath"
  ""
  )
  if(BLAS_LIBRARIES)
    set(BLAS_LINKER_FLAGS "-xlic_lib=sunperf")
  endif()
 endif()
endif ()

# BLAS in SCSL library?  (SGI/Cray Scientific Library)
if (BLA_VENDOR STREQUAL "SCSL" OR BLA_VENDOR STREQUAL "All")
 if(NOT BLAS_LIBRARIES)
  check_fortran_libraries(
  BLAS_LIBRARIES
  BLAS
  sgemm
  ""
  "scsl"
  ""
  )
 endif()
endif ()

# BLAS in SGIMATH library?
if (BLA_VENDOR STREQUAL "SGIMATH" OR BLA_VENDOR STREQUAL "All")
 if(NOT BLAS_LIBRARIES)
  check_fortran_libraries(
  BLAS_LIBRARIES
  BLAS
  sgemm
  ""
  "complib.sgimath"
  ""
  )
 endif()
endif ()

# BLAS in IBM ESSL library? (requires generic BLAS lib, too)
if (BLA_VENDOR STREQUAL "IBMESSL" OR BLA_VENDOR STREQUAL "All")
 if(NOT BLAS_LIBRARIES)
  check_fortran_libraries(
  BLAS_LIBRARIES
  BLAS
  sgemm
  ""
  "essl;blas"
  ""
  )
 endif()
endif ()

#BLAS in acml library?
if (BLA_VENDOR MATCHES "ACML" OR BLA_VENDOR STREQUAL "All")
 if( ((BLA_VENDOR STREQUAL "ACML") AND (NOT BLAS_ACML_LIB_DIRS)) OR
     ((BLA_VENDOR STREQUAL "ACML_MP") AND (NOT BLAS_ACML_MP_LIB_DIRS)) OR
     ((BLA_VENDOR STREQUAL "ACML_GPU") AND (NOT BLAS_ACML_GPU_LIB_DIRS))
   )
   # try to find acml in "standard" paths
   if( WIN32 )
    file( GLOB _ACML_ROOT "C:/AMD/acml*/ACML-EULA.txt" )
   else()
    file( GLOB _ACML_ROOT "/opt/acml*/ACML-EULA.txt" )
   endif()
   if( WIN32 )
    file( GLOB _ACML_GPU_ROOT "C:/AMD/acml*/GPGPUexamples" )
   else()
    file( GLOB _ACML_GPU_ROOT "/opt/acml*/GPGPUexamples" )
   endif()
   list(GET _ACML_ROOT 0 _ACML_ROOT)
   list(GET _ACML_GPU_ROOT 0 _ACML_GPU_ROOT)
   if( _ACML_ROOT )
    get_filename_component( _ACML_ROOT ${_ACML_ROOT} PATH )
    if( SIZEOF_INTEGER EQUAL 8 )
     set( _ACML_PATH_SUFFIX "_int64" )
    else()
    set( _ACML_PATH_SUFFIX "" )
   endif()
   if( CMAKE_Fortran_COMPILER_ID STREQUAL "Intel" )
    set( _ACML_COMPILER32 "ifort32" )
    set( _ACML_COMPILER64 "ifort64" )
   elseif( CMAKE_Fortran_COMPILER_ID STREQUAL "SunPro" )
    set( _ACML_COMPILER32 "sun32" )
    set( _ACML_COMPILER64 "sun64" )
   elseif( CMAKE_Fortran_COMPILER_ID STREQUAL "PGI" )
    set( _ACML_COMPILER32 "pgi32" )
    if( WIN32 )
     set( _ACML_COMPILER64 "win64" )
    else()
     set( _ACML_COMPILER64 "pgi64" )
    endif()
   elseif( CMAKE_Fortran_COMPILER_ID STREQUAL "Open64" )
    # 32 bit builds not supported on Open64 but for code simplicity
    # We'll just use the same directory twice
    set( _ACML_COMPILER32 "open64_64" )
    set( _ACML_COMPILER64 "open64_64" )
   elseif( CMAKE_Fortran_COMPILER_ID STREQUAL "NAG" )
    set( _ACML_COMPILER32 "nag32" )
    set( _ACML_COMPILER64 "nag64" )
   else()
    set( _ACML_COMPILER32 "gfortran32" )
    set( _ACML_COMPILER64 "gfortran64" )
   endif()

   if( BLA_VENDOR STREQUAL "ACML_MP" )
    set(_ACML_MP_LIB_DIRS
     "${_ACML_ROOT}/${_ACML_COMPILER32}_mp${_ACML_PATH_SUFFIX}/lib"
     "${_ACML_ROOT}/${_ACML_COMPILER64}_mp${_ACML_PATH_SUFFIX}/lib" )
   else()
    set(_ACML_LIB_DIRS
     "${_ACML_ROOT}/${_ACML_COMPILER32}${_ACML_PATH_SUFFIX}/lib"
     "${_ACML_ROOT}/${_ACML_COMPILER64}${_ACML_PATH_SUFFIX}/lib" )
   endif()
  endif()
 elseif(BLAS_${BLA_VENDOR}_LIB_DIRS)
   set(_${BLA_VENDOR}_LIB_DIRS ${BLAS_${BLA_VENDOR}_LIB_DIRS})
 endif()

 if( BLA_VENDOR STREQUAL "ACML_MP" )
  foreach( BLAS_ACML_MP_LIB_DIRS ${_ACML_MP_LIB_DIRS})
   check_fortran_libraries (
     BLAS_LIBRARIES
     BLAS
     sgemm
     "" "acml_mp;acml_mv" "" ${BLAS_ACML_MP_LIB_DIRS}
   )
   if( BLAS_LIBRARIES )
    break()
   endif()
  endforeach()
 elseif( BLA_VENDOR STREQUAL "ACML_GPU" )
  foreach( BLAS_ACML_GPU_LIB_DIRS ${_ACML_GPU_LIB_DIRS})
   check_fortran_libraries (
     BLAS_LIBRARIES
     BLAS
     sgemm
     "" "acml;acml_mv;CALBLAS" "" ${BLAS_ACML_GPU_LIB_DIRS}
   )
   if( BLAS_LIBRARIES )
    break()
   endif()
  endforeach()
 else()
  foreach( BLAS_ACML_LIB_DIRS ${_ACML_LIB_DIRS} )
   check_fortran_libraries (
     BLAS_LIBRARIES
     BLAS
     sgemm
     "" "acml;acml_mv" "" ${BLAS_ACML_LIB_DIRS}
   )
   if( BLAS_LIBRARIES )
    break()
   endif()
  endforeach()
 endif()

 # Either acml or acml_mp should be in LD_LIBRARY_PATH but not both
 if(NOT BLAS_LIBRARIES)
  check_fortran_libraries(
  BLAS_LIBRARIES
  BLAS
  sgemm
  ""
  "acml;acml_mv"
  ""
  )
 endif()
 if(NOT BLAS_LIBRARIES)
  check_fortran_libraries(
  BLAS_LIBRARIES
  BLAS
  sgemm
  ""
  "acml_mp;acml_mv"
  ""
  )
 endif()
 if(NOT BLAS_LIBRARIES)
  check_fortran_libraries(
  BLAS_LIBRARIES
  BLAS
  sgemm
  ""
  "acml;acml_mv;CALBLAS"
  ""
  )
 endif()
endif () # ACML

# Apple BLAS library?
if (BLA_VENDOR STREQUAL "Apple" OR BLA_VENDOR STREQUAL "All")
if(NOT BLAS_LIBRARIES)
  check_fortran_libraries(
  BLAS_LIBRARIES
  BLAS
  dgemm
  ""
  "Accelerate"
  ""
  )
 endif()
endif ()

if (BLA_VENDOR STREQUAL "NAS" OR BLA_VENDOR STREQUAL "All")
 if ( NOT BLAS_LIBRARIES )
    check_fortran_libraries(
    BLAS_LIBRARIES
    BLAS
    dgemm
    ""
    "vecLib"
    ""
    )
 endif ()
endif ()
# Generic BLAS library?
if (BLA_VENDOR STREQUAL "Generic" OR BLA_VENDOR STREQUAL "All")
 if(NOT BLAS_LIBRARIES)
  check_fortran_libraries(
  BLAS_LIBRARIES
  BLAS
  sgemm
  ""
  "blas"
  ""
  )
 endif()
endif ()

#BLAS in intel mkl 10 library? (em64t 64bit)
if (BLA_VENDOR MATCHES "Intel" OR BLA_VENDOR STREQUAL "All")
 if (NOT WIN32)
  set(LM "-lm")
 endif ()
 if (CMAKE_C_COMPILER_LOADED OR CMAKE_CXX_COMPILER_LOADED)
  if(BLAS_FIND_QUIETLY OR NOT BLAS_FIND_REQUIRED)
    find_package(Threads)
  else()
    find_package(Threads REQUIRED)
  endif()

  set(BLAS_SEARCH_LIBS "")

  if(BLA_F95)
    set(BLAS_mkl_SEARCH_SYMBOL SGEMM)
    set(_LIBRARIES BLAS95_LIBRARIES)
    if (WIN32)
      if (BLA_STATIC)
        set(BLAS_mkl_DLL_SUFFIX "")
      else()
        set(BLAS_mkl_DLL_SUFFIX "_dll")
      endif()

      # Find the main file (32-bit or 64-bit)
      set(BLAS_SEARCH_LIBS_WIN_MAIN "")
      if (BLA_VENDOR STREQUAL "Intel10_32" OR BLA_VENDOR STREQUAL "All")
        list(APPEND BLAS_SEARCH_LIBS_WIN_MAIN
          "mkl_blas95${BLAS_mkl_DLL_SUFFIX} mkl_intel_c${BLAS_mkl_DLL_SUFFIX}")
      endif()
      if (BLA_VENDOR MATCHES "^Intel10_64lp" OR BLA_VENDOR STREQUAL "All")
        list(APPEND BLAS_SEARCH_LIBS_WIN_MAIN
          "mkl_blas95_lp64${BLAS_mkl_DLL_SUFFIX} mkl_intel_lp64${BLAS_mkl_DLL_SUFFIX}")
      endif ()

      # Add threading/sequential libs
      set(BLAS_SEARCH_LIBS_WIN_THREAD "")
      if (BLA_VENDOR MATCHES "_seq$" OR BLA_VENDOR STREQUAL "All")
        list(APPEND BLAS_SEARCH_LIBS_WIN_THREAD
          "mkl_sequential${BLAS_mkl_DLL_SUFFIX}")
      endif()
      if (NOT BLA_VENDOR MATCHES "_seq$" OR BLA_VENDOR STREQUAL "All")
        # old version
        list(APPEND BLAS_SEARCH_LIBS_WIN_THREAD
          "libguide40 mkl_intel_thread${BLAS_mkl_DLL_SUFFIX}")
        # mkl >= 10.3
        list(APPEND BLAS_SEARCH_LIBS_WIN_THREAD
          "libiomp5md mkl_intel_thread${BLAS_mkl_DLL_SUFFIX}")
      endif()

      # Cartesian product of the above
      foreach (MAIN ${BLAS_SEARCH_LIBS_WIN_MAIN})
        foreach (THREAD ${BLAS_SEARCH_LIBS_WIN_THREAD})
          list(APPEND BLAS_SEARCH_LIBS
            "${MAIN} ${THREAD} mkl_core${BLAS_mkl_DLL_SUFFIX}")
        endforeach()
      endforeach()
    else ()
      if (BLA_VENDOR STREQUAL "Intel10_32" OR BLA_VENDOR STREQUAL "All")
        list(APPEND BLAS_SEARCH_LIBS
          "mkl_blas95 mkl_intel mkl_intel_thread mkl_core guide")
      endif ()
      if (BLA_VENDOR STREQUAL "Intel10_64lp" OR BLA_VENDOR STREQUAL "All")
        # old version
        list(APPEND BLAS_SEARCH_LIBS
          "mkl_blas95 mkl_intel_lp64 mkl_intel_thread mkl_core guide")

        # mkl >= 10.3
        if (CMAKE_C_COMPILER MATCHES ".+gcc")
          list(APPEND BLAS_SEARCH_LIBS
            "mkl_blas95_lp64 mkl_intel_lp64 mkl_gnu_thread mkl_core gomp")
        else ()
          list(APPEND BLAS_SEARCH_LIBS
            "mkl_blas95_lp64 mkl_intel_lp64 mkl_intel_thread mkl_core iomp5")
        endif ()
      endif ()
      if (BLA_VENDOR STREQUAL "Intel10_64lp_seq" OR BLA_VENDOR STREQUAL "All")
        list(APPEND BLAS_SEARCH_LIBS
          "mkl_intel_lp64 mkl_sequential mkl_core")
      endif ()
    endif ()
  else ()
    set(BLAS_mkl_SEARCH_SYMBOL sgemm)
    set(_LIBRARIES BLAS_LIBRARIES)
    if (WIN32)
      if (BLA_STATIC)
        set(BLAS_mkl_DLL_SUFFIX "")
      else()
        set(BLAS_mkl_DLL_SUFFIX "_dll")
      endif()

      # Find the main file (32-bit or 64-bit)
      set(BLAS_SEARCH_LIBS_WIN_MAIN "")
      if (BLA_VENDOR STREQUAL "Intel10_32" OR BLA_VENDOR STREQUAL "All")
        list(APPEND BLAS_SEARCH_LIBS_WIN_MAIN
          "mkl_intel_c${BLAS_mkl_DLL_SUFFIX}")
      endif()
      if (BLA_VENDOR MATCHES "^Intel10_64lp" OR BLA_VENDOR STREQUAL "All")
        list(APPEND BLAS_SEARCH_LIBS_WIN_MAIN
          "mkl_intel_lp64${BLAS_mkl_DLL_SUFFIX}")
      endif ()

      # Add threading/sequential libs
      set(BLAS_SEARCH_LIBS_WIN_THREAD "")
      if (NOT BLA_VENDOR MATCHES "_seq$" OR BLA_VENDOR STREQUAL "All")
        # old version
        list(APPEND BLAS_SEARCH_LIBS_WIN_THREAD
          "libguide40 mkl_intel_thread${BLAS_mkl_DLL_SUFFIX}")
        # mkl >= 10.3
        list(APPEND BLAS_SEARCH_LIBS_WIN_THREAD
          "libiomp5md mkl_intel_thread${BLAS_mkl_DLL_SUFFIX}")
      endif()
      if (BLA_VENDOR MATCHES "_seq$" OR BLA_VENDOR STREQUAL "All")
        list(APPEND BLAS_SEARCH_LIBS_WIN_THREAD
          "mkl_sequential${BLAS_mkl_DLL_SUFFIX}")
      endif()

      # Cartesian product of the above
      foreach (MAIN ${BLAS_SEARCH_LIBS_WIN_MAIN})
        foreach (THREAD ${BLAS_SEARCH_LIBS_WIN_THREAD})
          list(APPEND BLAS_SEARCH_LIBS
            "${MAIN} ${THREAD} mkl_core${BLAS_mkl_DLL_SUFFIX}")
        endforeach()
      endforeach()
    else ()
      if (BLA_VENDOR STREQUAL "Intel10_32" OR BLA_VENDOR STREQUAL "All")
        list(APPEND BLAS_SEARCH_LIBS
          "mkl_intel mkl_intel_thread mkl_core guide")
      endif ()
      if (BLA_VENDOR STREQUAL "Intel10_64lp" OR BLA_VENDOR STREQUAL "All")

        # old version
        list(APPEND BLAS_SEARCH_LIBS
          "mkl_intel_lp64 mkl_intel_thread mkl_core guide")

        # mkl >= 10.3
        if (CMAKE_C_COMPILER MATCHES ".+gcc")
          list(APPEND BLAS_SEARCH_LIBS
            "mkl_intel_lp64 mkl_gnu_thread mkl_core gomp")
        else ()
          list(APPEND BLAS_SEARCH_LIBS
            "mkl_intel_lp64 mkl_intel_thread mkl_core iomp5")
        endif ()
      endif ()
      if (BLA_VENDOR STREQUAL "Intel10_64lp_seq" OR BLA_VENDOR STREQUAL "All")
        list(APPEND BLAS_SEARCH_LIBS
          "mkl_intel_lp64 mkl_sequential mkl_core")
      endif ()

      #older vesions of intel mkl libs
      if (BLA_VENDOR STREQUAL "Intel" OR BLA_VENDOR STREQUAL "All")
        list(APPEND BLAS_SEARCH_LIBS
          "mkl")
        list(APPEND BLAS_SEARCH_LIBS
          "mkl_ia32")
        list(APPEND BLAS_SEARCH_LIBS
          "mkl_em64t")
      endif ()
    endif ()
  endif ()

  foreach (IT ${BLAS_SEARCH_LIBS})
    string(REPLACE " " ";" SEARCH_LIBS ${IT})
    if (NOT ${_LIBRARIES})
      check_fortran_libraries(
        ${_LIBRARIES}
        BLAS
        ${BLAS_mkl_SEARCH_SYMBOL}
        ""
        "${SEARCH_LIBS}"
        "${CMAKE_THREAD_LIBS_INIT};${LM}"
        )
    endif ()
  endforeach ()

 endif ()
endif ()

if(BLA_F95)
  find_package_handle_standard_args(BLAS REQUIRED_VARS BLAS95_LIBRARIES)
  set(BLAS95_FOUND ${BLAS_FOUND})
  if(BLAS_FOUND)
    set(BLAS_LIBRARIES "${BLAS95_LIBRARIES}")
  endif()
else()
  find_package_handle_standard_args(BLAS REQUIRED_VARS BLAS_LIBRARIES)
endif()

cmake_pop_check_state()
set(CMAKE_FIND_LIBRARY_SUFFIXES ${_blas_ORIG_CMAKE_FIND_LIBRARY_SUFFIXES})
