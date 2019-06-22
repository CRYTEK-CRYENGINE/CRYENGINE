Fortran_FORMAT
--------------

Set to FIXED or FREE to indicate the Fortran source layout.

This property tells CMake whether a given Fortran source file uses
fixed-format or free-format.  CMake will pass the corresponding format
flag to the compiler.  Consider using the target-wide Fortran_FORMAT
property if all source files in a target share the same format.
