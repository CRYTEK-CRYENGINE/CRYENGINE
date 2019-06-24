// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "IPlatformService.h"
#include "PSNAccount.h"
#include "PSNLeaderboards.h"
#include "PSNStatistics.h"

#include <CrySystem/ICryPlugin.h>

namespace Cry
{
	namespace GamePlatform
	{
		namespace PSN
		{
			class CService final : public Cry::IEnginePlugin, public IService
			{
				CRYINTERFACE_BEGIN()
					CRYINTERFACE_ADD(Cry::IEnginePlugin)
				CRYINTERFACE_END()
				CRYGENERATE_SINGLETONCLASS_GUID(CService, "Plugin_PSNService", PSNServiceID)

			public:
				CService();
				~CService() = default;

				// Cry::IEnginePlugin
				virtual bool Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams) override;
				virtual void MainUpdate(float frameTime) override;
				virtual const char* GetName() const override { return "PSNService"; }
				virtual const char* GetCategory() const override { return "GamePlatform"; }
				// ~Cry::IEnginePlugin

				// IService
				virtual void AddListener(IListener& listener) override { m_listeners.push_back(&listener); }
				virtual void RemoveListener(IListener& listener) override { stl::find_and_erase(m_listeners, &listener); }

				virtual void Shutdown() override { CRY_ASSERT_MESSAGE(false, "Not implemented for PSN!"); }

				virtual ServiceIdentifier GetServiceIdentifier() const override { return PSNServiceID; }
				virtual int GetBuildIdentifier() const override { CRY_ASSERT_MESSAGE(false, "Not implemented for PSN!"); return 0; }
 
 				virtual CAccount* GetLocalAccount() const override { return &m_user; }

				virtual const DynArray<IAccount*>& GetFriendAccounts() const override { CRY_ASSERT_MESSAGE(false, "Not implemented for PSN!"); return m_friends; }
 				virtual CAccount* GetAccountById(const AccountIdentifier& accountId) const override { CRY_ASSERT_MESSAGE(false, "Not implemented for PSN!"); return nullptr; }
				virtual bool IsFriendWith(const AccountIdentifier& accountId) const override { CRY_ASSERT_MESSAGE(false, "Not implemented for PSN!"); return false; }
				virtual EFriendRelationship GetFriendRelationship(const AccountIdentifier& accountId) const override { CRY_ASSERT_MESSAGE(false, "Not implemented for PSN!"); return EFriendRelationship::None; }

				virtual IServer* CreateServer(bool bLocal) override { CRY_ASSERT_MESSAGE(false, "Not implemented for PSN!"); return nullptr; }
				virtual IServer* GetLocalServer() const override { CRY_ASSERT_MESSAGE(false, "Not implemented for PSN!"); return nullptr; }

				virtual ILeaderboards* GetLeaderboards() const override { return m_pLeaderboards.get(); }
				virtual IStatistics* GetStatistics() const override { return const_cast<CStatistics*>(&m_statistics); }
				virtual IRemoteStorage* GetRemoteStorage() const override { CRY_ASSERT_MESSAGE(false, "Not implemented for PSN!"); return nullptr; }

				virtual IMatchmaking* GetMatchmaking() const override { CRY_ASSERT_MESSAGE(false, "Not implemented for PSN!"); return nullptr; }

				virtual INetworking* GetNetworking() const override { CRY_ASSERT_MESSAGE(false, "Not implemented for PSN!"); return nullptr; }

				virtual bool OwnsApplication(ApplicationIdentifier id) const override { CRY_ASSERT_MESSAGE(false, "Not implemented for PSN!"); return false; }
				virtual ApplicationIdentifier GetApplicationIdentifier() const override  { CRY_ASSERT_MESSAGE(false, "Not implemented for PSN!"); return ApplicationIdentifier(); }

				virtual bool OpenDialog(EDialog dialog) const override { CRY_ASSERT_MESSAGE(false, "Not implemented for PSN!"); return false; }
				virtual bool OpenDialogWithTargetUser(EUserTargetedDialog dialog, const AccountIdentifier& accountId) const override { CRY_ASSERT_MESSAGE(false, "Not implemented for PSN!"); return false; }
				virtual bool OpenBrowser(const char* szURL) const override { CRY_ASSERT_MESSAGE(false, "Not implemented for PSN!"); return false; }

				virtual bool CanOpenPurchaseOverlay() const override { return false; }

				virtual bool GetAuthToken(string& tokenOut, int& issuerId) override { return m_user.GetAuthToken(tokenOut, issuerId); }

				virtual EConnectionStatus GetConnectionStatus() const override { return m_connectionStatus; }
				virtual void CanAccessMultiplayerServices(std::function<void(bool authorized)> asynchronousCallback) override;

				virtual bool RequestUserInformation(const AccountIdentifier& accountId, UserInformationMask info) override;
				virtual bool IsLoggedIn() const override { CRY_ASSERT_MESSAGE(false, "Not implemented for PSN!"); return false; }
				// ~IService

				void SetConnectionStatus(EConnectionStatus status) { m_connectionStatus = status; }

			private:
				static void PSNStateCallback(SceUserServiceUserId userId, SceNpState state, void* userdata);
				void SetLocalAccountStatus(ESignInStatus status);

			private:
				mutable CAccount m_user;
				CStatistics m_statistics;

				std::unique_ptr<CLeaderboards> m_pLeaderboards;
				std::function<void(bool authorized)> m_multiplayerAccessCallback;

				DynArray<IAccount*> m_friends;

				int m_sceNetCtlCallbackId;
				std::vector<IListener*> m_listeners;
				EConnectionStatus m_connectionStatus;

				SceNpCheckPlusResult m_npCheckPlusResult;
				int m_npCheckPlusRequestId = 0;
				float m_npNotifyPlusFeatureTimer = 0.f;
			};
		}
	}
}