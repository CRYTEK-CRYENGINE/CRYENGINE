// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "../Common/IConnection.h"

#include <PoolObject.h>
#include <CryAudioImplWwise/GlobalData.h>

namespace ACE
{
namespace Impl
{
namespace Wwise
{
class CParameterToStateConnection final : public IConnection, public CryAudio::CPoolObject<CParameterToStateConnection, stl::PSyncNone>
{
public:

	CParameterToStateConnection() = delete;
	CParameterToStateConnection(CParameterToStateConnection const&) = delete;
	CParameterToStateConnection(CParameterToStateConnection&&) = delete;
	CParameterToStateConnection& operator=(CParameterToStateConnection const&) = delete;
	CParameterToStateConnection& operator=(CParameterToStateConnection&&) = delete;

	explicit CParameterToStateConnection(ControlId const id, float const value)
		: m_id(id)
		, m_value(value)
	{}

	explicit CParameterToStateConnection(ControlId const id)
		: m_id(id)
		, m_value(CryAudio::Impl::Wwise::g_defaultStateValue)
	{}

	virtual ~CParameterToStateConnection() override = default;

	// CBaseConnection
	virtual ControlId GetID() const override final   { return m_id; }
	virtual bool      HasProperties() const override { return true; }
	virtual void      Serialize(Serialization::IArchive& ar) override;
	// ~CBaseConnection

	float GetValue() const { return m_value; }

private:

	ControlId const m_id;
	float           m_value;
};
} // namespace Wwise
} // namespace Impl
} // namespace ACE
