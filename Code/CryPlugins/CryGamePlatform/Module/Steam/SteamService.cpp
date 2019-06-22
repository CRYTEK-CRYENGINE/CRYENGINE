// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "SteamService.h"
#include "IGamePlatform.h"

#include "SteamUserIdentifier.h"

#include <CrySystem/ICmdLine.h>
#include <CrySystem/ConsoleRegistration.h>

// Included only once per DLL module.
#include <CryCore/Platform/platform_impl.inl>

namespace Cry
{
	namespace GamePlatform
	{
		namespace Steam
		{
			namespace Predicate
			{
				struct SWithSteamID
				{
					explicit SWithSteamID(const CSteamID& id) : m_id(id) {}
					bool operator()(const std::unique_ptr<CAccount>& pUser) const { return pUser->GetSteamID() == m_id; }

				private:
					CSteamID m_id;
				};
			}

			//-----------------------------------------------------------------------------
			// Purpose: callback hook for debug text emitted from the Steam API
			//-----------------------------------------------------------------------------
			extern "C" void __cdecl SteamAPIDebugTextHook(int nSeverity, const char* pchDebugText)
			{
				// if you're running in the debugger, only warnings (nSeverity >= 1) will be sent
				// if you add -debug_steamapi to the command-line, a lot of extra informational messages will also be sent
				CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "[Steam] %s", pchDebugText);
			}

			CService::CService()
				: m_callbackGameOverlayActivated(this, &CService::OnGameOverlayActivated)
				, m_onAvatarImageLoadedCallback(this, &CService::OnAvatarImageLoaded)
				, m_onFriendStateChangeCallback(this, &CService::OnFriendStateChange)
				, m_callbackGetSteamAuthTicketResponse(this, &CService::OnGetSteamAuthTicketResponse)
				, m_callbackOnPersonaChange(this, &CService::OnPersonaStateChange)
				, m_pServer(nullptr)
			{
				// Map Steam API language codes to engine
				// Full list of Steam API language codes can be found here: https://partner.steamgames.com/doc/store/localization
				m_translationMappings["arabic"] = ILocalizationManager::ePILID_Arabic;
				m_translationMappings["schinese"] = ILocalizationManager::ePILID_ChineseS;
				m_translationMappings["tchinese"] = ILocalizationManager::ePILID_ChineseT;
				m_translationMappings["czech"] = ILocalizationManager::ePILID_Czech;
				m_translationMappings["danish"] = ILocalizationManager::ePILID_Danish;
				m_translationMappings["dutch"] = ILocalizationManager::ePILID_Dutch;
				m_translationMappings["english"] = ILocalizationManager::ePILID_English;
				m_translationMappings["finnish"] = ILocalizationManager::ePILID_Finnish;
				m_translationMappings["french"] = ILocalizationManager::ePILID_French;
				m_translationMappings["german"] = ILocalizationManager::ePILID_German;
				m_translationMappings["italian"] = ILocalizationManager::ePILID_Italian;
				m_translationMappings["japanese"] = ILocalizationManager::ePILID_Japanese;
				m_translationMappings["koreana"] = ILocalizationManager::ePILID_Korean;
				m_translationMappings["norwegian"] = ILocalizationManager::ePILID_Norwegian;
				m_translationMappings["polish"] = ILocalizationManager::ePILID_Polish;
				m_translationMappings["portuguese"] = ILocalizationManager::ePILID_Portuguese;
				m_translationMappings["russian"] = ILocalizationManager::ePILID_Russian;
				m_translationMappings["spanish"] = ILocalizationManager::ePILID_Spanish;
				m_translationMappings["swedish"] = ILocalizationManager::ePILID_Swedish;
				m_translationMappings["turkish"] = ILocalizationManager::ePILID_Turkish;
			}

