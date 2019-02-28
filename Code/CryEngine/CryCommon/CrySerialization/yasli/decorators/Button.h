// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySerialization/yasli/Archive.h>

namespace yasli{

struct Button
{
	Button(const char* _text = 0)
	: pressed(false)
	, text(_text) {}

	operator bool() const{
		return pressed;
	}
	void YASLI_SERIALIZE_METHOD(yasli::Archive& ar) {}

	bool pressed;
	const char* text;
};

inline bool YASLI_SERIALIZE_OVERRIDE(Archive& ar, Button& button, const char* name, const char* label)
{
	if (ar.isEdit())
		return ar(Serializer(button), name, label);
	else
	{
		button.pressed = false;
		return false;
	}
}

}
