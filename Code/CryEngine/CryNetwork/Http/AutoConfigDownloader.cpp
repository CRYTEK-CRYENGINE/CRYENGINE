// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AutoConfigDownloader.h"
#include <CryNetwork/INetwork.h>

CAutoConfigDownloader* CAutoConfigDownloader::m_pThis = 0;

CAutoConfigDownloader::CAutoConfigDownloader()
{
	m_executing = false;
	m_pThis = this;
	REGISTER_STRING_CB("sv_autoconfigurl", "", VF_NULL, "Automatically download configuration data from a URL", OnCVarChanged);
}

void CAutoConfigDownloader::TriggerRefresh()
{
	if (m_url.empty() || !gEnv->IsDedicated())
		return;

	NetLogAlways("not downloading anything %s", m_url.c_str());
	// michiel - GS
}

void CAutoConfigDownloader::Update()
{
	if (!m_toExec.empty())
	{
		string cmd;
		for (size_t i = 0; i < m_toExec.size(); i++)
		{
			switch (m_toExec[i])
			{
			case '\r':
			case '\n':
				if (!cmd.empty() && cmd[0] != '#')
					gEnv->pConsole->ExecuteString(cmd.c_str());
				cmd.resize(0);
				break;
			default:
				cmd += m_toExec[i];
				break;
			}
		}
		if (!cmd.empty() && cmd[0] != '#')
			gEnv->pConsole->ExecuteString(cmd.c_str());
		m_toExec.resize(0);
	}
}

void CAutoConfigDownloader::GotData(const uint8* pData, uint32 length)
{
	m_buffer += string((const char*)pData, (const char*)(pData + length));
}

void CAutoConfigDownloader::Complete(bool success)
{
	m_executing = false;
	if (success)
		m_toExec = m_buffer;
	else
		NetWarning("Configuration download failed");
	m_buffer.resize(0);
}

void CAutoConfigDownloader::OnCVarChanged(ICVar* pVar)
{
	m_pThis->Reconfigure(pVar->GetString());
}

void CAutoConfigDownloader::Reconfigure(const string& loc)
{
	m_url = loc;
	TriggerRefresh();
}
