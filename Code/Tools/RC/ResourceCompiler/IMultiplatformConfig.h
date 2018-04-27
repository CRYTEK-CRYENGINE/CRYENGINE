// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __imultiplatformconfig_h__
#define __imultiplatformconfig_h__
#pragma once

#include "IConfig.h"          // IConfig, IConfigKeyRegistry, enum EConfigPriority

class IMultiplatformConfig
{
public:
	virtual ~IMultiplatformConfig()
	{
	}

	virtual void init(int platformCount, int activePlatform, IConfigKeyRegistry* pConfigKeyRegistry) = 0;

	virtual int getPlatformCount() const = 0;
	virtual int getActivePlatform() const = 0;

	virtual const IConfig& getConfig(int platform) const = 0;
	virtual IConfig& getConfig(int platform) = 0;
	virtual const IConfig& getConfig() const = 0;
	virtual IConfig& getConfig() = 0;

	virtual void setKeyValue(EConfigPriority ePri, const char* key, const char* value) = 0;
};

#endif
