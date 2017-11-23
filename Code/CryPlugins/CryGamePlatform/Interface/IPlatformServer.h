#pragma once

#include "IPlatformUser.h"

namespace Cry
{
	namespace GamePlatform
	{
		//! Interface to a server registered on the platform's services
		struct IServer
		{
			// Unique identifier of the server
			using Identifier = uint64;

			virtual ~IServer() {}

			//! Gets the server's unique identifier
			virtual Identifier GetIdentifier() const = 0;

			//! Gets the public IP address of the server according to the platform's network
			virtual uint32 GetPublicIP() const = 0;
			//! Gets the public IP address of the server according to the platform's network as a string
			virtual const char* GetPublicIPString() const = 0;
			//! Gets the port that clients should connect to
			virtual uint16 GetPort() const = 0;
			//! Checks whether or not the server expects clients to connect with the server's local IP (LAN)
			virtual bool IsLocal() const = 0;
			//! Authenticates a specific user on the server using an auth token generated on the client using IPlugin::GetAuthToken.
			//! \param clientIP Publicly accessibly IP address of the specific user
			//! \param szAuthData Base address of the auth token
			//! \param authDataLength Length of the auth token
			//! \param userId Assigned on success to the user's platform-specific identifier
			//! \returns True if the authentication was successful, otherwise false
			virtual bool AuthenticateUser(uint32 clientIP, char* szAuthData, int authDataLength, IUser::Identifier& userId) = 0;
			//! Should be called whenever a user leaves the game server, allowing the platform services to track what users are on which servers.
			virtual void SendUserDisconnect(IUser::Identifier userId) = 0;
		};
	}
}