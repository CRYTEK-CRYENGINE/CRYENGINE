// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __NETDEBUGSERVERINFO_H__
#define __NETDEBUGSERVERINFO_H__

#pragma once

#include "Config.h"

#if ENABLE_NET_DEBUG_INFO

	#include "NetDebugInfo.h"

class CNetDebugServerInfo : public CNetDebugInfoSection, public INetDumpLogger
{
public:
	CNetDebugServerInfo();
	virtual ~CNetDebugServerInfo();

	// CNetDebugInfoSection
	virtual void Tick(const SNetworkProfilingStats* const pProfilingStats);
	virtual void Draw(const SNetworkProfilingStats* const pProfilingStats);
	// ~CNetDebugInfoSection

	// INetDumpLogger
	virtual void Log(const char* pFmt, ...);
	//~INetDumpLogger

private:

	uint32 m_line;
};

#endif // ENABLE_NET_DEBUG_INFO

#endif
