// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
Description:
	- Contains various shared network util functions
-------------------------------------------------------------------------
History:
	- 19/07/2010 : Created by Colin Gulliver

*************************************************************************/

#include "StdAfx.h"
#include "GameNetworkUtils.h"
#include "IPlayerProfiles.h"
#include "Lobby/SessionNames.h"

namespace GameNetworkUtils
{
	ECryLobbyError SendToAll( CCryLobbyPacket* pPacket, CrySessionHandle h, SSessionNames &clients, bool bCheckConnectionState )
	{
		ECryLobbyError result = eCLE_Success;

		ICryLobby *pLobby = gEnv->pNetwork->GetLobby();
		if (pLobby)
		{
			ICryMatchMaking *pMatchMaking = pLobby->GetMatchMaking();
			if (pMatchMaking)
			{
				const unsigned int numClients = clients.m_sessionNames.size();
				// Start from 1 since we don't want to send to ourselves (unless we're a dedicated server)
				const int startIndex = gEnv->IsDedicated() ? 0 : 1;
				for (unsigned int i = startIndex; (i < numClients) && (result == eCLE_Success); ++ i)
				{
					SSessionNames::SSessionName &client = clients.m_sessionNames[i];

					if (!bCheckConnectionState || client.m_bFullyConnected)
					{
						result = pMatchMaking->SendToClient(pPacket, h, client.m_conId);
					}
				}
			}
		}

		return result;
	}

	//---------------------------------------
	const bool CompareCrySessionId(const CrySessionID &lhs, const CrySessionID &rhs)
	{
		if (!lhs && !rhs)
		{
			return true;
		}
		if ((lhs && !rhs) || (!lhs && rhs))
		{
			return false;
		}
		return (*lhs == *rhs);
	}

	void WebSafeEscapeString(string				*ioString)
	{
		/*	from RFC2396
			escape the following by replacing them with %<hex of their ascii code>
		 
				delims      = "<" | ">" | "#" | "%" | <">
				unwise      = "{" | "}" | "|" | "\" | "^" | "[" | "]" | "`"
				reserved    = ";" | "/" | "?" | ":" | "@" | "&" | "=" | "+" |
									"$" | ","
			also replace spaces with %20
				space->%20
		*/
		const char				k_escapeCharacters[]="%<>#\"{}|\\^[]`;/?:@&=+$, ";		// IMPORTANT: replace % first!
		const int				k_numEscapedCharacters=sizeof(k_escapeCharacters)-1;
		int						numToReplace=0;
		int						origLen=ioString->length();

		{
			const char		*rawStr=ioString->c_str();

			for (int i=0; i<k_numEscapedCharacters; i++)
			{
				char	escChar=k_escapeCharacters[i];

				for (int j=0; j<origLen; j++)
				{
					if (rawStr[j]==escChar)
					{
						numToReplace++;
					}
				}
			}
		}

		if (numToReplace == 0)
		{
			return;
		}

		ioString->reserve(origLen+numToReplace*2);		// replace with 3 chars, as escaped chars are replaced with a % then two hex numbers. replace len = 3, old len = 1, need numReplace*2 extra bytes

		{
			CryFixedStringT<8>		find,replace;

			for (int i=0; i<k_numEscapedCharacters; i++)
			{
				char	escChar=k_escapeCharacters[i];

				find.Format("%c",escChar);
				replace.Format("%%%x",int(escChar));
				ioString->replace(find,replace);
			}
		}
}

} //~GameNetworkUtils