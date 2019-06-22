CSFLAGS
-------

Preferred executable for compiling ``CSharp`` language files. Will only be
used by CMake on the first configuration to determine ``CSharp`` default
compilation flags, after which the value for ``CSFLAGS`` is stored in the cache
as :variable:`CMAKE_CSharp_FLAGS <CMAKE_<LANG>_FLAGS>`. For any configuration
run (including the first), the environment variable will be ignored if the
:variable:`CMAKE_CSharp_FLAGS <CMAKE_<LANG>_FLAGS>` variable is defined.
