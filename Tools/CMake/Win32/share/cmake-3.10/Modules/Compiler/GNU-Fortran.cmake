include(Compiler/GNU)
__compiler_gnu(Fortran)

set(CMAKE_Fortran_PREPROCESS_SOURCE
  "<CMAKE_Fortran_COMPILER> -cpp <DEFINES> <INCLUDES> <FLAGS> -E <SOURCE> -o <PREPROCESSED_SOURCE>")

set(CMAKE_Fortran_FORMAT_FIXED_FLAG "-ffixed-form")
set(CMAKE_Fortran_FORMAT_FREE_FLAG "-ffree-form")

set(CMAKE_Fortran_POSTPROCESS_FLAG "-fpreprocessed")

# No -DNDEBUG for Fortran.
string(APPEND CMAKE_Fortran_FLAGS_MINSIZEREL_INIT " -Os")
string(APPEND CMAKE_Fortran_FLAGS_RELEASE_INIT " -O3")

# No -isystem for Fortran because it will not find .mod files.
unset(CMAKE_INCLUDE_SYSTEM_FLAG_Fortran)

# Fortran-specific feature flags.
set(CMAKE_Fortran_MODDIR_FLAG -J)
