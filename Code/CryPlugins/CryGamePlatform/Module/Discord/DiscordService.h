// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "IPlatformService.h"
#include "DiscordAccount.h"

#include <CrySystem/ICryPlugin.h>

namespace Cry
{
	namespace GamePlatform
	{
		namespace Discord
		{
			class CService final : public Cry::IEnginePlugin, public IService
			{
			public:
				CRYINTERFACE_BEGIN()
					CRYINTERFACE_ADD(Cry::IEnginePlugin)
				CRYINTERFACE_END()
				CRYGENERATE_SINGLETONCLASS_GUID(CService, "Plugin_DiscordService", DiscordServiceID)

				CService();
				~CService();

				// Cry::IEnginePlugin
				virtual bool Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams) override;
				virtual void MainUpdate(float frameTime) override;
				virtual const char* GetName() const override { return "DiscordService"; }
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

				virtual IServer* CreateServer(bool bLocal) override;
				virtual IServer* GetLocalServer() const override { return nullptr; }

				virtual ILeaderboards* GetLeaderboards() const override { return nullptr; }
				virtual IStatistics* GetStatistics() const override { return nullptr; }
				virtual IRemoteStorage* GetRemoteStorage() const override { return nullptr; }

				virtual IMatchmaking* GetMatchmaking() const override { return nullptr; }

				virtual INetworking* GetNetworking() const override { return nullptr; }

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

			private:
				void SetLocalUser(const DiscordUser* pUser);
				void NotifyAccountRemoved(CAccount* pAccount) const;
				void NotifyAccountAdded(CAccount* pAccount) const;

			private:
				static void OnDiscordReady(const DiscordUser* pUser);
				static void OnDiscordDisconnected(int errorCode, const char* szMessage);
				static void OnDiscordError(int errorCode, const char* szMessage);

			private:
				static CService* s_pInstance;

				const ApplicationIdentifierValue m_appId;

				std::unique_ptr<CAccount> m_localAccount;
				mutable DynArray<IAccount*> m_friends;
#if CRY_GAMEPLATFORM_EXPERIMENTAL
				DynArray<IAccount*> m_blockedAccounts;
				DynArray<IAccount*> m_mutedAccounts;
#endif // CRY_GAMEPLATFORM_EXPERIMENTAL

				std::vector<IListener*> m_listeners;
			};
		}
	}
}