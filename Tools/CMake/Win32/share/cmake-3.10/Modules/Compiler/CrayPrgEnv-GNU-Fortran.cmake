if(__craylinux_crayprgenv_gnu_fortran)
  return()
endif()
set(__craylinux_crayprgenv_gnu_fortran 1)

include(Compiler/CrayPrgEnv-Fortran)
__CrayPrgEnv_setup_Fortran("/opt/gcc/.*/f951" "/opt/gcc/.*/collect2")
