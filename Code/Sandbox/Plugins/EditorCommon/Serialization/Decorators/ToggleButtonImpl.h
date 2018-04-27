// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace Serialization
{

inline bool Serialize(Serialization::IArchive& ar, Serialization::ToggleButton& button, const char* name, const char* label)
{
	if (ar.isEdit())
		return ar(Serialization::SStruct::forEdit(button), name, label);
	else
		return ar(*button.value, name, label);
}

inline bool Serialize(Serialization::IArchive& ar, Serialization::RadioButton& button, const char* name, const char* label)
{
	if (ar.isEdit())
		return ar(Serialization::SStruct::forEdit(button), name, label);
	else
		return false;
}

}

