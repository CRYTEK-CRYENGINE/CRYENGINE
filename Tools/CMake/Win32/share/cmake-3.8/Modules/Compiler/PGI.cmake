# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.


# This module is shared by multiple languages; use include blocker.
if(__COMPILER_PGI)
  return()
endif()
set(__COMPILER_PGI 1)

macro(__compiler_pgi lang)
  # Feature flags.
  set(CMAKE_${lang}_VERBOSE_FLAG "-v")

  # Initial configuration flags.
  string(APPEND CMAKE_${lang}_FLAGS_INIT " ")
  string(APPEND CMAKE_${lang}_FLAGS_DEBUG_INIT " -g -O0")
  string(APPEND CMAKE_${lang}_FLAGS_MINSIZEREL_INIT " -O2 -s")
  string(APPEND CMAKE_${lang}_FLAGS_RELEASE_INIT " -fast -O3 -Mipa=fast")
  string(APPEND CMAKE_${lang}_FLAGS_RELWITHDEBINFO_INIT " -O2 -gopt")

  # Preprocessing and assembly rules.
  set(CMAKE_${lang}_CREATE_PREPROCESSED_SOURCE "<CMAKE_${lang}_COMPILER> <DEFINES> <INCLUDES> <FLAGS> -E <SOURCE> > <PREPROCESSED_SOURCE>")
  set(CMAKE_${lang}_CREATE_ASSEMBLY_SOURCE "<CMAKE_${lang}_COMPILER> <DEFINES> <INCLUDES> <FLAGS> -S <SOURCE> -o <ASSEMBLY_SOURCE>")
endmacro()
