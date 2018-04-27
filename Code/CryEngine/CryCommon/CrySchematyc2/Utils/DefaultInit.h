// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc2/Prerequisites.h"

namespace Schematyc2
{
	namespace Private
	{
		template <typename TYPE> struct SDefaultInit
		{
			inline const TYPE& operator () () const
			{
				static const TYPE s_defaultValue = TYPE();
				return s_defaultValue;
			}
		};

		template <typename TYPE, size_t SIZE> struct SDefaultInit<TYPE[SIZE]>
		{
			typedef TYPE Array[SIZE];

			inline const Array& operator () () const
			{
				static const Array s_defaultValue = { TYPE() };
				return s_defaultValue;
			}
		};

		template <> struct SDefaultInit<void>
		{
			inline void operator () () const {}
		};
	}

	template <typename TYPE> inline const TYPE& DefaultInit()
	{
		return Private::SDefaultInit<TYPE>()();
	}
}
