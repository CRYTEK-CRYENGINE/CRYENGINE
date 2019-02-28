#pragma once

#include "IPlatformUser.h"
#include "IPlatformServer.h"

namespace Cry
{
	namespace GamePlatform
	{
		//! Represents a lobby containing users interested in a multiplayer match
		struct IUserLobby
		{
			// Platform-specific identifier for a lobby
			using Identifier = uint64;

			//! Determines the visibility of the lobby to other users
			enum class EVisbility
			{
				//! Only visible to the creator / existing members
				Private,
				//! Only visible to friends
				FriendsOnly,
				//! Visible to everyone
				Public,
				// Not visible
				Invisible
			};

			//! Allows for comparing lobbies based on certain filters
			enum class EComparison
			{
				//! The filter is valid if equal to or less than the specified value
				EqualToOrLess = -2,
				//! The filter is valid if less than the specified value
				LessThan = -1,
				//! The filter is valid if equal tothe specified value
				Equal = 0,
				//! The filter is valid if greater than the specified value
				GreaterThan = 1,
				//! The filter is valid if equal to or greater than the specified value
				EqualToOrGreaterThan = 2,
				//! The filter is valid if not equal to the specified value
				NotEqual = 3
			};

			//! Exposes callbacks for lobby events
			struct IListener
			{
				//! Sent when a new player has entered the lobby
				virtual void OnPlayerEntered(IUser::Identifier userId) = 0;
				//! Sent when a player has left the lobby
				virtual void OnPlayerLeft(IUser::Identifier userId) = 0;
				//! Sent when a player disconnected
				virtual void OnPlayerDisconnected(IUser::Identifier userId) = 0;
				//! Sent when a player was kicked from the lobby
				virtual void OnPlayerKicked(IUser::Identifier userId, IUser::Identifier moderatorId) = 0;
				//! Sent when a player was banned from the lobby
				virtual void OnPlayerBanned(IUser::Identifier userId, IUser::Identifier moderatorId) = 0;
				//! Sent to the local player upon leaving the lobby
				virtual void OnLeave() = 0;
				//! Sent when a server / game session is started, expecting clients to connect
				virtual void OnGameCreated(IServer::Identifier serverId, uint32 ipAddress, uint16 port, bool bLocal) = 0;
				//! Sent when a chat message is received from another user
				virtual void OnChatMessage(IUser::Identifier userId, const char* message) = 0;
				//! Sent when lobby metadata has changed
				virtual void OnDataUpdate(IUser::Identifier userId) = 0;
			};

			virtual ~IUserLobby() {}

			//! Adds a listener to lobby events
			virtual void AddListener(IListener& listener) = 0;
			//! Removes a listener to lobby events
			virtual void RemoveListener(IListener& listener) = 0;
			//! Creates a server (calling IPlugin::CreateServer), and loads the specified map
			//! \param szLevel name of the level to load
			//! \param Whether or not to host using the local IP address, otherwise attempts to obtain the public address
			virtual bool HostServer(const char* szLevel, bool isLocal) = 0;

			//! Gets the maximum number of players that can fit in the lobby
			virtual int GetMemberLimit() const = 0;
			//! Gets the current number of players in the lobby
			virtual int GetNumMembers() const = 0;
			//! Gets the specified user at the index, where max is GetNumMembers() - 1
			virtual IUser::Identifier GetMemberAtIndex(int index) const = 0;

			//! Checks whether the lobby is currently connected to a server
			virtual bool IsInServer() const = 0;
			//! Leaves the server as the local user
			virtual void Leave() = 0;

			//! Gets the user identifier of the creator / owner of the lobby
			virtual IUser::Identifier GetOwnerId() const = 0;
			//! Gets the unique platform-specific identifier of the lobby
			virtual IUserLobby::Identifier GetIdentifier() const = 0;

			//! Sends a chat message to other users in the lobby
			virtual bool SendChatMessage(const char* szMessage) const = 0;

			//! Shows the invite dialog via the in-game dialog, allowing invitations for other players on the local user's playlist
			virtual void ShowInviteDialog() const = 0;

			//! Assigns a specific value to a key, and broadcasts it to the other users
			//! Can be queried using IMatchmaking::AddLobbyQueryFilterString
			virtual bool SetData(const char* szKey, const char* szValue) = 0;
			//! Retrieves a specific value by key, set using SetData
			virtual const char* GetData(const char* szKey) const = 0;
		};
	}
}