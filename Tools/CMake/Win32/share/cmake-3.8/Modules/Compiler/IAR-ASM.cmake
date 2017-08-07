# This file is processed when the IAR compiler is used for an assembler file

include(Compiler/IAR)

set(CMAKE_ASM_COMPILE_OBJECT  "<CMAKE_ASM_COMPILER> <SOURCE> <DEFINES> <INCLUDES> <FLAGS> -o <OBJECT>")

if("${IAR_TARGET_ARCHITECTURE}" STREQUAL "ARM")
  set(CMAKE_ASM_SOURCE_FILE_EXTENSIONS s;asm;msa)
endif()


if("${IAR_TARGET_ARCHITECTURE}" STREQUAL "AVR")
  set(CMAKE_ASM_SOURCE_FILE_EXTENSIONS s90;asm;msa)
endif()
