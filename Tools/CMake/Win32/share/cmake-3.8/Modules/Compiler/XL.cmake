# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.


# This module is shared by multiple languages; use include blocker.
if(__COMPILER_XL)
  return()
endif()
set(__COMPILER_XL 1)

# Find the CreateExportList program that comes with this toolchain.
find_program(CMAKE_XL_CreateExportList
  NAMES CreateExportList
  DOC "IBM XL CreateExportList tool"
  )

macro(__compiler_xl lang)
  # Feature flags.
  set(CMAKE_${lang}_VERBOSE_FLAG "-V")
  set(CMAKE_${lang}_COMPILE_OPTIONS_PIC "-qpic")

  string(APPEND CMAKE_${lang}_FLAGS_DEBUG_INIT " -g")
  string(APPEND CMAKE_${lang}_FLAGS_RELEASE_INIT " -O")
  string(APPEND CMAKE_${lang}_FLAGS_MINSIZEREL_INIT " -O")
  string(APPEND CMAKE_${lang}_FLAGS_RELWITHDEBINFO_INIT " -g")
  set(CMAKE_${lang}_CREATE_PREPROCESSED_SOURCE "<CMAKE_${lang}_COMPILER> <DEFINES> <INCLUDES> <FLAGS> -E <SOURCE> > <PREPROCESSED_SOURCE>")
  set(CMAKE_${lang}_CREATE_ASSEMBLY_SOURCE     "<CMAKE_${lang}_COMPILER> <DEFINES> <INCLUDES> <FLAGS> -S <SOURCE> -o <ASSEMBLY_SOURCE>")

  # CMAKE_XL_CreateExportList is part of the AIX XL compilers but not the linux ones.
  # If we found the tool, we'll use it to create exports, otherwise stick with the regular
  # create shared library compile line.
  if (CMAKE_XL_CreateExportList)
    # The compiler front-end passes all object files, archive files, and shared
    # library files named on the command line to CreateExportList to create a
    # list of all symbols to be exported from the shared library.  This causes
    # all archive members to be copied into the shared library whether they are
    # needed or not.  Instead we run the tool ourselves to pass only the object
    # files so that we export only the symbols actually provided by the sources.
    set(CMAKE_${lang}_CREATE_SHARED_LIBRARY
      "${CMAKE_XL_CreateExportList} <OBJECT_DIR>/objects.exp <OBJECTS>"
      "<CMAKE_${lang}_COMPILER> <CMAKE_SHARED_LIBRARY_${lang}_FLAGS> <LANGUAGE_COMPILE_FLAGS> <LINK_FLAGS> <CMAKE_SHARED_LIBRARY_CREATE_${lang}_FLAGS> -Wl,-bE:<OBJECT_DIR>/objects.exp <SONAME_FLAG><TARGET_SONAME> -o <TARGET> <OBJECTS> <LINK_LIBRARIES>"
      )
  endif()
endmacro()
