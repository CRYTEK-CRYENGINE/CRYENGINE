// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// #SchematycTODO : Merge with string utils?
// #SchematycTODO : Move to utils folder?

#pragma once

#include "CrySchematyc2/Prerequisites.h"

#include <CrySerialization/Forward.h>

#include "CrySchematyc2/Serialization/StringOArchive.h"
#include "CrySchematyc2/Utils/StringUtils.h"

namespace Schematyc2
{
	namespace Private
	{
		template <typename TYPE, bool TYPE_IS_ENUM = std::is_enum<TYPE>::value> struct SToString;

		template <typename TYPE> struct SToString<TYPE, true>
		{
			inline bool operator () (const TYPE& value, const CharArrayView& output) const
			{
				const Serialization::EnumDescription& enumDescription = Serialization::getEnumDescription<TYPE>();
				StringUtils::Copy(enumDescription.labelByIndex(enumDescription.indexByValue(static_cast<int>(value))), output);
				return true;
			}
		};

		template <typename TYPE> struct SToString<TYPE, false>
		{
			inline bool operator () (const TYPE& value, const CharArrayView& output) const
			{
				CStringOArchive archive(output);
				const bool      bResult = archive(value);
				archive.TrimRight();
				return bResult;
			}
		};
	}

	template <typename TYPE> inline bool ToString(const TYPE& value, const CharArrayView& output)
	{
		return Private::SToString<TYPE>()(value, output);
	}

	inline bool ToString(bool value, const CharArrayView& output)
	{
		StringUtils::BoolToString(value, output);
		return true;
	}

	inline bool ToString(int8 value, const CharArrayView& output)
	{
		StringUtils::Int8ToString(value, output);
		return true;
	}

	inline bool ToString(uint8 value, const CharArrayView& output)
	{
		StringUtils::UInt8ToString(value, output);
		return true;
	}

	inline bool ToString(int16 value, const CharArrayView& output)
	{
		StringUtils::Int16ToString(value, output);
		return true;
	}

	inline bool ToString(uint16 value, const CharArrayView& output)
	{
		StringUtils::UInt16ToString(value, output);
		return true;
	}

	inline bool ToString(int32 value, const CharArrayView& output)
	{
		StringUtils::Int32ToString(value, output);
		return true;
	}

	inline bool ToString(uint32 value, const CharArrayView& output)
	{
		StringUtils::UInt32ToString(value, output);
		return true;
	}

	inline bool ToString(int64 value, const CharArrayView& output)
	{
		StringUtils::Int64ToString(value, output);
		return true;
	}

	inline bool ToString(uint64 value, const CharArrayView& output)
	{
		StringUtils::UInt64ToString(value, output);
		return true;
	}

	inline bool ToString(float value, const CharArrayView& output)
	{
		StringUtils::FloatToString(value, output);
		return true;
	}

	inline bool ToString(const Vec2& value, const CharArrayView& output)
	{
		StringUtils::Vec2ToString(value, output);
		return true;
	}

	inline bool ToString(const Vec3& value, const CharArrayView& output)
	{
		StringUtils::Vec3ToString(value, output);
		return true;
	}

	inline bool ToString(const CPoolString& value, const CharArrayView& output)
	{
		StringUtils::Copy(value.c_str(), output);
		return true;
	}
}
