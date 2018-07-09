set(CMAKE_DL_LIBS "")
set(CMAKE_SHARED_LIBRARY_CREATE_C_FLAGS "-shared -rdata_shared")
set(CMAKE_SHARED_LIBRARY_RUNTIME_C_FLAG "-Wl,-rpath,")       # -rpath
set(CMAKE_SHARED_LIBRARY_RUNTIME_C_FLAG_SEP "")   # : or empty
if(NOT CMAKE_COMPILER_IS_GNUCXX)
  set(CMAKE_CXX_CREATE_STATIC_LIBRARY
      "<CMAKE_CXX_COMPILER> -ar -o <TARGET> <OBJECTS>")
  set (CMAKE_CXX_FLAGS_INIT "")
  set (CMAKE_CXX_FLAGS_DEBUG_INIT "-g")
  set (CMAKE_CXX_FLAGS_MINSIZEREL_INIT "-O3 -DNDEBUG")
  set (CMAKE_CXX_FLAGS_RELEASE_INIT "-O2 -DNDEBUG")
  set (CMAKE_CXX_FLAGS_RELWITHDEBINFO_INIT "-O2")
  set (CMAKE_C_FLAGS_INIT "")
endif()
# set flags for gcc support
include(Platform/UnixPaths)

if(NOT CMAKE_COMPILER_IS_GNUCC)
  set (CMAKE_C_CREATE_PREPROCESSED_SOURCE "<CMAKE_C_COMPILER> <FLAGS> -E <SOURCE> > <PREPROCESSED_SOURCE>")
  set (CMAKE_C_CREATE_ASSEMBLY_SOURCE
    "<CMAKE_C_COMPILER> <FLAGS> -S <SOURCE>"
    "mv `basename \"<SOURCE>\" | sed 's/\\.[^./]*$$//'`.s <ASSEMBLY_SOURCE>"
    )
endif()

if(NOT CMAKE_COMPILER_IS_GNUCXX)
  set (CMAKE_CXX_CREATE_PREPROCESSED_SOURCE "<CMAKE_CXX_COMPILER> <FLAGS> -E <SOURCE> > <PREPROCESSED_SOURCE>")
  set (CMAKE_CXX_CREATE_ASSEMBLY_SOURCE
    "<CMAKE_CXX_COMPILER> <FLAGS> -S <SOURCE>"
    "mv `basename \"<SOURCE>\" | sed 's/\\.[^./]*$$//'`.s <ASSEMBLY_SOURCE>"
    )
endif()

if(NOT CMAKE_COMPILER_IS_GNUG77)
  set (CMAKE_Fortran_CREATE_PREPROCESSED_SOURCE "<CMAKE_Fortran_COMPILER> <FLAGS> -E <SOURCE> > <PREPROCESSED_SOURCE>")
  set (CMAKE_Fortran_CREATE_ASSEMBLY_SOURCE
    "<CMAKE_Fortran_COMPILER> <FLAGS> -S <SOURCE>"
    "mv `basename \"<SOURCE>\" | sed 's/\\.[^./]*$$//'`.s <ASSEMBLY_SOURCE>"
    )
endif()

# Initialize C link type selection flags.  These flags are used when
# building a shared library, shared module, or executable that links
# to other libraries to select whether to use the static or shared
# versions of the libraries.
foreach(type SHARED_LIBRARY SHARED_MODULE EXE)
  set(CMAKE_${type}_LINK_STATIC_C_FLAGS "-Wl,-Bstatic")
  set(CMAKE_${type}_LINK_DYNAMIC_C_FLAGS "-Wl,-Bdynamic")
endforeach()

# The IRIX linker needs to find transitive shared library dependencies
# in the -L path.
set(CMAKE_LINK_DEPENDENT_LIBRARY_DIRS 1)
