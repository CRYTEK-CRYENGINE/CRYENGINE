
macro(_record_compiler_features lang compile_flags feature_list)
  include("${CMAKE_ROOT}/Modules/Compiler/${CMAKE_${lang}_COMPILER_ID}-${lang}-FeatureTests.cmake" OPTIONAL)

  string(TOLOWER ${lang} lang_lc)
  file(REMOVE "${CMAKE_BINARY_DIR}/CMakeFiles/feature_tests.bin")
  file(WRITE "${CMAKE_BINARY_DIR}/CMakeFiles/feature_tests.${lang_lc}" "
  const char features[] = {\"\\n\"\n")

  get_property(known_features GLOBAL PROPERTY CMAKE_${lang}_KNOWN_FEATURES)

  foreach(feature ${known_features})
    if (_cmake_feature_test_${feature})
      if (${_cmake_feature_test_${feature}} STREQUAL 1)
        set(_feature_condition "\"1\" ")
      else()
        set(_feature_condition "#if ${_cmake_feature_test_${feature}}\n\"1\"\n#else\n\"0\"\n#endif\n")
      endif()
      file(APPEND "${CMAKE_BINARY_DIR}/CMakeFiles/feature_tests.${lang_lc}" "\"${lang}_FEATURE:\"\n${_feature_condition}\"${feature}\\n\"\n")
    endif()
  endforeach()
  file(APPEND "${CMAKE_BINARY_DIR}/CMakeFiles/feature_tests.${lang_lc}"
    "\n};\n\nint main(int argc, char** argv) { (void)argv; return features[argc]; }\n")

  try_compile(CMAKE_${lang}_FEATURE_TEST
    ${CMAKE_BINARY_DIR} "${CMAKE_BINARY_DIR}/CMakeFiles/feature_tests.${lang_lc}"
    COMPILE_DEFINITIONS "${compile_flags}"
    OUTPUT_VARIABLE _output
    COPY_FILE "${CMAKE_BINARY_DIR}/CMakeFiles/feature_tests.bin"
    COPY_FILE_ERROR _copy_error
    )
  if(CMAKE_${lang}_FEATURE_TEST AND NOT _copy_error)
    set(_result 0)
  else()
    set(_result 255)
  endif()
  unset(CMAKE_${lang}_FEATURE_TEST CACHE)

  if (_result EQUAL 0)
    file(APPEND ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeOutput.log
      "\n\nDetecting ${lang} [${compile_flags}] compiler features compiled with the following output:\n${_output}\n\n")
    if(EXISTS "${CMAKE_BINARY_DIR}/CMakeFiles/feature_tests.bin")
      file(STRINGS "${CMAKE_BINARY_DIR}/CMakeFiles/feature_tests.bin"
        features REGEX "${lang}_FEATURE:.*")
      foreach(info ${features})
        file(APPEND ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeOutput.log
          "    Feature record: ${info}\n")
        string(REPLACE "${lang}_FEATURE:" "" info ${info})
        string(SUBSTRING ${info} 0 1 has_feature)
        if(has_feature)
          string(REGEX REPLACE "^1" "" feature ${info})
          list(APPEND ${feature_list} ${feature})
        endif()
      endforeach()
    endif()
  else()
    file(APPEND ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeError.log
      "Detecting ${lang} [${compile_flags}] compiler features failed to compile with the following output:\n${_output}\n${_copy_error}\n\n")
  endif()
endmacro()

macro(_record_compiler_features_c std)
  list(APPEND CMAKE_C${std}_COMPILE_FEATURES c_std_${std})
  _record_compiler_features(C "${CMAKE_C${std}_STANDARD_COMPILE_OPTION}" CMAKE_C${std}_COMPILE_FEATURES)
endmacro()

macro(_record_compiler_features_cxx std)
  list(APPEND CMAKE_CXX${std}_COMPILE_FEATURES cxx_std_${std})
  _record_compiler_features(CXX "${CMAKE_CXX${std}_STANDARD_COMPILE_OPTION}" CMAKE_CXX${std}_COMPILE_FEATURES)
endmacro()
