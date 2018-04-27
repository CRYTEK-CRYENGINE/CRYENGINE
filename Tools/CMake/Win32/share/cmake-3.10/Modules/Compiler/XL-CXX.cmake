include(Compiler/XL)
__compiler_xl(CXX)
string(APPEND CMAKE_CXX_FLAGS_RELEASE_INIT " -DNDEBUG")
string(APPEND CMAKE_CXX_FLAGS_MINSIZEREL_INIT " -DNDEBUG")

# -qthreaded = Ensures that all optimizations will be thread-safe
string(APPEND CMAKE_CXX_FLAGS_INIT " -qthreaded")

# XL v13.1.1 for Linux ppc64 little-endian switched to using a clang based
# front end and accepts the -std= option while only reserving -qlanglevel= for
# compatibility.  All other versions (previous versions on Linux ppc64
# little-endian, all versions on Linux ppc64 big-endian, all versions on AIX
# and BGQ, etc) are derived from the UNIX compiler and only accept the
# -qlanglvl option.
if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 10.1)
  if (CMAKE_SYSTEM MATCHES "Linux.*ppc64")
    if (CMAKE_SYSTEM MATCHES "Linux.*ppc64le" AND
        CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 13.1.1)
      set(CMAKE_CXX98_STANDARD_COMPILE_OPTION "-std=c++98")
      set(CMAKE_CXX98_EXTENSION_COMPILE_OPTION "-std=gnu++98")
      if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 13.1.2)
        set(CMAKE_CXX11_STANDARD_COMPILE_OPTION "-std=c++11")
        set(CMAKE_CXX11_EXTENSION_COMPILE_OPTION "-std=gnu++11")
        set(CMAKE_CXX14_STANDARD_COMPILE_OPTION "-std=c++1y")
        set(CMAKE_CXX14_EXTENSION_COMPILE_OPTION "-qlanglvl=extended1y")
      else ()
        set(CMAKE_CXX11_STANDARD_COMPILE_OPTION "-qlanglvl=extended0x")
        set(CMAKE_CXX11_EXTENSION_COMPILE_OPTION "-qlanglvl=extended0x")
      endif ()
    else ()
      # The non-clang based Linux ppc64 compiler, both big-endian and
      # little-endian lacks, the non-extension language level flags
      set(CMAKE_CXX98_STANDARD_COMPILE_OPTION "-qlanglvl=extended")
      set(CMAKE_CXX98_EXTENSION_COMPILE_OPTION "-qlanglvl=extended")
      set(CMAKE_CXX11_STANDARD_COMPILE_OPTION "-qlanglvl=extended0x")
      set(CMAKE_CXX11_EXTENSION_COMPILE_OPTION "-qlanglvl=extended0x")
    endif ()
  else ()
    set(CMAKE_CXX98_STANDARD_COMPILE_OPTION "-qlanglvl=strict98")
    set(CMAKE_CXX98_EXTENSION_COMPILE_OPTION "-qlanglvl=extended")
    set(CMAKE_CXX11_STANDARD_COMPILE_OPTION "-qlanglvl=extended0x")
    set(CMAKE_CXX11_EXTENSION_COMPILE_OPTION "-qlanglvl=extended0x")
  endif ()
endif ()

__compiler_check_default_language_standard(CXX 10.1 98)

set(CMAKE_CXX_COMPILE_OBJECT
  "<CMAKE_CXX_COMPILER> -+ <DEFINES> <INCLUDES> <FLAGS> -o <OBJECT> -c <SOURCE>")
