# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#.rst:
# FindGTest
# ---------
#
# Locate the Google C++ Testing Framework.
#
# Imported targets
# ^^^^^^^^^^^^^^^^
#
# This module defines the following :prop_tgt:`IMPORTED` targets:
#
# ``GTest::GTest``
#   The Google Test ``gtest`` library, if found; adds Thread::Thread
#   automatically
# ``GTest::Main``
#   The Google Test ``gtest_main`` library, if found
#
#
# Result variables
# ^^^^^^^^^^^^^^^^
#
# This module will set the following variables in your project:
#
# ``GTEST_FOUND``
#   Found the Google Testing framework
# ``GTEST_INCLUDE_DIRS``
#   the directory containing the Google Test headers
#
# The library variables below are set as normal variables.  These
# contain debug/optimized keywords when a debugging library is found.
#
# ``GTEST_LIBRARIES``
#   The Google Test ``gtest`` library; note it also requires linking
#   with an appropriate thread library
# ``GTEST_MAIN_LIBRARIES``
#   The Google Test ``gtest_main`` library
# ``GTEST_BOTH_LIBRARIES``
#   Both ``gtest`` and ``gtest_main``
#
# Cache variables
# ^^^^^^^^^^^^^^^
#
# The following cache variables may also be set:
#
# ``GTEST_ROOT``
#   The root directory of the Google Test installation (may also be
#   set as an environment variable)
# ``GTEST_MSVC_SEARCH``
#   If compiling with MSVC, this variable can be set to ``MT`` or
#   ``MD`` (the default) to enable searching a GTest build tree
#
#
# Example usage
# ^^^^^^^^^^^^^
#
# ::
#
#     enable_testing()
#     find_package(GTest REQUIRED)
#
#     add_executable(foo foo.cc)
#     target_link_libraries(foo GTest::GTest GTest::Main)
#
#     add_test(AllTestsInFoo foo)
#
#
# Deeper integration with CTest
# ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
#
# If you would like each Google test to show up in CTest as a test you
# may use the following macro::
#
#     GTEST_ADD_TESTS(executable extra_args files...)
#
# ``executable``
#   the path to the test executable
# ``extra_args``
#   a list of extra arguments to be passed to executable enclosed in
#   quotes (or ``""`` for none)
# ``files...``
#   a list of source files to search for tests and test fixtures.  Or
#   ``AUTO`` to find them from executable target
#
# However, note that this macro will slow down your tests by running
# an executable for each test and test fixture.
#
# Example usage::
#
#      set(FooTestArgs --foo 1 --bar 2)
#      add_executable(FooTest FooUnitTest.cc)
#      GTEST_ADD_TESTS(FooTest "${FooTestArgs}" AUTO)

#
# Thanks to Daniel Blezek <blezek@gmail.com> for the GTEST_ADD_TESTS code

function(GTEST_ADD_TESTS executable extra_args)
    if(NOT ARGN)
        message(FATAL_ERROR "Missing ARGN: Read the documentation for GTEST_ADD_TESTS")
    endif()
    if(ARGN STREQUAL "AUTO")
        # obtain sources used for building that executable
        get_property(ARGN TARGET ${executable} PROPERTY SOURCES)
    endif()
    set(gtest_case_name_regex ".*\\( *([A-Za-z_0-9]+) *, *([A-Za-z_0-9]+) *\\).*")
    set(gtest_test_type_regex "(TYPED_TEST|TEST_?[FP]?)")
    foreach(source ${ARGN})
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${source})
        file(READ "${source}" contents)
        string(REGEX MATCHALL "${gtest_test_type_regex} *\\(([A-Za-z_0-9 ,]+)\\)" found_tests ${contents})
        foreach(hit ${found_tests})
          string(REGEX MATCH "${gtest_test_type_regex}" test_type ${hit})

          # Parameterized tests have a different signature for the filter
          if("x${test_type}" STREQUAL "xTEST_P")
            string(REGEX REPLACE ${gtest_case_name_regex}  "*/\\1.\\2/*" test_name ${hit})
          elseif("x${test_type}" STREQUAL "xTEST_F" OR "x${test_type}" STREQUAL "xTEST")
            string(REGEX REPLACE ${gtest_case_name_regex} "\\1.\\2" test_name ${hit})
          elseif("x${test_type}" STREQUAL "xTYPED_TEST")
            string(REGEX REPLACE ${gtest_case_name_regex} "\\1/*.\\2" test_name ${hit})
          else()
            message(WARNING "Could not parse GTest ${hit} for adding to CTest.")
            continue()
          endif()
          add_test(NAME ${test_name} COMMAND ${executable} --gtest_filter=${test_name} ${extra_args})
        endforeach()
    endforeach()
endfunction()

function(_gtest_append_debugs _endvar _library)
    if(${_library} AND ${_library}_DEBUG)
        set(_output optimized ${${_library}} debug ${${_library}_DEBUG})
    else()
        set(_output ${${_library}})
    endif()
    set(${_endvar} ${_output} PARENT_SCOPE)
endfunction()

function(_gtest_find_library _name)
    find_library(${_name}
        NAMES ${ARGN}
        HINTS
            ENV GTEST_ROOT
            ${GTEST_ROOT}
        PATH_SUFFIXES ${_gtest_libpath_suffixes}
    )
    mark_as_advanced(${_name})
endfunction()

#

if(NOT DEFINED GTEST_MSVC_SEARCH)
    set(GTEST_MSVC_SEARCH MD)
endif()

