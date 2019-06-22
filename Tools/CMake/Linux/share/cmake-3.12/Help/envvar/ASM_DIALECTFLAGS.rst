ASM<DIALECT>FLAGS
-----------------

Default compilation flags to be used when compiling a specific dialect of an
assembly language. ``ASM<DIALECT>FLAGS`` can be ``ASMFLAGS``, ``ASM_NASMFLAGS``,
``ASM_MASMFLAGS`` or ``ASM-ATTFLAGS``. Will only be used by CMake on the
first configuration to determine ``ASM<DIALECT>`` default compilation flags, after
which the value for ``ASM<DIALECT>FLAGS`` is stored in the cache as
:variable:`CMAKE_ASM<DIALECT>_FLAGS <CMAKE_<LANG>_FLAGS>`. For any configuration
run (including the first), the environment variable will be ignored if the
:variable:`CMAKE_ASM<DIALECT>_FLAGS <CMAKE_<LANG>_FLAGS>` variable is defined.
