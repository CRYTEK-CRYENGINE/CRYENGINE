// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// #SchematycTODO : Move serialization utils to separate header?

#pragma once

#include <CrySerialization/IArchive.h>
#include <CrySerialization/Math.h>

#include <CryMath/Angle.h>

#include <CrySchematyc/Reflection/TypeDesc.h>

namespace Schematyc
{
	template<int TMin, int TMax, int TSoftMin = 0, int TSoftMax = 100, typename TType = float>
	struct Range
	{
		Range() : value(0) {}
		Range(TType val) : value(val) {}

		operator TType() const { return value; }
		operator TType&() { return value; }
		void operator=(const TType other) { value = other; }
		bool operator==(const Range &rhs) const { return value == rhs.value; }

		TType value;
	};
	template<int TMin, int TMax, int TSoftMin, int TSoftMax>
	inline void ReflectType(CTypeDesc<Range<TMin, TMax, TSoftMin, TSoftMax, float>>& desc)
	{
		desc.SetGUID("{A3A15703-B420-42F0-97B7-4957B20CE376}"_cry_guid);
		desc.SetLabel("Range");
	}
	template<int TMin, int TMax, int TSoftMin, int TSoftMax>
	inline void ReflectType(CTypeDesc<Range<TMin, TMax, TSoftMin, TSoftMax, int>>& desc)
	{
		desc.SetGUID("{D6B663CD-94BD-4B26-B2DF-0AF244FD437C}"_cry_guid);
		desc.SetLabel("Range");
	}
	template<int TMin, int TMax, int TSoftMin, int TSoftMax, typename TType>
	inline bool Serialize(Serialization::IArchive& archive, Range<TMin, TMax, TSoftMin, TSoftMax, TType>& value, const char* szName, const char* szLabel)
	{
		return archive(Serialization::Range(value.value, (TType)TMin, (TType)TMax, (TType)TSoftMin, (TType)TSoftMax), szName, szLabel);
	}

	typedef Range<0, std::numeric_limits<int>::max()> PositiveFloat;

	template<typename T>
	struct UnitLength
	{
		UnitLength() : value(ZERO) {}
		UnitLength(const T& val) : value(val) {}
		operator T() const { return value; }
		operator T&() { return value; }
		operator const T&() const { return value; }
		void operator=(const T& other) { value = other; }
		bool operator==(const UnitLength &rhs) const { return value == rhs.value; }

		T value;
	};
	inline void ReflectType(CTypeDesc<UnitLength<Vec3>>& desc)
	{
		desc.SetGUID("{C5539EB6-D206-46AF-B247-8A105DF071D6}"_cry_guid);
		desc.SetLabel("Unit-length three-dimensional vector");
	}
	inline bool Serialize(Serialization::IArchive& archive, UnitLength<Vec3>& value, const char* szName, const char* szLabel)
	{
		if (archive(value.value, szName, szLabel) && archive.isEdit())
		{
			value.value.Normalize();
			return true;
		}

		return false;
	}
} // Schematyc
