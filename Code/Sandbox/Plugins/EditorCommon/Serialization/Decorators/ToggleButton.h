// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySerialization/Forward.h>

namespace Serialization
{

struct ToggleButton
{
	bool*       value;
	const char* iconOn;
	const char* iconOff;

	ToggleButton(bool& value, const char* iconOn = "", const char* iconOff = "")
		: value(&value)
		, iconOn(iconOn)
		, iconOff(iconOff)
	{
	}
};

struct RadioButton
{
	int* value;
	int  buttonValue;

	RadioButton(int& value, int buttonValue)
		: value(&value)
		, buttonValue(buttonValue)
	{
	}
};

bool Serialize(Serialization::IArchive& ar, Serialization::ToggleButton& button, const char* name, const char* label);
bool Serialize(Serialization::IArchive& ar, Serialization::RadioButton& button, const char* name, const char* label);

}

#include "ToggleButtonImpl.h"

