CMAKE_HOST_SYSTEM_PROCESSOR
---------------------------

The name of the CPU CMake is running on.

On systems that support ``uname``, this variable is set to the output of
``uname -p``.  On Windows it is set to the value of the environment variable
``PROCESSOR_ARCHITECTURE``.
