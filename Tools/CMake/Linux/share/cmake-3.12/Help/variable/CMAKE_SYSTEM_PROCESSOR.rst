CMAKE_SYSTEM_PROCESSOR
----------------------

The name of the CPU CMake is building for.

This variable is the same as :variable:`CMAKE_HOST_SYSTEM_PROCESSOR` if
you build for the host system instead of the target system when
cross compiling.

* The :generator:`Green Hills MULTI` generator sets this to ``ARM`` by default.
