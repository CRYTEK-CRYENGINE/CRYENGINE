// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "BaseConnection.h"

#include <CryAudioImplSDLMixer/GlobalData.h>

namespace ACE
{
namespace Impl
{
namespace SDLMixer
{
class CStateConnection final : public CBaseConnection
{
public:

	explicit CStateConnection(ControlId const id, float const value = CryAudio::Impl::SDL_mixer::s_defaultStateValue)
		: CBaseConnection(id)
		, m_value(value)
	{}

	CStateConnection() = delete;

	// CBaseConnection
	virtual void Serialize(Serialization::IArchive& ar) override;
	// ~CBaseConnection

	float GetValue() const { return m_value; }

private:

	float m_value;
};
} // namespace SDLMixer
} // namespace Impl
} // namespace ACE
