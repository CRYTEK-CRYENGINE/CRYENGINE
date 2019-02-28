// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: IRemoteControlClientListener implementation
   -------------------------------------------------------------------------
   History:
   - Created by Lin Luo, November 09, 2006
*************************************************************************/

#include "StdAfx.h"
#include <CryNetwork/IRemoteControl.h>
#include "RConClientListener.h"

CRConClientListener CRConClientListener::s_singleton;

IRemoteControlClient* CRConClientListener::s_rcon_client = NULL;

CRConClientListener& CRConClientListener::GetSingleton(IRemoteControlClient* rcon_client)
{
	s_rcon_client = rcon_client;

	return s_singleton;
}

CRConClientListener::CRConClientListener()
{
	m_sessionAuthorized = false;
}

CRConClientListener::~CRConClientListener()
{

}

void CRConClientListener::OnConnectResult(bool okay, EResultDesc desc)
{
	if (okay)
		gEnv->pLog->LogToConsole("RCON: connecting to server");
	else
	{
		string sdesc;
		switch (desc)
		{
		case eRD_Failed:
			sdesc = "failed starting up client";
			break;

		case eRD_CouldNotResolveServerAddr:
			sdesc = "could not resolve server address";
			break;

		case eRD_UnsupportedAddressType:
			sdesc = "unsupported address type for server";
			break;

		case eRD_ConnectAgain:
			sdesc = "client already started";
			break;
		}

		GameWarning("RCON: %s", sdesc.c_str());
		gEnv->pConsole->ExecuteString("rcon_disconnect"); // so user don't need to disconnect explicitly
	}
}

void CRConClientListener::OnSessionStatus(bool connected, EStatusDesc desc)
{
	if (connected)
	{
		m_sessionAuthorized = true;
		gEnv->pLog->LogToConsole("RCON: session successfully connected and authorized");
	}
	else
	{
		m_sessionAuthorized = false;

		string sdesc;
		switch (desc)
		{
		case eSD_ConnectFailed:
			sdesc = "failed connecting to server";
			break;

		case eSD_ServerSessioned:
			sdesc = "server already in a session";
			break;

		case eSD_AuthFailed:
			sdesc = "authorization failed";
			break;

		case eSD_AuthTimeout:
			sdesc = "authorization timed out";
			break;

		case eSD_ServerClosed:
			sdesc = "server closed";
			break;

		case eSD_BogusMessage:
			sdesc = "received bogus message";
			break;
		}
		GameWarning("RCON: session failed - %s", sdesc.c_str());
		gEnv->pConsole->ExecuteString("rcon_disconnect"); // so user don't need to disconnect explicitly
	}
}

void CRConClientListener::OnCommandResult(uint32 commandId, string command, string result)
{
	const char NEWLINE = '\n';

	gEnv->pLog->LogToConsole("RCON: command [%08x]%s result:", commandId, command.c_str());

	size_t off = 0;
	while (off < result.size())
	{
		size_t pos = result.find(NEWLINE, off);
		if (pos == string::npos)
		{
			gEnv->pLog->LogToConsole("RCON:\t%s", result.substr(off).c_str());
			break;
		}
		if (pos > off)
			gEnv->pLog->LogToConsole("RCON:\t%s", result.substr(off, pos - off).c_str());
		off = pos + 1;
	}
}

bool CRConClientListener::IsSessionAuthorized() const
{
	return m_sessionAuthorized;
}
