CUDAFLAGS
---------

Default compilation flags to be used when compiling ``CUDA`` files. Will only be
used by CMake on the first configuration to determine ``CUDA`` default
compilation flags, after which the value for ``CUDAFLAGS`` is stored in the
cache as :variable:`CMAKE_CUDA_FLAGS <CMAKE_<LANG>_FLAGS>`. For any configuration
run (including the first), the environment variable will be ignored if
the :variable:`CMAKE_CUDA_FLAGS <CMAKE_<LANG>_FLAGS>` variable is defined.
