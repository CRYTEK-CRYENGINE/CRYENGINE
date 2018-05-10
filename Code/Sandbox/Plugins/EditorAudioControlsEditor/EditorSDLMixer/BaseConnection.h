// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <IConnection.h>

namespace ACE
{
namespace Impl
{
namespace SDLMixer
{
extern float const g_precision;

class CBaseConnection : public IConnection
{
public:

	CBaseConnection() = delete;
	virtual ~CBaseConnection() override = default;

	// IConnection
	virtual ControlId GetID() const override final                                                                   { return m_id; }
	virtual bool      HasProperties() const override final                                                           { return true; }
	virtual void      Serialize(Serialization::IArchive& ar) override                                                {}
	virtual void      SetPlatformEnabled(PlatformIndexType const platformIndex, bool const isEnabled) override final {}
	virtual bool      IsPlatformEnabled(PlatformIndexType const platformIndex) const override final                  { return true; }
	virtual void      ClearPlatforms() override final                                                                {}
	// ~IConnection

protected:

	explicit CBaseConnection(ControlId const id)
		: m_id(id)
	{}

private:

	ControlId const m_id;
};
} // namespace SDLMixer
} // namespace Impl
} // namespace ACE
