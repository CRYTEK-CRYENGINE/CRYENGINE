// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CryNetwork/ISimpleHttpServer.h>

#include "SimpleHttpServer/SimpleHttpServerWebsocketEchoListener.h"

#if defined(HTTP_WEBSOCKETS)

CSimpleHttpServerWebsocketEchoListener CSimpleHttpServerWebsocketEchoListener::s_singleton;

CSimpleHttpServerWebsocketEchoListener& CSimpleHttpServerWebsocketEchoListener::GetSingleton()
{
	return s_singleton;
}

void CSimpleHttpServerWebsocketEchoListener::OnUpgrade(int connectionID)
{
	CryLogAlways("CSimpleHttpServerWebsocketEchoListener::OnUpgrade: connection %d upgraded to websocket!", connectionID);

	SMessageData data;
	data.m_eType = eMT_Text;
	data.m_pBuffer = "Upgraded connection to Websocket.\n";
	data.m_bufferSize = 35;

	gEnv->pNetwork->GetSimpleHttpServerSingleton()->SendWebsocketData(connectionID, data, false);
};

void CSimpleHttpServerWebsocketEchoListener::OnReceive(int connectionID, SMessageData& data)
{
	// BEWARE! This is for test purposes only!
	// Technically we shouldn't display data.m_pBuffer directly as a string. There is no guarantee that the buffer will contain a null terminator,
	// even if the data.type is set to eMT_Text.
	// For safety (for exactly this reason), our message handler always appends a null, but does not include in the length of the message.
	// (data.m_pBuffer is actually data.m_bufferSize + 1 in length, with the last char set to null).
	CryLogAlways("CSimpleHttpServerWebsocketEchoListener::OnReceive: connection %d, MessageType %d: \"%s\"", connectionID, data.m_eType, data.m_pBuffer);

	// ECHO
	// Send the incoming data back to the client
	gEnv->pNetwork->GetSimpleHttpServerSingleton()->SendWebsocketData(connectionID, data, false);
}

void CSimpleHttpServerWebsocketEchoListener::OnClosed(int connectionID, bool bGraceful)
{
	CryLogAlways("CSimpleHttpServerWebsocketEchoListener::OnClosed: connection %d, graceful = %d", connectionID, bGraceful);
}

#endif // HTTP_WEBSOCKETS
