# References:
#   - https://software.intel.com/en-us/articles/c0x-features-supported-by-intel-c-compiler
#   - https://software.intel.com/en-us/articles/c14-features-supported-by-intel-c-compiler
#   - http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/p0096r3.html

# FIXME: Intel C++ feature detection works only when simulating the GNU compiler.
# When simulating MSVC, Intel always sets __cplusplus to 199711L.
if("x${CMAKE_CXX_SIMULATE_ID}" STREQUAL "xMSVC")
  return()
endif()

set(_cmake_feature_test_cxx_variable_templates "__cpp_variable_templates >= 201304")
set(_cmake_feature_test_cxx_relaxed_constexpr "__cpp_constexpr >= 201304")

set(_cmake_oldestSupported "__INTEL_COMPILER >= 1210")
set(DETECT_CXX11 "((__cplusplus >= 201103L) || defined(__INTEL_CXX11_MODE__) || defined(__GXX_EXPERIMENTAL_CXX0X__))")
#ICC version 15 update 1 has a bug where __cplusplus is defined as 1 no matter
#if you are compiling as 98/11/14. So to properly detect C++14 with this version
#we look for the existence of __GXX_EXPERIMENTAL_CXX0X__ but not __INTEL_CXX11_MODE__
set(DETECT_BUGGY_ICC15 "((__INTEL_COMPILER == 1500) && (__INTEL_COMPILER_UPDATE == 1))")
set(DETECT_CXX14 "((__cplusplus >= 201300L) || ((__cplusplus == 201103L) && !defined(__INTEL_CXX11_MODE__)) || ((${DETECT_BUGGY_ICC15}) && defined(__GXX_EXPERIMENTAL_CXX0X__) && !defined(__INTEL_CXX11_MODE__) ) )")

set(Intel16_CXX14 "__INTEL_COMPILER >= 1600 && ${DETECT_CXX14}")
set(_cmake_feature_test_cxx_aggregate_default_initializers "${Intel16_CXX14}")
set(_cmake_feature_test_cxx_contextual_conversions "${Intel16_CXX14}")
set(_cmake_feature_test_cxx_generic_lambdas "${Intel16_CXX14}")
set(_cmake_feature_test_cxx_digit_separators "${Intel16_CXX14}")
# This test is supposed to work in Intel 14 but the compiler has a bug
# in versions 14 and 15::
# https://software.intel.com/en-us/forums/intel-c-compiler/topic/600514
# It also appears to fail with an internal compiler error on Intel 16.
#set(_cmake_feature_test_cxx_generalized_initializers "${Intel16_CXX14}")

set(Intel15_CXX14 "__INTEL_COMPILER >= 1500 && ${DETECT_CXX14}")
set(_cmake_feature_test_cxx_decltype_auto "${Intel15_CXX14}")
set(_cmake_feature_test_cxx_lambda_init_captures "${Intel15_CXX14}")
set(_cmake_feature_test_cxx_attribute_deprecated "${Intel15_CXX14}")
set(_cmake_feature_test_cxx_return_type_deduction "${Intel15_CXX14}")

set(Intel15_CXX11 "__INTEL_COMPILER >= 1500 && ${DETECT_CXX11}")
set(_cmake_feature_test_cxx_alignas "${Intel15_CXX11}")
set(_cmake_feature_test_cxx_alignof "${Intel15_CXX11}")
set(_cmake_feature_test_cxx_inheriting_constructors "${Intel15_CXX11}")
set(_cmake_feature_test_cxx_user_literals "${Intel15_CXX11}")
set(_cmake_feature_test_cxx_thread_local "${Intel15_CXX11}")

set(Intel14_CXX11 "${DETECT_CXX11} && (__INTEL_COMPILER > 1400 || (__INTEL_COMPILER == 1400 && __INTEL_COMPILER_UPDATE >= 2))")
# Documented as 12.0+ but in testing it only works on 14.0.2+
set(_cmake_feature_test_cxx_decltype_incomplete_return_types "${Intel14_CXX11}")

