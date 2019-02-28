// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySerialization/IArchive.h>
#include <CrySerialization/Decorators/ActionButton.h>
#include <functional>

namespace Serialization
{

typedef std::function<void ()> StdFunctionActionButtonCalback;

struct StdFunctionActionButton : public IActionButton
{
	StdFunctionActionButtonCalback callback;
	string                         icon;

	explicit StdFunctionActionButton(const StdFunctionActionButtonCalback& callback, const char* icon = "")
		: callback(callback)
		, icon(icon)
	{
	}

	// IActionButton

	virtual void Callback() const override
	{
		if (callback)
		{
			callback();
		}
	}

	virtual const char* Icon() const override
	{
		return icon.c_str();
	}

	virtual IActionButtonPtr Clone() const override
	{
		return IActionButtonPtr(new StdFunctionActionButton(callback, icon.c_str()));
	}

	// ~IActionButton
};

inline bool Serialize(Serialization::IArchive& ar, StdFunctionActionButton& button, const char* name, const char* label)
{
	if (ar.isEdit())
		return ar(Serialization::SStruct::forEdit(static_cast<Serialization::IActionButton&>(button)), name, label);
	else
		return false;
}

inline StdFunctionActionButton ActionButton(const StdFunctionActionButtonCalback& callback, const char* icon = "")
{
	return StdFunctionActionButton(callback, icon);
}

}

