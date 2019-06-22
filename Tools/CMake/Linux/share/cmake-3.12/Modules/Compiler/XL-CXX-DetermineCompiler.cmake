
set(_compiler_id_pp_test "defined(__ibmxl__) || (defined(__IBMCPP__) && !defined(__COMPILER_VER__) && __IBMCPP__ >= 800)")

include("${CMAKE_CURRENT_LIST_DIR}/IBMCPP-CXX-DetermineVersionInternal.cmake")
