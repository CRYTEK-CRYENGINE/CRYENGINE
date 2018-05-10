#include "StdAfx.h"

#include <steam/steam_api.h>
#include <steam/steam_gameserver.h>

#include "SteamNetworking.h"

namespace Cry
{
	namespace GamePlatform
	{
		namespace Steam
		{
			CNetworking::CNetworking()
				: m_callbackP2PSessionRequest(this, &CNetworking::OnSessionRequest)
				, m_callbackP2PSessionConnectFail(this, &CNetworking::OnSessionConnectFail)
			{
			}

			ISteamNetworking* CNetworking::GetSteamNetworking() const
			{
#ifdef DEDICATED_SERVER
				return SteamGameServerNetworking();
#else
				return SteamNetworking();
#endif
			}

			bool CNetworking::SendPacket(IUser::Identifier remoteUser, void* pData, uint32 dataLength)
			{
				ISteamNetworking* pSteamNetworking = GetSteamNetworking();
				if (!pSteamNetworking)
				{
					return false;
				}
				return pSteamNetworking->SendP2PPacket(remoteUser, pData, dataLength, k_EP2PSendReliable);
			}

			bool CNetworking::CloseSession(IUser::Identifier remoteUser)
			{
				ISteamNetworking* pSteamNetworking = GetSteamNetworking();
				if (!pSteamNetworking)
				{
					return false;
				}
				return pSteamNetworking->CloseP2PSessionWithUser(remoteUser);
			}

			bool CNetworking::IsPacketAvailable(uint32* pPacketSizeOut) const
			{
				ISteamNetworking* pSteamNetworking = GetSteamNetworking();
				if (!pSteamNetworking)
				{
					return false;
				}
				return pSteamNetworking->IsP2PPacketAvailable(pPacketSizeOut);
			}

			bool CNetworking::ReadPacket(void* pDest, uint32 destLength, uint32* pMessageSizeOut, IUser::Identifier* pRemoteIdOut)
			{
				ISteamNetworking* pSteamNetworking = GetSteamNetworking();
				if (!pSteamNetworking)
				{
					return false;
				}
				return pSteamNetworking->ReadP2PPacket(pDest, destLength, pMessageSizeOut, reinterpret_cast<CSteamID*>(pRemoteIdOut));
			}

			// Steam callbacks
			void CNetworking::OnSessionRequest(P2PSessionRequest_t* pP2PSessionRequest)
			{
				// always accept packets
				ISteamNetworking* pSteamNetworking = GetSteamNetworking();
				if (!pSteamNetworking)
				{
					return;
				}
				pSteamNetworking->AcceptP2PSessionWithUser(pP2PSessionRequest->m_steamIDRemote);
			}

			void CNetworking::OnSessionConnectFail(P2PSessionConnectFail_t* pP2PSessionConnectFail)
			{
			}
		}
	}
}