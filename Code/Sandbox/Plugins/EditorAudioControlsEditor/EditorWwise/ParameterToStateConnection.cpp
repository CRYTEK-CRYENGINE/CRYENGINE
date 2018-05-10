// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ParameterToStateConnection.h"

namespace ACE
{
namespace Impl
{
namespace Wwise
{
//////////////////////////////////////////////////////////////////////////
void CParameterToStateConnection::Serialize(Serialization::IArchive& ar)
{
	float const value = m_value;

	ar(m_value, "value", "Value");

	if (ar.isInput())
	{
		if (fabs(value - m_value) > g_precision)
		{
			SignalConnectionChanged();
		}
	}
}
} // namespace Wwise
} // namespace Impl
} // namespace ACE
