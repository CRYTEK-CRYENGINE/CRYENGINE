// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySerialization/IArchive.h>
#include <CrySerialization/Decorators/Range.h>

template<typename T>
inline bool Serialize(Serialization::IArchive& ar, Color_tpl<T>& c, const char* name, const char* label);

namespace Serialization
{
struct Vec3AsColor
{
	Vec3& v;
	Vec3AsColor(Vec3& v) : v(v) {}

	void Serialize(Serialization::IArchive& ar)
	{
		ar(Range(v.x, 0.0f, 1.0f), "r", nullptr);
		ar(Range(v.y, 0.0f, 1.0f), "g", nullptr);
		ar(Range(v.z, 0.0f, 1.0f), "b", nullptr);
	}
};

inline bool Serialize(Serialization::IArchive& ar, Vec3AsColor& c, const char* name, const char* label)
{
	if (ar.isEdit())
	{
		return ar(Serialization::SStruct(c), name, label);
	}
	else if (ar.caps(Serialization::IArchive::XML_VERSION_1))
	{
		typedef float (* Array)[3];
		return ar(*((Array) & c.v.x), name, label);
	}
	else
	{
		return ar(Serialization::SStruct(c), name, label);
	}
}
}

#include "ColorImpl.h"
