// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ImplConnections.h"

#include <CrySerialization/Enum.h>
#include <CrySerialization/Decorators/Range.h>

namespace ACE
{
namespace PortAudio
{
//////////////////////////////////////////////////////////////////////////
void CConnection::Serialize(Serialization::IArchive& ar)
{
	ar(m_type, "action", "Action");
	ar(m_isPanningEnabled, "panning", "Enable Panning");

	if (ar.openBlock("DistanceAttenuation", "+Distance Attenuation"))
	{
		ar(m_isAttenuationEnabled, "attenuation", "Enable");

		if (m_isAttenuationEnabled)
		{
			if (ar.isInput())
			{
				float minAtt = m_minAttenuation;
				float maxAtt = m_maxAttenuation;
				ar(minAtt, "min_att", "Min Distance");
				ar(maxAtt, "max_att", "Max Distance");

				if (minAtt > maxAtt)
				{
					if (minAtt != m_minAttenuation)
					{
						maxAtt = minAtt;
					}
					else
					{
						minAtt = maxAtt;
					}
				}

				m_minAttenuation = minAtt;
				m_maxAttenuation = maxAtt;
			}
			else
			{
				ar(m_minAttenuation, "min_att", "Min Distance");
				ar(m_maxAttenuation, "max_att", "Max Distance");
			}
		}
		else
		{
			ar(m_minAttenuation, "min_att", "!Min Distance");
			ar(m_maxAttenuation, "max_att", "!Max Distance");
		}

		ar.closeBlock();
	}

	ar(Serialization::Range(m_volume, -96.0f, 0.0f), "vol", "Volume (dB)");

	if (ar.openBlock("Looping", "+Looping"))
	{
		ar(m_isInfiniteLoop, "infinite", "Infinite");

		if (m_isInfiniteLoop)
		{
			ar(m_loopCount, "loop_count", "!Count");
		}
		else
		{
			ar(m_loopCount, "loop_count", "Count");
		}

		ar.closeBlock();
	}

	if (ar.isInput())
	{
		signalConnectionChanged();
	}
}

SERIALIZATION_ENUM_BEGIN(EConnectionType, "Event Type")
SERIALIZATION_ENUM(EConnectionType::Start, "start", "Start")
SERIALIZATION_ENUM(EConnectionType::Stop, "stop", "Stop")
SERIALIZATION_ENUM_END()
} // namespace PortAudio
} //namespace ACE
