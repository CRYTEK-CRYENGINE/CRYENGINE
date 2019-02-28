// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: IRemoteControlServerListener implementation
   -------------------------------------------------------------------------
   History:
   - Created by Lin Luo, November 08, 2006
*************************************************************************/

#include "StdAfx.h"
#include <CryNetwork/IRemoteControl.h>
#include "RConServerListener.h"

CRConServerListener CRConServerListener::s_singleton;

IRemoteControlServer* CRConServerListener::s_rcon_server = NULL;

CRConServerListener& CRConServerListener::GetSingleton(IRemoteControlServer* rcon_server)
{
	s_rcon_server = rcon_server;

	return s_singleton;
}

CRConServerListener& CRConServerListener::GetSingleton()
{
	return s_singleton;
}

CRConServerListener::CRConServerListener()
{

}

CRConServerListener::~CRConServerListener()
{

}

void CRConServerListener::Update()
{
	for (TCommandsMap::const_iterator it = m_commands.begin(); it != m_commands.end(); ++it)
	{
		gEnv->pConsole->AddOutputPrintSink(this);
#if defined(CVARS_WHITELIST)
		ICVarsWhitelist* pCVarsWhitelist = gEnv->pSystem->GetCVarsWhiteList();
		bool execute = (pCVarsWhitelist) ? pCVarsWhitelist->IsWhiteListed(it->second.c_str(), false) : true;
		if (execute)
#endif // defined(CVARS_WHITELIST)
		{
			gEnv->pConsole->ExecuteString(it->second.c_str());
		}
		gEnv->pConsole->RemoveOutputPrintSink(this);

		s_rcon_server->SendResult(it->first, m_output);
		m_output.resize(0);
	}

	m_commands.clear();
}

void CRConServerListener::Print(const char* inszText)
{
	m_output += string().Format("%s\n", inszText);
}

void CRConServerListener::OnStartResult(bool started, EResultDesc desc)
{
	if (started)
		gEnv->pLog->LogToConsole("RCON: server successfully started");
	else
	{
		string sdesc;
		switch (desc)
		{
		case eRD_Failed:
			sdesc = "failed starting server";
			break;

		case eRD_AlreadyStarted:
			sdesc = "server already started";
			break;
		}
		gEnv->pLog->LogToConsole("RCON: %s", sdesc.c_str());

		gEnv->pConsole->ExecuteString("rcon_stopserver");
	}
}

void CRConServerListener::OnClientAuthorized(string clientAddr)
{
	gEnv->pLog->LogToConsole("RCON: client from %s is authorized", clientAddr.c_str());
}

void CRConServerListener::OnAuthorizedClientLeft(string clientAddr)
{
	gEnv->pLog->LogToConsole("RCON: client from %s is gone", clientAddr.c_str());
}

void CRConServerListener::OnClientCommand(uint32 commandId, string command)
{
	// execute the command on the server
	gEnv->pLog->LogToConsole("RCON: received remote control command: [%08x]%s", commandId, command.c_str());

	m_commands[commandId] = command;
}