set(_gtest_libpath_suffixes lib)
if(MSVC)
    if(GTEST_MSVC_SEARCH STREQUAL "MD")
        list(APPEND _gtest_libpath_suffixes
            msvc/gtest-md/Debug
            msvc/gtest-md/Release
            msvc/x64/Debug
            msvc/x64/Release
            )
    elseif(GTEST_MSVC_SEARCH STREQUAL "MT")
        list(APPEND _gtest_libpath_suffixes
            msvc/gtest/Debug
            msvc/gtest/Release
            msvc/x64/Debug
            msvc/x64/Release
            )
    endif()
endif()


find_path(GTEST_INCLUDE_DIR gtest/gtest.h
    HINTS
        $ENV{GTEST_ROOT}/include
        ${GTEST_ROOT}/include
)
mark_as_advanced(GTEST_INCLUDE_DIR)

if(MSVC AND GTEST_MSVC_SEARCH STREQUAL "MD")
    # The provided /MD project files for Google Test add -md suffixes to the
    # library names.
    _gtest_find_library(GTEST_LIBRARY            gtest-md  gtest)
    _gtest_find_library(GTEST_LIBRARY_DEBUG      gtest-mdd gtestd)
    _gtest_find_library(GTEST_MAIN_LIBRARY       gtest_main-md  gtest_main)
    _gtest_find_library(GTEST_MAIN_LIBRARY_DEBUG gtest_main-mdd gtest_maind)
else()
    _gtest_find_library(GTEST_LIBRARY            gtest)
    _gtest_find_library(GTEST_LIBRARY_DEBUG      gtestd)
    _gtest_find_library(GTEST_MAIN_LIBRARY       gtest_main)
    _gtest_find_library(GTEST_MAIN_LIBRARY_DEBUG gtest_maind)
endif()

include(${CMAKE_CURRENT_LIST_DIR}/FindPackageHandleStandardArgs.cmake)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(GTest DEFAULT_MSG GTEST_LIBRARY GTEST_INCLUDE_DIR GTEST_MAIN_LIBRARY)

if(GTEST_FOUND)
    set(GTEST_INCLUDE_DIRS ${GTEST_INCLUDE_DIR})
    _gtest_append_debugs(GTEST_LIBRARIES      GTEST_LIBRARY)
    _gtest_append_debugs(GTEST_MAIN_LIBRARIES GTEST_MAIN_LIBRARY)
    set(GTEST_BOTH_LIBRARIES ${GTEST_LIBRARIES} ${GTEST_MAIN_LIBRARIES})

    include(CMakeFindDependencyMacro)
    find_dependency(Threads)

    if(NOT TARGET GTest::GTest)
        add_library(GTest::GTest UNKNOWN IMPORTED)
        set_target_properties(GTest::GTest PROPERTIES
            INTERFACE_LINK_LIBRARIES "Threads::Threads")
        if(GTEST_INCLUDE_DIRS)
            set_target_properties(GTest::GTest PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES "${GTEST_INCLUDE_DIRS}")
        endif()
        if(EXISTS "${GTEST_LIBRARY}")
            set_target_properties(GTest::GTest PROPERTIES
                IMPORTED_LINK_INTERFACE_LANGUAGES "CXX"
                IMPORTED_LOCATION "${GTEST_LIBRARY}")
        endif()
        if(EXISTS "${GTEST_LIBRARY_RELEASE}")
            set_property(TARGET GTest::GTest APPEND PROPERTY
                IMPORTED_CONFIGURATIONS RELEASE)
            set_target_properties(GTest::GTest PROPERTIES
                IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
                IMPORTED_LOCATION_RELEASE "${GTEST_LIBRARY_RELEASE}")
        endif()
        if(EXISTS "${GTEST_LIBRARY_DEBUG}")
            set_property(TARGET GTest::GTest APPEND PROPERTY
                IMPORTED_CONFIGURATIONS DEBUG)
            set_target_properties(GTest::GTest PROPERTIES
                IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "CXX"
                IMPORTED_LOCATION_DEBUG "${GTEST_LIBRARY_DEBUG}")
        endif()
      endif()
      if(NOT TARGET GTest::Main)
          add_library(GTest::Main UNKNOWN IMPORTED)
          set_target_properties(GTest::Main PROPERTIES
              INTERFACE_LINK_LIBRARIES "GTest::GTest")
          if(EXISTS "${GTEST_MAIN_LIBRARY}")
              set_target_properties(GTest::Main PROPERTIES
                  IMPORTED_LINK_INTERFACE_LANGUAGES "CXX"
                  IMPORTED_LOCATION "${GTEST_MAIN_LIBRARY}")
          endif()
          if(EXISTS "${GTEST_MAIN_LIBRARY_RELEASE}")
            set_property(TARGET GTest::Main APPEND PROPERTY
                IMPORTED_CONFIGURATIONS RELEASE)
            set_target_properties(GTest::Main PROPERTIES
                IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
                IMPORTED_LOCATION_RELEASE "${GTEST_MAIN_LIBRARY_RELEASE}")
          endif()
          if(EXISTS "${GTEST_MAIN_LIBRARY_DEBUG}")
            set_property(TARGET GTest::Main APPEND PROPERTY
                IMPORTED_CONFIGURATIONS DEBUG)
            set_target_properties(GTest::Main PROPERTIES
                IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "CXX"
                IMPORTED_LOCATION_DEBUG "${GTEST_MAIN_LIBRARY_DEBUG}")
          endif()
    endif()
endif()