			bool CService::Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams)
			{
				if (env.IsEditor())
				{
					// Don't use Steam integration in Sandbox
					return true;
				}

				int steam_appId = 0;
				REGISTER_CVAR(steam_appId, steam_appId, VF_REQUIRE_APP_RESTART, "Override Steam application id, only possible during development! Requires application restart.");

#if !defined(RELEASE)
				char buff[MAX_PATH];
				CryGetExecutableFolder(CRY_ARRAY_COUNT(buff), buff);
				const string appIdPath = PathUtil::Make(buff, "steam_appid", "txt");

				FILE* const pSteamAppID = fopen(appIdPath.c_str(), "wt");
				fprintf(pSteamAppID, "%d", steam_appId);
				fclose(pSteamAppID);
#endif // !defined(RELEASE)

				const bool isInitialized = SteamAPI_Init();

#if defined(RELEASE)
				// Validate that the application was started through Steam,
				// If this fails we should quit the application
				if (SteamAPI_RestartAppIfNecessary(steam_appId))
				{
					gEnv->pSystem->Quit();
					return true;
				}
#else
				// Remove the file, no longer needed
				remove(appIdPath.c_str());
#endif

				gEnv->pConsole->UnregisterVariable("steam_appId");

				if (!isInitialized)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[Steam] SteamApi_Init failed");

#ifdef RELEASE
					return false;
#else
					// Allow silent failure (barring the error message above) in development mode
					// Otherwise we wouldn't be able to start the engine without Steam running
					return true;
#endif
				}

				EnableUpdate(Cry::IEnginePlugin::EUpdateStep::MainUpdate, true);

				m_pStatistics = stl::make_unique<CStatistics>(*this);
				m_pSteamLeaderboards = stl::make_unique<CLeaderboards>(*this);

				m_pMatchmaking = stl::make_unique<CMatchmaking>(*this);
				m_pNetworking = stl::make_unique<CNetworking>();

				m_pRemoteStorage = stl::make_unique<CRemoteStorage>(*this);

				ICmdLine* pCmdLine = gEnv->pSystem->GetICmdLine();

				ISteamApps* pSteamApps = SteamApps();
				if (!pSteamApps)
				{
					return false;
				}

				char betaName[256];
				if (pSteamApps->GetCurrentBetaName(betaName, 256))
				{
					CryLogAlways("[Steam] Running Steam game beta %s", betaName);
				}

				ISteamUser* pSteamUser = SteamUser();
				if (!pSteamUser)
				{
					return false;
				}

				CryLogAlways("[Steam] Successfully initialized Steam API, user_id=%" PRIu64 " build_id=%i", pSteamUser->GetSteamID().ConvertToUint64(), pSteamApps->GetAppBuildId());

#if CRY_GAMEPLATFORM_EXPERIMENTAL
				m_environment["ApplicationId"].Format("%d", steam_appId);
				m_environment["BuildId"].Format("%d", pSteamApps->GetAppBuildId());
				if (strlen(betaName) > 0)
				{
					m_environment["NetworkEnvironment"] = betaName;
				}
#endif // CRY_GAMEPLATFORM_EXPERIMENTAL

				if (Cry::GamePlatform::IPlugin* pPlugin = gEnv->pSystem->GetIPluginManager()->QueryPlugin<Cry::GamePlatform::IPlugin>())
				{
					pPlugin->RegisterMainService(*this);
				}

