#pragma once

#include "IPlatformNetworking.h"

namespace Cry
{
	namespace GamePlatform
	{
		namespace Steam
		{
			class CNetworking
				: public INetworking
			{
			public:
				CNetworking();
				virtual ~CNetworking() = default;

				// INetworking
				virtual bool SendPacket(IUser::Identifier remoteUser, void* pData, uint32 dataLength);
				virtual bool CloseSession(IUser::Identifier remoteUser);

				virtual bool IsPacketAvailable(uint32* pPacketSizeOut) const;
				virtual bool ReadPacket(void* pDest, uint32 destLength, uint32* pMessageSizeOut, IUser::Identifier* pRemoteIdOut);
				// ~INetworking

				ISteamNetworking* GetSteamNetworking() const;

				STEAM_CALLBACK(CNetworking, OnSessionRequest, P2PSessionRequest_t, m_callbackP2PSessionRequest);
				STEAM_CALLBACK(CNetworking, OnSessionConnectFail, P2PSessionConnectFail_t, m_callbackP2PSessionConnectFail);
			};
		}
	}
}