#pragma once

#include "IPlatformUser.h"

namespace Cry
{
	namespace GamePlatform
	{
		//! Interface to access the platform's own networking functionality, allowing for communication with other users
		struct INetworking
		{
			virtual ~INetworking() {}

			//! Sends a packet to the specified user, identified by their platform-specific user id
			virtual bool SendPacket(IUser::Identifier remoteUser, void* pData, uint32 dataLength) = 0;
			//! Closes a session opened with SendPacket
			virtual bool CloseSession(IUser::Identifier remoteUser) = 0;
			//! Checks whether there is data available to read
			virtual bool IsPacketAvailable(uint32* pPacketSizeOut) const = 0;
			//! Reads a packet, if there is one available
			//! \param pDest Buffer to which the packet will be read
			//! \param destLength Size of the buffer, and the maximum amount of data we can read
			//! \param pMessageSizeOut Updated after a successful packet read with the actual size of the packet, does not necessarily match the provided buffer
			//! \param pRemoteIdOut Updated after a successful packet read with the unique platform-specific identifier of the user that sent the packet
			//! \returns True if the packet was read, otherwise false.
			virtual bool ReadPacket(void* pDest, uint32 destLength, uint32* pMessageSizeOut, IUser::Identifier* pRemoteIdOut) = 0;
		};
	}
}