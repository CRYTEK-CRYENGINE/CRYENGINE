# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.


find_program(CMAKE_MAKE_PROGRAM mingw32-make.exe PATHS
  "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\MinGW;InstallLocation]/bin"
  c:/MinGW/bin /MinGW/bin
  "[HKEY_CURRENT_USER\\Software\\CodeBlocks;Path]/MinGW/bin"
  )
find_program(CMAKE_SH sh.exe )
if(CMAKE_SH)
  message(FATAL_ERROR "sh.exe was found in your PATH, here:\n${CMAKE_SH}\nFor MinGW make to work correctly sh.exe must NOT be in your path.\nRun cmake from a shell that does not have sh.exe in your PATH.\nIf you want to use a UNIX shell, then use MSYS Makefiles.\n")
  set(CMAKE_MAKE_PROGRAM NOTFOUND)
endif()

mark_as_advanced(CMAKE_MAKE_PROGRAM CMAKE_SH)
