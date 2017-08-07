// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

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
	if (ar.isEdit() && InfiniteMax())
	{
		// Enable infinite values in editor via a toggle
		struct EnabledValue
		{
			T& m_value;

			void Serialize(Serialization::IArchive& ar)
			{
				bool enabled = m_value < HardMax();
				ar(enabled, "enabled", "^");
				if (enabled)
				{
					bool res = ar(
						Serialization::Range(m_value, HardMin(), std::numeric_limits<T>::max()),
						"value", "^");
					if (m_value == HardMax())
						m_value = TType(1);
				}
				else
				{
					// Tell editor to display "Infinity" (which text archives already do)
					m_value = HardMax();
					string infinity = "Infinity";
					ar(infinity, "value", "!^");
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