				// Check if user requested to join a lobby right away
				if (const ICmdLineArg* pCmdArg = pCmdLine->FindArg(eCLAT_Post, "connect_lobby"))
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "[Steam] Received pre-launch join request, loading lobby");
					CSteamID lobbyId = CSteamID((uint64)atoll(pCmdArg->GetValue()));
					if (lobbyId.IsValid())
					{
						m_pMatchmaking->JoinLobby(CreateLobbyIdentifier(lobbyId));
					}
				}

				// Set localization according to the settings made in Steam
				const char* szDesiredLanguage = pSteamApps->GetCurrentGameLanguage();
				ILocalizationManager::EPlatformIndependentLanguageID languageId = ILocalizationManager::ePILID_English; // Default to english if we the language is not supported
				const auto remapIt = m_translationMappings.find(szDesiredLanguage);
				if (remapIt != m_translationMappings.end())
				{
					languageId = remapIt->second;
				}
				else
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[GamePlatform] Language mapping missing for %s", szDesiredLanguage);
				}

				CRY_ASSERT_MESSAGE(languageId != ILocalizationManager::ePILID_MAX_OR_INVALID, "Invalid language ID");
				if (languageId != ILocalizationManager::ePILID_MAX_OR_INVALID)
				{
					const char* szLanguage = gEnv->pSystem->GetLocalizationManager()->LangNameFromPILID(languageId);
					CRY_ASSERT(szLanguage != nullptr && szLanguage[0] != '\0');
					CryLogAlways("Attempting to automatically set user preferred language %s", szLanguage);
					ICVar* pVar = gEnv->pConsole->GetCVar("g_language");
					if (pVar != nullptr)
					{
						// Don't override the language if it was loaded from a config file.
						// We assume that it was set via an ingame-menu in that case.
						if ((pVar->GetFlags() & VF_WASINCONFIG) == 0)
						{
							pVar->Set(szLanguage);
						}
					}

					pVar = gEnv->pConsole->GetCVar("g_languageAudio");
					if (pVar != nullptr)
					{
						// Don't override the language if it was loaded from a config file.
						// We assume that it was set via an ingame-menu in that case.
						if ((pVar->GetFlags() & VF_WASINCONFIG) == 0)
						{
							pVar->Set(szLanguage);
						}
					}
				}

				return true;
			}

			void CService::MainUpdate(float frameTime)
			{
				CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);

				SteamAPI_RunCallbacks();
			}

			void CService::Shutdown()
			{
				m_pServer.reset();
				m_pRemoteStorage.reset();
				m_pNetworking.reset();
				m_pMatchmaking.reset();
				m_pSteamLeaderboards.reset();
				m_pStatistics.reset();

				m_friends.clear();

				for (const std::unique_ptr<CAccount>& pRemovedAccount : m_accounts)
				{
					NotifyAccountRemoved(pRemovedAccount.get());
				}

				m_accounts.clear();

				for (IListener* pListener : m_listeners)
				{
					pListener->OnShutdown(SteamServiceID);
				}

				SteamAPI_Shutdown();
			}

			ServiceIdentifier CService::GetServiceIdentifier() const
			{
				return SteamServiceID;
			}

			int CService::GetBuildIdentifier() const
			{
				if (ISteamApps* pSteamApps = SteamApps())
				{
					return pSteamApps->GetAppBuildId();
				}
				return 0;
			}

			CAccount* CService::GetLocalAccount() const
			{
				if (ISteamUser* pSteamUser = SteamUser())
				{
					return TryGetAccount(pSteamUser->GetSteamID());
				}

				return nullptr;
			}

			void CService::OnGameOverlayActivated(GameOverlayActivated_t* pData)
			{
				// Upload statistics added during gameplay
				m_pStatistics->Upload();

				for (IListener* pListener : m_listeners)
				{
					pListener->OnOverlayActivated(SteamServiceID, pData->m_bActive != 0);
				}
			}

			void CService::OnAvatarImageLoaded(AvatarImageLoaded_t* pCallback)
			{
				const AccountIdentifier accountId = CreateAccountIdentifier(pCallback->m_steamID);
				for (IListener* pListener : m_listeners)
				{
					pListener->OnAvatarImageLoaded(accountId);
				}
			}

			void CService::OnFriendStateChange(PersonaStateChange_t* pCallback)
			{
				if (pCallback->m_nChangeFlags & k_EPersonaChangeComeOnline)
				{
					TryGetAccount(pCallback->m_ulSteamID);
				}
				else if (pCallback->m_nChangeFlags & k_EPersonaChangeGoneOffline)
				{
					if (auto pRemovedAccount = RemoveAccount(pCallback->m_ulSteamID))
					{
						NotifyAccountRemoved(pRemovedAccount.get());
					}
				}
			}

			void CService::OnGetSteamAuthTicketResponse(GetAuthSessionTicketResponse_t* pData)
			{
				const HAuthTicket authTicket = pData->m_hAuthTicket;
				const auto it = m_pendingAuthTicketHandles.find(authTicket);
				if (it != std::end(m_pendingAuthTicketHandles))
				{
					const bool success = pData->m_eResult == EResult::k_EResultOK;
					for (IListener* pListener : m_listeners)
					{
						pListener->OnGetSteamAuthTicketResponse(success, authTicket);
					}
					m_pendingAuthTicketHandles.erase(it);
				}
				else
				{
					CryLog("[Steam] Ignoring auth token %u as it was not requested by GamePlatform", authTicket);
				}
			}

			void CService::OnPersonaStateChange(PersonaStateChange_t* pData)
			{
				for (IListener* pListener : m_listeners)
				{
					pListener->OnPersonaStateChanged(CAccount(pData->m_ulSteamID), static_cast<IListener::EPersonaChangeFlags>(pData->m_nChangeFlags));
				}
			}

			bool CService::OwnsApplication(ApplicationIdentifier id) const
			{
				if (ISteamApps* pSteamApps = SteamApps())
				{
					return pSteamApps->BIsSubscribedApp(ExtractSteamID(id));
				}

				return false;
			}

			ApplicationIdentifier CService::GetApplicationIdentifier() const
			{
				AppId_t rawValue = k_uAppIdInvalid;

				if (ISteamUtils* pSteamUtils = SteamUtils())
				{
					rawValue = pSteamUtils->GetAppID();
				}

				return CreateApplicationIdentifier(rawValue);
			}

			bool CService::OpenDialog(EDialog dialog) const
			{
				switch (dialog)
				{
				case EDialog::Friends:
					return OpenDialog("friends");
				case EDialog::Community:
					return OpenDialog("community");
				case EDialog::Players:
					return OpenDialog("players");
				case EDialog::Settings:
					return OpenDialog("settings");
				case EDialog::Group:
					return OpenDialog("officialgamegroup");
				case EDialog::Achievements:
					return OpenDialog("achievements");
				case EDialog::Stats:
					return OpenDialog("stats");
				}

				return false;
			}

			bool CService::OpenDialogWithTargetUser(EUserTargetedDialog dialog, const AccountIdentifier& accountId) const
			{
				const CSteamID steamID = ExtractSteamID(accountId);

				switch (dialog)
				{
				case EUserTargetedDialog::UserInfo:
					return OpenDialogWithTargetUser("steamid", accountId);
				case EUserTargetedDialog::FriendAdd:
					return OpenDialogWithTargetUser("friendadd", accountId);
				case EUserTargetedDialog::FriendRemove:
					return OpenDialogWithTargetUser("friendremove", accountId);
				case EUserTargetedDialog::FriendRequestAccept:
					return OpenDialogWithTargetUser("friendrequestaccept", accountId);
				case EUserTargetedDialog::FriendRequestIgnore:
					return OpenDialogWithTargetUser("friendrequestignore", accountId);
				case EUserTargetedDialog::Chat:
					return OpenDialogWithTargetUser("chat", accountId);
				case EUserTargetedDialog::JoinTrade:
					return OpenDialogWithTargetUser("jointrade", accountId);
				case EUserTargetedDialog::Stats:
					return OpenDialogWithTargetUser("stats", accountId);
				case EUserTargetedDialog::Achievements:
					return OpenDialogWithTargetUser("achievements", accountId);
				}

				return false;
			}

			bool CService::OpenDialog(const char* szPage) const
			{
				if (ISteamFriends* pSteamFriends = SteamFriends())
				{
					pSteamFriends->ActivateGameOverlay(szPage);
					return true;
				}

				return false;
			}

			bool CService::OpenDialogWithTargetUser(const char* szPage, const AccountIdentifier& accountId) const
			{
				if (ISteamFriends* pSteamFriends = SteamFriends())
				{
					const CSteamID steamID = ExtractSteamID(accountId);
					pSteamFriends->ActivateGameOverlayToUser(szPage, steamID);
					return true;
				}

				return false;
			}

			bool CService::OpenBrowser(const char* szURL) const
			{
				if (ISteamFriends* pSteamFriends = SteamFriends())
				{
					pSteamFriends->ActivateGameOverlayToWebPage(szURL);
					return true;
				}

				return false;
			}

			bool CService::CanOpenPurchaseOverlay() const
			{
				if (ISteamUtils* pSteamUtils = SteamUtils())
				{
					return pSteamUtils->IsOverlayEnabled();
				}

				return false;
			}

			const DynArray<IAccount*>& CService::GetFriendAccounts() const
			{
				if (ISteamFriends* const pSteamFriends = SteamFriends())
				{
					m_friends.clear();

					constexpr int friendFlags = k_EFriendFlagImmediate;
					const int friendCount = pSteamFriends->GetFriendCount(friendFlags);
					for (int i = 0; i < friendCount; ++i)
					{
						const CSteamID friendId = pSteamFriends->GetFriendByIndex(i, friendFlags);
						m_friends.push_back(TryGetAccount(friendId));
					}
				}

				return m_friends;
			}

