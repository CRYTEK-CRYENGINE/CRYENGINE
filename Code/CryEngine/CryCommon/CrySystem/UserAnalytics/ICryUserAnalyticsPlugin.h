// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySystem/ICryPlugin.h>

struct IUserAnalytics;

struct ICryUserAnalyticsPlugin : public Cry::IEnginePlugin
{
	CRYINTERFACE_DECLARE_GUID(ICryUserAnalyticsPlugin, "c97ad475-fd95-416d-9c30-48b2d2c5b7f6"_cry_guid);

public:
	virtual IUserAnalytics*         GetIUserAnalytics() const = 0;
};
