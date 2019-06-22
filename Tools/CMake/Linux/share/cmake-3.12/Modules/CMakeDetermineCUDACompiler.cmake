# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

include(${CMAKE_ROOT}/Modules/CMakeDetermineCompiler.cmake)
include(${CMAKE_ROOT}/Modules//CMakeParseImplicitLinkInfo.cmake)

if( NOT ( ("${CMAKE_GENERATOR}" MATCHES "Make") OR
          ("${CMAKE_GENERATOR}" MATCHES "Ninja") OR
          ("${CMAKE_GENERATOR}" MATCHES "Visual Studio (1|[9][0-9])") ) )
  message(FATAL_ERROR "CUDA language not currently supported by \"${CMAKE_GENERATOR}\" generator")
endif()

if(${CMAKE_GENERATOR} MATCHES "Visual Studio")
else()
  if(NOT CMAKE_CUDA_COMPILER)
    set(CMAKE_CUDA_COMPILER_INIT NOTFOUND)

      # prefer the environment variable CUDACXX
      if(NOT $ENV{CUDACXX} STREQUAL "")
        get_filename_component(CMAKE_CUDA_COMPILER_INIT $ENV{CUDACXX} PROGRAM PROGRAM_ARGS CMAKE_CUDA_FLAGS_ENV_INIT)
        if(CMAKE_CUDA_FLAGS_ENV_INIT)
          set(CMAKE_CUDA_COMPILER_ARG1 "${CMAKE_CUDA_FLAGS_ENV_INIT}" CACHE STRING "First argument to CXX compiler")
        endif()
        if(NOT EXISTS ${CMAKE_CUDA_COMPILER_INIT})
          message(FATAL_ERROR "Could not find compiler set in environment variable CUDACXX:\n$ENV{CUDACXX}.\n${CMAKE_CUDA_COMPILER_INIT}")
        endif()
      endif()

    # finally list compilers to try
    if(NOT CMAKE_CUDA_COMPILER_INIT)
      set(CMAKE_CUDA_COMPILER_LIST nvcc)
    endif()

    _cmake_find_compiler(CUDA)
  else()
    _cmake_find_compiler_path(CUDA)
  endif()

  mark_as_advanced(CMAKE_CUDA_COMPILER)
endif()

#Allow the user to specify a host compiler
if(NOT $ENV{CUDAHOSTCXX} STREQUAL "")
  get_filename_component(CMAKE_CUDA_HOST_COMPILER $ENV{CUDAHOSTCXX} PROGRAM)
  if(NOT EXISTS ${CMAKE_CUDA_HOST_COMPILER})
    message(FATAL_ERROR "Could not find compiler set in environment variable CUDAHOSTCXX:\n$ENV{CUDAHOSTCXX}.\n${CMAKE_CUDA_HOST_COMPILER}")
  endif()
endif()

# Build a small source file to identify the compiler.
if(NOT CMAKE_CUDA_COMPILER_ID_RUN)
  set(CMAKE_CUDA_COMPILER_ID_RUN 1)

  # Try to identify the compiler.
  set(CMAKE_CUDA_COMPILER_ID)
  set(CMAKE_CUDA_PLATFORM_ID)
  file(READ ${CMAKE_ROOT}/Modules/CMakePlatformId.h.in
    CMAKE_CUDA_COMPILER_ID_PLATFORM_CONTENT)

  list(APPEND CMAKE_CUDA_COMPILER_ID_MATCH_VENDORS NVIDIA)
  set(CMAKE_CUDA_COMPILER_ID_MATCH_VENDOR_REGEX_NVIDIA "nvcc: NVIDIA \(R\) Cuda compiler driver")

  set(CMAKE_CXX_COMPILER_ID_TOOL_MATCH_REGEX "\nLd[^\n]*(\n[ \t]+[^\n]*)*\n[ \t]+([^ \t\r\n]+)[^\r\n]*-o[^\r\n]*CompilerIdCUDA/(\\./)?(CompilerIdCUDA.xctest/)?CompilerIdCUDA[ \t\n\\\"]")
  set(CMAKE_CXX_COMPILER_ID_TOOL_MATCH_INDEX 2)

  set(CMAKE_CUDA_COMPILER_ID_FLAGS_ALWAYS -v --keep --keep-dir tmp)
  if(CMAKE_CUDA_HOST_COMPILER)
      list(APPEND CMAKE_CUDA_COMPILER_ID_FLAGS_ALWAYS "-ccbin=${CMAKE_CUDA_HOST_COMPILER}")
  endif()

  include(${CMAKE_ROOT}/Modules/CMakeDetermineCompilerId.cmake)
  CMAKE_DETERMINE_COMPILER_ID(CUDA CUDAFLAGS CMakeCUDACompilerId.cu)
endif()

set(_CMAKE_PROCESSING_LANGUAGE "CUDA")
include(CMakeFindBinUtils)
unset(_CMAKE_PROCESSING_LANGUAGE)

if(MSVC_CUDA_ARCHITECTURE_ID)
  set(SET_MSVC_CUDA_ARCHITECTURE_ID
    "set(MSVC_CUDA_ARCHITECTURE_ID ${MSVC_CUDA_ARCHITECTURE_ID})")
endif()

if(${CMAKE_GENERATOR} MATCHES "Visual Studio")
  set(CMAKE_CUDA_HOST_LINK_LAUNCHER "${CMAKE_LINKER}")
  set(CMAKE_CUDA_HOST_IMPLICIT_LINK_LIBRARIES "")
  set(CMAKE_CUDA_HOST_IMPLICIT_LINK_DIRECTORIES "")
  set(CMAKE_CUDA_HOST_IMPLICIT_LINK_FRAMEWORK_DIRECTORIES "")
elseif(CMAKE_CUDA_COMPILER_ID STREQUAL NVIDIA)
  set(_nvcc_log "")
  string(REPLACE "\r" "" _nvcc_output_orig "${CMAKE_CUDA_COMPILER_PRODUCED_OUTPUT}")
  if(_nvcc_output_orig MATCHES "#\\\$ +PATH= *([^\n]*)\n")
    set(_nvcc_path "${CMAKE_MATCH_1}")
    string(APPEND _nvcc_log "  found 'PATH=' string: [${_nvcc_path}]\n")
    string(REPLACE ":" ";" _nvcc_path "${_nvcc_path}")
  else()
    set(_nvcc_path "")
    string(REPLACE "\n" "\n    " _nvcc_output_log "\n${_nvcc_output_orig}")
    string(APPEND _nvcc_log "  no 'PATH=' string found in nvcc output:${_nvcc_output_log}\n")
  endif()
  if(_nvcc_output_orig MATCHES "#\\\$ +LIBRARIES= *([^\n]*)\n")
    set(_nvcc_libraries "${CMAKE_MATCH_1}")
    string(APPEND _nvcc_log "  found 'LIBRARIES=' string: [${_nvcc_libraries}]\n")
  else()
    set(_nvcc_libraries "")
    string(REPLACE "\n" "\n    " _nvcc_output_log "\n${_nvcc_output_orig}")
    string(APPEND _nvcc_log "  no 'LIBRARIES=' string found in nvcc output:${_nvcc_output_log}\n")
  endif()

  set(_nvcc_link_line "")
  if(_nvcc_libraries)
    # Remove variable assignments.
    string(REGEX REPLACE "#\\\$ *[^= ]+=[^\n]*\n" "" _nvcc_output "${_nvcc_output_orig}")
    # Split lines.
    string(REGEX REPLACE "\n+(#\\\$ )?" ";" _nvcc_output "${_nvcc_output}")
    foreach(line IN LISTS _nvcc_output)
      set(_nvcc_output_line "${line}")
      string(APPEND _nvcc_log "  considering line: [${_nvcc_output_line}]\n")
      if("${_nvcc_output_line}" MATCHES "^ *nvlink")
        string(APPEND _nvcc_log "    ignoring nvlink line\n")
      elseif(_nvcc_libraries)
        if("${_nvcc_output_line}" MATCHES "(@\"?tmp/a\\.exe\\.res\"?)")
          set(_nvcc_link_res_arg "${CMAKE_MATCH_1}")
          set(_nvcc_link_res "${CMAKE_PLATFORM_INFO_DIR}/CompilerIdCUDA/tmp/a.exe.res")
          if(EXISTS "${_nvcc_link_res}")
            file(READ "${_nvcc_link_res}" _nvcc_link_res_content)
            string(REPLACE "${_nvcc_link_res_arg}" "${_nvcc_link_res_content}" _nvcc_output_line "${_nvcc_output_line}")
          endif()
        endif()
        string(FIND "${_nvcc_output_line}" "${_nvcc_libraries}" _nvcc_libraries_pos)
        if(NOT _nvcc_libraries_pos EQUAL -1)
          set(_nvcc_link_line "${_nvcc_output_line}")
          string(APPEND _nvcc_log "    extracted link line: [${_nvcc_link_line}]\n")
        endif()
      endif()
    endforeach()
  endif()

  if(_nvcc_link_line)
    if("x${CMAKE_CUDA_SIMULATE_ID}" STREQUAL "xMSVC")
      set(CMAKE_CUDA_HOST_LINK_LAUNCHER "${CMAKE_LINKER}")
    else()
      #extract the compiler that is being used for linking
      separate_arguments(_nvcc_link_line_args UNIX_COMMAND "${_nvcc_link_line}")
      list(GET _nvcc_link_line_args 0 _nvcc_host_link_launcher)
      if(IS_ABSOLUTE "${_nvcc_host_link_launcher}")
        string(APPEND _nvcc_log "  extracted link launcher absolute path: [${_nvcc_host_link_launcher}]\n")
        set(CMAKE_CUDA_HOST_LINK_LAUNCHER "${_nvcc_host_link_launcher}")
      else()
        string(APPEND _nvcc_log "  extracted link launcher name: [${_nvcc_host_link_launcher}]\n")
        find_program(_nvcc_find_host_link_launcher
          NAMES ${_nvcc_host_link_launcher}
          PATHS ${_nvcc_path} NO_DEFAULT_PATH)
        find_program(_nvcc_find_host_link_launcher
          NAMES ${_nvcc_host_link_launcher})
        if(_nvcc_find_host_link_launcher)
          string(APPEND _nvcc_log "  found link launcher absolute path: [${_nvcc_find_host_link_launcher}]\n")
          set(CMAKE_CUDA_HOST_LINK_LAUNCHER "${_nvcc_find_host_link_launcher}")
        else()
          string(APPEND _nvcc_log "  could not find link launcher absolute path\n")
          set(CMAKE_CUDA_HOST_LINK_LAUNCHER "${_nvcc_host_link_launcher}")
        endif()
        unset(_nvcc_find_host_link_launcher CACHE)
      endif()
    endif()

    #prefix the line with cuda-fake-ld so that implicit link info believes it is
    #a link line
    set(_nvcc_link_line "cuda-fake-ld ${_nvcc_link_line}")
    CMAKE_PARSE_IMPLICIT_LINK_INFO("${_nvcc_link_line}"
                                   CMAKE_CUDA_HOST_IMPLICIT_LINK_LIBRARIES
                                   CMAKE_CUDA_HOST_IMPLICIT_LINK_DIRECTORIES
                                   CMAKE_CUDA_HOST_IMPLICIT_LINK_FRAMEWORK_DIRECTORIES
                                   log
                                   "${CMAKE_CUDA_IMPLICIT_OBJECT_REGEX}")

    file(APPEND ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeOutput.log
      "Parsed CUDA nvcc implicit link information from above output:\n${_nvcc_log}\n${log}\n\n")
  else()
    file(APPEND ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeError.log
      "Failed to parsed CUDA nvcc implicit link information:\n${_nvcc_log}\n\n")
    message(FATAL_ERROR "Failed to extract nvcc implicit link line.")
  endif()

  set(CMAKE_CUDA_TOOLKIT_INCLUDE_DIRECTORIES )
  if(_nvcc_output_orig MATCHES "#\\\$ +INCLUDES= *([^\n]*)\n")
    set(_nvcc_includes "${CMAKE_MATCH_1}")
    string(APPEND _nvcc_log "  found 'INCLUDES=' string: [${_nvcc_includes}]\n")
  else()
    set(_nvcc_includes "")
    string(REPLACE "\n" "\n    " _nvcc_output_log "\n${_nvcc_output_orig}")
    string(APPEND _nvcc_log "  no 'INCLUDES=' string found in nvcc output:${_nvcc_output_log}\n")
  endif()
  if(_nvcc_includes)
    # across all operating system each include directory is prefixed with -I
    separate_arguments(_nvcc_output UNIX_COMMAND "${_nvcc_includes}")
    foreach(line IN LISTS _nvcc_output)
      string(REGEX REPLACE "^-I" "" line "${line}")
      get_filename_component(line "${line}" ABSOLUTE)
      list(APPEND CMAKE_CUDA_TOOLKIT_INCLUDE_DIRECTORIES "${line}")
    endforeach()

    file(APPEND ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeOutput.log
      "Parsed CUDA nvcc include information from above output:\n${_nvcc_log}\n${log}\n\n")
  else()
    file(APPEND ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeOutput.log
      "Failed to detect CUDA nvcc include information:\n${_nvcc_log}\n\n")
  endif()


endif()

# configure all variables set in this file
configure_file(${CMAKE_ROOT}/Modules/CMakeCUDACompiler.cmake.in
  ${CMAKE_PLATFORM_INFO_DIR}/CMakeCUDACompiler.cmake
  @ONLY
  )

set(CMAKE_CUDA_COMPILER_ENV_VAR "CUDACXX")
set(CMAKE_CUDA_HOST_COMPILER_ENV_VAR "CUDAHOSTCXX")
