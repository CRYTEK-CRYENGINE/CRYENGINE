// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "../Common/IConnection.h"

#include <PoolObject.h>
#include <CryAudioImplAdx2/GlobalData.h>

namespace ACE
{
namespace Impl
{
namespace Adx2
{
class CSnapshotConnection final : public IConnection, public CryAudio::CPoolObject<CSnapshotConnection, stl::PSyncNone>
{
public:

	enum class EActionType : CryAudio::EnumFlagsType
	{
		Start,
		Stop, };

	CSnapshotConnection() = delete;
	CSnapshotConnection(CSnapshotConnection const&) = delete;
	CSnapshotConnection(CSnapshotConnection&&) = delete;
	CSnapshotConnection& operator=(CSnapshotConnection const&) = delete;
	CSnapshotConnection& operator=(CSnapshotConnection&&) = delete;

	explicit CSnapshotConnection(ControlId const id, EActionType const actionType, int const fadeTime)
		: m_id(id)
		, m_actionType(actionType)
		, m_changeoverTime(fadeTime)
	{}

	explicit CSnapshotConnection(ControlId const id)
		: m_id(id)
		, m_actionType(EActionType::Start)
		, m_changeoverTime(CryAudio::Impl::Adx2::g_defaultChangeoverTime)
	{}

	virtual ~CSnapshotConnection() override = default;

	// CBaseConnection
	virtual ControlId GetID() const override final   { return m_id; }
	virtual bool      HasProperties() const override { return true; }
	virtual void      Serialize(Serialization::IArchive& ar) override;
	// ~CBaseConnection

	EActionType GetActionType() const     { return m_actionType; }
	int         GetChangeoverTime() const { return m_changeoverTime; }

private:

	ControlId const m_id;
	EActionType     m_actionType;
	int             m_changeoverTime;
};
} // namespace Adx2
} // namespace Impl
} // namespace ACE