set(Intel14_CXX11 "__INTEL_COMPILER >= 1400 && ${DETECT_CXX11}")
set(_cmake_feature_test_cxx_delegating_constructors "${Intel14_CXX11}")
set(_cmake_feature_test_cxx_constexpr "${Intel14_CXX11}")
set(_cmake_feature_test_cxx_sizeof_member "${Intel14_CXX11}")
set(_cmake_feature_test_cxx_strong_enums "${Intel14_CXX11}")
set(_cmake_feature_test_cxx_reference_qualified_functions "${Intel14_CXX11}")
set(_cmake_feature_test_cxx_raw_string_literals "${Intel14_CXX11}")
set(_cmake_feature_test_cxx_unicode_literals "${Intel14_CXX11}")
set(_cmake_feature_test_cxx_inline_namespaces "${Intel14_CXX11}")
set(_cmake_feature_test_cxx_unrestricted_unions "${Intel14_CXX11}")
set(_cmake_feature_test_cxx_nonstatic_member_init "${Intel14_CXX11}")
set(_cmake_feature_test_cxx_enum_forward_declarations "${Intel14_CXX11}")
set(_cmake_feature_test_cxx_override "${Intel14_CXX11}")
set(_cmake_feature_test_cxx_final "${Intel14_CXX11}")
set(_cmake_feature_test_cxx_noexcept "${Intel14_CXX11}")
set(_cmake_feature_test_cxx_defaulted_move_initializers "${Intel14_CXX11}")

set(Intel13_CXX11 "__INTEL_COMPILER >= 1300 && ${DETECT_CXX11}")
set(_cmake_feature_test_cxx_explicit_conversions "${Intel13_CXX11}")
set(_cmake_feature_test_cxx_range_for "${Intel13_CXX11}")
# Cannot find Intel documentation for N2640: cxx_uniform_initialization
set(_cmake_feature_test_cxx_uniform_initialization "${Intel13_CXX11}")

set(Intel121_CXX11 "${_cmake_oldestSupported} && ${DETECT_CXX11}")
set(_cmake_feature_test_cxx_variadic_templates "${Intel121_CXX11}")
set(_cmake_feature_test_cxx_alias_templates "${Intel121_CXX11}")
set(_cmake_feature_test_cxx_nullptr "${Intel121_CXX11}")
set(_cmake_feature_test_cxx_trailing_return_types "${Intel121_CXX11}")
set(_cmake_feature_test_cxx_attributes "${Intel121_CXX11}")
set(_cmake_feature_test_cxx_default_function_template_args "${Intel121_CXX11}")
set(_cmake_feature_test_cxx_extended_friend_declarations "${Intel121_CXX11}")
set(_cmake_feature_test_cxx_rvalue_references "${Intel121_CXX11}")
set(_cmake_feature_test_cxx_decltype "${Intel121_CXX11}")
set(_cmake_feature_test_cxx_defaulted_functions "${Intel121_CXX11}")
set(_cmake_feature_test_cxx_deleted_functions "${Intel121_CXX11}")
set(_cmake_feature_test_cxx_local_type_template_args "${Intel121_CXX11}")
set(_cmake_feature_test_cxx_lambdas "${Intel121_CXX11}")
set(_cmake_feature_test_cxx_binary_literals "${Intel121_CXX11}")
set(_cmake_feature_test_cxx_static_assert "${Intel121_CXX11}")
set(_cmake_feature_test_cxx_right_angle_brackets "${Intel121_CXX11}")
set(_cmake_feature_test_cxx_auto_type "${Intel121_CXX11}")
set(_cmake_feature_test_cxx_extern_templates "${Intel121_CXX11}")
set(_cmake_feature_test_cxx_variadic_macros "${Intel121_CXX11}")
set(_cmake_feature_test_cxx_long_long_type "${Intel121_CXX11}")
set(_cmake_feature_test_cxx_func_identifier "${Intel121_CXX11}")
set(_cmake_feature_test_cxx_template_template_parameters "${Intel121_CXX11}")
