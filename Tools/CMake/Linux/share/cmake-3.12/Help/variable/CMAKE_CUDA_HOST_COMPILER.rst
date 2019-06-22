CMAKE_CUDA_HOST_COMPILER
------------------------

Executable to use when compiling host code when compiling ``CUDA`` language
files. Maps to the nvcc -ccbin option.  Will only be used by CMake on the first
configuration to determine a valid host compiler for ``CUDA``. After a valid
host compiler has been found, this value is read-only.
