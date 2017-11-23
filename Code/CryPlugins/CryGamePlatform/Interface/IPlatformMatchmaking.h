#pragma once

#include "IPlatformUser.h"
#include "IPlatformLobby.h"

namespace Cry
{
	namespace GamePlatform
	{
		struct IUserLobby;

		//! Interface to access the platform's matchmaking functionality, allowing finding lobbies with servers based on certain properties
		struct IMatchmaking
		{
			//! Exposes listeners to listen to matchmaking query callbacks
			struct IListener
			{
				// Called after QueryLobbies success, use IMatchmaking::GetQueriedLobbyIdByIndex to get the lobby identifiers
				virtual void OnLobbyQueryComplete(int numLobbiesFound) = 0;
				//! Called when a lobby was created by the local user
				virtual void OnCreatedLobby(IUserLobby* pLobby) = 0;
				//! Called when the local user joins a lobby
				virtual void OnJoinedLobby(IUserLobby* pLobby) = 0;
				//! Called if the local user fails to join a specific lobby
				virtual void OnLobbyJoinFailed(IUserLobby::Identifier lobbyId) = 0;
			};

			virtual ~IMatchmaking() {}

			//! Used to register matchmaking listeners
			virtual void AddListener(IListener& listener) = 0;
			//! Used to remove matchmaking listeners
			virtual void RemoveListener(IListener& listener) = 0;
			//! Creates a new lobby for the local user
			//! \param visibility Determines who will be able to see the lobby
			//! \param maxMemberCount Maximum amount of users that can join the lobby, including the local user
			virtual void CreateLobby(IUserLobby::EVisbility visibility, int maxMemberCount) = 0;
			//! Retrieves the lobby that the specified user is in, if any
			virtual IUserLobby* GetUserLobby(IUser::Identifier user) const = 0;
			//! Retrieves a lobby by its unique platform-specific identifier
			virtual IUserLobby* GetLobbyById(IUserLobby::Identifier id) = 0;
			//! Adds a filter to be applied to the next call to QueryLobbies, querying lobbies that match the specified comparison
			//! The keys are set using IUserLobby::SetData
			//! \param szKey The key of the filter to match
			//! \param szValue The value of the filter to match
			//! \param comparison The type of comparison to apply to results
			virtual void AddLobbyQueryFilterString(const char* szKey, const char* szValue, IUserLobby::EComparison comparison) = 0;
			//! Queries available lobbies, triggering matchmaking listeners for each result.
			virtual void QueryLobbies() = 0;
			//! Retrieves a queried lobby found via QueryLobbies (and notified via IListener::OnLobbyQueryComplete).
			virtual IUserLobby::Identifier GetQueriedLobbyIdByIndex(int index) const = 0;
			//! Joins a specific lobby as the local user
			virtual void JoinLobby(IUserLobby::Identifier lobbyId) = 0;
		};
	}
}