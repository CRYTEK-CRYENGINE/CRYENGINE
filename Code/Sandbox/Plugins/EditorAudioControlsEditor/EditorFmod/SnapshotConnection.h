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
class CSnapshotConnection final : public IConnection, public CryAudio::CPoolObject<CSnapshotConnection, stl::PSyncNone>
{
public:

	CSnapshotConnection() = delete;
	CSnapshotConnection(CSnapshotConnection const&) = delete;
	CSnapshotConnection(CSnapshotConnection&&) = delete;
	CSnapshotConnection& operator=(CSnapshotConnection const&) = delete;
	CSnapshotConnection& operator=(CSnapshotConnection&&) = delete;

	enum class EActionType : CryAudio::EnumFlagsType
	{
		Start,
		Stop, };

	explicit CSnapshotConnection(ControlId const id, EActionType const actionType)
		: m_id(id)
		, m_actionType(actionType)
	{}

	explicit CSnapshotConnection(ControlId const id)
		: m_id(id)
		, m_actionType(EActionType::Start)
	{}

	virtual ~CSnapshotConnection() override = default;

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
