if(__craylinux_crayprgenv_intel_c)
  return()
endif()
set(__craylinux_crayprgenv_intel_c 1)

include(Compiler/CrayPrgEnv-C)
__CrayPrgEnv_setup_C("/opt/intel/.*/mcpcom" "^ld ")
