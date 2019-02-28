// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryCore/CryCustomTypes.h>

#ifdef _MSC_VER
#define COMPILE_TIME_FUNCTION_NAME __FUNCSIG__
#else
#define COMPILE_TIME_FUNCTION_NAME __PRETTY_FUNCTION__
#endif

#define TYPE_OF(x) decltype(x)

// This macro declares a selection of conversion operators so that you can use enumerations to
// store flags without having to worry about type safety issues.
////////////////////////////////////////////////////////////////////////////////////////////////////
#define DECLARE_ENUM_FLAGS(type)                              \
inline const type operator & (const type lhs, const type rhs) \
{                                                             \
	return static_cast<const type>(+lhs & +rhs);                \
}                                                             \
                                                              \
inline const type operator &= (type &lhs, const type rhs)     \
{                                                             \
	return lhs = lhs & rhs;                                     \
}                                                             \
                                                              \
inline const type operator | (const type lhs, const type rhs) \
{                                                             \
	return static_cast<const type>(+lhs | +rhs);                \
}                                                             \
                                                              \
inline const type operator |= (type &lhs, const type rhs)     \
{                                                             \
	return lhs = lhs | rhs;                                     \
}                                                             \
                                                              \
inline const type operator ^ (const type lhs, const type rhs) \
{                                                             \
	return static_cast<const type>(+lhs ^ +rhs);                \
}                                                             \
                                                              \
inline const type operator ^= (type &lhs, const type rhs)     \
{                                                             \
	return lhs = lhs ^ rhs;                                     \
}                                                             \
                                                              \
inline const type operator ~ (const type rhs)                 \
{                                                             \
	return static_cast<const type>(~(+rhs));                    \
}

// This macro declares a selection of conversion operators so that you can use enumerations to
// store flags without having to worry about type safety issues.
////////////////////////////////////////////////////////////////////////////////////////////////////
#define DECLARE_ENUM_CLASS_FLAGS(enumType)                                                                                                                               \
inline const bool operator == (const enumType lhs, const std::underlying_type<enumType>::type rhs)                                                                       \
{                                                                                                                                                                        \
	return lhs == static_cast<const enumType>(rhs);                                                                                                                        \
}                                                                                                                                                                        \
                                                                                                                                                                         \
inline const bool operator != (const enumType lhs, const std::underlying_type<enumType>::type rhs)                                                                       \
{                                                                                                                                                                        \
	return lhs != static_cast<const enumType>(rhs);                                                                                                                        \
}                                                                                                                                                                        \
                                                                                                                                                                         \
inline const enumType operator & (const enumType lhs, const enumType rhs)                                                                                                \
{                                                                                                                                                                        \
	return static_cast<const enumType>(static_cast<const std::underlying_type<enumType>::type>(lhs) & static_cast<const std::underlying_type<enumType>::type>(rhs));       \
}                                                                                                                                                                        \
                                                                                                                                                                         \
inline const enumType operator &= (enumType &lhs, const enumType rhs)                                                                                                    \
{                                                                                                                                                                        \
	return lhs = static_cast<const enumType>(static_cast<const std::underlying_type<enumType>::type>(lhs) & static_cast<const std::underlying_type<enumType>::type>(rhs)); \
}                                                                                                                                                                        \
                                                                                                                                                                         \
inline const enumType operator | (const enumType lhs, const enumType rhs)                                                                                                \
{                                                                                                                                                                        \
	return static_cast<const enumType>(static_cast<const std::underlying_type<enumType>::type>(lhs) | static_cast<const std::underlying_type<enumType>::type>(rhs));       \
}                                                                                                                                                                        \
                                                                                                                                                                         \
inline const enumType operator |= (enumType &lhs, const enumType rhs)                                                                                                    \
{                                                                                                                                                                        \
	return lhs = static_cast<const enumType>(static_cast<const std::underlying_type<enumType>::type>(lhs) | static_cast<const std::underlying_type<enumType>::type>(rhs)); \
}                                                                                                                                                                        \
                                                                                                                                                                         \
