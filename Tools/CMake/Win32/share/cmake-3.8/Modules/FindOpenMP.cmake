# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#.rst:
# FindOpenMP
# ----------
#
# Finds OpenMP support
#
# This module can be used to detect OpenMP support in a compiler.  If
# the compiler supports OpenMP, the flags required to compile with
# OpenMP support are returned in variables for the different languages.
# The variables may be empty if the compiler does not need a special
# flag to support OpenMP.
#
# The following variables are set:
#
# ``OpenMP_C_FLAGS``
#   Flags to add to the C compiler for OpenMP support.
# ``OpenMP_CXX_FLAGS``
#   Flags to add to the CXX compiler for OpenMP support.
# ``OpenMP_Fortran_FLAGS``
#   Flags to add to the Fortran compiler for OpenMP support.
# ``OPENMP_FOUND``
#   True if openmp is detected.
#
# The following internal variables are set, if detected:
#
# ``OpenMP_C_SPEC_DATE``
#   Specification date of OpenMP version of C compiler.
# ``OpenMP_CXX_SPEC_DATE``
#   Specification date of OpenMP version of CXX compiler.
# ``OpenMP_Fortran_SPEC_DATE``
#   Specification date of OpenMP version of Fortran compiler.
#
# The specification dates are formatted as integers of the form
# ``CCYYMM`` where these represent the decimal digits of the century,
# year, and month.

set(_OPENMP_REQUIRED_VARS)
set(CMAKE_REQUIRED_QUIET_SAVE ${CMAKE_REQUIRED_QUIET})
set(CMAKE_REQUIRED_QUIET ${OpenMP_FIND_QUIETLY})

function(_OPENMP_FLAG_CANDIDATES LANG)
  set(OpenMP_FLAG_CANDIDATES
    #Empty, if compiler automatically accepts openmp
    " "
    #GNU
    "-fopenmp"
    #Clang
    "-fopenmp=libomp"
    #Microsoft Visual Studio
    "/openmp"
    #Intel windows
    "-Qopenmp"
    #PathScale, Intel
    "-openmp"
    #Sun
    "-xopenmp"
    #HP
    "+Oopenmp"
    #IBM XL C/c++
    "-qsmp"
    #Portland Group, MIPSpro
    "-mp"
  )

  set(OMP_FLAG_GNU "-fopenmp")
  set(OMP_FLAG_Clang "-fopenmp=libomp")
  set(OMP_FLAG_HP "+Oopenmp")
  if(WIN32)
    set(OMP_FLAG_Intel "-Qopenmp")
  elseif(CMAKE_${LANG}_COMPILER_ID STREQUAL "Intel" AND
         "${CMAKE_${LANG}_COMPILER_VERSION}" VERSION_LESS "15.0.0.20140528")
    set(OMP_FLAG_Intel "-openmp")
  else()
    set(OMP_FLAG_Intel "-qopenmp")
  endif()
  set(OMP_FLAG_MIPSpro "-mp")
  set(OMP_FLAG_MSVC "/openmp")
  set(OMP_FLAG_PathScale "-openmp")
  set(OMP_FLAG_PGI "-mp")
  set(OMP_FLAG_SunPro "-xopenmp")
  set(OMP_FLAG_XL "-qsmp")
  set(OMP_FLAG_Cray " ")

  # Move the flag that matches the compiler to the head of the list,
  # this is faster and doesn't clutter the output that much. If that
  # flag doesn't work we will still try all.
  if(OMP_FLAG_${CMAKE_${LANG}_COMPILER_ID})
    list(REMOVE_ITEM OpenMP_FLAG_CANDIDATES "${OMP_FLAG_${CMAKE_${LANG}_COMPILER_ID}}")
    list(INSERT OpenMP_FLAG_CANDIDATES 0 "${OMP_FLAG_${CMAKE_${LANG}_COMPILER_ID}}")
  endif()

  set(OpenMP_${LANG}_FLAG_CANDIDATES "${OpenMP_FLAG_CANDIDATES}" PARENT_SCOPE)
