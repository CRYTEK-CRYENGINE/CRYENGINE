#include "StdAfx.h"

#include <steam/steam_api.h>
#include <steam/steam_gameserver.h>

#include <steam/isteamuser.h>

#include "SteamPlatform.h"

#include "SteamStatistics.h"
#include "SteamLeaderboards.h"
#include "SteamRemoteStorage.h"
#include "SteamMatchmaking.h"

#include "SteamUser.h"
#include "SteamServer.h"

#include "SteamNetworking.h"
#include "SteamUserLobby.h"

#include <CrySystem/ICmdLine.h>
#include <CrySystem/ILocalizationManager.h>

// Included only once per DLL module.
#include <CryCore/Platform/platform_impl.inl>

namespace Cry
{
	namespace GamePlatform
	{
		namespace Steam
		{
			CPlugin* CPlugin::s_pInstance = nullptr;

			//-----------------------------------------------------------------------------
			// Purpose: callback hook for debug text emitted from the Steam API
			//-----------------------------------------------------------------------------
			extern "C" void __cdecl SteamAPIDebugTextHook(int nSeverity, const char* pchDebugText)
			{
				// if you're running in the debugger, only warnings (nSeverity >= 1) will be sent
				// if you add -debug_steamapi to the command-line, a lot of extra informational messages will also be sent
				CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "[Steam] %s", pchDebugText);
			}

			CPlugin::CPlugin()
				: m_callbackGameOverlayActivated(this, &CPlugin::OnGameOverlayActivated)
				, m_pServer(nullptr)
				, m_awaitingCallbacks(0)
				, m_authTicketHandle(k_HAuthTicketInvalid)
			{
				s_pInstance = this;
			}

			bool CPlugin::Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams)
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

				m_isInitialized = SteamAPI_Init();

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

				if (!m_isInitialized)
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

				EnableUpdate(IPlugin::EUpdateStep::MainUpdate, true);

				m_pStatistics = std::make_shared<CStatistics>();
				m_pSteamLeaderboards = std::make_shared<CLeaderboards>();

				m_pMatchmaking = std::make_shared<CMatchmaking>();
				m_pNetworking = std::make_shared<CNetworking>();

				m_pRemoteStorage = std::make_shared<CRemoteStorage>();

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

				CryLogAlways("[Steam] Successfully initialized Steam API, running build id %i", pSteamApps->GetAppBuildId());

