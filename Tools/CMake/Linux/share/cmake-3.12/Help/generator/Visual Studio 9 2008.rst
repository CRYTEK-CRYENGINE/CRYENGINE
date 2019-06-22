Visual Studio 9 2008
--------------------

Generates Visual Studio 9 2008 project files.

The :variable:`CMAKE_GENERATOR_PLATFORM` variable may be set
to specify a target platform name.

For compatibility with CMake versions prior to 3.1, one may specify
a target platform name optionally at the end of this generator name:

``Visual Studio 9 2008 Win64``
  Specify target platform ``x64``.

``Visual Studio 9 2008 IA64``
  Specify target platform ``Itanium``.

``Visual Studio 9 2008 <WinCE-SDK>``
  Specify target platform matching a Windows CE SDK name.
