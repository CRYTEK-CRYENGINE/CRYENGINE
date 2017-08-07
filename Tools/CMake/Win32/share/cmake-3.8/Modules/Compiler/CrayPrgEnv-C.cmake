if(__craylinux_crayprgenv_c)
  return()
endif()
set(__craylinux_crayprgenv_c 1)

include(Compiler/CrayPrgEnv)
macro(__CrayPrgEnv_setup_C compiler_cmd link_cmd)
  __CrayPrgEnv_setup(C
    ${CMAKE_ROOT}/Modules/CMakeCCompilerABI.c
    ${compiler_cmd} ${link_cmd})
endmacro()
