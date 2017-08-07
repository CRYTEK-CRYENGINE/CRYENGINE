# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#.rst:
# FindPike
# --------
#
# Find Pike
#
# This module finds if PIKE is installed and determines where the
# include files and libraries are.  It also determines what the name of
# the library is.  This code sets the following variables:
#
# ::
#
#   PIKE_INCLUDE_PATH       = path to where program.h is found
#   PIKE_EXECUTABLE         = full path to the pike binary

file(GLOB PIKE_POSSIBLE_INCLUDE_PATHS
  /usr/include/pike/*
  /usr/local/include/pike/*)

find_path(PIKE_INCLUDE_PATH program.h
  ${PIKE_POSSIBLE_INCLUDE_PATHS})

find_program(PIKE_EXECUTABLE
  NAMES pike7.4
  )

mark_as_advanced(
  PIKE_EXECUTABLE
  PIKE_INCLUDE_PATH
  )
