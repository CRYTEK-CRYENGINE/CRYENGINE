set(_cmake_oldestSupported "_MSC_VER >= 1600")

# Not yet supported:
#set(_cmake_feature_test_c_static_assert "")
#set(_cmake_feature_test_c_restrict "")

set(_cmake_feature_test_c_variadic_macros "${_cmake_oldestSupported}")
set(_cmake_feature_test_c_function_prototypes "${_cmake_oldestSupported}")
