// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ParameterToStateConnection.h"

#include <CrySerialization/Decorators/Range.h>

namespace ACE
{
namespace Impl
{
namespace Fmod
{
//////////////////////////////////////////////////////////////////////////
void CParameterToStateConnection::Serialize(Serialization::IArchive& ar)
{
	float const value = m_value;

	if (m_itemType == EItemType::Parameter)
	{
		ar(m_value, "value", "Value");
	}
	else if (m_itemType == EItemType::VCA)
	{
		ar(Serialization::Range(m_value, 0.0f, 1.0f), "value", "Value");
		m_value = crymath::clamp(m_value, 0.0f, 1.0f);
	}

	if (ar.isInput())
	{
		if (fabs(value - m_value) > g_precision)
		{
			SignalConnectionChanged();
		}
	}
}
} // namespace Fmod
} // namespace Impl
} // namespace ACE
