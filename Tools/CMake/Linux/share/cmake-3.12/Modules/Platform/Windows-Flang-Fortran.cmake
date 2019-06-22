include(Platform/Windows-MSVC)
__windows_compiler_msvc(Fortran)
set(CMAKE_Fortran_COMPILE_OBJECT "<CMAKE_Fortran_COMPILER> ${_COMPILE_Fortran} <DEFINES> <INCLUDES> <FLAGS> -o <OBJECT> -c <SOURCE>")
