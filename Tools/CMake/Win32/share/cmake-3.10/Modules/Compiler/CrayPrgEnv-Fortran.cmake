if(__craylinux_crayprgenv_fortran)
  return()
endif()
set(__craylinux_crayprgenv_fortran 1)

include(Compiler/CrayPrgEnv)
macro(__CrayPrgEnv_setup_Fortran compiler_cmd link_cmd)
  __CrayPrgEnv_setup(Fortran
    ${CMAKE_ROOT}/Modules/CMakeFortranCompilerABI.F
    ${compiler_cmd} ${link_cmd})
endmacro()
