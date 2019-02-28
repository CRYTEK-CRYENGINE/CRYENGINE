// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#if ENABLE_NET_DEBUG_INFO

	#include "NetDebugServerInfo.h"
	#include "NetCVars.h"
	#include <CrySystem/ISystem.h>
	#include <CryGame/IGameFramework.h>

CNetDebugServerInfo::CNetDebugServerInfo()
	: CNetDebugInfoSection(CNetDebugInfo::eP_ServerInfo, 50.0F, 100.0F)
{
}

CNetDebugServerInfo::~CNetDebugServerInfo()
{
}

void CNetDebugServerInfo::Tick(const SNetworkProfilingStats* const pProfilingStats)
{
}

void CNetDebugServerInfo::Draw(const SNetworkProfilingStats* const pProfilingStats)
{
	IGameFramework* pGameFramework = gEnv->pGameFramework;

	if (pGameFramework)
	{
		m_line = 0;

		CNetNub* pServerNub = static_cast<CNetNub*>(pGameFramework->GetServerNetNub());

		if (pServerNub)
		{
			pServerNub->NetDump(eNDT_ServerConnectionState, *this);
		}

		CNetNub* pClientNub = static_cast<CNetNub*>(pGameFramework->GetClientNetNub());

		if (pClientNub)
		{
			pClientNub->NetDump(eNDT_ClientConnectionState, *this);
		}
	}
}

void CNetDebugServerInfo::Log(const char* pFmt, ...)
{
	va_list args;

	va_start(args, pFmt);
	VDrawLine(m_line, s_white, pFmt, args);
	va_end(args);
	++m_line;
}

#endif
