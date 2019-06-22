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
class CParameterConnection final : public IConnection, public CryAudio::CPoolObject<CParameterConnection, stl::PSyncNone>
{
public:

	CParameterConnection() = delete;
	CParameterConnection(CParameterConnection const&) = delete;
	CParameterConnection(CParameterConnection&&) = delete;
	CParameterConnection& operator=(CParameterConnection const&) = delete;
	CParameterConnection& operator=(CParameterConnection&&) = delete;

	explicit CParameterConnection(ControlId const id, float const mult, float const shift)
		: m_id(id)
		, m_mult(mult)
		, m_shift(shift)
	{}

	explicit CParameterConnection(ControlId const id)
		: m_id(id)
		, m_mult(CryAudio::Impl::Wwise::g_defaultParamMultiplier)
		, m_shift(CryAudio::Impl::Wwise::g_defaultParamShift)
	{}

	virtual ~CParameterConnection() override = default;

	// CBaseConnection
	virtual ControlId GetID() const override final   { return m_id; }
	virtual bool      HasProperties() const override { return true; }
	virtual void      Serialize(Serialization::IArchive& ar) override;
	// ~CBaseConnection

	float GetMultiplier() const { return m_mult; }
	float GetShift() const      { return m_shift; }

private:

	ControlId const m_id;
	float           m_mult;
	float           m_shift;
};
} // namespace Wwise
} // namespace Impl
} // namespace ACE