inline const enumType operator ^ (const enumType lhs, const enumType rhs)                                                                                                \
{                                                                                                                                                                        \
	return static_cast<const enumType>(static_cast<const std::underlying_type<enumType>::type>(lhs) ^ static_cast<const std::underlying_type<enumType>::type>(rhs));       \
}                                                                                                                                                                        \
                                                                                                                                                                         \
inline const enumType operator ^= (enumType &lhs, const enumType rhs)                                                                                                    \
{                                                                                                                                                                        \
	return lhs = static_cast<const enumType>(static_cast<const std::underlying_type<enumType>::type>(lhs) ^ static_cast<const std::underlying_type<enumType>::type>(rhs)); \
}                                                                                                                                                                        \
                                                                                                                                                                         \
inline const enumType operator ~ (const enumType rhs)                                                                                                                    \
{                                                                                                                                                                        \
	return static_cast<const enumType>(~(static_cast<const std::underlying_type<enumType>::type>(rhs)));                                                                   \
}

#define PP_EMPTY
#define PP_COMMA         ,
#define PP_LEFT_BRACKET  (
#define PP_RIGHT_BRACKET )
#define PP_SEMI_COLON    ;
#define PP_COLON         :

#define PP_NUMBER(x)	x

#define PP_TO_STRING_(x)      #x
#define PP_TO_STRING(x)       PP_TO_STRING_(x)
#define PP_JOIN_XY_(x, y)     x##y
#define PP_JOIN_XY(x, y)      PP_JOIN_XY_(x, y)
#define PP_JOIN_XYZ_(x, y, z) x##y##z
#define PP_JOIN_XYZ(x, y, z)  PP_JOIN_XYZ_(x, y, z)

#define PP_BOOL_0  0
#define PP_BOOL_1  1
#define PP_BOOL_2  1
#define PP_BOOL_3  1
#define PP_BOOL_4  1
#define PP_BOOL_5  1
#define PP_BOOL_6  1
#define PP_BOOL_7  1
#define PP_BOOL_8  1
#define PP_BOOL_9  1
#define PP_BOOL_10 1
#define PP_BOOL(x) PP_JOIN_XY(PP_BOOL_, x)

#define PP_NOT_0  1
#define PP_NOT_1  0
#define PP_NOT(x) PP_JOIN_XY(PP_NOT_, x)

#define PP_AND_0_0     0
#define PP_AND_0_1     0
#define PP_AND_1_0     0
#define PP_AND_1_1     1
#define PP_AND_Y(x, y) PP_AND_##x##_##y
#define PP_AND_X(x, y) PP_AND_Y(x, y)
#define PP_AND(x, y)   PP_AND_X(PP_BOOL(x), PP_BOOL(y))

#define PP_OR_0_0     0
#define PP_OR_0_1     1
#define PP_OR_1_0     1
#define PP_OR_1_1     1
#define PP_OR_Y(x, y) PP_OR_##x##_##y
#define PP_OR_X(x, y) PP_OR_Y(x, y)
#define PP_OR(x, y)   PP_OR_X(PP_BOOL(x), PP_BOOL(y))

#define PP_IF_0(on_true)
#define PP_IF_1(on_true)  on_true
#define PP_IF(x, on_true) PP_JOIN_XY(PP_IF_, PP_BOOL(x))(on_true)

#define PP_COMMA_IF_0  PP_EMPTY
#define PP_COMMA_IF_1  PP_COMMA
#define PP_COMMA_IF(x) PP_JOIN_XY(PP_COMMA_IF_, PP_BOOL(x))

#define PP_COLON_IF_0  PP_EMPTY
#define PP_COLON_IF_1  PP_COLON
#define PP_COLON_IF(x) PP_JOIN_XY(PP_COLON_IF_, PP_BOOL(x))

