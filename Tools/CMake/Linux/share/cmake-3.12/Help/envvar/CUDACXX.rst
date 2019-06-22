CUDACXX
-------

Preferred executable for compiling ``CUDA`` language files. Will only be used by
CMake on the first configuration to determine ``CUDA`` compiler, after which the
value for ``CUDA`` is stored in the cache as
:variable:`CMAKE_CUDA_COMPILER <CMAKE_<LANG>_COMPILER>`. For any configuration
run (including the first), the environment variable will be ignored if the
:variable:`CMAKE_CUDA_COMPILER <CMAKE_<LANG>_COMPILER>` variable is defined.
