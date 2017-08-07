# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#.rst:
# FindCygwin
# ----------
#
# this module looks for Cygwin

if (WIN32)
  find_path(CYGWIN_INSTALL_PATH
    cygwin.bat
    "C:/Cygwin"
    "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Cygwin\\setup;rootdir]"
    "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Cygnus Solutions\\Cygwin\\mounts v2\\/;native]"
  )

  mark_as_advanced(
    CYGWIN_INSTALL_PATH
  )
endif ()
