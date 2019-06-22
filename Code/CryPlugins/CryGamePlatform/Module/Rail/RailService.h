// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "IPlatformService.h"
#include "RailAccount.h"
#include "RailMatchmaking.h"
#include "RailNetworking.h"

#include <CrySystem/ILocalizationManager.h>
#include <CrySystem/ICryPlugin.h>

namespace Cry
{
	namespace GamePlatform
	{
		namespace Rail
		{
			class CService final
				: public Cry::IEnginePlugin
				, public IService
				, public rail::IRailEvent
			{
			public:
				CRYINTERFACE_BEGIN()
					CRYINTERFACE_ADD(Cry::IEnginePlugin)
				CRYINTERFACE_END()
				CRYGENERATE_SINGLETONCLASS_GUID(CService, "Plugin_CryGamePlatformRail", RailServiceID)

				CService() = default;
				~CService() = default;

				// Cry::IEnginePlugin
				virtual bool Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams) override;
				virtual void MainUpdate(float frameTime) override;
				virtual const char* GetName() const override { return "CryGamePlatformRail"; }
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

				virtual IServer* CreateServer(bool bLocal) override { return nullptr; }
				virtual IServer* GetLocalServer() const override { return nullptr; }

				virtual ILeaderboards* GetLeaderboards() const override { return nullptr; }
				virtual IStatistics* GetStatistics() const override { return nullptr; }
				virtual IRemoteStorage* GetRemoteStorage() const override { return nullptr; }

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

				// rail::IRailEvent
				virtual void OnRailEvent(rail::RAIL_EVENT_ID eventId, rail::EventBase* pEvent) override;
				// ~rail::IRailEvent

				AccountIdentifier GetLocalAccountIdentifier() const;

			private:
				static void WarningMessageCallback(uint32_t, const char* szMessage);

				void RegisterEvents();
				void UnregisterEvents();
				void RequestAuthToken();

				void OnSystemStateChange(const rail::rail_event::RailSystemStateChanged* pChange);
				void OnShowNotifyWindow(const rail::rail_event::ShowNotifyWindow* pNotify);
				void OnShowFloatingWindowResult(const rail::rail_event::ShowFloatingWindowResult* pResult);
				void OnGetImageDataResult(const rail::rail_event::RailGetImageDataResult* pResult);
				void OnFriendsOnlineStateChanged(const rail::rail_event::RailFriendsOnlineStateChanged* pResult);
				void OnFriendsListChanged(const rail::rail_event::RailFriendsListChanged* pResult);
				void OnAcquireSessionTicketResponse(const rail::rail_event::AcquireSessionTicketResponse* pResult);
				void OnRailUsersInfoData(const rail::rail_event::RailUsersInfoData* pResult);

				CAccount* FindAccount(rail::RailID id) const;

				CAccount* TryGetAccount(rail::RailID id) const;
				CAccount* TryGetAccount(const rail::RailFriendInfo friendInfo) const;
				CAccount* TryGetAccount(const rail::PlayerPersonalInfo& playerInfo) const;
				CAccount* TryGetAccount(const AccountIdentifier& accountId) const;

				std::unique_ptr<CAccount> RemoveAccount(rail::RailID id);

				void NotifyAccountAdded(CAccount* pAccount) const;
				void NotifyAccountRemoved(CAccount* pAccount) const;

			private:
				std::unique_ptr<CMatchmaking> m_pMatchmaking;
				std::unique_ptr<CNetworking> m_pNetworking;

				mutable std::vector<std::unique_ptr<CAccount>> m_accounts;
				mutable DynArray<IAccount*> m_friends;
#if CRY_GAMEPLATFORM_EXPERIMENTAL
				DynArray<IAccount*> m_blockedAccounts;
				DynArray<IAccount*> m_mutedAccounts;
#endif // CRY_GAMEPLATFORM_EXPERIMENTAL

				std::vector<IListener*> m_listeners;

				rail::RailGameID m_applicationId = rail::kInvalidGameId;
				string* m_pSessionTicket = nullptr;
			};
		}
	}
}