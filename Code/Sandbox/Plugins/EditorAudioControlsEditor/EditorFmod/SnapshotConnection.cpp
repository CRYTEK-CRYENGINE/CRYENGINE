// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SnapshotConnection.h"

#include <CrySerialization/Enum.h>

namespace ACE
{
namespace Impl
{
namespace Fmod
{
//////////////////////////////////////////////////////////////////////////
void CSnapshotConnection::Serialize(Serialization::IArchive& ar)
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

SERIALIZATION_ENUM_BEGIN_NESTED(CSnapshotConnection, EActionType, "Action Type")
SERIALIZATION_ENUM(CSnapshotConnection::EActionType::Start, "start", "Start")
SERIALIZATION_ENUM(CSnapshotConnection::EActionType::Stop, "stop", "Stop")
SERIALIZATION_ENUM_END()
} // namespace Fmod
} // namespace Impl
} // namespace ACE
