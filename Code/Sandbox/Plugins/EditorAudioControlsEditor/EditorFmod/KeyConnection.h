// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "../Common/IConnection.h"

#include <PoolObject.h>
#include <CryAudioImplFmod/GlobalData.h>

namespace ACE
{
namespace Impl
{
namespace Fmod
{
class CKeyConnection final : public IConnection, public CryAudio::CPoolObject<CKeyConnection, stl::PSyncNone>
{
public:

	CKeyConnection() = delete;
	CKeyConnection(CKeyConnection const&) = delete;
	CKeyConnection(CKeyConnection&&) = delete;
	CKeyConnection& operator=(CKeyConnection const&) = delete;
	CKeyConnection& operator=(CKeyConnection&&) = delete;

	explicit CKeyConnection(ControlId const id, string const& event)
		: m_id(id)
		, m_event(event)
	{}

	explicit CKeyConnection(ControlId const id)
		: m_id(id)
		, m_event("")
	{}

	virtual ~CKeyConnection() override = default;

	// CBaseConnection
	virtual ControlId GetID() const override final   { return m_id; }
	virtual bool      HasProperties() const override { return true; }
	virtual void      Serialize(Serialization::IArchive& ar) override;
	// ~CBaseConnection

	string const& GetEvent() const { return m_event; }

private:

	ControlId const m_id;
	string          m_event;
};
} // namespace Fmod
} // namespace Impl
} // namespace ACE
