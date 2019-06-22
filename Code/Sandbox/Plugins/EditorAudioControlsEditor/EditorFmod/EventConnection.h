// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "../Common/IConnection.h"

#include <PoolObject.h>

namespace ACE
{
namespace Impl
{
namespace Fmod
{
class CEventConnection final : public IConnection, public CryAudio::CPoolObject<CEventConnection, stl::PSyncNone>
{
public:

	CEventConnection() = delete;
	CEventConnection(CEventConnection const&) = delete;
	CEventConnection(CEventConnection&&) = delete;
	CEventConnection& operator=(CEventConnection const&) = delete;
	CEventConnection& operator=(CEventConnection&&) = delete;

	enum class EActionType : CryAudio::EnumFlagsType
	{
		Start,
		Stop,
		Pause,
		Resume, };

	explicit CEventConnection(ControlId const id, EActionType const actionType)
		: m_id(id)
		, m_actionType(actionType)
	{}

	explicit CEventConnection(ControlId const id)
		: m_id(id)
		, m_actionType(EActionType::Start)
	{}

	virtual ~CEventConnection() override = default;

	// CBaseConnection
	virtual ControlId GetID() const override final   { return m_id; }
	virtual bool      HasProperties() const override { return true; }
	virtual void      Serialize(Serialization::IArchive& ar) override;
	// ~CBaseConnection

	EActionType GetActionType() const { return m_actionType; }

private:

	ControlId const m_id;
	EActionType     m_actionType;
};
} // namespace Fmod
} // namespace Impl
} // namespace ACE
