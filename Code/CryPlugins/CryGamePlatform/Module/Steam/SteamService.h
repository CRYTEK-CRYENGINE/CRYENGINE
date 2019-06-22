// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "IPlatformService.h"
#include "SteamAccount.h"
#include "SteamServer.h"
#include "SteamLeaderboards.h"
#include "SteamStatistics.h"
#include "SteamRemoteStorage.h"
#include "SteamMatchmaking.h"
#include "SteamNetworking.h"

#include <CrySystem/ILocalizationManager.h>
#include <CrySystem/ICryPlugin.h>

namespace Cry
{
	namespace GamePlatform
	{
		namespace Steam
		{
			class CService final
				: public Cry::IEnginePlugin
				, public IService
			{
			public:
				CRYINTERFACE_BEGIN()
					CRYINTERFACE_ADD(Cry::IEnginePlugin)
				CRYINTERFACE_END()
				CRYGENERATE_SINGLETONCLASS_GUID(CService, "Plugin_CryGamePlatformSteam", SteamServiceID)

				CService();
				virtual ~CService() = default;

				// Cry::IEnginePlugin
				virtual bool Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams) override;
				virtual void MainUpdate(float frameTime) override;
				virtual const char* GetName() const override { return "CryGamePlatformSteam"; }
				virtual const char* GetCategory() const override { return "GamePlatform"; }
				// ~Cry::IEnginePlugin

				// IService
				virtual void AddListener(IListener& listener) override { m_listeners.push_back(&listener); }
				virtual void RemoveListener(IListener& listener) override { stl::find_and_erase(m_listeners, &listener); }

				virtual void Shutdown() override;

				virtual ServiceIdentifier GetServiceIdentifier() const override;
				virtual int GetBuildIdentifier() const override;

				virtual CAccount* GetLocalAccount() const override;

				virtual const DynArray<IAccount*>& GetFriendAccounts() const override;
#if CRY_GAMEPLATFORM_EXPERIMENTAL
				virtual const DynArray<IAccount*>& GetBlockedAccounts() const override;
				virtual const DynArray<IAccount*>& GetMutedAccounts() const override;
				virtual bool GetEnvironmentValue(const char* szVarName, string& valueOut) const override;
#endif // CRY_GAMEPLATFORM_EXPERIMENTAL
				virtual CAccount* GetAccountById(const AccountIdentifier& accountId) const override;
				virtual void AddAccountToLocalSession(const AccountIdentifier& accountId) override {};
				virtual void RemoveAccountFromLocalSession(const AccountIdentifier& accountId) override {};
				virtual bool IsFriendWith(const AccountIdentifier& accountId) const override;
				virtual EFriendRelationship GetFriendRelationship(const AccountIdentifier& accountId) const override;

				virtual CServer* CreateServer(bool bLocal) override;
				virtual CServer* GetLocalServer() const override { return m_pServer.get(); }

				virtual CLeaderboards* GetLeaderboards() const override { return m_pSteamLeaderboards.get(); }
				virtual CStatistics* GetStatistics() const override { return m_pStatistics.get(); }
				virtual CRemoteStorage* GetRemoteStorage() const override { return m_pRemoteStorage.get(); }

				virtual CMatchmaking* GetMatchmaking() const override { return m_pMatchmaking.get(); }

				virtual CNetworking* GetNetworking() const override { return m_pNetworking.get(); }

				virtual bool OwnsApplication(ApplicationIdentifier id) const override;
				virtual ApplicationIdentifier GetApplicationIdentifier() const override;

				virtual bool OpenDialog(EDialog dialog) const override;
				virtual bool OpenDialogWithTargetUser(EUserTargetedDialog dialog, const AccountIdentifier& accountId) const override;
				virtual bool OpenBrowser(const char* szURL) const override;

				virtual bool CanOpenPurchaseOverlay() const override;

				virtual bool GetAuthToken(string &tokenOut, int &issuerId) override;

				virtual EConnectionStatus GetConnectionStatus() const override { return EConnectionStatus::Connected; }
				virtual void CanAccessMultiplayerServices(std::function<void(bool authorized)> asynchronousCallback) override { asynchronousCallback(true); }

				virtual bool RequestUserInformation(const AccountIdentifier& accountId, UserInformationMask info) override;
				virtual bool IsLoggedIn() const override;

				virtual bool HasPermission(const AccountIdentifier& accountId, EPermission permission) const override { return true; }
				virtual bool HasPrivacyPermission(const AccountIdentifier& accountId, const AccountIdentifier& targetAccountId, EPrivacyPermission permission) const override { return true; };
				// ~IService

			protected:
				STEAM_CALLBACK(CService, OnGameOverlayActivated, GameOverlayActivated_t, m_callbackGameOverlayActivated);
				STEAM_CALLBACK(CService, OnAvatarImageLoaded, AvatarImageLoaded_t, m_onAvatarImageLoadedCallback);
				STEAM_CALLBACK(CService, OnFriendStateChange, PersonaStateChange_t, m_onFriendStateChangeCallback);
				STEAM_CALLBACK(CService, OnGetSteamAuthTicketResponse, GetAuthSessionTicketResponse_t, m_callbackGetSteamAuthTicketResponse);
				STEAM_CALLBACK(CService, OnPersonaStateChange, PersonaStateChange_t, m_callbackOnPersonaChange);

				bool OpenDialog(const char* szPage) const;
				bool OpenDialogWithTargetUser(const char* szPage, const AccountIdentifier& accountId) const;

				CAccount* TryGetAccount(CSteamID id) const;
				CAccount* TryGetAccount(const AccountIdentifier& accountId) const;
				std::unique_ptr<CAccount> RemoveAccount(CSteamID id);

				void NotifyAccountAdded(CAccount* pAccount) const;
				void NotifyAccountRemoved(CAccount* pAccount) const;

			private:
				std::unique_ptr<CStatistics> m_pStatistics;
				std::unique_ptr<CLeaderboards> m_pSteamLeaderboards;
				std::unique_ptr<CRemoteStorage> m_pRemoteStorage;
				std::unique_ptr<CMatchmaking> m_pMatchmaking;

				std::unique_ptr<CNetworking> m_pNetworking;

				std::unique_ptr<CServer> m_pServer;

				std::map<string, ILocalizationManager::EPlatformIndependentLanguageID> m_translationMappings;

				mutable std::vector<std::unique_ptr<CAccount>> m_accounts;
				mutable DynArray<IAccount*> m_friends;
#if CRY_GAMEPLATFORM_EXPERIMENTAL
				mutable DynArray<IAccount*> m_blockedAccounts;
				mutable DynArray<IAccount*> m_mutedAccounts;
				std::unordered_map<string, string, stl::hash_strcmp<string>, stl::hash_strcmp<string>> m_environment;
#endif // CRY_GAMEPLATFORM_EXPERIMENTAL

				std::vector<IListener*> m_listeners;

				using HAuthTicket = uint32;
				std::set<HAuthTicket> m_pendingAuthTicketHandles;
			};
		}
	}
}