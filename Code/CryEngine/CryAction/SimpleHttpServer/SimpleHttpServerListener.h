// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __SIMPLEHTTPSERVERLISTENER_H__
#define __SIMPLEHTTPSERVERLISTENER_H__

#pragma once

class CSimpleHttpServerListener : public IHttpServerListener, public IOutputPrintSink
{
public:
	static CSimpleHttpServerListener& GetSingleton(ISimpleHttpServer* http_server);
	static CSimpleHttpServerListener& GetSingleton();

	void                              Update();

private:
	CSimpleHttpServerListener();
	~CSimpleHttpServerListener();

	void OnStartResult(bool started, EResultDesc desc);

	void OnClientConnected(int connectionID, string client);
	void OnClientDisconnected(int connectionID);

	void OnGetRequest(int connectionID, string url);
	void OnRpcRequest(int connectionID, string xml);

	string m_output;
	void Print(const char* inszText);

	typedef std::deque<string> TCommandsVec;
	TCommandsVec m_commands;

	enum EAuthorizationState {eAS_Disconnected, eAS_WaitChallengeRequest, eAS_WaitAuthenticationRequest, eAS_Authorized};
	EAuthorizationState              m_state;
	int                              m_connectionID;

	string                           m_client; // current session client
	string                           m_challenge;

	static CSimpleHttpServerListener s_singleton;
	static ISimpleHttpServer*        s_http_server;
};

#endif