#if CRY_GAMEPLATFORM_EXPERIMENTAL
			const DynArray<IAccount*>& CService::GetBlockedAccounts() const
			{
				if (ISteamFriends* const pSteamFriends = SteamFriends())
				{
					m_blockedAccounts.clear();

					constexpr int friendFlags = k_EFriendFlagBlocked | k_EFriendFlagIgnored | k_EFriendFlagIgnoredFriend;
					const int friendCount = pSteamFriends->GetFriendCount(friendFlags);
					for (int i = 0; i < friendCount; ++i)
					{
						const CSteamID friendId = pSteamFriends->GetFriendByIndex(i, friendFlags);
						m_blockedAccounts.push_back(TryGetAccount(friendId));
					}
				}

				return m_blockedAccounts;
			}

			const DynArray<IAccount*>& CService::GetMutedAccounts() const
			{
				CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[Steam][Service] GetMutedAccounts() is not implemented yet");

				return m_mutedAccounts;
			}

			bool CService::GetEnvironmentValue(const char* szVarName, string& valueOut) const
			{
				auto pos = m_environment.find(szVarName);
				if (pos != m_environment.end())
				{
					valueOut = pos->second;
					return true;
				}

				return false;
			}
#endif // CRY_GAMEPLATFORM_EXPERIMENTAL

			CAccount* CService::GetAccountById(const AccountIdentifier& accountId) const
			{
				return TryGetAccount(accountId);
			}

			bool CService::IsFriendWith(const AccountIdentifier& accountId) const
			{
				if (ISteamFriends* pSteamFriends = SteamFriends())
				{
					const CSteamID steamId = ExtractSteamID(accountId);
					return pSteamFriends->HasFriend(steamId, k_EFriendFlagImmediate);
				}

				return false;
			}

			EFriendRelationship CService::GetFriendRelationship(const AccountIdentifier& accountId) const
			{
				if (ISteamFriends* pSteamFriends = SteamFriends())
				{
					const CSteamID steamId = ExtractSteamID(accountId);
					switch (pSteamFriends->GetFriendRelationship(steamId))
					{
					case k_EFriendRelationshipNone:
						return EFriendRelationship::None;

					case k_EFriendRelationshipRequestRecipient:
						return EFriendRelationship::RequestReceived;

					case k_EFriendRelationshipFriend:
						return EFriendRelationship::Friend;

					case k_EFriendRelationshipRequestInitiator:
						return EFriendRelationship::RequestSent;

					case k_EFriendRelationshipIgnored:
						return EFriendRelationship::Blocked;

					default:
						break;
					}
				}

				return EFriendRelationship::None;
			}

			CServer* CService::CreateServer(bool bLocal)
			{
				if (m_pServer == nullptr)
				{
					m_pServer = stl::make_unique<CServer>(bLocal);
				}

				return m_pServer.get();
			}

			CAccount* CService::TryGetAccount(CSteamID id) const
			{
				for (const std::unique_ptr<CAccount>& pUser : m_accounts)
				{
					if (pUser->GetSteamID() == id)
					{
						return pUser.get();
					}
				}

				m_accounts.emplace_back(stl::make_unique<CAccount>(id));
				NotifyAccountAdded(m_accounts.back().get());

				return m_accounts.back().get();
			}

			CAccount* CService::TryGetAccount(const AccountIdentifier& accountId) const
			{
				const CSteamID steamId = ExtractSteamID(accountId);
				return TryGetAccount(steamId);
			}

			std::unique_ptr<CAccount> CService::RemoveAccount(CSteamID id)
			{
				std::unique_ptr<CAccount> removedAccount;

				auto accPos = std::find_if(m_accounts.begin(), m_accounts.end(), Predicate::SWithSteamID(id));
				if (accPos != m_accounts.end())
				{
					removedAccount.swap(*accPos);
					m_accounts.erase(accPos);

					stl::find_and_erase(m_friends, removedAccount.get());
				}

				return removedAccount;
			}

			bool CService::GetAuthToken(string &tokenOut, int &issuerId)
			{
				char rgchToken[2048];
				uint32 unTokenLen = 0;
				ISteamUser* pSteamUser = SteamUser();
				if (!pSteamUser)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "Steam user service not available");
					return false;
				}
				const HAuthTicket authTicket = pSteamUser->GetAuthSessionTicket(rgchToken, sizeof(rgchToken), &unTokenLen);
				m_pendingAuthTicketHandles.insert(authTicket);

				const char hex_chars[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };

				for (int i = 0; i < unTokenLen; ++i)
				{
					char const byte = rgchToken[i];

					tokenOut += hex_chars[(byte & 0xF0) >> 4];
					tokenOut += hex_chars[(byte & 0x0F) >> 0];
				}

				return true;
			}

			bool CService::RequestUserInformation(const AccountIdentifier& accountId, UserInformationMask info)
			{
				if (ISteamFriends* pFriends = SteamFriends())
				{
					const bool requireNameOnly = info == eUIF_Name;
					return pFriends->RequestUserInformation(ExtractSteamID(accountId), requireNameOnly);
				}

				return false;
			}

			bool CService::IsLoggedIn() const
			{
				if (ISteamUser* pSteamUser = SteamUser())
					return pSteamUser->BLoggedOn();

				return false;
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

			CRYREGISTER_SINGLETON_CLASS(CService)
		}
	}
}
