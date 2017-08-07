if(__craylinux_crayprgenv_pgi_c)
  return()
endif()
set(__craylinux_crayprgenv_pgi_c 1)

include(Compiler/CrayPrgEnv-C)
__CrayPrgEnv_setup_C("/opt/pgi/[^ ]*/pgc" "/usr/bin/ld")
