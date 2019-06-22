// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "../Common/IConnection.h"

#include <PoolObject.h>
#include <CryAudioImplSDLMixer/GlobalData.h>

namespace ACE
{
namespace Impl
{
namespace SDLMixer
{
class CStateConnection final : public IConnection, public CryAudio::CPoolObject<CStateConnection, stl::PSyncNone>
{
public:

	CStateConnection() = delete;
	CStateConnection(CStateConnection const&) = delete;
	CStateConnection(CStateConnection&&) = delete;
	CStateConnection& operator=(CStateConnection const&) = delete;
	CStateConnection& operator=(CStateConnection&&) = delete;

	explicit CStateConnection(ControlId const id, float const value)
		: m_id(id)
		, m_value(value)
	{}

	explicit CStateConnection(ControlId const id)
		: m_id(id)
		, m_value(CryAudio::Impl::SDL_mixer::g_defaultStateValue)
	{}

	virtual ~CStateConnection() override = default;

	// CBaseConnection
	virtual ControlId GetID() const override final         { return m_id; }
	virtual bool      HasProperties() const override final { return true; }
	virtual void      Serialize(Serialization::IArchive& ar) override;
	// ~CBaseConnection

	float GetValue() const { return m_value; }

private:

	ControlId const m_id;
	float           m_value;
};
} // namespace SDLMixer
} // namespace Impl
} // namespace ACE
