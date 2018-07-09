// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __NETDEBUGTRAFFICBANDWIDTH_H__
#define __NETDEBUGTRAFFICBANDWIDTH_H__

#pragma once

#include "Config.h"

#if ENABLE_NET_DEBUG_INFO

	#include "NetDebugInfo.h"

class CNetDebugTrafficBandwidth : public CNetDebugInfoSection
{
public:
	CNetDebugTrafficBandwidth();
	virtual ~CNetDebugTrafficBandwidth();

	virtual void Tick(const SNetworkProfilingStats* const pProfilingStats);
	virtual void Draw(const SNetworkProfilingStats* const pProfilingStats);

private:

	SBandwidthStats m_bandwidthStats;
};

#endif // ENABLE_NET_DEBUG_INFO

#endif