#define PP_SELECT_0(on_true, on_false)  on_false
#define PP_SELECT_1(on_true, on_false)  on_true
#define PP_SELECT(x, on_true, on_false) PP_JOIN_XY(PP_SELECT_, PP_BOOL(x))(on_true, on_false)

#define PP_ENUM_0(element, separator, user_data)
#define PP_ENUM_1(element, separator, user_data)  element(0, user_data)
#define PP_ENUM_2(element, separator, user_data)  element(0, user_data) separator element(1, user_data)
#define PP_ENUM_3(element, separator, user_data)  element(0, user_data) separator element(1, user_data) separator element(2, user_data)
#define PP_ENUM_4(element, separator, user_data)  element(0, user_data) separator element(1, user_data) separator element(2, user_data) separator element(3, user_data)
#define PP_ENUM_5(element, separator, user_data)  element(0, user_data) separator element(1, user_data) separator element(2, user_data) separator element(3, user_data) separator element(4, user_data)
#define PP_ENUM_6(element, separator, user_data)  element(0, user_data) separator element(1, user_data) separator element(2, user_data) separator element(3, user_data) separator element(4, user_data) separator element(5, user_data)
#define PP_ENUM_7(element, separator, user_data)  element(0, user_data) separator element(1, user_data) separator element(2, user_data) separator element(3, user_data) separator element(4, user_data) separator element(5, user_data) separator element(6, user_data)
#define PP_ENUM_8(element, separator, user_data)  element(0, user_data) separator element(1, user_data) separator element(2, user_data) separator element(3, user_data) separator element(4, user_data) separator element(5, user_data) separator element(6, user_data) separator element(7, user_data)
#define PP_ENUM_9(element, separator, user_data)  element(0, user_data) separator element(1, user_data) separator element(2, user_data) separator element(3, user_data) separator element(4, user_data) separator element(5, user_data) separator element(6, user_data) separator element(7, user_data) separator element(8, user_data)
#define PP_ENUM_10(element, separator, user_data) element(0, user_data) separator element(1, user_data) separator element(2, user_data) separator element(3, user_data) separator element(4, user_data) separator element(5, user_data) separator element(6, user_data) separator element(7, user_data) separator element(8, user_data) separator element(9, user_data)

#define PP_ENUM(count, element, separator, user_data) PP_JOIN_XY(PP_ENUM_, count)(element, separator, user_data)

#define PP_COMMA_ENUM_0(element, user_data)
#define PP_COMMA_ENUM_1(element, user_data)  element(0, user_data)
#define PP_COMMA_ENUM_2(element, user_data)  element(0, user_data) PP_COMMA element(1, user_data)
#define PP_COMMA_ENUM_3(element, user_data)  element(0, user_data) PP_COMMA element(1, user_data) PP_COMMA element(2, user_data)
#define PP_COMMA_ENUM_4(element, user_data)  element(0, user_data) PP_COMMA element(1, user_data) PP_COMMA element(2, user_data) PP_COMMA element(3, user_data)
#define PP_COMMA_ENUM_5(element, user_data)  element(0, user_data) PP_COMMA element(1, user_data) PP_COMMA element(2, user_data) PP_COMMA element(3, user_data) PP_COMMA element(4, user_data)
#define PP_COMMA_ENUM_6(element, user_data)  element(0, user_data) PP_COMMA element(1, user_data) PP_COMMA element(2, user_data) PP_COMMA element(3, user_data) PP_COMMA element(4, user_data) PP_COMMA element(5, user_data)
#define PP_COMMA_ENUM_7(element, user_data)  element(0, user_data) PP_COMMA element(1, user_data) PP_COMMA element(2, user_data) PP_COMMA element(3, user_data) PP_COMMA element(4, user_data) PP_COMMA element(5, user_data) PP_COMMA element(6, user_data)
#define PP_COMMA_ENUM_8(element, user_data)  element(0, user_data) PP_COMMA element(1, user_data) PP_COMMA element(2, user_data) PP_COMMA element(3, user_data) PP_COMMA element(4, user_data) PP_COMMA element(5, user_data) PP_COMMA element(6, user_data) PP_COMMA element(7, user_data)
#define PP_COMMA_ENUM_9(element, user_data)  element(0, user_data) PP_COMMA element(1, user_data) PP_COMMA element(2, user_data) PP_COMMA element(3, user_data) PP_COMMA element(4, user_data) PP_COMMA element(5, user_data) PP_COMMA element(6, user_data) PP_COMMA element(7, user_data) PP_COMMA element(8, user_data)
#define PP_COMMA_ENUM_10(element, user_data) element(0, user_data) PP_COMMA element(1, user_data) PP_COMMA element(2, user_data) PP_COMMA element(3, user_data) PP_COMMA element(4, user_data) PP_COMMA element(5, user_data) PP_COMMA element(6, user_data) PP_COMMA element(7, user_data) PP_COMMA element(8, user_data) PP_COMMA element(9, user_data)

