include(Compiler/XL)
__compiler_xl(C)
string(APPEND CMAKE_C_FLAGS_RELEASE_INIT " -DNDEBUG")
string(APPEND CMAKE_C_FLAGS_MINSIZEREL_INIT " -DNDEBUG")

# -qthreaded = Ensures that all optimizations will be thread-safe
string(APPEND CMAKE_C_FLAGS_INIT " -qthreaded")

# XL v13.1.1 for Linux ppc64 little-endian switched to using a clang based
# front end and accepts the -std= option while only reserving -qlanglevel= for
# compatibility.  All other versions (previous versions on Linux ppc64
# little-endian, all versions on Linux ppc64 big-endian, all versions on AIX
# and BGQ, etc) are derived from the UNIX compiler and only accept the
# -qlanglvl option.
if (CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 10.1)
  if (CMAKE_SYSTEM MATCHES "Linux.*ppc64le" AND
      CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 13.1.1)
    set(CMAKE_C90_STANDARD_COMPILE_OPTION  "-std=c89")
    set(CMAKE_C90_EXTENSION_COMPILE_OPTION "-std=gnu89")
    set(CMAKE_C99_STANDARD_COMPILE_OPTION  "-std=c99")
    set(CMAKE_C99_EXTENSION_COMPILE_OPTION "-std=gnu99")
    if (CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 13.1.2)
      set(CMAKE_C11_STANDARD_COMPILE_OPTION  "-std=c11")
      set(CMAKE_C11_EXTENSION_COMPILE_OPTION "-std=gnu11")
    else ()
      set(CMAKE_C11_STANDARD_COMPILE_OPTION "-qlanglvl=extc1x")
      set(CMAKE_C11_EXTENSION_COMPILE_OPTION "-qlanglvl=extc1x")
    endif ()
  else ()
    set(CMAKE_C90_STANDARD_COMPILE_OPTION "-qlanglvl=stdc89")
    set(CMAKE_C90_EXTENSION_COMPILE_OPTION "-qlanglvl=extc89")
    set(CMAKE_C99_STANDARD_COMPILE_OPTION "-qlanglvl=stdc99")
    set(CMAKE_C99_EXTENSION_COMPILE_OPTION "-qlanglvl=extc99")
    if (CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 12.1)
      set(CMAKE_C11_STANDARD_COMPILE_OPTION "-qlanglvl=extc1x")
      set(CMAKE_C11_EXTENSION_COMPILE_OPTION "-qlanglvl=extc1x")
    endif ()
  endif ()
endif()

__compiler_check_default_language_standard(C 10.1 90)
