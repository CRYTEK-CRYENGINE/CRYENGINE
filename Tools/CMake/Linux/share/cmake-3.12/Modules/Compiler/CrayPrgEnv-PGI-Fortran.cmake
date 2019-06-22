if(__craylinux_crayprgenv_pgi_fortran)
  return()
endif()
set(__craylinux_crayprgenv_pgi_fortran 1)

include(Compiler/CrayPrgEnv-Fortran)
__CrayPrgEnv_setup_Fortran("/opt/pgi/[^ ]*/pgf" "/usr/bin/ld")
