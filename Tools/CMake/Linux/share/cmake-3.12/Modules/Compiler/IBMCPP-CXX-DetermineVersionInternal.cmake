
set(_compiler_id_version_compute "
# if defined(__ibmxl__)
#  define @PREFIX@COMPILER_VERSION_MAJOR @MACRO_DEC@(__ibmxl_version__)
#  define @PREFIX@COMPILER_VERSION_MINOR @MACRO_DEC@(__ibmxl_release__)
#  define @PREFIX@COMPILER_VERSION_PATCH @MACRO_DEC@(__ibmxl_modification__)
#  define @PREFIX@COMPILER_VERSION_TWEAK @MACRO_DEC@(__ibmxl_ptf_fix_level__)
# else
   /* __IBMCPP__ = VRP */
#  define @PREFIX@COMPILER_VERSION_MAJOR @MACRO_DEC@(__IBMCPP__/100)
#  define @PREFIX@COMPILER_VERSION_MINOR @MACRO_DEC@(__IBMCPP__/10 % 10)
#  define @PREFIX@COMPILER_VERSION_PATCH @MACRO_DEC@(__IBMCPP__    % 10)
# endif
")
