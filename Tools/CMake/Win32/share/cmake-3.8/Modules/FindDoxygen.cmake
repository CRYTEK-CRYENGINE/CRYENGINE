# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#.rst:
# FindDoxygen
# -----------
#
# This module looks for Doxygen and the path to Graphviz's dot
#
# Doxygen is a documentation generation tool.  Please see
# http://www.doxygen.org
#
# This module accepts the following optional variables:
#
# ::
#
#    DOXYGEN_SKIP_DOT       = If true this module will skip trying to find Dot
#                             (an optional component often used by Doxygen)
#
#
#
# This modules defines the following variables:
#
# ::
#
#    DOXYGEN_EXECUTABLE     = The path to the doxygen command.
#    DOXYGEN_FOUND          = Was Doxygen found or not?
#    DOXYGEN_VERSION        = The version reported by doxygen --version
#
#
#
# ::
#
#    DOXYGEN_DOT_EXECUTABLE = The path to the dot program used by doxygen.
#    DOXYGEN_DOT_FOUND      = Was Dot found or not?
#
# For compatibility with older versions of CMake, the now-deprecated
# variable ``DOXYGEN_DOT_PATH`` is set to the path to the directory
# containing ``dot`` as reported in ``DOXYGEN_DOT_EXECUTABLE``.
# The path may have forward slashes even on Windows and is not
# suitable for direct substitution into a ``Doxyfile.in`` template.
# If you need this value, use :command:`get_filename_component`
# to compute it from ``DOXYGEN_DOT_EXECUTABLE`` directly, and
# perhaps the :command:`file(TO_NATIVE_PATH)` command to prepare
# the path for a Doxygen configuration file.

# For backwards compatibility support
if(Doxygen_FIND_QUIETLY)
  set(DOXYGEN_FIND_QUIETLY TRUE)
endif()

# ===== Rationale for OS X AppBundle mods below =====
#     With the OS X GUI version, Doxygen likes to be installed to /Applications and
#     it contains the doxygen executable in the bundle. In the versions I've
#     seen, it is located in Resources, but in general, more often binaries are
#     located in MacOS.
#
#     NOTE: The official Doxygen.app that is distributed for OS X uses non-standard
#     conventions.  Instead of the command-line "doxygen" tool being placed in
#     Doxygen.app/Contents/MacOS, "Doxywizard" is placed there instead and
#     "doxygen" is placed in Contents/Resources.  This is most likely done
#     so that something happens when people double-click on the Doxygen.app
#     package.  Unfortunately, CMake gets confused by this as when it sees the
#     bundle it uses "Doxywizard" as the executable to use instead of
#     "doxygen".  Therefore to work-around this issue we temporarily disable
#     the app-bundle feature, just for this CMake module:
if(APPLE)
    #  Save the old setting
    set(TEMP_DOXYGEN_SAVE_CMAKE_FIND_APPBUNDLE ${CMAKE_FIND_APPBUNDLE})
    # Disable the App-bundle detection feature
    set(CMAKE_FIND_APPBUNDLE "NEVER")
endif()
#     FYI:
#     In the older versions of OS X Doxygen, dot was included with the
#     Doxygen bundle. But the new versions require you to download
#     Graphviz.app which contains "dot" in it's bundle.
# ============== End OSX stuff ================

#
# Find Doxygen...
#

find_program(DOXYGEN_EXECUTABLE
  NAMES doxygen
  PATHS
    "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\doxygen_is1;Inno Setup: App Path]/bin"
    /Applications/Doxygen.app/Contents/Resources
    /Applications/Doxygen.app/Contents/MacOS
  DOC "Doxygen documentation generation tool (http://www.doxygen.org)"
)

if(DOXYGEN_EXECUTABLE)
  execute_process(COMMAND ${DOXYGEN_EXECUTABLE} "--version" OUTPUT_VARIABLE DOXYGEN_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()

include(${CMAKE_CURRENT_LIST_DIR}/FindPackageHandleStandardArgs.cmake)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Doxygen REQUIRED_VARS DOXYGEN_EXECUTABLE VERSION_VAR DOXYGEN_VERSION)

#
# Find Dot...
#

set(_x86 "(x86)")
file(GLOB _Doxygen_GRAPHVIZ_BIN_DIRS
  "$ENV{ProgramFiles}/Graphviz*/bin"
  "$ENV{ProgramFiles${_x86}}/Graphviz*/bin"
  )
unset(_x86)

if(NOT DOXYGEN_SKIP_DOT)
  find_program(DOXYGEN_DOT_EXECUTABLE
    NAMES dot
    PATHS
      ${_Doxygen_GRAPHVIZ_BIN_DIRS}
      "$ENV{ProgramFiles}/ATT/Graphviz/bin"
      "C:/Program Files/ATT/Graphviz/bin"
      [HKEY_LOCAL_MACHINE\\SOFTWARE\\ATT\\Graphviz;InstallPath]/bin
      /Applications/Graphviz.app/Contents/MacOS
      /Applications/Doxygen.app/Contents/Resources
      /Applications/Doxygen.app/Contents/MacOS
    DOC "Graphviz Dot tool for using Doxygen"
  )

  if(DOXYGEN_DOT_EXECUTABLE)
    set(DOXYGEN_DOT_FOUND TRUE)
    # The Doxyfile wants the path to Dot, not the entire path and executable
    get_filename_component(DOXYGEN_DOT_PATH "${DOXYGEN_DOT_EXECUTABLE}" PATH)
  endif()

endif()

#
# Backwards compatibility...
#

if(APPLE)
  # Restore the old app-bundle setting setting
  set(CMAKE_FIND_APPBUNDLE ${TEMP_DOXYGEN_SAVE_CMAKE_FIND_APPBUNDLE})
endif()

# Maintain the _FOUND variables as "YES" or "NO" for backwards compatibility
# (allows people to stuff them directly into Doxyfile with configure_file())
if(DOXYGEN_FOUND)
  set(DOXYGEN_FOUND "YES")
else()
  set(DOXYGEN_FOUND "NO")
endif()
if(DOXYGEN_DOT_FOUND)
  set(DOXYGEN_DOT_FOUND "YES")
else()
  set(DOXYGEN_DOT_FOUND "NO")
endif()

# For backwards compatibility support
set (DOXYGEN ${DOXYGEN_EXECUTABLE} )
set (DOT ${DOXYGEN_DOT_EXECUTABLE} )

mark_as_advanced(
  DOXYGEN_EXECUTABLE
  DOXYGEN_DOT_EXECUTABLE
  )
