// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "EventConnection.h"

#include <CrySerialization/Enum.h>

namespace ACE
{
namespace Impl
{
namespace Fmod
{
//////////////////////////////////////////////////////////////////////////
void CEventConnection::Serialize(Serialization::IArchive& ar)
{
	EActionType const actionType = m_actionType;

	ar(m_actionType, "action", "Action");

	if (ar.isInput())
	{
		if (actionType != m_actionType)
		{
			SignalConnectionChanged();
		}
	}
}

SERIALIZATION_ENUM_BEGIN_NESTED(CEventConnection, EActionType, "Action Type")
SERIALIZATION_ENUM(CEventConnection::EActionType::Start, "start", "Start")
SERIALIZATION_ENUM(CEventConnection::EActionType::Stop, "stop", "Stop")
SERIALIZATION_ENUM(CEventConnection::EActionType::Pause, "pause", "Pause")
SERIALIZATION_ENUM(CEventConnection::EActionType::Resume, "resume", "Resume")
SERIALIZATION_ENUM_END()
} // namespace Fmod
} // namespace Impl
} // namespace ACE
