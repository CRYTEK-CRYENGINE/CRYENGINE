// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __multiplatformconfig_h__
#define __multiplatformconfig_h__
#pragma once

#include "Config.h"
#include "IMultiplatformConfig.h"


class MultiplatformConfig
	: public IMultiplatformConfig
{
public:
	MultiplatformConfig()
		: m_platformCount(0)
		, m_activePlatform(-1)
	{
	}

	virtual void init(int platformCount, int activePlatform, IConfigKeyRegistry* pConfigKeyRegistry)
	{
		if (platformCount < 0 || 
			platformCount > kMaxPlatformCount ||
			unsigned(activePlatform) >= platformCount ||
			pConfigKeyRegistry == 0)
		{
			assert(0);
			m_platformCount = 0;
			return;
		}

		m_platformCount = platformCount;
		m_activePlatform = activePlatform;

		for (int i = 0; i < platformCount; ++i)
		{
			m_config[i].SetConfigKeyRegistry(pConfigKeyRegistry);
		}
	}

	virtual int getPlatformCount() const
	{
		return m_platformCount;
	}

	virtual int getActivePlatform() const
	{
		return m_activePlatform;
	}

	virtual const IConfig& getConfig(int platform) const
	{
		assert(unsigned(platform) < unsigned(m_platformCount));
		return m_config[platform];
	}

	virtual IConfig& getConfig(int platform)
	{
		assert(unsigned(platform) < unsigned(m_platformCount));
		return m_config[platform];
	}

	virtual const IConfig& getConfig() const 
	{
		return getConfig(m_activePlatform);
	}

	virtual IConfig& getConfig()
	{
		return getConfig(m_activePlatform);
	}

	virtual void setKeyValue(EConfigPriority ePri, const char* key, const char* value)
	{
		for (int i = 0; i < m_platformCount; ++i)
		{
			m_config[i].SetKeyValue(ePri, key, value);
		}
	}

private:
	enum { kMaxPlatformCount = 20 };
	int m_platformCount;
	int m_activePlatform;
	Config m_config[kMaxPlatformCount];
};

#endif
