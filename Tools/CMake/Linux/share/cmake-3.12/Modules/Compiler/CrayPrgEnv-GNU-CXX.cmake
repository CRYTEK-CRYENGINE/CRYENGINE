if(__craylinux_crayprgenv_gnu_cxx)
  return()
endif()
set(__craylinux_crayprgenv_gnu_cxx 1)

include(Compiler/CrayPrgEnv-CXX)
__CrayPrgEnv_setup_CXX("/opt/gcc/.*/cc1plus" "/opt/gcc/.*/collect2")
