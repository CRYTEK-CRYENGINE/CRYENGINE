include(Compiler/XL)
__compiler_xl(CXX)
string(APPEND CMAKE_CXX_FLAGS_RELEASE_INIT " -DNDEBUG")
string(APPEND CMAKE_CXX_FLAGS_MINSIZEREL_INIT " -DNDEBUG")

# -qthreaded     = Ensures that all optimizations will be thread-safe
# -qhalt=e       = Halt on error messages (rather than just severe errors)
string(APPEND CMAKE_CXX_FLAGS_INIT " -qthreaded -qhalt=e")

set(CMAKE_CXX_COMPILE_OBJECT
  "<CMAKE_CXX_COMPILER> -+ <DEFINES> <INCLUDES> <FLAGS> -o <OBJECT> -c <SOURCE>")