#define PP_COMMA_ENUM(count, element, user_data) PP_JOIN_XY(PP_COMMA_ENUM_, count)(element, user_data)

#define PP_ENUM_PARAMS_0(param)
#define PP_ENUM_PARAMS_1(param)  PP_JOIN_XY(param, 0)
#define PP_ENUM_PARAMS_2(param)  PP_JOIN_XY(param, 0), PP_JOIN_XY(param, 1)
#define PP_ENUM_PARAMS_3(param)  PP_JOIN_XY(param, 0), PP_JOIN_XY(param, 1), PP_JOIN_XY(param, 2)
#define PP_ENUM_PARAMS_4(param)  PP_JOIN_XY(param, 0), PP_JOIN_XY(param, 1), PP_JOIN_XY(param, 2), PP_JOIN_XY(param, 3)
#define PP_ENUM_PARAMS_5(param)  PP_JOIN_XY(param, 0), PP_JOIN_XY(param, 1), PP_JOIN_XY(param, 2), PP_JOIN_XY(param, 3), PP_JOIN_XY(param, 4)
#define PP_ENUM_PARAMS_6(param)  PP_JOIN_XY(param, 0), PP_JOIN_XY(param, 1), PP_JOIN_XY(param, 2), PP_JOIN_XY(param, 3), PP_JOIN_XY(param, 4), PP_JOIN_XY(param, 5)
#define PP_ENUM_PARAMS_7(param)  PP_JOIN_XY(param, 0), PP_JOIN_XY(param, 1), PP_JOIN_XY(param, 2), PP_JOIN_XY(param, 3), PP_JOIN_XY(param, 4), PP_JOIN_XY(param, 5), PP_JOIN_XY(param, 6)
#define PP_ENUM_PARAMS_8(param)  PP_JOIN_XY(param, 0), PP_JOIN_XY(param, 1), PP_JOIN_XY(param, 2), PP_JOIN_XY(param, 3), PP_JOIN_XY(param, 4), PP_JOIN_XY(param, 5), PP_JOIN_XY(param, 6), PP_JOIN_XY(param, 7)
#define PP_ENUM_PARAMS_9(param)  PP_JOIN_XY(param, 0), PP_JOIN_XY(param, 1), PP_JOIN_XY(param, 2), PP_JOIN_XY(param, 3), PP_JOIN_XY(param, 4), PP_JOIN_XY(param, 5), PP_JOIN_XY(param, 6), PP_JOIN_XY(param, 7), PP_JOIN_XY(param, 8)
#define PP_ENUM_PARAMS_10(param) PP_JOIN_XY(param, 0), PP_JOIN_XY(param, 1), PP_JOIN_XY(param, 2), PP_JOIN_XY(param, 3), PP_JOIN_XY(param, 4), PP_JOIN_XY(param, 5), PP_JOIN_XY(param, 6), PP_JOIN_XY(param, 7), PP_JOIN_XY(param, 8), PP_JOIN_XY(param, 9)

#define PP_ENUM_PARAMS(count, param) PP_JOIN_XY(PP_ENUM_PARAMS_, count)(param)