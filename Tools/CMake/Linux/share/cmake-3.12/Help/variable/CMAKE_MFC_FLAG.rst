CMAKE_MFC_FLAG
--------------

Tell cmake to use MFC for an executable or dll.

This can be set in a ``CMakeLists.txt`` file and will enable MFC in the
application.  It should be set to ``1`` for the static MFC library, and ``2``
for the shared MFC library.  This is used in Visual Studio
project files.  The CMakeSetup dialog used MFC and the ``CMakeLists.txt``
looks like this:

::

  add_definitions(-D_AFXDLL)
  set(CMAKE_MFC_FLAG 2)
  add_executable(CMakeSetup WIN32 ${SRCS})
