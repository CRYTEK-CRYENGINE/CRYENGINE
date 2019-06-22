RC
--

Preferred executable for compiling ``resource`` files. Will only be used by CMake
on the first configuration to determine ``resource`` compiler, after which the
value for ``RC`` is stored in the cache as
:variable:`CMAKE_RC_COMPILER <CMAKE_<LANG>_COMPILER>`. For any configuration run
(including the first), the environment variable will be ignored if the
:variable:`CMAKE_RC_COMPILER <CMAKE_<LANG>_COMPILER>` variable is defined.