endfunction()

# sample openmp source code to test
set(OpenMP_C_TEST_SOURCE
"
#include <omp.h>
int main() {
#ifdef _OPENMP
  return 0;
#else
  breaks_on_purpose
#endif
}
")

# same in Fortran
set(OpenMP_Fortran_TEST_SOURCE
  "
      program test
      use omp_lib
      integer :: n
      n = omp_get_num_threads()
      end program test
  "
  )

set(OpenMP_C_CXX_CHECK_VERSION_SOURCE
"
#include <stdio.h>
#include <omp.h>
const char ompver_str[] = { 'I', 'N', 'F', 'O', ':', 'O', 'p', 'e', 'n', 'M',
                            'P', '-', 'd', 'a', 't', 'e', '[',
                            ('0' + ((_OPENMP/100000)%10)),
                            ('0' + ((_OPENMP/10000)%10)),
                            ('0' + ((_OPENMP/1000)%10)),
                            ('0' + ((_OPENMP/100)%10)),
                            ('0' + ((_OPENMP/10)%10)),
                            ('0' + ((_OPENMP/1)%10)),
                            ']', '\\0' };
int main(int argc, char *argv[])
{
  printf(\"%s\\n\", ompver_str);
  return 0;
}
")

set(OpenMP_Fortran_CHECK_VERSION_SOURCE
"
      program omp_ver
      use omp_lib
      integer, parameter :: zero = ichar('0')
      integer, parameter :: ompv = openmp_version
      character, dimension(24), parameter :: ompver_str =&
      (/ 'I', 'N', 'F', 'O', ':', 'O', 'p', 'e', 'n', 'M', 'P', '-',&
         'd', 'a', 't', 'e', '[',&
         char(zero + mod(ompv/100000, 10)),&
         char(zero + mod(ompv/10000, 10)),&
         char(zero + mod(ompv/1000, 10)),&
         char(zero + mod(ompv/100, 10)),&
         char(zero + mod(ompv/10, 10)),&
         char(zero + mod(ompv/1, 10)), ']' /)
      print *, ompver_str
      end program omp_ver
")

function(_OPENMP_GET_SPEC_DATE LANG SPEC_DATE)
  set(WORK_DIR ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/FindOpenMP)
  if("${LANG}" STREQUAL "C")
    set(SRC_FILE ${WORK_DIR}/ompver.c)
    file(WRITE ${SRC_FILE} "${OpenMP_C_CXX_CHECK_VERSION_SOURCE}")
  elseif("${LANG}" STREQUAL "CXX")
    set(SRC_FILE ${WORK_DIR}/ompver.cpp)
    file(WRITE ${SRC_FILE} "${OpenMP_C_CXX_CHECK_VERSION_SOURCE}")
  else() # ("${LANG}" STREQUAL "Fortran")
    set(SRC_FILE ${WORK_DIR}/ompver.f90)
    file(WRITE ${SRC_FILE} "${OpenMP_Fortran_CHECK_VERSION_SOURCE}")
  endif()

  set(BIN_FILE ${WORK_DIR}/ompver_${LANG}.bin)
  try_compile(OpenMP_TRY_COMPILE_RESULT ${CMAKE_BINARY_DIR} ${SRC_FILE}
              CMAKE_FLAGS "-DCOMPILE_DEFINITIONS:STRING=${OpenMP_${LANG}_FLAGS}"
              COPY_FILE ${BIN_FILE})

  if(${OpenMP_TRY_COMPILE_RESULT})
    file(STRINGS ${BIN_FILE} specstr LIMIT_COUNT 1 REGEX "INFO:OpenMP-date")
    set(regex_spec_date ".*INFO:OpenMP-date\\[0*([^]]*)\\].*")
    if("${specstr}" MATCHES "${regex_spec_date}")
      set(${SPEC_DATE} "${CMAKE_MATCH_1}" PARENT_SCOPE)
    endif()
  endif()

  unset(OpenMP_TRY_COMPILE_RESULT CACHE)
endfunction()


# check c compiler
if(CMAKE_C_COMPILER_LOADED)
  # if these are set then do not try to find them again,
  # by avoiding any try_compiles for the flags
  if(OpenMP_C_FLAGS)
    unset(OpenMP_C_FLAG_CANDIDATES)
  else()
    _OPENMP_FLAG_CANDIDATES("C")
    include(${CMAKE_CURRENT_LIST_DIR}/CheckCSourceCompiles.cmake)
  endif()

  foreach(FLAG IN LISTS OpenMP_C_FLAG_CANDIDATES)
    set(SAFE_CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS}")
    set(CMAKE_REQUIRED_FLAGS "${FLAG}")
    unset(OpenMP_FLAG_DETECTED CACHE)
    if(NOT CMAKE_REQUIRED_QUIET)
      message(STATUS "Try OpenMP C flag = [${FLAG}]")
    endif()
    check_c_source_compiles("${OpenMP_C_TEST_SOURCE}" OpenMP_FLAG_DETECTED)
    set(CMAKE_REQUIRED_FLAGS "${SAFE_CMAKE_REQUIRED_FLAGS}")
    if(OpenMP_FLAG_DETECTED)
      set(OpenMP_C_FLAGS_INTERNAL "${FLAG}")
      break()
    endif()
  endforeach()

  set(OpenMP_C_FLAGS "${OpenMP_C_FLAGS_INTERNAL}"
    CACHE STRING "C compiler flags for OpenMP parallization")

  list(APPEND _OPENMP_REQUIRED_VARS OpenMP_C_FLAGS)
  unset(OpenMP_C_FLAG_CANDIDATES)

  if (NOT OpenMP_C_SPEC_DATE)
    _OPENMP_GET_SPEC_DATE("C" OpenMP_C_SPEC_DATE_INTERNAL)
    set(OpenMP_C_SPEC_DATE "${OpenMP_C_SPEC_DATE_INTERNAL}" CACHE
      INTERNAL "C compiler's OpenMP specification date")
  endif()
endif()

# check cxx compiler
if(CMAKE_CXX_COMPILER_LOADED)
  # if these are set then do not try to find them again,
  # by avoiding any try_compiles for the flags
  if(OpenMP_CXX_FLAGS)
    unset(OpenMP_CXX_FLAG_CANDIDATES)
  else()
    _OPENMP_FLAG_CANDIDATES("CXX")
    include(${CMAKE_CURRENT_LIST_DIR}/CheckCXXSourceCompiles.cmake)

    # use the same source for CXX as C for now
    set(OpenMP_CXX_TEST_SOURCE ${OpenMP_C_TEST_SOURCE})
  endif()

  foreach(FLAG IN LISTS OpenMP_CXX_FLAG_CANDIDATES)
    set(SAFE_CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS}")
    set(CMAKE_REQUIRED_FLAGS "${FLAG}")
    unset(OpenMP_FLAG_DETECTED CACHE)
    if(NOT CMAKE_REQUIRED_QUIET)
      message(STATUS "Try OpenMP CXX flag = [${FLAG}]")
    endif()
    check_cxx_source_compiles("${OpenMP_CXX_TEST_SOURCE}" OpenMP_FLAG_DETECTED)
    set(CMAKE_REQUIRED_FLAGS "${SAFE_CMAKE_REQUIRED_FLAGS}")
    if(OpenMP_FLAG_DETECTED)
      set(OpenMP_CXX_FLAGS_INTERNAL "${FLAG}")
      break()
    endif()
  endforeach()

  set(OpenMP_CXX_FLAGS "${OpenMP_CXX_FLAGS_INTERNAL}"
    CACHE STRING "C++ compiler flags for OpenMP parallization")

  list(APPEND _OPENMP_REQUIRED_VARS OpenMP_CXX_FLAGS)
  unset(OpenMP_CXX_FLAG_CANDIDATES)

  if (NOT OpenMP_CXX_SPEC_DATE)
    _OPENMP_GET_SPEC_DATE("CXX" OpenMP_CXX_SPEC_DATE_INTERNAL)
    set(OpenMP_CXX_SPEC_DATE "${OpenMP_CXX_SPEC_DATE_INTERNAL}" CACHE
      INTERNAL "C++ compiler's OpenMP specification date")
  endif()
endif()

# check Fortran compiler
if(CMAKE_Fortran_COMPILER_LOADED)
  # if these are set then do not try to find them again,
  # by avoiding any try_compiles for the flags
  if(OpenMP_Fortran_FLAGS)
    unset(OpenMP_Fortran_FLAG_CANDIDATES)
  else()
    _OPENMP_FLAG_CANDIDATES("Fortran")
    include(${CMAKE_CURRENT_LIST_DIR}/CheckFortranSourceCompiles.cmake)
  endif()

  foreach(FLAG IN LISTS OpenMP_Fortran_FLAG_CANDIDATES)
    set(SAFE_CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS}")
    set(CMAKE_REQUIRED_FLAGS "${FLAG}")
    unset(OpenMP_FLAG_DETECTED CACHE)
    if(NOT CMAKE_REQUIRED_QUIET)
      message(STATUS "Try OpenMP Fortran flag = [${FLAG}]")
    endif()
    check_fortran_source_compiles("${OpenMP_Fortran_TEST_SOURCE}" OpenMP_FLAG_DETECTED)
    set(CMAKE_REQUIRED_FLAGS "${SAFE_CMAKE_REQUIRED_FLAGS}")
    if(OpenMP_FLAG_DETECTED)
      set(OpenMP_Fortran_FLAGS_INTERNAL "${FLAG}")
      break()
    endif()
  endforeach()

  set(OpenMP_Fortran_FLAGS "${OpenMP_Fortran_FLAGS_INTERNAL}"
    CACHE STRING "Fortran compiler flags for OpenMP parallization")

  list(APPEND _OPENMP_REQUIRED_VARS OpenMP_Fortran_FLAGS)
  unset(OpenMP_Fortran_FLAG_CANDIDATES)

  if (NOT OpenMP_Fortran_SPEC_DATE)
    _OPENMP_GET_SPEC_DATE("Fortran" OpenMP_Fortran_SPEC_DATE_INTERNAL)
    set(OpenMP_Fortran_SPEC_DATE "${OpenMP_Fortran_SPEC_DATE_INTERNAL}" CACHE
      INTERNAL "Fortran compiler's OpenMP specification date")
  endif()
endif()

set(CMAKE_REQUIRED_QUIET ${CMAKE_REQUIRED_QUIET_SAVE})

if(_OPENMP_REQUIRED_VARS)
  include(${CMAKE_CURRENT_LIST_DIR}/FindPackageHandleStandardArgs.cmake)

  find_package_handle_standard_args(OpenMP
                                    REQUIRED_VARS ${_OPENMP_REQUIRED_VARS})

  mark_as_advanced(${_OPENMP_REQUIRED_VARS})

  unset(_OPENMP_REQUIRED_VARS)
else()
  message(SEND_ERROR "FindOpenMP requires C or CXX language to be enabled")
endif()

unset(OpenMP_C_TEST_SOURCE)
unset(OpenMP_CXX_TEST_SOURCE)
unset(OpenMP_Fortran_TEST_SOURCE)
unset(OpenMP_C_CXX_CHECK_VERSION_SOURCE)
unset(OpenMP_Fortran_CHECK_VERSION_SOURCE)
