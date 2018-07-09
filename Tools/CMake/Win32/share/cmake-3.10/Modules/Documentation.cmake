# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#.rst:
# Documentation
# -------------
#
# DocumentationVTK.cmake
#
# This file provides support for the VTK documentation framework.  It
# relies on several tools (Doxygen, Perl, etc).

#
# Build the documentation ?
#
option(BUILD_DOCUMENTATION "Build the documentation (Doxygen)." OFF)
mark_as_advanced(BUILD_DOCUMENTATION)

if (BUILD_DOCUMENTATION)

  #
  # Check for the tools
  #
  find_package(UnixCommands)
  find_package(Doxygen)
  find_package(Gnuplot)
  find_package(HTMLHelp)
  find_package(Perl)
  find_package(Wget)

  option(DOCUMENTATION_HTML_HELP
    "Build the HTML Help file (CHM)." OFF)

  option(DOCUMENTATION_HTML_TARZ
    "Build a compressed tar archive of the HTML doc." OFF)

  mark_as_advanced(
    DOCUMENTATION_HTML_HELP
    DOCUMENTATION_HTML_TARZ
    )

  #
  # The documentation process is controled by a batch file.
  # We will probably need bash to create the custom target
  #

endif ()
