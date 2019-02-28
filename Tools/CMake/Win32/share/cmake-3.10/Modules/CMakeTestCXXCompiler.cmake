# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.


if(CMAKE_CXX_COMPILER_FORCED)
  # The compiler configuration was forced by the user.
  # Assume the user has configured all compiler information.
  set(CMAKE_CXX_COMPILER_WORKS TRUE)
  return()
endif()

include(CMakeTestCompilerCommon)

# Remove any cached result from an older CMake version.
# We now store this in CMakeCXXCompiler.cmake.
unset(CMAKE_CXX_COMPILER_WORKS CACHE)

# This file is used by EnableLanguage in cmGlobalGenerator to
# determine that that selected C++ compiler can actually compile
# and link the most basic of programs.   If not, a fatal error
# is set and cmake stops processing commands and will not generate
# any makefiles or projects.
if(NOT CMAKE_CXX_COMPILER_WORKS)
  PrintTestCompilerStatus("CXX" "")
  file(WRITE ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp/testCXXCompiler.cxx
    "#ifndef __cplusplus\n"
    "# error \"The CMAKE_CXX_COMPILER is set to a C compiler\"\n"
    "#endif\n"
    "int main(){return 0;}\n")
  try_compile(CMAKE_CXX_COMPILER_WORKS ${CMAKE_BINARY_DIR}
    ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp/testCXXCompiler.cxx
    OUTPUT_VARIABLE __CMAKE_CXX_COMPILER_OUTPUT)
  # Move result from cache to normal variable.
  set(CMAKE_CXX_COMPILER_WORKS ${CMAKE_CXX_COMPILER_WORKS})
  unset(CMAKE_CXX_COMPILER_WORKS CACHE)
  set(CXX_TEST_WAS_RUN 1)
endif()

if(NOT CMAKE_CXX_COMPILER_WORKS)
  PrintTestCompilerStatus("CXX" " -- broken")
  file(APPEND ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeError.log
    "Determining if the CXX compiler works failed with "
    "the following output:\n${__CMAKE_CXX_COMPILER_OUTPUT}\n\n")
  string(REPLACE "\n" "\n  " _output "${__CMAKE_CXX_COMPILER_OUTPUT}")
  message(FATAL_ERROR "The C++ compiler\n  \"${CMAKE_CXX_COMPILER}\"\n"
    "is not able to compile a simple test program.\nIt fails "
    "with the following output:\n  ${_output}\n\n"
    "CMake will not be able to correctly generate this project.")
else()
  if(CXX_TEST_WAS_RUN)
    PrintTestCompilerStatus("CXX" " -- works")
    file(APPEND ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeOutput.log
      "Determining if the CXX compiler works passed with "
      "the following output:\n${__CMAKE_CXX_COMPILER_OUTPUT}\n\n")
  endif()

  # Try to identify the ABI and configure it into CMakeCXXCompiler.cmake
  include(${CMAKE_ROOT}/Modules/CMakeDetermineCompilerABI.cmake)
  CMAKE_DETERMINE_COMPILER_ABI(CXX ${CMAKE_ROOT}/Modules/CMakeCXXCompilerABI.cpp)
  # Try to identify the compiler features
  include(${CMAKE_ROOT}/Modules/CMakeDetermineCompileFeatures.cmake)
  CMAKE_DETERMINE_COMPILE_FEATURES(CXX)

  # Re-configure to save learned information.
  configure_file(
    ${CMAKE_ROOT}/Modules/CMakeCXXCompiler.cmake.in
    ${CMAKE_PLATFORM_INFO_DIR}/CMakeCXXCompiler.cmake
    @ONLY
    )
  include(${CMAKE_PLATFORM_INFO_DIR}/CMakeCXXCompiler.cmake)

  if(CMAKE_CXX_SIZEOF_DATA_PTR)
    foreach(f ${CMAKE_CXX_ABI_FILES})
      include(${f})
    endforeach()
    unset(CMAKE_CXX_ABI_FILES)
  endif()
endif()

unset(__CMAKE_CXX_COMPILER_OUTPUT)
