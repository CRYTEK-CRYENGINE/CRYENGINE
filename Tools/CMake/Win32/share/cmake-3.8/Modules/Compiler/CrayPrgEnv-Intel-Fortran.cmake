if(__craylinux_crayprgenv_intel_fortran)
  return()
endif()
set(__craylinux_crayprgenv_intel_fortran 1)

include(Compiler/CrayPrgEnv-Fortran)
__CrayPrgEnv_setup_Fortran("/opt/intel/.*/fortcom" "^ld ")
