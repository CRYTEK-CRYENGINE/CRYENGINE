// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "BaseConnection.h"

namespace ACE
{
namespace Impl
{
namespace Fmod
{
class CEventConnection final : public CBaseConnection
{
public:

	enum class EActionType
	{
		Start,
		Stop,
		Pause,
		Resume,
	};

	explicit CEventConnection(ControlId const id, EActionType const actionType = EActionType::Start)
		: CBaseConnection(id)
		, m_actionType(actionType)
	{}

	CEventConnection() = delete;

	// CBaseConnection
	virtual bool HasProperties() const override { return true; }
	virtual void Serialize(Serialization::IArchive& ar) override;
	// ~CBaseConnection

	EActionType GetActionType() const { return m_actionType; }

private:

	EActionType m_actionType;
};
} // namespace Fmod
} // namespace Impl
} // namespace ACE
