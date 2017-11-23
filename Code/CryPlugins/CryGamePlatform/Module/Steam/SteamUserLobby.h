#pragma once

#include "IPlatformLobby.h"

#include <CrySystem/ISystem.h>

namespace Cry
{
	namespace GamePlatform
	{
		namespace Steam
		{
			class CUserLobby
				: public IUserLobby
				, public ISystemEventListener
			{
			public:
				CUserLobby(IUserLobby::Identifier lobbyId);
				virtual ~CUserLobby();

				// IUserLobby
				virtual void AddListener(IListener& listener) override { m_listeners.push_back(&listener); }
				virtual void RemoveListener(IListener& listener) override { stl::find_and_erase(m_listeners, &listener); }

				virtual bool HostServer(const char* szLevel, bool isLocal) override;

				virtual int GetMemberLimit() const override;
				virtual int GetNumMembers() const override;
				virtual IUser::Identifier GetMemberAtIndex(int index) const override;

				virtual bool IsInServer() const override { return m_serverIP != 0; }
				virtual void Leave() override;

				virtual IUser::Identifier GetOwnerId() const override;
				virtual IUserLobby::Identifier GetIdentifier() const override { return m_steamLobbyId; }

				virtual bool SendChatMessage(const char* message) const override;
				
				virtual void ShowInviteDialog() const override;

				virtual bool SetData(const char* key, const char* value) override;
				virtual const char* GetData(const char* key) const override;
				// ~IUserLobby

				// ISystemEventListener
				virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam) override;
				// ~ISystemEventListener

				void ConnectToServer(uint32 ip, uint16 port, IServer::Identifier serverId);
				
			protected:
				STEAM_CALLBACK(CUserLobby, OnLobbyChatUpdate, LobbyChatUpdate_t, m_callbackChatDataUpdate);
				STEAM_CALLBACK(CUserLobby, OnLobbyDataUpdate, LobbyDataUpdate_t, m_callbackDataUpdate);

				STEAM_CALLBACK(CUserLobby, OnLobbyChatMessage, LobbyChatMsg_t, m_callbackChatMessage);

				STEAM_CALLBACK(CUserLobby, OnLobbyGameCreated, LobbyGameCreated_t, m_callbackGameCreated);

				std::vector<IListener*> m_listeners;
				IUserLobby::Identifier m_steamLobbyId;

				uint32 m_serverIP;
				uint16 m_serverPort;

				IServer::Identifier m_serverId;
			};
		}
	}
}