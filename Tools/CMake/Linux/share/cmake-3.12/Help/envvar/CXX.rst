CXX
---

Preferred executable for compiling ``CXX`` language files. Will only be used by
CMake on the first configuration to determine ``CXX`` compiler, after which the
value for ``CXX`` is stored in the cache as
:variable:`CMAKE_CXX_COMPILER <CMAKE_<LANG>_COMPILER>`. For any configuration
run (including the first), the environment variable will be ignored if the
:variable:`CMAKE_CXX_COMPILER <CMAKE_<LANG>_COMPILER>` variable is defined.
