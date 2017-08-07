# Guard against multiple inclusions
if(__craylinux_crayprgenv)
  return()
endif()
set(__craylinux_crayprgenv 1)

macro(__cray_extract_args cmd tag_regex out_var make_absolute)
  string(REGEX MATCHALL "${tag_regex}" args "${cmd}")
  foreach(arg IN LISTS args)
    string(REGEX REPLACE "^${tag_regex}$" "\\2" param "${arg}")
    if(make_absolute)
      get_filename_component(param "${param}" ABSOLUTE)
    endif()
    list(APPEND ${out_var} ${param})
  endforeach()
endmacro()

function(__cray_extract_implicit src compiler_cmd link_cmd lang include_dirs_var link_dirs_var link_libs_var)
  set(BIN "${CMAKE_PLATFORM_INFO_DIR}/CrayExtractImplicit_${lang}.bin")
  execute_process(
    COMMAND ${CMAKE_${lang}_COMPILER} ${CMAKE_${lang}_VERBOSE_FLAG} -o ${BIN}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
    )
  if(EXISTS "${BIN}")
    file(REMOVE "${BIN}")
  endif()
  set(include_dirs)
  set(link_dirs)
  set(link_libs)
  string(REGEX REPLACE "\r?\n" ";" output_lines "${output}\n${error}")
  foreach(line IN LISTS output_lines)
    if("${line}" MATCHES "${compiler_cmd}")
      __cray_extract_args("${line}" " -(I ?|isystem )([^ ]*)" include_dirs 1)
      set(processed_include 1)
    endif()
    if("${line}" MATCHES "${link_cmd}")
      __cray_extract_args("${line}" " -(L ?)([^ ]*)" link_dirs 1)
      __cray_extract_args("${line}" " -(l ?)([^ ]*)" link_libs 0)
      set(processed_link 1)
    endif()
    if(processed_include AND processed_link)
      break()
    endif()
  endforeach()

  set(${include_dirs_var} "${include_dirs}" PARENT_SCOPE)
  set(${link_dirs_var}    "${link_dirs}" PARENT_SCOPE)
  set(${link_libs_var}    "${link_libs}" PARENT_SCOPE)
  set(CRAY_${lang}_EXTRACTED_IMPLICIT 1 CACHE INTERNAL "" FORCE)
endfunction()

macro(__CrayPrgEnv_setup lang test_src compiler_cmd link_cmd)
  if(DEFINED ENV{CRAYPE_VERSION})
    message(STATUS "Cray Programming Environment $ENV{CRAYPE_VERSION} ${lang}")
  elseif(DEFINED ENV{ASYNCPE_VERSION})
    message(STATUS "Cray XT Programming Environment $ENV{ASYNCPE_VERSION} ${lang}")
  else()
    message(STATUS "Cray Programming Environment (unknown version) ${lang}")
  endif()

  # Flags for the Cray wrappers
  set(CMAKE_STATIC_LIBRARY_LINK_${lang}_FLAGS "-static")
  set(CMAKE_SHARED_LIBRARY_${lang}_FLAGS "")
  set(CMAKE_SHARED_LIBRARY_CREATE_${lang}_FLAGS "-shared")
  set(CMAKE_SHARED_LIBRARY_LINK_${lang}_FLAGS "-dynamic")

  # If the link type is not explicitly specified in the environment then
  # the Cray wrappers assume that the code will be built staticly so
  # we check the following condition(s) are NOT met
  #  Compiler flags are explicitly dynamic
  #  Env var is dynamic and compiler flags are not explicitly static
  if(NOT (((CMAKE_${lang}_FLAGS MATCHES "(^| )-dynamic($| )") OR
         (CMAKE_EXE_LINKER_FLAGS MATCHES "(^| )-dynamic($| )"))
         OR
         (("$ENV{CRAYPE_LINK_TYPE}" STREQUAL "dynamic") AND
          NOT ((CMAKE_${lang}_FLAGS MATCHES "(^| )-static($| )") OR
               (CMAKE_EXE_LINKER_FLAGS MATCHES "(^| )-static($| )")))))
    set_property(GLOBAL PROPERTY TARGET_SUPPORTS_SHARED_LIBS FALSE)
    set(BUILD_SHARED_LIBS FALSE CACHE BOOL "")
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
    set(CMAKE_LINK_SEARCH_START_STATIC TRUE)
  endif()
  if(NOT CRAY_${lang}_EXTRACTED_IMPLICIT)
    __cray_extract_implicit(
      ${test_src} ${compiler_cmd} ${link_cmd} ${lang}
      CMAKE_${lang}_IMPLICIT_INCLUDE_DIRECTORIES
      CMAKE_${lang}_IMPLICIT_LINK_DIRECTORIES
      CMAKE_${lang}_IMPLICIT_LINK_LIBRARIES
      )
  endif()
endmacro()
