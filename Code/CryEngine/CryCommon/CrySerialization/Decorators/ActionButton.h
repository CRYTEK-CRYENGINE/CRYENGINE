// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySerialization/IArchive.h>
#include <CryCore/functor.h>

namespace Serialization
{

struct IActionButton;

DECLARE_SHARED_POINTERS(IActionButton)

struct IActionButton
{
	virtual ~IActionButton() {}

	virtual void             Callback() const = 0;
	virtual const char*      Icon() const = 0;
	virtual IActionButtonPtr Clone() const = 0;
};

template<typename Functor>
struct FunctorActionButton : public IActionButton
{
	Functor callback;
	string  icon;

	FunctorActionButton() {}

	explicit FunctorActionButton(const Functor& callback, const char* icon = "")
		: callback(callback)
		, icon(icon)
	{
	}

	inline bool operator==(const FunctorActionButton &rhs) const 
	{
		// No way to compare callback
		return icon == rhs.icon;
	}

	// IActionButton

	virtual void Callback() const override
	{
		callback();
	}

	virtual const char* Icon() const override
	{
		return icon.c_str();
	}

	virtual IActionButtonPtr Clone() const override
	{
		return IActionButtonPtr(new FunctorActionButton<Functor>(callback, icon.c_str()));
	}

	// ~IActionButton
};

template<typename Functor>
inline bool Serialize(Serialization::IArchive& ar, FunctorActionButton<Functor>& button, const char* name, const char* label)
{
	if (ar.isEdit())
		return ar(Serialization::SStruct::forEdit(static_cast<Serialization::IActionButton&>(button)), name, label);
	else
		return false;
}

template<typename Functor>
inline FunctorActionButton<Functor> ActionButton(const Functor& callback, const char* icon = "")
{
	return FunctorActionButton<Functor>(callback, icon);
}

}
