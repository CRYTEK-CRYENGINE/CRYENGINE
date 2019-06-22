# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

include(Compiler/Cray)
__compiler_cray(Fortran)

set(CMAKE_Fortran_MODOUT_FLAG -em)
set(CMAKE_Fortran_MODDIR_FLAG -J)
set(CMAKE_Fortran_MODDIR_DEFAULT .)
set(CMAKE_Fortran_FORMAT_FIXED_FLAG "-f fixed")
set(CMAKE_Fortran_FORMAT_FREE_FLAG "-f free")
