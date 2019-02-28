// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  Created:     04/03/2015 by Filipe amim
//  Description:
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#ifndef PARAMTRAITSIMPL_H
#define PARAMTRAITSIMPL_H

#pragma once

namespace pfx2
{

ILINE bool Serialize(Serialization::IArchive& ar, SEnable& val, const char* name, const char* label)
{
	name = (name && *name != 0) ? name : "Enabled";
	if (!ar.isEdit())
	{
		if (ar.isInput() || (ar.isOutput() && val.m_value == false))
		{
			if (!ar(val.m_value, name, label))
			{
				if (ar.isInput())
					val.m_value = true;
				else
					return false;
			}
		}
	}
	else
	{
		return ar(val.m_value, name, name);
	}
	return true;
}

template<typename T, typename TTraits>
bool TValue<T, TTraits>::Serialize(Serialization::IArchive& ar, const char* name, const char* label)
{
	T v = TTraits::From(m_value);
	if (TTraits::HideDefault() && ar.isEdit())
	{
		// Create a toggle in editor to override default
		struct EnabledValue
		{
			T& m_value;

			void Serialize(Serialization::IArchive& ar)
			{
				bool enabled = m_value != Default();
				ar(enabled, "enabled", "^");
				if (enabled)
				{
					ar(Serialization::Range(m_value, HardMin(), HardMax()), "value", "^");
					if (m_value == Default())
						m_value = Enabled();
				}
				else
				{
					m_value = Default();
					string display = TTraits::DefaultName();
					ar(display, "value", "!^");
				}
			}
		};

		if (!ar(EnabledValue {v}, name, label))
			return false;
	}
	else
	{
		if (!ar(Serialization::Range(v, HardMin(), HardMax()), name, label))
			return false;
	}
	if (ar.isInput())
		Set(v);
	return true;
}

}

#endif // PARAMTRAITSIMPL_H
