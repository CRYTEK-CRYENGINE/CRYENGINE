if(__craylinux_crayprgenv_cray_c)
  return()
endif()
set(__craylinux_crayprgenv_cray_c 1)

include(Compiler/CrayPrgEnv-C)
__CrayPrgEnv_setup_C("/opt/cray/cce/.*/ccfe" "/opt/cray/cce/.*/ld")
