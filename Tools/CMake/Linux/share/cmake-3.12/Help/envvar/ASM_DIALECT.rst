ASM<DIALECT>
------------

Preferred executable for compiling a specific dialect of assembly language
files. ``ASM<DIALECT>`` can be ``ASM``, ``ASM_NASM``, ``ASM_MASM`` or
``ASM-ATT``. Will only be used by CMake on the first configuration to determine
``ASM<DIALECT>`` compiler, after which the value for ``ASM<DIALECT>`` is stored
in the cache as
:variable:`CMAKE_ASM<DIALECT>_COMPILER <CMAKE_<LANG>_COMPILER>`. For subsequent
configuration runs, the environment variable will be ignored in favor of
:variable:`CMAKE_ASM<DIALECT>_COMPILER <CMAKE_<LANG>_COMPILER>`.
