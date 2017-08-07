set(CMAKE_C_VERBOSE_FLAG "-#")

set(CMAKE_C_COMPILE_OPTIONS_PIC -KPIC)
set(CMAKE_SHARED_LIBRARY_C_FLAGS "-KPIC")
set(CMAKE_SHARED_LIBRARY_CREATE_C_FLAGS "-G")
set(CMAKE_SHARED_LIBRARY_RUNTIME_C_FLAG "-R")
set(CMAKE_SHARED_LIBRARY_RUNTIME_C_FLAG_SEP ":")
set(CMAKE_SHARED_LIBRARY_SONAME_C_FLAG "-h")

string(APPEND CMAKE_C_FLAGS_INIT " ")
string(APPEND CMAKE_C_FLAGS_DEBUG_INIT " -g")
string(APPEND CMAKE_C_FLAGS_MINSIZEREL_INIT " -xO2 -xspace -DNDEBUG")
string(APPEND CMAKE_C_FLAGS_RELEASE_INIT " -xO3 -DNDEBUG")
string(APPEND CMAKE_C_FLAGS_RELWITHDEBINFO_INIT " -g -xO2 -DNDEBUG")

# Initialize C link type selection flags.  These flags are used when
# building a shared library, shared module, or executable that links
# to other libraries to select whether to use the static or shared
# versions of the libraries.
foreach(type SHARED_LIBRARY SHARED_MODULE EXE)
  set(CMAKE_${type}_LINK_STATIC_C_FLAGS "-Bstatic")
  set(CMAKE_${type}_LINK_DYNAMIC_C_FLAGS "-Bdynamic")
endforeach()

set(CMAKE_C_CREATE_PREPROCESSED_SOURCE "<CMAKE_C_COMPILER> <DEFINES> <INCLUDES> <FLAGS> -E <SOURCE> > <PREPROCESSED_SOURCE>")
set(CMAKE_C_CREATE_ASSEMBLY_SOURCE "<CMAKE_C_COMPILER> <DEFINES> <INCLUDES> <FLAGS> -S <SOURCE> -o <ASSEMBLY_SOURCE>")
