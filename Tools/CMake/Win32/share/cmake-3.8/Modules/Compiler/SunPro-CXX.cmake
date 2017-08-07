set(CMAKE_CXX_VERBOSE_FLAG "-v")

set(CMAKE_CXX_COMPILE_OPTIONS_PIC -KPIC)
set(CMAKE_SHARED_LIBRARY_CXX_FLAGS "-KPIC")
set(CMAKE_SHARED_LIBRARY_CREATE_CXX_FLAGS "-G")
set(CMAKE_SHARED_LIBRARY_RUNTIME_CXX_FLAG "-R")
set(CMAKE_SHARED_LIBRARY_RUNTIME_CXX_FLAG_SEP ":")
set(CMAKE_SHARED_LIBRARY_SONAME_CXX_FLAG "-h")

string(APPEND CMAKE_CXX_FLAGS_INIT " ")
string(APPEND CMAKE_CXX_FLAGS_DEBUG_INIT " -g")
string(APPEND CMAKE_CXX_FLAGS_MINSIZEREL_INIT " -xO2 -xspace -DNDEBUG")
string(APPEND CMAKE_CXX_FLAGS_RELEASE_INIT " -xO3 -DNDEBUG")
string(APPEND CMAKE_CXX_FLAGS_RELWITHDEBINFO_INIT " -g -xO2 -DNDEBUG")

# Initialize C link type selection flags.  These flags are used when
# building a shared library, shared module, or executable that links
# to other libraries to select whether to use the static or shared
# versions of the libraries.
foreach(type SHARED_LIBRARY SHARED_MODULE EXE)
  set(CMAKE_${type}_LINK_STATIC_CXX_FLAGS "-Bstatic")
  set(CMAKE_${type}_LINK_DYNAMIC_CXX_FLAGS "-Bdynamic")
endforeach()

set(CMAKE_CXX_CREATE_PREPROCESSED_SOURCE "<CMAKE_CXX_COMPILER> <DEFINES> <INCLUDES> <FLAGS> -E <SOURCE> > <PREPROCESSED_SOURCE>")
set(CMAKE_CXX_CREATE_ASSEMBLY_SOURCE "<CMAKE_CXX_COMPILER> <INCLUDES> <FLAGS> -S <SOURCE> -o <ASSEMBLY_SOURCE>")

# Create archives with "CC -xar" in case user adds "-instances=extern"
# so that template instantiations are available to archive members.
set(CMAKE_CXX_CREATE_STATIC_LIBRARY
  "<CMAKE_CXX_COMPILER> -xar -o <TARGET> <OBJECTS> "
  "<CMAKE_RANLIB> <TARGET> ")

if (NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 5.13)
  set(CMAKE_CXX98_STANDARD_COMPILE_OPTION "")
  set(CMAKE_CXX98_EXTENSION_COMPILE_OPTION "")
  set(CMAKE_CXX11_STANDARD_COMPILE_OPTION "-std=c++11")
  set(CMAKE_CXX11_EXTENSION_COMPILE_OPTION "-std=c++11")
endif()

if(NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 5.13)
  if (NOT CMAKE_CXX_COMPILER_FORCED)
    if (NOT CMAKE_CXX_STANDARD_COMPUTED_DEFAULT)
      message(FATAL_ERROR "CMAKE_CXX_STANDARD_COMPUTED_DEFAULT should be set for ${CMAKE_CXX_COMPILER_ID} (${CMAKE_CXX_COMPILER}) version ${CMAKE_CXX_COMPILER_VERSION}")
    endif()
    set(CMAKE_CXX_STANDARD_DEFAULT ${CMAKE_CXX_STANDARD_COMPUTED_DEFAULT})
  elseif(NOT DEFINED CMAKE_CXX_STANDARD_DEFAULT)
    # Compiler id was forced so just guess the default standard level.
    set(CMAKE_CXX_STANDARD_DEFAULT 98)
  endif()
endif()

macro(cmake_record_cxx_compile_features)
  set(_result 0)
  if (NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 5.13)
    if (_result EQUAL 0)
      _record_compiler_features_cxx(11)
    endif()
    if (_result EQUAL 0)
      _record_compiler_features_cxx(98)
    endif()
  endif()
endmacro()
