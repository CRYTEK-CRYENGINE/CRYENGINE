if(__craylinux_crayprgenv_cxx)
  return()
endif()
set(__craylinux_crayprgenv_cxx 1)

include(Compiler/CrayPrgEnv)
macro(__CrayPrgEnv_setup_CXX compiler_cmd link_cmd)
  __CrayPrgEnv_setup(CXX
    ${CMAKE_ROOT}/Modules/CMakeCXXCompilerABI.cpp
    ${compiler_cmd} ${link_cmd})
endmacro()
