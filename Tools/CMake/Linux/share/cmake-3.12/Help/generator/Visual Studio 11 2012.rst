Visual Studio 11 2012
---------------------

Generates Visual Studio 11 (VS 2012) project files.

The :variable:`CMAKE_GENERATOR_PLATFORM` variable may be set
to specify a target platform name (architecture).

For compatibility with CMake versions prior to 3.1, one may specify
a target platform name optionally at the end of this generator name:

``Visual Studio 11 2012 Win64``
  Specify target platform ``x64``.

``Visual Studio 11 2012 ARM``
  Specify target platform ``ARM``.

``Visual Studio 11 2012 <WinCE-SDK>``
  Specify target platform matching a Windows CE SDK name.

For compatibility with CMake versions prior to 3.0, one may specify this
generator using the name "Visual Studio 11" without the year component.

Toolset Selection
^^^^^^^^^^^^^^^^^

The ``v110`` toolset that comes with Visual Studio 11 2012 is selected by
default.  The :variable:`CMAKE_GENERATOR_TOOLSET` option may be set, perhaps
via the :manual:`cmake(1)` ``-T`` option, to specify another toolset.
