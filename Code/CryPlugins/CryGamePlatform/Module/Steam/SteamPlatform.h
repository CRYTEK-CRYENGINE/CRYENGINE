#pragma once

#include "IGamePlatform.h"

namespace Cry
{
	namespace GamePlatform
	{
		namespace Steam
		{
			class CPlugin final : public IPlugin
			{
				CRYINTERFACE_BEGIN()
					CRYINTERFACE_ADD(Cry::GamePlatform::IPlugin)
					CRYINTERFACE_ADD(Cry::IEnginePlugin)
				CRYINTERFACE_END()

				CRYGENERATE_SINGLETONCLASS_GUID(CPlugin, "Plugin_GamePlatform", "{0C7B8742-5FBF-4C48-AE7C-6E70308538EC}"_cry_guid)

			public:
				CPlugin();
				virtual ~CPlugin() = default;

				// Cry::IEnginePlugin
				virtual bool Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams) override;
				virtual void MainUpdate(float frameTime) override;
				virtual const char* GetName() const override { return "SteamGamePlatform"; }
				virtual const char* GetCategory() const override { return "GamePlatform"; }
				// ~Cry::IEnginePlugin

				// IGamePlatform
				virtual bool IsInitialized() const override { return m_isInitialized; }
				virtual EType GetType() const override { return EType::Steam; }

				virtual void AddListener(IListener& listener) override { m_listeners.push_back(&listener); }
				virtual void RemoveListener(IListener& listener) override { stl::find_and_erase(m_listeners, &listener); }

				virtual int GetBuildIdentifier() const override;

				virtual IUser* GetLocalClient() override;
				virtual IUser* GetUserById(IUser::Identifier id) override;

				virtual bool IsFriendWith(IUser::Identifier otherUserId) const override;

				virtual IServer* CreateServer(bool bLocal) override;
				virtual IServer* GetLocalServer() const override { return m_pServer.get(); }

				virtual ILeaderboards* GetLeaderboards() const override { return m_pSteamLeaderboards.get(); }
				virtual IStatistics* GetStatistics() const override { return m_pStatistics.get(); }
				virtual IRemoteStorage* GetRemoteStorage() const override { return m_pRemoteStorage.get(); }

				virtual IMatchmaking* GetMatchmaking() const override { return m_pMatchmaking.get(); }

				virtual INetworking* GetNetworking() const override { return m_pNetworking.get(); }

				virtual bool OwnsApplication(ApplicationIdentifier id) const override;
				virtual ApplicationIdentifier GetApplicationIdentifier() const override;

				virtual bool OpenDialog(EDialog dialog) const override;
				virtual bool OpenDialog(const char* szPage) const override;
				virtual bool OpenBrowser(const char* szURL) const override;

				virtual bool CanOpenPurchaseOverlay() const override;

				virtual bool GetAuthToken(string &tokenOut, int &issuerId) override;

				virtual EConnectionStatus GetConnectionStatus() const override { return EConnectionStatus::Connected; }
				virtual void CanAccessMultiplayerServices(std::function<void(bool authorized)> asynchronousCallback) override { asynchronousCallback(true);}
				// ~IGamePlatform

				ApplicationIdentifier GetAppId() const;

				void SetAwaitingCallback(int iAwaiting) { m_awaitingCallbacks += iAwaiting; }

				static CPlugin* GetInstance() { return s_pInstance; }

			protected:
				STEAM_CALLBACK(CPlugin, OnGameOverlayActivated, GameOverlayActivated_t, m_callbackGameOverlayActivated);

				IUser* TryGetUser(CSteamID id);
				static CPlugin* s_pInstance;

			private:
				bool m_isInitialized = false;

				std::shared_ptr<IStatistics> m_pStatistics;
				std::shared_ptr<ILeaderboards> m_pSteamLeaderboards;
				std::shared_ptr<IRemoteStorage> m_pRemoteStorage;
				std::shared_ptr<IMatchmaking> m_pMatchmaking;

				std::shared_ptr<INetworking> m_pNetworking;

				std::shared_ptr<IServer> m_pServer;

				std::vector<std::unique_ptr<IUser>> m_users;

				int m_awaitingCallbacks;
				std::vector<IListener*> m_listeners;

				uint32 m_authTicketHandle;
			};
		}
	}
}