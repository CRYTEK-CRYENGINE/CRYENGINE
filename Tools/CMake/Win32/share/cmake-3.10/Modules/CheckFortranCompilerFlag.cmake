# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
CheckFortranCompilerFlag
------------------------

Check whether the Fortran compiler supports a given flag.

.. command:: check_fortran_compiler_flag

  ::

    check_fortran_compiler_flag(<flag> <var>)

  Check that the ``<flag>`` is accepted by the compiler without
  a diagnostic.  Stores the result in an internal cache entry
  named ``<var>``.

This command temporarily sets the ``CMAKE_REQUIRED_DEFINITIONS`` variable
and calls the ``check_fortran_source_compiles`` macro from the
:module:`CheckFortranSourceCompiles` module.  See documentation of that
module for a listing of variables that can otherwise modify the build.

A positive result from this check indicates only that the compiler did not
issue a diagnostic message when given the flag.  Whether the flag has any
effect or even a specific one is beyond the scope of this module.

.. note::
  Since the :command:`try_compile` command forwards flags from variables
  like :variable:`CMAKE_Fortran_FLAGS <CMAKE_<LANG>_FLAGS>`, unknown flags
  in such variables may cause a false negative for this check.
#]=======================================================================]

include(CheckFortranSourceCompiles)
include(CMakeCheckCompilerFlagCommonPatterns)

macro (CHECK_Fortran_COMPILER_FLAG _FLAG _RESULT)
  set(SAFE_CMAKE_REQUIRED_DEFINITIONS "${CMAKE_REQUIRED_DEFINITIONS}")
  set(CMAKE_REQUIRED_DEFINITIONS "${_FLAG}")

  # Normalize locale during test compilation.
  set(_CheckFortranCompilerFlag_LOCALE_VARS LC_ALL LC_MESSAGES LANG)
  foreach(v ${_CheckFortranCompilerFlag_LOCALE_VARS})
    set(_CheckFortranCompilerFlag_SAVED_${v} "$ENV{${v}}")
    set(ENV{${v}} C)
  endforeach()
  CHECK_COMPILER_FLAG_COMMON_PATTERNS(_CheckFortranCompilerFlag_COMMON_PATTERNS)
  CHECK_Fortran_SOURCE_COMPILES("       program test\n       stop\n       end program" ${_RESULT}
    # Some compilers do not fail with a bad flag
    FAIL_REGEX "command line option .* is valid for .* but not for Fortran" # GNU
    ${_CheckFortranCompilerFlag_COMMON_PATTERNS}
    )
  foreach(v ${_CheckFortranCompilerFlag_LOCALE_VARS})
    set(ENV{${v}} ${_CheckFortranCompilerFlag_SAVED_${v}})
    unset(_CheckFortranCompilerFlag_SAVED_${v})
  endforeach()
  unset(_CheckFortranCompilerFlag_LOCALE_VARS)
  unset(_CheckFortranCompilerFlag_COMMON_PATTERNS)

  set (CMAKE_REQUIRED_DEFINITIONS "${SAFE_CMAKE_REQUIRED_DEFINITIONS}")
endmacro ()