				// Check if user requested to join a lobby right away
				if (const ICmdLineArg* pCmdArg = pCmdLine->FindArg(eCLAT_Post, "connect_lobby"))
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "[Steam] Received pre-launch join request, loading lobby");
					CSteamID lobbyId = CSteamID((uint64)atoll(pCmdArg->GetValue()));
					if (lobbyId.IsValid())
					{
						m_pMatchmaking->JoinLobby(lobbyId.ConvertToUint64());
					}
				}

				ILocalizationManager* pLocalizationManager = gEnv->pSystem->GetLocalizationManager();
				const char* szDesiredLanguage = pSteamApps->GetCurrentGameLanguage();
				if (szDesiredLanguage != nullptr && szDesiredLanguage[0] != '\0')
				{
					CryLogAlways("Attempting to automatically set user preferred language %s", szDesiredLanguage);

					for (int i = 0; i < ILocalizationManager::ePILID_MAX_OR_INVALID; i++)
					{
						const char* languageName = pLocalizationManager->LangNameFromPILID((ILocalizationManager::EPlatformIndependentLanguageID)i);

						if (!stricmp(languageName, szDesiredLanguage))
						{
							if (pLocalizationManager->IsLanguageSupported((ILocalizationManager::EPlatformIndependentLanguageID)i))
							{
								gEnv->pConsole->GetCVar("g_language")->Set(languageName);
							}

							break;
						}
					}
				}

				return true;
			}

			void CPlugin::MainUpdate(float frameTime)
			{
				CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);

				if (m_awaitingCallbacks > 0)
				{
					SteamAPI_RunCallbacks();
				}
			}

			int CPlugin::GetBuildIdentifier() const
			{
				if (ISteamApps* pSteamApps = SteamApps())
				{
					return pSteamApps->GetAppBuildId();
				}
				return 0;
			}

			IUser* CPlugin::GetLocalClient()
			{
				if (ISteamUser* pSteamUser = SteamUser())
				{
					return TryGetUser(pSteamUser->GetSteamID());
				}

				return nullptr;
			}

			IUser* CPlugin::GetUserById(IUser::Identifier id)
			{
				return TryGetUser(id);
			}

			void CPlugin::OnGameOverlayActivated(GameOverlayActivated_t* pData)
			{
				// Upload statistics added during gameplay
				m_pStatistics->Upload();

				for (IListener* pListener : m_listeners)
				{
					pListener->OnOverlayActivated(pData->m_bActive != 0);
				}

				m_awaitingCallbacks -= 1;
			}

			bool CPlugin::OwnsApplication(ApplicationIdentifier id) const
			{
				if (ISteamApps* pSteamApps = SteamApps())
				{
					return pSteamApps->BIsSubscribedApp(id);
				}

				return false;
			}

			ApplicationIdentifier CPlugin::GetApplicationIdentifier() const
			{
				if (ISteamUtils* pSteamUtils = SteamUtils())
				{
					return pSteamUtils->GetAppID();
				}

				return 0;
			}

			bool CPlugin::OpenDialog(EDialog dialog) const
			{
				switch (dialog)
				{
					case EDialog::Friends:
						return OpenDialog("Friends");
					case EDialog::Community:
						return OpenDialog("Community");
					case EDialog::Players:
						return OpenDialog("Players");
					case EDialog::Settings:
						return OpenDialog("Settings");
					case EDialog::Group:
						return OpenDialog("OfficialGameGroup");
					case EDialog::Achievements:
						return OpenDialog("Achievements");
				}

				return false;
			}

			bool CPlugin::OpenDialog(const char* szPage) const
			{
				if (ISteamFriends* pSteamFriends = SteamFriends())
				{
					pSteamFriends->ActivateGameOverlay(szPage);
					return true;
				}

				return false;
			}

			bool CPlugin::OpenBrowser(const char* szURL) const
			{
				if (ISteamFriends* pSteamFriends = SteamFriends())
				{
					pSteamFriends->ActivateGameOverlayToWebPage(szURL);
					return true;
				}

				return false;
			}


			bool CPlugin::CanOpenPurchaseOverlay() const
			{
				if (ISteamUtils* pSteamUtils = SteamUtils())
				{
					return pSteamUtils->IsOverlayEnabled();
				}

				return false;
			}

			bool CPlugin::IsFriendWith(IUser::Identifier otherUserId) const
			{
				if (auto* pSteamFriends = SteamFriends())
				{
					return pSteamFriends->HasFriend(otherUserId, k_EFriendFlagImmediate);
				}

				return false;
			}

			ApplicationIdentifier CPlugin::GetAppId() const
			{
				if (ISteamUtils* pSteamUtils = SteamUtils())
					return pSteamUtils->GetAppID();
				return k_uAppIdInvalid;
			}

			IServer* CPlugin::CreateServer(bool bLocal)
			{
				if (m_pServer == nullptr)
				{
					m_pServer = std::make_shared<CServer>(bLocal);
				}

				return m_pServer.get();
			}

			IUser* CPlugin::TryGetUser(CSteamID id)
			{
				uint64 cleanId = id.ConvertToUint64();

				for (const std::unique_ptr<IUser>& pUser : m_users)
				{
					if (pUser->GetIdentifier() == cleanId)
					{
						return pUser.get();
					}
				}

				m_users.emplace_back(stl::make_unique<CUser>(id));
				return m_users.back().get();
			}

			bool CPlugin::GetAuthToken(string &tokenOut, int &issuerId)
			{
				char rgchToken[2048];
				uint32 unTokenLen = 0;
				ISteamUser* pSteamUser = SteamUser();
				if (!pSteamUser)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "Steam user service not available");
					return false;
				}
				m_authTicketHandle = (uint32)pSteamUser->GetAuthSessionTicket(rgchToken, sizeof(rgchToken), &unTokenLen);

				const char hex_chars[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };

				for (int i = 0; i < unTokenLen; ++i)
				{
					char const byte = rgchToken[i];

					tokenOut += hex_chars[(byte & 0xF0) >> 4];
					tokenOut += hex_chars[(byte & 0x0F) >> 0];
				}

				return true;
			}

			CRYREGISTER_SINGLETON_CLASS(CPlugin)
		}
	}
}

#include <CryCore/CrtDebugStats.h>