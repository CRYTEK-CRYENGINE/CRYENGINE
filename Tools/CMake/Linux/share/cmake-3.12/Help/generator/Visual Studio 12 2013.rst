Visual Studio 12 2013
---------------------

Generates Visual Studio 12 (VS 2013) project files.

The :variable:`CMAKE_GENERATOR_PLATFORM` variable may be set
to specify a target platform name (architecture).

For compatibility with CMake versions prior to 3.1, one may specify
a target platform name optionally at the end of this generator name:

``Visual Studio 12 2013 Win64``
  Specify target platform ``x64``.

``Visual Studio 12 2013 ARM``
  Specify target platform ``ARM``.

For compatibility with CMake versions prior to 3.0, one may specify this
generator using the name "Visual Studio 12" without the year component.

Toolset Selection
^^^^^^^^^^^^^^^^^

The ``v120`` toolset that comes with Visual Studio 12 2013 is selected by
default.  The :variable:`CMAKE_GENERATOR_TOOLSET` option may be set, perhaps
via the :manual:`cmake(1)` ``-T`` option, to specify another toolset.

.. include:: VS_TOOLSET_HOST_ARCH.txt
