// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ParameterConnection.h"

#include <CrySerialization/Decorators/Range.h>

namespace ACE
{
namespace Impl
{
namespace SDLMixer
{
//////////////////////////////////////////////////////////////////////////
void CParameterConnection::Serialize(Serialization::IArchive& ar)
{
	float const mult = m_mult;
	float const shift = m_shift;

	ar(m_mult, "mult", "Multiply");
	m_mult = std::max(0.0f, m_mult);

	ar(Serialization::Range(m_shift, -1.0f, 1.0f, 0.1f), "shift", "Shift");
	m_shift = crymath::clamp(m_shift, -1.0f, 1.0f);

	if (ar.isInput())
	{
		if (fabs(mult - m_mult) > g_precision ||
		    fabs(shift - m_shift) > g_precision)
		{
			SignalConnectionChanged();
		}
	}
}
} // namespace SDLMixer
} // namespace Impl
} // namespace ACE
