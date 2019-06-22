CMAKE_VS_PLATFORM_TOOLSET_HOST_ARCHITECTURE
-------------------------------------------

Visual Studio preferred tool architecture.

The :ref:`Visual Studio Generators` for VS 2013 and above support optional
selection of a 64-bit toolchain on 64-bit hosts by specifying a ``host=x64``
value in the :variable:`CMAKE_GENERATOR_TOOLSET` option.  CMake provides
the selected toolchain architecture preference in this variable (either
``x64`` or empty).
