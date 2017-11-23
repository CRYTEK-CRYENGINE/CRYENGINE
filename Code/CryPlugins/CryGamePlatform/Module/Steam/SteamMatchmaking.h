#pragma once

#include "IPlatformMatchmaking.h"

namespace Cry
{
	namespace GamePlatform
	{
		namespace Steam
		{
			class CUserLobby;

			class CMatchmaking
				: public IMatchmaking
			{
			public:
				CMatchmaking();
				virtual ~CMatchmaking() = default;

				// IMatchmaking
				virtual void AddListener(IMatchmaking::IListener& listener) override { m_listeners.push_back(&listener); }
				virtual void RemoveListener(IMatchmaking::IListener& listener) override { stl::find_and_erase(m_listeners, &listener); }

				virtual void CreateLobby(IUserLobby::EVisbility visibility, int maxMemberCount) override;
				virtual IUserLobby* GetUserLobby(IUser::Identifier user) const override;

				virtual IUserLobby* GetLobbyById(IUserLobby::Identifier id) override;

				virtual void QueryLobbies() override;
				virtual IUserLobby::Identifier GetQueriedLobbyIdByIndex(int index) const override;

				virtual void AddLobbyQueryFilterString(const char* key, const char* value, IUserLobby::EComparison comparison) override;

				virtual void JoinLobby(IUserLobby::Identifier lobbyId) override;
				// ~IMatchmaking

				void OnLobbyRemoved(CUserLobby* pLobby);
			protected:
				CUserLobby* TryGetLobby(uint64 id);

			protected:
				void OnCreateLobby(LobbyCreated_t* pResult, bool bIOFailure);
				CCallResult<CMatchmaking, LobbyCreated_t> m_callResultCreateLobby;

				void OnLobbyMatchList(LobbyMatchList_t* pLobbyMatchList, bool bIOFailure);
				CCallResult<CMatchmaking, LobbyMatchList_t> m_callResultLobbyMatchList;

				void OnJoin(LobbyEnter_t* pLobbyMatchList, bool bIOFailure);
				CCallResult<CMatchmaking, LobbyEnter_t> m_callResultLobbyEntered;

				STEAM_CALLBACK(CMatchmaking, OnJoinRequested, GameLobbyJoinRequested_t, m_callbackJoinRequested);
				STEAM_CALLBACK(CMatchmaking, OnGameServerChangeRequested, GameServerChangeRequested_t, m_callbackGameServerChangeRequested);
				STEAM_CALLBACK(CMatchmaking, OnInvited, LobbyInvite_t, m_callbackInvited);

			protected:
				std::vector<IMatchmaking::IListener*> m_listeners;
				std::vector<std::unique_ptr<CUserLobby>> m_lobbies;
			};
		}
	}
}