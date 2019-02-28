// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
Description:
	- Contains various shared network util functions
-------------------------------------------------------------------------
History:
	- 19/07/2010 : Created by Colin Gulliver

*************************************************************************/

#ifndef __GAMENETWORKUTILS_H__
#define __GAMENETWORKUTILS_H__

struct SSessionNames;

namespace GameNetworkUtils
{
	ECryLobbyError SendToAll(CCryLobbyPacket* pPacket, CrySessionHandle h, SSessionNames &clients, bool bCheckConnectionState);

	const bool CompareCrySessionId(const CrySessionID &lhs, const CrySessionID &rhs);

	void WebSafeEscapeString(string *ioString);
};

#endif //~__GAMENETWORKUTILS_H__


