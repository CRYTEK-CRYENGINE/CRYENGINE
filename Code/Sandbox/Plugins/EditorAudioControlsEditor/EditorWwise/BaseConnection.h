// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <IConnection.h>

namespace ACE
{
namespace Impl
{
namespace Wwise
{
extern float const g_precision;

class CBaseConnection : public IConnection
{
public:

	explicit CBaseConnection(ControlId const id)
		: m_id(id)
	{}

	virtual ~CBaseConnection() override = default;

	CBaseConnection() = delete;

	// IConnection
	virtual ControlId GetID() const override final                                                             { return m_id; }
	virtual bool      HasProperties() const override                                                           { return false; }
	virtual void      Serialize(Serialization::IArchive& ar) override                                          {}
	virtual void      SetPlatformEnabled(PlatformIndexType const platformIndex, bool const isEnabled) override {}
	virtual bool      IsPlatformEnabled(PlatformIndexType const platformIndex) const override                  { return true; }
	virtual void      ClearPlatforms() override                                                                {}
	// ~IConnection

private:

	ControlId const m_id;
};
} // namespace Wwise
} // namespace Impl
} // namespace ACE
