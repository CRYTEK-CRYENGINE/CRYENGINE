// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ImplConnections.h"

#include <CrySerialization/Enum.h>

namespace ACE
{
namespace Fmod
{
//////////////////////////////////////////////////////////////////////////
void CEventConnection::Serialize(Serialization::IArchive& ar)
{
	ar(m_type, "action", "Action");
	if (ar.isInput())
	{
		signalConnectionChanged();
	}
}

//////////////////////////////////////////////////////////////////////////
void CParamConnection::Serialize(Serialization::IArchive& ar)
{
	ar(m_mult, "mult", "Multiply");
	ar(m_shift, "shift", "Shift");

	if (ar.isInput())
	{
		signalConnectionChanged();
	}
}

//////////////////////////////////////////////////////////////////////////
void CParamToStateConnection::Serialize(Serialization::IArchive& ar)
{
	ar(m_value, "value", "Value");

	if (ar.isInput())
	{
		signalConnectionChanged();
	}
}

SERIALIZATION_ENUM_BEGIN(EEventType, "Event Type")
SERIALIZATION_ENUM(EEventType::Start, "start", "Start")
SERIALIZATION_ENUM(EEventType::Stop, "stop", "Stop")
SERIALIZATION_ENUM_END()
} // namespace Fmod
} //namespace ACE
