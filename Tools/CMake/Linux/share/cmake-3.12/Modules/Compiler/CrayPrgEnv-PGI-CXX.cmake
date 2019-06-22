if(__craylinux_crayprgenv_pgi_cxx)
  return()
endif()
set(__craylinux_crayprgenv_pgi_cxx 1)

include(Compiler/CrayPrgEnv-CXX)
__CrayPrgEnv_setup_CXX("/opt/pgi/[^ ]*/pgcpp" "/usr/bin/ld")
