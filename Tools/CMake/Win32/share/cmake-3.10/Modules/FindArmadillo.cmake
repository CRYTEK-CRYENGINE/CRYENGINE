# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#.rst:
# FindArmadillo
# -------------
#
# Find Armadillo
#
# Find the Armadillo C++ library
#
# Using Armadillo:
#
# ::
#
#   find_package(Armadillo REQUIRED)
#   include_directories(${ARMADILLO_INCLUDE_DIRS})
#   add_executable(foo foo.cc)
#   target_link_libraries(foo ${ARMADILLO_LIBRARIES})
#
# This module sets the following variables:
#
# ::
#
#   ARMADILLO_FOUND - set to true if the library is found
#   ARMADILLO_INCLUDE_DIRS - list of required include directories
#   ARMADILLO_LIBRARIES - list of libraries to be linked
#   ARMADILLO_VERSION_MAJOR - major version number
#   ARMADILLO_VERSION_MINOR - minor version number
#   ARMADILLO_VERSION_PATCH - patch version number
#   ARMADILLO_VERSION_STRING - version number as a string (ex: "1.0.4")
#   ARMADILLO_VERSION_NAME - name of the version (ex: "Antipodean Antileech")

# UNIX paths are standard, no need to write.
find_library(ARMADILLO_LIBRARY
  NAMES armadillo
  PATHS "$ENV{ProgramFiles}/Armadillo/lib"  "$ENV{ProgramFiles}/Armadillo/lib64" "$ENV{ProgramFiles}/Armadillo"
  )
find_path(ARMADILLO_INCLUDE_DIR
  NAMES armadillo
  PATHS "$ENV{ProgramFiles}/Armadillo/include"
  )


if(ARMADILLO_INCLUDE_DIR)

  # ------------------------------------------------------------------------
  #  Extract version information from <armadillo>
  # ------------------------------------------------------------------------

  # WARNING: Early releases of Armadillo didn't have the arma_version.hpp file.
  # (e.g. v.0.9.8-1 in ubuntu maverick packages (2001-03-15))
  # If the file is missing, set all values to 0
  set(ARMADILLO_VERSION_MAJOR 0)
  set(ARMADILLO_VERSION_MINOR 0)
  set(ARMADILLO_VERSION_PATCH 0)
  set(ARMADILLO_VERSION_NAME "EARLY RELEASE")

  if(EXISTS "${ARMADILLO_INCLUDE_DIR}/armadillo_bits/arma_version.hpp")

    # Read and parse armdillo version header file for version number
    file(STRINGS "${ARMADILLO_INCLUDE_DIR}/armadillo_bits/arma_version.hpp" _armadillo_HEADER_CONTENTS REGEX "#define ARMA_VERSION_[A-Z]+ ")
    string(REGEX REPLACE ".*#define ARMA_VERSION_MAJOR ([0-9]+).*" "\\1" ARMADILLO_VERSION_MAJOR "${_armadillo_HEADER_CONTENTS}")
    string(REGEX REPLACE ".*#define ARMA_VERSION_MINOR ([0-9]+).*" "\\1" ARMADILLO_VERSION_MINOR "${_armadillo_HEADER_CONTENTS}")
    string(REGEX REPLACE ".*#define ARMA_VERSION_PATCH ([0-9]+).*" "\\1" ARMADILLO_VERSION_PATCH "${_armadillo_HEADER_CONTENTS}")

    # WARNING: The number of spaces before the version name is not one.
    string(REGEX REPLACE ".*#define ARMA_VERSION_NAME +\"([0-9a-zA-Z _-]+)\".*" "\\1" ARMADILLO_VERSION_NAME "${_armadillo_HEADER_CONTENTS}")

    unset(_armadillo_HEADER_CONTENTS)
  endif()

  set(ARMADILLO_VERSION_STRING "${ARMADILLO_VERSION_MAJOR}.${ARMADILLO_VERSION_MINOR}.${ARMADILLO_VERSION_PATCH}")
endif ()

include(${CMAKE_CURRENT_LIST_DIR}/FindPackageHandleStandardArgs.cmake)
find_package_handle_standard_args(Armadillo
  REQUIRED_VARS ARMADILLO_LIBRARY ARMADILLO_INCLUDE_DIR
  VERSION_VAR ARMADILLO_VERSION_STRING)
# version_var fails with cmake < 2.8.4.

if (ARMADILLO_FOUND)
  set(ARMADILLO_INCLUDE_DIRS ${ARMADILLO_INCLUDE_DIR})
  set(ARMADILLO_LIBRARIES ${ARMADILLO_LIBRARY})
endif ()

# Hide internal variables
mark_as_advanced(
  ARMADILLO_INCLUDE_DIR
  ARMADILLO_LIBRARY)
