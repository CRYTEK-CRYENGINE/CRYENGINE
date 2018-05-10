# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

set(prefix "${TEST_PREFIX}")
set(suffix "${TEST_SUFFIX}")
set(extra_args ${TEST_EXTRA_ARGS})
set(properties ${TEST_PROPERTIES})
set(script)
set(suite)
set(tests)

function(add_command NAME)
  set(_args "")
  foreach(_arg ${ARGN})
    if(_arg MATCHES "[^-./:a-zA-Z0-9_]")
      set(_args "${_args} [==[${_arg}]==]")
    else()
      set(_args "${_args} ${_arg}")
    endif()
  endforeach()
  set(script "${script}${NAME}(${_args})\n" PARENT_SCOPE)
endfunction()

# Run test executable to get list of available tests
if(NOT EXISTS "${TEST_EXECUTABLE}")
  message(FATAL_ERROR
    "Specified test executable does not exist.\n"
    "  Path: '${TEST_EXECUTABLE}'"
  )
endif()
execute_process(
  COMMAND ${TEST_EXECUTOR} "${TEST_EXECUTABLE}" --gtest_list_tests
  TIMEOUT ${TEST_DISCOVERY_TIMEOUT}
  OUTPUT_VARIABLE output
  RESULT_VARIABLE result
)
if(NOT ${result} EQUAL 0)
  string(REPLACE "\n" "\n    " output "${output}")
  message(FATAL_ERROR
    "Error running test executable.\n"
    "  Path: '${TEST_EXECUTABLE}'\n"
    "  Result: ${result}\n"
    "  Output:\n"
    "    ${output}\n"
  )
endif()

string(REPLACE "\n" ";" output "${output}")

# Parse output
foreach(line ${output})
  # Skip header
  if(NOT line MATCHES "gtest_main\\.cc")
    # Do we have a module name or a test name?
    if(NOT line MATCHES "^  ")
      # Module; remove trailing '.' to get just the name...
      string(REGEX REPLACE "\\.( *#.*)?" "" suite "${line}")
      if(line MATCHES "#" AND NOT NO_PRETTY_TYPES)
        string(REGEX REPLACE "/[0-9]\\.+ +#.*= +" "/" pretty_suite "${line}")
      else()
        set(pretty_suite "${suite}")
      endif()
      string(REGEX REPLACE "^DISABLED_" "" pretty_suite "${pretty_suite}")
    else()
      # Test name; strip spaces and comments to get just the name...
      string(REGEX REPLACE " +" "" test "${line}")
      if(test MATCHES "#" AND NOT NO_PRETTY_VALUES)
        string(REGEX REPLACE "/[0-9]+#GetParam..=" "/" pretty_test "${test}")
      else()
        string(REGEX REPLACE "#.*" "" pretty_test "${test}")
      endif()
      string(REGEX REPLACE "^DISABLED_" "" pretty_test "${pretty_test}")
      string(REGEX REPLACE "#.*" "" test "${test}")
      # ...and add to script
      add_command(add_test
        "${prefix}${pretty_suite}.${pretty_test}${suffix}"
        ${TEST_EXECUTOR}
        "${TEST_EXECUTABLE}"
        "--gtest_filter=${suite}.${test}"
        "--gtest_also_run_disabled_tests"
        ${extra_args}
      )
      if(suite MATCHES "^DISABLED" OR test MATCHES "^DISABLED")
        add_command(set_tests_properties
          "${prefix}${pretty_suite}.${pretty_test}${suffix}"
          PROPERTIES DISABLED TRUE
        )
      endif()
      add_command(set_tests_properties
        "${prefix}${pretty_suite}.${pretty_test}${suffix}"
        PROPERTIES
        WORKING_DIRECTORY "${TEST_WORKING_DIR}"
        ${properties}
      )
     list(APPEND tests "${prefix}${pretty_suite}.${pretty_test}${suffix}")
    endif()
  endif()
endforeach()

# Create a list of all discovered tests, which users may use to e.g. set
# properties on the tests
add_command(set ${TEST_LIST} ${tests})

# Write CTest script
file(WRITE "${CTEST_FILE}" "${script}")
