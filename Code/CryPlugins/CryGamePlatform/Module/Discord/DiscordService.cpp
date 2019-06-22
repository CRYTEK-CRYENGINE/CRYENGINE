// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "DiscordService.h"
#include "IGamePlatform.h"

#include "DiscordUserIdentifier.h"

#include <CrySystem/ICmdLine.h>
#include <CrySystem/ConsoleRegistration.h>

// Included only once per DLL module.
#include <CryCore/Platform/platform_impl.inl>

namespace Cry
{
	namespace GamePlatform
	{
		namespace Discord
		{
			inline ApplicationIdentifierValue ReadAppIdFromCVar()
			{
				const char* discord_appId = "";
				REGISTER_CVAR(discord_appId, "", VF_REQUIRE_APP_RESTART, "Discord application id. Requires application restart.");
				return discord_appId;
			}

			CService* CService::s_pInstance = nullptr;

			CService::CService()
				: m_appId(ReadAppIdFromCVar())
			{
				s_pInstance = this;
			}

			CService::~CService()
			{
				s_pInstance = nullptr;
			}

			bool CService::Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams)
			{
				if (env.IsEditor())
				{
					// Don't use Discord integration in Sandbox
					return true;
				}

				DiscordEventHandlers handlers;
				memset(&handlers, 0, sizeof(handlers));
				handlers.ready = OnDiscordReady;
				handlers.errored = OnDiscordError;
				handlers.disconnected = OnDiscordDisconnected;
				handlers.joinGame = nullptr;
				handlers.spectateGame = nullptr;
				handlers.joinRequest = nullptr;

				Cry::GamePlatform::IPlugin* pGamePlatform = gEnv->pSystem->GetIPluginManager()->QueryPlugin<Cry::GamePlatform::IPlugin>();
				Cry::GamePlatform::IService* pSteamService = pGamePlatform ? pGamePlatform->GetService(SteamServiceID) : nullptr;

				stack_string steamAppId;
				if (pSteamService)
				{
					steamAppId.Format("%ud", pSteamService->GetApplicationIdentifier());
				}
#ifdef CRY_PLATFORM_WINDOWS
				else
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "[Discord] Unable to retrieve steam app id. Integration will be limited.");
				}
#endif

				Discord_Initialize(m_appId.c_str(), &handlers, 1, steamAppId.c_str());

				EnableUpdate(Cry::IEnginePlugin::EUpdateStep::MainUpdate, true);

				m_localAccount = stl::make_unique<CAccount>();

				CryLogAlways("[Discord] Successfully initialized Discord Rich Presence API");

				if (pGamePlatform)
				{
					pGamePlatform->RegisterService(*this);
				}

				return true;
			}

			void CService::MainUpdate(float frameTime)
			{
				CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);

				Discord_RunCallbacks();
			}

			void CService::Shutdown()
			{
				SetLocalUser(nullptr);

				for (IListener* pListener : m_listeners)
				{
					pListener->OnShutdown(DiscordServiceID);
				}

				Discord_Shutdown();
			}

			ServiceIdentifier CService::GetServiceIdentifier() const
			{
				return DiscordServiceID;
			}

			int CService::GetBuildIdentifier() const
			{
				return 0;
			}

			CAccount* CService::GetLocalAccount() const
			{
				return m_localAccount.get();
			}
			
			bool CService::OwnsApplication(ApplicationIdentifier id) const
			{
				return false;
			}

			ApplicationIdentifier CService::GetApplicationIdentifier() const
			{
				return CreateApplicationIdentifier(m_appId);
			}

			bool CService::OpenDialog(EDialog dialog) const
			{
				return false;
			}

			bool CService::OpenDialogWithTargetUser(EUserTargetedDialog dialog, const AccountIdentifier& accountId) const
			{
				return false;
			}

			bool CService::OpenBrowser(const char* szURL) const
			{
				return false;
			}

			bool CService::CanOpenPurchaseOverlay() const
			{
				return false;
			}

			const DynArray<IAccount*>& CService::GetFriendAccounts() const
			{
				return m_friends;
			}

#if CRY_GAMEPLATFORM_EXPERIMENTAL
			const DynArray<IAccount*>& CService::GetBlockedAccounts() const
			{
				CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[Discord][Service] GetBlockedAccounts() is not implemented yet");

				return m_blockedAccounts;
			}

			const DynArray<IAccount*>& CService::GetMutedAccounts() const
			{
				CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[Discord][Service] GetMutedAccounts() is not implemented yet");

				return m_mutedAccounts;
			}

			bool CService::GetEnvironmentValue(const char* szVarName, string& valueOut) const
			{
				CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[Discord][Service] GetEnvironmentValue() is not implemented yet");
				return false;
			}
#endif // CRY_GAMEPLATFORM_EXPERIMENTAL

			CAccount* CService::GetAccountById(const AccountIdentifier& accountId) const
			{
				if (m_localAccount && m_localAccount->GetIdentifier() == accountId)
				{
					return m_localAccount.get();
				}

				return nullptr;
			}

			bool CService::IsFriendWith(const AccountIdentifier& accountId) const
			{
				return false;
			}

			EFriendRelationship CService::GetFriendRelationship(const AccountIdentifier& accountId) const
			{
				return EFriendRelationship::None;
			}

			IServer* CService::CreateServer(bool bLocal)
			{
				return nullptr;
			}

			bool CService::GetAuthToken(string &tokenOut, int &issuerId)
			{
				return false;
			}

			bool CService::RequestUserInformation(const AccountIdentifier& accountId, UserInformationMask info)
			{
				return false;
			}

			bool CService::IsLoggedIn() const
			{
				return true;
			}

			void CService::SetLocalUser(const DiscordUser* pUser)
			{
				if (m_localAccount)
				{
					if (pUser)
					{
						m_localAccount->SetDiscordUser(*pUser);
						NotifyAccountAdded(m_localAccount.get());
					}
					else
					{
						NotifyAccountRemoved(m_localAccount.get());
						m_localAccount.reset();
					}
				}
			}

			void CService::NotifyAccountAdded(CAccount* pAccount) const
			{
				if (pAccount)
				{
					for (IListener* pListener : m_listeners)
					{
						pListener->OnAccountAdded(*pAccount);
					}
				}
			}

			void CService::NotifyAccountRemoved(CAccount* pAccount) const
			{
				if (pAccount)
				{
					for (IListener* pListener : m_listeners)
					{
						pListener->OnAccountRemoved(*pAccount);
					}
				}
			}

			void CService::OnDiscordReady(const DiscordUser* pUser)
			{
				if (s_pInstance)
				{
					CryComment("[Discord] Rich Presence SDK user connected: userid='%s' nick='%s'", pUser->userId, pUser->username);

					s_pInstance->SetLocalUser(pUser);
				}
			}

			void CService::OnDiscordDisconnected(int errorCode, const char* szMessage)
			{
				if (errorCode)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "[Discord] Disconnected! API Error %d: '%s'", errorCode, szMessage);
				}
				else
				{
					CryComment("[Discord] Rich Presence SDK user disconnected.");
				}

				if (s_pInstance)
				{
					s_pInstance->SetLocalUser(nullptr);
				}
			}

			void CService::OnDiscordError(int errorCode, const char* szMessage)
			{
				CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR,  "[Discord] API Error %d: '%s'", errorCode, szMessage);
			}

			CRYREGISTER_SINGLETON_CLASS(CService)
		}
	}
}
