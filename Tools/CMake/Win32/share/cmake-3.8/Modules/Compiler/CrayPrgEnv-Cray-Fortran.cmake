if(__craylinux_crayprgenv_cray_fortran)
  return()
endif()
set(__craylinux_crayprgenv_cray_fortran 1)

include(Compiler/CrayPrgEnv-Fortran)
__CrayPrgEnv_setup_Fortran("/opt/cray/cce/.*/ftnfe" "/opt/cray/cce/.*/ld")
