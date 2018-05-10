if(__craylinux_crayprgenv_intel_cxx)
  return()
endif()
set(__craylinux_crayprgenv_intel_cxx 1)

include(Compiler/CrayPrgEnv-CXX)
__CrayPrgEnv_setup_CXX("/opt/intel/.*/mcpcom" "^ld ")
