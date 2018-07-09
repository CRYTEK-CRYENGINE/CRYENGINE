// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace Cry {
namespace Type {
namespace Traits {

// Operator: Equal
template<typename TYPE>
std::false_type operator==(TYPE const&, TYPE const&) {}

template<typename TYPE>
struct IsEqualComparable : std::integral_constant<bool, !std::is_same<decltype(std::declval<TYPE const&>() == std::declval<TYPE const&>()), std::false_type>::value>
{};

template<>
struct IsEqualComparable<void> : std::integral_constant<bool, false> {};

// Operator: Unequal
template<typename TYPE>
std::false_type operator!=(TYPE const&, TYPE const&) {}

template<typename TYPE>
struct IsUnequalComparable : std::integral_constant<bool, !std::is_same<decltype(std::declval<TYPE const&>() != std::declval<TYPE const&>()), std::false_type>::value>
{};

template<>
struct IsUnequalComparable<void> : std::integral_constant<bool, false> {};

// Operator: Greater or equal
template<typename TYPE>
std::false_type operator>=(TYPE const&, TYPE const&) {}

template<typename TYPE>
struct IsGreaterOrEqualComparable : std::integral_constant<bool, !std::is_same<decltype(std::declval<TYPE const&>() >= std::declval<TYPE const&>()), std::false_type>::value>
{};

template<>
struct IsGreaterOrEqualComparable<void> : std::integral_constant<bool, false> {};

// Operator: Less or equal
template<typename TYPE>
std::false_type operator<=(TYPE const&, TYPE const&) {}

template<typename TYPE>
struct IsLessOrEqualComparable : std::integral_constant<bool, !std::is_same<decltype(std::declval<TYPE const&>() <= std::declval<TYPE const&>()), std::false_type>::value>
{};

template<>
struct IsLessOrEqualComparable<void> : std::integral_constant<bool, false> {};

// Operator: Greater
template<typename TYPE>
std::false_type operator>(TYPE const&, TYPE const&) {}

template<typename TYPE>
struct IsGreaterComparable : std::integral_constant<bool, !std::is_same<decltype(std::declval<TYPE const&>() > std::declval<TYPE const&>()), std::false_type>::value>
{};

template<>
struct IsGreaterComparable<void> : std::integral_constant<bool, false> {};

// Operator: Less
template<typename TYPE>
std::false_type operator<(TYPE const&, TYPE const&) {}

template<typename TYPE>
struct IsLessComparable : std::integral_constant < bool, !std::is_same < decltype(std::declval<TYPE const&>() < std::declval<TYPE const&>()), std::false_type > ::value >
{};

template<>
struct IsLessComparable<void> : std::integral_constant<bool, false> {};

// Operator: Addition
template<typename TYPE>
std::false_type operator+(TYPE const&, TYPE const&) {}

template<typename TYPE>
struct IsAddable : std::integral_constant<bool, !std::is_same<decltype(std::declval<TYPE const&>() + std::declval<TYPE const&>()), std::false_type>::value>
{};

template<>
struct IsAddable<void> : std::integral_constant<bool, false> {};

// Operator: Addition assign
template<typename TYPE>
std::false_type operator+=(TYPE const&, TYPE const&) {}

template<typename TYPE>
struct IsAddAssignable : std::integral_constant<bool, !std::is_same<decltype(std::declval<TYPE const&>() += std::declval<TYPE const&>()), std::false_type>::value>
{};

template<>
struct IsAddAssignable<void> : std::integral_constant<bool, false> {};

// Operator: Subtraction
template<typename TYPE>
std::false_type operator-(TYPE const&, TYPE const&) {}

template<typename TYPE>
struct IsDeductable : std::integral_constant<bool, !std::is_same<decltype(std::declval<TYPE const&>() - std::declval<TYPE const&>()), std::false_type>::value>
{};

template<>
struct IsDeductable<void> : std::integral_constant<bool, false> {};

// Operator: Subtract assign
template<typename TYPE>
std::false_type operator-=(TYPE const&, TYPE const&) {}

template<typename TYPE>
struct IsDeductAssignable : std::integral_constant<bool, !std::is_same<decltype(std::declval<TYPE const&>() -= std::declval<TYPE const&>()), std::false_type>::value>
{};

template<>
struct IsDeductAssignable<void> : std::integral_constant<bool, false> {};

// Operator: Multiplication
template<typename TYPE>
std::false_type operator*(TYPE const&, TYPE const&) {}

template<typename TYPE>
struct IsMultipliable : std::integral_constant<bool, !std::is_same<decltype(std::declval<TYPE const&>()* std::declval<TYPE const&>()), std::false_type>::value>
{};

template<>
struct IsMultipliable<void> : std::integral_constant<bool, false> {};

// Operator: Multiplication assign
template<typename TYPE>
std::false_type operator*=(TYPE const&, TYPE const&) {}

template<typename TYPE>
struct IsMultiplyAssignable : std::integral_constant<bool, !std::is_same<decltype(std::declval<TYPE const&>() *= std::declval<TYPE const&>()), std::false_type>::value>
{};

template<>
struct IsMultiplyAssignable<void> : std::integral_constant<bool, false> {};

// Operator: Division
template<typename TYPE>
std::false_type operator/(TYPE const&, TYPE const&) {}

template<typename TYPE>
struct IsDividable : std::integral_constant<bool, !std::is_same<decltype(std::declval<TYPE const&>() / std::declval<TYPE const&>()), std::false_type>::value>
{};

template<>
struct IsDividable<void> : std::integral_constant<bool, false> {};

// Operator: Division assign
template<typename TYPE>
std::false_type operator/=(TYPE const&, TYPE const&) {}

template<typename TYPE>
struct IsDivideAssignable : std::integral_constant<bool, !std::is_same<decltype(std::declval<TYPE const&>() /= std::declval<TYPE const&>()), std::false_type>::value>
{};

template<>
struct IsDivideAssignable<void> : std::integral_constant<bool, false> {};

//////////////////////////////////////////////////////////////////////////
// Methods
//////////////////////////////////////////////////////////////////////////

// Operator: Conversion
template<typename TYPE_LHS, typename TYPE_RHS>
struct IsConvertible : std::integral_constant<bool, !std::is_convertible<TYPE_LHS, TYPE_RHS>::value>
{};

} // ~Traits namespace
} // ~Typen amespace
} // ~Cry namespace
