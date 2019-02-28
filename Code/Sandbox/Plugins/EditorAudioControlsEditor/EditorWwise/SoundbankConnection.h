// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "BaseConnection.h"

namespace ACE
{
namespace Impl
{
namespace Wwise
{
class CSoundbankConnection final : public CBaseConnection
{
public:

	explicit CSoundbankConnection(ControlId const id)
		: CBaseConnection(id)
		, m_configurationsMask(std::numeric_limits<PlatformIndexType>::max())
	{}

	CSoundbankConnection() = delete;

	// CBaseConnection
	virtual bool HasProperties() const override { return true; }
	virtual void Serialize(Serialization::IArchive& ar) override;
	virtual void SetPlatformEnabled(PlatformIndexType const platformIndex, bool const isEnabled) override;
	virtual bool IsPlatformEnabled(PlatformIndexType const platformIndex) const override;
	virtual void ClearPlatforms() override;
	// ~CBaseConnection

private:

	PlatformIndexType m_configurationsMask;
};
} // namespace Wwise
} // namespace Impl
} // namespace ACE
