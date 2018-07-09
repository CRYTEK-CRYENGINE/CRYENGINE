/**
 *  yasli - Serialization Library.
 *  Copyright (C) 2007-2013 Evgeny Andreeshchev <eugene.andreeshchev@gmail.com>
 *                          Alexander Kotliar <alexander.kotliar@gmail.com>
 * 
 *  This code is distributed under the MIT License:
 *                          http://www.opensource.org/licenses/MIT
 */

#include "StdAfx.h"
#include "PropertyTree.h"
#include "PropertyTreeModel.h"
#include "Serialization.h"
#include "PropertyRowNumber.h"

bool isDigit(int ch)
{
	return unsigned(ch - '0') <= 9;
}

double parseFloat(const char* s)
{
	double res = 0, f = 1, sign = 1;
	while (*s && (*s == ' ' || *s == '\t'))
		s++;

	if (*s == '-')
		sign = -1, s++;
	else if (*s == '+')
		s++;

	for (; isDigit(*s); s++)
		res = res * 10 + (*s - '0');

	if (*s == '.' || *s == ',')
		for (s++; isDigit(*s); s++)
			res += (f *= 0.1)*(*s - '0');
	return res*sign;
}

#define REGISTER_NUMBER_ROW(TypeName, postfix) \
	typedef PropertyRowNumber<TypeName> PropertyRow##postfix; \
	typedef yasli::RangeDecorator<TypeName> RangeDecorator##postfix; \
	REGISTER_IN_PROPERTY_ROW_FACTORY(yasli::TypeID::get<RangeDecorator##postfix>().name(), PropertyRow##postfix); \
	YASLI_CLASS_NAME(PropertyRow, PropertyRow##postfix, #TypeName, #TypeName);

using yasli::string;
using yasli::i8;
using yasli::i16;
using yasli::i32;
using yasli::i64;
using yasli::u8;
using yasli::u16;
using yasli::u32;
using yasli::u64;

REGISTER_NUMBER_ROW(float, Float)
REGISTER_NUMBER_ROW(double , Double)
REGISTER_NUMBER_ROW(char, Char)
REGISTER_NUMBER_ROW(i8, I8)
REGISTER_NUMBER_ROW(i16, I16)
REGISTER_NUMBER_ROW(i32, I32)
REGISTER_NUMBER_ROW(i64, I64)
REGISTER_NUMBER_ROW(u8, U8)
REGISTER_NUMBER_ROW(u16, U16)
REGISTER_NUMBER_ROW(u32, U32)
REGISTER_NUMBER_ROW(u64, U64)

#undef REGISTER_NUMBER_ROW

DECLARE_SEGMENT(PropertyRowNumber)

// ---------------------------------------------------------------------------

