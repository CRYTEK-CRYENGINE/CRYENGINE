if(NOT DEFINED _CMAKE_PROCESSING_LANGUAGE OR _CMAKE_PROCESSING_LANGUAGE STREQUAL "")
  message(FATAL_ERROR "Internal error: _CMAKE_PROCESSING_LANGUAGE is not set")
endif()

# Try to find tools in the same directory as Clang itself
get_filename_component(__iar_hint_1 "${CMAKE_${_CMAKE_PROCESSING_LANGUAGE}_COMPILER}" REALPATH)
get_filename_component(__iar_hint_1 "${__iar_hint_1}" DIRECTORY)

get_filename_component(__iar_hint_2 "${CMAKE_${_CMAKE_PROCESSING_LANGUAGE}_COMPILER}" DIRECTORY)

set(__iar_hints "${__iar_hint_1}" "${__iar_hint_2}")

if("${CMAKE_${_CMAKE_PROCESSING_LANGUAGE}_COMPILER_ARCHITECTURE_ID}" STREQUAL "ARM")
  # could allow using normal binutils ar, since objects are normal ELF files?
  find_program(CMAKE_IAR_LINKARM ilinkarm.exe HINTS ${__iar_hints}
      DOC "The IAR ARM linker")
  find_program(CMAKE_IAR_ARCHIVE iarchive.exe HINTS ${__iar_hints}
      DOC "The IAR archiver")

  # find auxillary tools
  find_program(CMAKE_IAR_ELFTOOL ielftool.exe HINTS ${__iar_hints}
      DOC "The IAR ELF Tool")
    find_program(CMAKE_IAR_ELFDUMP ielfdumparm.exe HINTS ${__iar_hints}
      DOC "The IAR ELF Dumper")
  find_program(CMAKE_IAR_OBJMANIP iobjmanip.exe HINTS ${__iar_hints}
      DOC "The IAR ELF Object Tool")
  find_program(CMAKE_IAR_SYMEXPORT isymexport.exe HINTS ${__iar_hints}
      DOC "The IAR Absolute Symbol Exporter")
  mark_as_advanced(CMAKE_IAR_LINKARM CMAKE_IAR_ARCHIVE CMAKE_IAR_ELFTOOL CMAKE_IAR_ELFDUMP CMAKE_IAR_OBJMANIP CMAKE_IAR_SYMEXPORT)

  set(CMAKE_${_CMAKE_PROCESSING_LANGUAGE}_COMPILER_CUSTOM_CODE
"set(CMAKE_IAR_LINKARM \"${CMAKE_IAR_LINKARM}\")
set(CMAKE_IAR_ARCHIVE \"${CMAKE_IAR_ARCHIVE}\")
set(CMAKE_IAR_ELFTOOL \"${CMAKE_IAR_ELFTOOL}\")
set(CMAKE_IAR_ELFDUMP \"${CMAKE_IAR_ELFDUMP}\")
set(CMAKE_IAR_OBJMANIP \"${CMAKE_IAR_OBJMANIP}\")
set(CMAKE_IAR_LINKARM \"${CMAKE_IAR_LINKARM}\")
")


elseif("${CMAKE_${_CMAKE_PROCESSING_LANGUAGE}_COMPILER_ARCHITECTURE_ID}" STREQUAL "AVR")

  # For AVR and AVR32, IAR uses the "xlink" linker and the "xar" archiver:
  find_program(CMAKE_IAR_LINKER xlink.exe HINTS ${__iar_hints}
      DOC "The IAR AVR linker")
  find_program(CMAKE_IAR_AR xar.exe HINTS ${__iar_hints}
      DOC "The IAR archiver")
  mark_as_advanced(CMAKE_IAR_LINKER CMAKE_IAR_AR)

  set(CMAKE_${_CMAKE_PROCESSING_LANGUAGE}_COMPILER_CUSTOM_CODE
"set(CMAKE_IAR_LINKER \"${CMAKE_IAR_LINKER}\")
set(CMAKE_IAR_AR \"${CMAKE_IAR_AR}\")
")
endif()
