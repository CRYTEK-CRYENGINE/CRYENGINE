# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#.rst:
# CheckCCompilerFlag
# ------------------
#
# Check whether the C compiler supports a given flag.
#
# CHECK_C_COMPILER_FLAG(<flag> <var>)
#
# ::
#
#   <flag> - the compiler flag
#   <var>  - variable to store the result
#            Will be created as an internal cache variable.
#
# This internally calls the check_c_source_compiles macro and sets
# CMAKE_REQUIRED_DEFINITIONS to <flag>.  See help for
# CheckCSourceCompiles for a listing of variables that can otherwise
# modify the build.  The result only tells that the compiler does not
# give an error message when it encounters the flag.  If the flag has
# any effect or even a specific one is beyond the scope of this module.

include(CheckCSourceCompiles)
include(CMakeCheckCompilerFlagCommonPatterns)

macro (CHECK_C_COMPILER_FLAG _FLAG _RESULT)
   set(SAFE_CMAKE_REQUIRED_DEFINITIONS "${CMAKE_REQUIRED_DEFINITIONS}")
   set(CMAKE_REQUIRED_DEFINITIONS "${_FLAG}")

   # Normalize locale during test compilation.
   set(_CheckCCompilerFlag_LOCALE_VARS LC_ALL LC_MESSAGES LANG)
   foreach(v ${_CheckCCompilerFlag_LOCALE_VARS})
     set(_CheckCCompilerFlag_SAVED_${v} "$ENV{${v}}")
     set(ENV{${v}} C)
   endforeach()
   CHECK_COMPILER_FLAG_COMMON_PATTERNS(_CheckCCompilerFlag_COMMON_PATTERNS)
   CHECK_C_SOURCE_COMPILES("int main(void) { return 0; }" ${_RESULT}
     # Some compilers do not fail with a bad flag
     FAIL_REGEX "command line option .* is valid for .* but not for C" # GNU
     ${_CheckCCompilerFlag_COMMON_PATTERNS}
     )
   foreach(v ${_CheckCCompilerFlag_LOCALE_VARS})
     set(ENV{${v}} ${_CheckCCompilerFlag_SAVED_${v}})
     unset(_CheckCCompilerFlag_SAVED_${v})
   endforeach()
   unset(_CheckCCompilerFlag_LOCALE_VARS)
   unset(_CheckCCompilerFlag_COMMON_PATTERNS)

   set (CMAKE_REQUIRED_DEFINITIONS "${SAFE_CMAKE_REQUIRED_DEFINITIONS}")
endmacro ()
