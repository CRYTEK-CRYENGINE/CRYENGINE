if(__craylinux_crayprgenv_gnu_c)
  return()
endif()
set(__craylinux_crayprgenv_gnu_c 1)

include(Compiler/CrayPrgEnv-C)
__CrayPrgEnv_setup_C("/opt/gcc/.*/cc1" "/opt/gcc/.*/collect2")
