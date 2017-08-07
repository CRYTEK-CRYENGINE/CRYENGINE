if(__craylinux_crayprgenv_cray_cxx)
  return()
endif()
set(__craylinux_crayprgenv_cray_cxx 1)

include(Compiler/CrayPrgEnv-CXX)
__CrayPrgEnv_setup_CXX("/opt/cray/cce/.*/ccfe" "/opt/cray/cce/.*/ld")
