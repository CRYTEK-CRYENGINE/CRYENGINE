Fortran_MODULE_DIRECTORY
------------------------

Specify output directory for Fortran modules provided by the target.

If the target contains Fortran source files that provide modules and
the compiler supports a module output directory this specifies the
directory in which the modules will be placed.  When this property is
not set the modules will be placed in the build directory
corresponding to the target's source directory.  If the variable
CMAKE_Fortran_MODULE_DIRECTORY is set when a target is created its
value is used to initialize this property.

Note that some compilers will automatically search the module output
directory for modules USEd during compilation but others will not.  If
your sources USE modules their location must be specified by
INCLUDE_DIRECTORIES regardless of this property.
