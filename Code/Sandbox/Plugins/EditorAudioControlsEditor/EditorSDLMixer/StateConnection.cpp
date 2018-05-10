// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "StateConnection.h"

#include <CrySerialization/Decorators/Range.h>

namespace ACE
{
namespace Impl
{
namespace SDLMixer
{
//////////////////////////////////////////////////////////////////////////
void CStateConnection::Serialize(Serialization::IArchive& ar)
{
	float const value = m_value;

	ar(Serialization::Range(m_value, 0.0f, 1.0f, 0.1f), "value", "Volume (normalized)");
	m_value = crymath::clamp(m_value, 0.0f, 1.0f);

	if (ar.isInput())
	{
		if (fabs(value - m_value) > g_precision)
		{
			SignalConnectionChanged();
		}
	}
}
} // namespace SDLMixer
} // namespace Impl
} // namespace ACE
