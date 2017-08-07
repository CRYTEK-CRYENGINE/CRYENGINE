# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.


# Find the nasm assembler. yasm (http://www.tortall.net/projects/yasm/) is nasm compatible

set(CMAKE_ASM_NASM_COMPILER_LIST nasm yasm)

if(NOT CMAKE_ASM_NASM_COMPILER)
  find_program(CMAKE_ASM_NASM_COMPILER nasm
    "$ENV{ProgramFiles}/NASM")
endif()

# Load the generic DetermineASM compiler file with the DIALECT set properly:
set(ASM_DIALECT "_NASM")
include(CMakeDetermineASMCompiler)
set(ASM_DIALECT)
