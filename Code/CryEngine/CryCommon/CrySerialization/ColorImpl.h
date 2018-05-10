// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Color.h"
#include <CryMath/Cry_Color.h>

//////////////////////////////////////////////////////////////////////////
template<typename T>
struct SerializableColor_tpl : Color_tpl<T>
{
	bool Serialize(Serialization::IArchive& ar)
	{
		// Must be empty as it in Editing UI doesn't show any sub items
		return true;
	}
};

namespace Serialization
{
	// Only used for non UI serialization
	template<typename T> struct SSerializeColor_tpl
	{
		Color_tpl<T>& value;
		SSerializeColor_tpl(Color_tpl<T>& v) : value(v) {}
		void Serialize(Serialization::IArchive& ar)
		{
			ar(value.r, "r", "r");
			ar(value.g, "g", "g");
			ar(value.b, "b", "b");
			ar(value.a, "a", "a");
		}
	};
}

template<typename T>
bool Serialize(Serialization::IArchive& ar, Color_tpl<T>& color, const char* name, const char* label)
{
	if (ar.isEdit())
		return Serialize(ar, static_cast<SerializableColor_tpl<T>&>(color), name, label);
	else if (ar.caps(Serialization::IArchive::XML_VERSION_1))
	{
		typedef T (& Array)[4];
		return ar((Array)color, name, label);
	}
	else
	{
		return ar(Serialization::SStruct(Serialization::SSerializeColor_tpl<T>(color)), name, label);
	}
}
