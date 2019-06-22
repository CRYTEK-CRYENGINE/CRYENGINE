// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "RailService.h"
#include "IGamePlatform.h"
#include "RailHelper.h"

#include <CrySystem/ICmdLine.h>
#include <CrySystem/ConsoleRegistration.h>

// Included only once per DLL module.
#include <CryCore/Platform/platform_impl.inl>

namespace Cry
{
	namespace GamePlatform
	{
		namespace Rail
		{
			namespace Predicate
			{
				struct SWithRailID
				{
					explicit SWithRailID(const rail::RailID& id) : m_id(id) {}
					bool operator()(const std::unique_ptr<CAccount>& pUser) const { return pUser->GetRailID() == m_id; }
					bool operator()(const IAccount* pUser) const { return ExtractRailID(pUser->GetIdentifier()) == m_id; }

				private:
					const rail::RailID m_id;
				};
			}

			/*static*/ void CService::WarningMessageCallback(uint32_t, const char* szMessage)
			{
				CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "[Rail][SDK] %s", szMessage);
			}

			void CService::RegisterEvents()
			{
				Helper::RegisterEvent(rail::kRailEventSystemStateChanged, *this);
				Helper::RegisterEvent(rail::kRailEventShowFloatingNotifyWindow, *this);
				Helper::RegisterEvent(rail::kRailEventShowFloatingWindow, *this);
				Helper::RegisterEvent(rail::kRailEventUtilsGetImageDataResult, *this);
				Helper::RegisterEvent(rail::kRailEventFriendsOnlineStateChanged, *this);
				Helper::RegisterEvent(rail::kRailEventFriendsFriendsListChanged, *this);
				Helper::RegisterEvent(rail::kRailEventSessionTicketGetSessionTicket, *this);
				Helper::RegisterEvent(rail::kRailEventUsersGetUsersInfo, *this);
			}

			void CService::UnregisterEvents()
			{
				Helper::UnregisterEvent(rail::kRailEventSystemStateChanged, *this);
				Helper::UnregisterEvent(rail::kRailEventShowFloatingNotifyWindow, *this);
				Helper::UnregisterEvent(rail::kRailEventShowFloatingWindow, *this);
				Helper::UnregisterEvent(rail::kRailEventUtilsGetImageDataResult, *this);
				Helper::UnregisterEvent(rail::kRailEventFriendsOnlineStateChanged, *this);
				Helper::UnregisterEvent(rail::kRailEventFriendsFriendsListChanged, *this);
				Helper::UnregisterEvent(rail::kRailEventSessionTicketGetSessionTicket, *this);
				Helper::UnregisterEvent(rail::kRailEventUsersGetUsersInfo, *this);
			}

			bool CService::Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams)
			{
				if (env.IsEditor() || env.IsDedicated())
				{
					// Don't use Rail integration in Sandbox or DS
					return true;
				}

				int rail_appId = 0;
				REGISTER_CVAR(rail_appId, rail_appId, VF_REQUIRE_APP_RESTART, "Override Rail application id, only possible during development! Requires application restart.");

				const bool setupSuccess = Helper::Setup(rail_appId);

				gEnv->pConsole->UnregisterVariable("rail_appId");

				if (!setupSuccess)
				{
					// Error messages are already printed by Helper code.
					return false;
				}

				if (rail::IRailUtils* pUtils = Helper::Utils())
				{
					const rail::RailResult res = pUtils->SetWarningMessageCallback(&CService::WarningMessageCallback);

					if (res != rail::kSuccess)
					{
						CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[Rail][Service] SetWarningMessageCallback() failed : %s", Helper::ErrorString(res));
					}
				}

				RegisterEvents();

				const bool isInitialized = Helper::Initialize();

				if (!isInitialized)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[Rail][Service] Initialization failed!");
					UnregisterEvents();
#ifdef RELEASE
					return false;
#else
					// Allow silent failure (barring the error message above) in development mode
					return true;
#endif
				}

				EnableUpdate(Cry::IEnginePlugin::EUpdateStep::MainUpdate, true);

				m_pMatchmaking = stl::make_unique<CMatchmaking>(*this);
				m_pNetworking = stl::make_unique<CNetworking>();

				rail::IRailPlayer* pRailPlayer = Helper::Player();
				CryLogAlways("[Rail][Service] Successfully initialized Rail API, user_id=%" PRIu64, pRailPlayer ? pRailPlayer->GetRailID().get_id() : rail::kInvalidRailId);

				if (Cry::GamePlatform::IPlugin* pPlugin = gEnv->pSystem->GetIPluginManager()->QueryPlugin<Cry::GamePlatform::IPlugin>())
				{
					pPlugin->RegisterMainService(*this);
				}

				ILocalizationManager::EPlatformIndependentLanguageID languageId = ILocalizationManager::ePILID_English; // Default to english if language is not supported

				if (rail::IRailGame* pRailGame = Helper::Game())
				{
					rail::RailBranchInfo branchInfo;
					rail::RailResult res = pRailGame->GetCurrentBranchInfo(&branchInfo);
					if (res == rail::kSuccess)
					{
						CryLogAlways("[Rail][Service] Running '%s' version, build '%s'. Branch id: '%s'. Branch name: '%s'",
							branchInfo.branch_type.c_str(), branchInfo.build_number.c_str(), branchInfo.branch_id.c_str(), branchInfo.branch_name.c_str());
					}
					else
					{
						CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "[Rail][Service] GetCurrentBranchInfo() failed : %s", Helper::ErrorString(res));
					}

					rail::RailString desiredLanguage;
					res = pRailGame->GetPlayerSelectedLanguageCode(&desiredLanguage);
					if (res == rail::kSuccess)
					{
						if (!Helper::MapLanguageCode(desiredLanguage, languageId))
						{
							CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[Rail][Service] Language mapping missing for '%s'", desiredLanguage.c_str());
						}
					}
					else
					{
						CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[Rail][Service] GetPlayerSelectedLanguageCode() failed : %s", Helper::ErrorString(res));
					}
				}

				CRY_ASSERT_MESSAGE(languageId != ILocalizationManager::ePILID_MAX_OR_INVALID, "Invalid language ID");
				if (languageId != ILocalizationManager::ePILID_MAX_OR_INVALID)
				{
					const char* szLanguage = gEnv->pSystem->GetLocalizationManager()->LangNameFromPILID(languageId);
					CRY_ASSERT(szLanguage != nullptr && szLanguage[0] != '\0');
					CryLogAlways("Attempting to automatically set user preferred language '%s'", szLanguage);
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

			void CService::Shutdown()
			{
				UnregisterEvents();

				m_pNetworking.reset();
				m_pMatchmaking.reset();

				m_friends.clear();

				for (const std::unique_ptr<CAccount>& pRemovedAccount : m_accounts)
				{
					NotifyAccountRemoved(pRemovedAccount.get());
				}

				m_accounts.clear();

				for (IListener* pListener : m_listeners)
				{
					pListener->OnShutdown(RailServiceID);
				}

				Helper::Finalize();
			}

			void CService::MainUpdate(float frameTime)
			{
				CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);

				Helper::FireEvents();
			}

			ServiceIdentifier CService::GetServiceIdentifier() const
			{
				return RailServiceID;
			}

			int CService::GetBuildIdentifier() const
			{
				if (rail::IRailGame* pRailGame = Helper::Game())
				{
					rail::RailString buildNumStr;
					const rail::RailResult res = pRailGame->GetBranchBuildNumber(&buildNumStr);
					if (res == rail::kSuccess)
					{
						return atoi(buildNumStr.c_str());
					}

					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[Rail][Service] GetBranchBuildNumber failed : %s", Helper::ErrorString(res));
				}

				return 0;
			}

			CAccount* CService::GetLocalAccount() const
			{
				if (rail::IRailPlayer* pRailUser = Helper::Player())
				{
					return TryGetAccount(pRailUser->GetRailID());
				}

				return nullptr;
			}

			AccountIdentifier CService::GetLocalAccountIdentifier() const
			{
				if (CAccount* pLocalAccount = GetLocalAccount())
				{
					return pLocalAccount->GetIdentifier();
				}

				return AccountIdentifier();
			}

			const DynArray<IAccount*>& CService::GetFriendAccounts() const
			{
				if (rail::IRailFriends* const pRailFriends = Helper::Friends())
				{
					rail::RailArray<rail::RailFriendInfo> railFriends;
					rail::RailResult res = pRailFriends->GetFriendsList(&railFriends);

					if (res == rail::kSuccess)
					{
						CryComment("[Rail][Service] Obtained %zu friends", railFriends.size());

						m_friends.clear();

						for (size_t friendIdx = 0; friendIdx < railFriends.size(); ++friendIdx)
						{
							const rail::RailFriendInfo& info = railFriends[friendIdx];

							CAccount* pAccount = TryGetAccount(info);

							m_friends.push_back(pAccount);
						}
					}
					else
					{
						CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[Rail][Service] GetFriendsList() failed : %s", Helper::ErrorString(res));
					}
				}

				return m_friends;
			}

#if CRY_GAMEPLATFORM_EXPERIMENTAL
			const DynArray<IAccount*>& CService::GetBlockedAccounts() const
			{
				CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[Rail][Service] GetBlockedAccounts() is not implemented yet");

				return m_blockedAccounts;
			}

			const DynArray<IAccount*>& CService::GetMutedAccounts() const
			{
				CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[Rail][Service] GetMutedAccounts() is not implemented yet");

				return m_mutedAccounts;
			}

			bool CService::GetEnvironmentValue(const char* szVarName, string& valueOut) const
			{
				CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[Rail][Service] GetEnvironmentValue() is not implemented yet");

				return false;
			}
#endif // CRY_GAMEPLATFORM_EXPERIMENTAL

			CAccount* CService::GetAccountById(const AccountIdentifier& accountId) const
			{
				return TryGetAccount(accountId);
			}

			bool CService::IsFriendWith(const AccountIdentifier& accountId) const
			{
				if (m_friends.empty())
				{
					GetFriendAccounts();
				}

				const rail::RailID railId = ExtractRailID(accountId);
				auto pos = std::find_if(m_friends.begin(), m_friends.end(), Predicate::SWithRailID(railId));

				return pos != m_friends.end();
			}

			EFriendRelationship CService::GetFriendRelationship(const AccountIdentifier& accountId) const
			{
				if (IsFriendWith(accountId))
				{
					return EFriendRelationship::Friend;
				}

				return EFriendRelationship::None;
			}

			bool CService::OwnsApplication(ApplicationIdentifier id) const
			{
				if (rail::IRailApps* pRailApps = Helper::Apps())
				{
					return pRailApps->IsGameInstalled(ExtractGameID(id));
				}

				return false;
			}

			ApplicationIdentifier CService::GetApplicationIdentifier() const
			{
				return CreateApplicationIdentifier(m_applicationId);
			}

			bool CService::OpenDialog(EDialog dialog) const
			{
				if (rail::IRailFloatingWindow* pFloatingWindow = Helper::FloatingWindow())
				{
					rail::RailResult res = rail::kFailure;

					switch (dialog)
					{
					case EDialog::Friends:
						res = pFloatingWindow->AsyncShowRailFloatingWindow(rail::kRailWindowFriendList, "");
						break;
					case EDialog::Achievements:
						res = pFloatingWindow->AsyncShowRailFloatingWindow(rail::kRailWindowAchievement, "");
						break;
					case EDialog::Stats:
						res = pFloatingWindow->AsyncShowRailFloatingWindow(rail::kRailWindowLeaderboard, "");
						break;
					default:
						CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "[Rail][Service] %s : unsupported dialog type. Falling back to friend list...", __func__);
						res = pFloatingWindow->AsyncShowRailFloatingWindow(rail::kRailWindowFriendList, "");
					}

					if (res == rail::kSuccess)
					{
						return true;
					}

					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[Rail][Service] OpenDialog('%d') failed : %s", static_cast<int>(dialog), Helper::ErrorString(res));
				}

				return false;
			}

			bool CService::OpenDialogWithTargetUser(EUserTargetedDialog dialog, const AccountIdentifier& accountId) const
			{
				if (rail::IRailUsersHelper* pUsers = Helper::UsersHelper())
				{
					const rail::RailID railID = ExtractRailID(accountId);

					rail::RailResult res = rail::kFailure;
					switch (dialog)
					{
					case EUserTargetedDialog::Chat:
						res = pUsers->AsyncShowChatWindowWithFriend(railID, "");
						break;
					case EUserTargetedDialog::UserInfo:
						res = pUsers->AsyncShowUserHomepageWindow(railID, "");
						break;
					default:
						CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "[Rail][Service] %s : unsupported dialog type. Falling back to chat...", __func__);
						res = pUsers->AsyncShowChatWindowWithFriend(railID, "");
					}

					if (res == rail::kSuccess)
					{
						return true;
					}

					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[Rail][Service] AsyncShowChatWindowWithFriend() failed : %s", Helper::ErrorString(res));
				}

				return false;
			}

			bool CService::OpenBrowser(const char* szURL) const
			{
				if (rail::IRailBrowserHelper* pHelper = Helper::BrowserHelper())
				{
					const rail::RailResult res = pHelper->NavigateWebPage(szURL, false);
					if (res == rail::kSuccess)
					{
						return true;
					}

					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[Rail][Service] NavigateWebPage('%s') failed : %s", szURL, Helper::ErrorString(res));
				}

				return false;
			}

			bool CService::CanOpenPurchaseOverlay() const
			{
				if (rail::IRailFloatingWindow* pFloat = Helper::FloatingWindow())
				{
					return pFloat->IsFloatingWindowAvailable();
				}

				return false;
			}

			void CService::RequestAuthToken()
			{
				if (rail::IRailPlayer* pPlayer = Helper::Player())
				{
					// Response in callback : OnAcquireSessionTicketResponse()
					const rail::RailResult res = pPlayer->AsyncAcquireSessionTicket("");
					if (res != rail::kSuccess)
					{
						CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[Rail][Service] AsyncAcquireSessionTicket() failed : %s", Helper::ErrorString(res));
					}
				}
			}

			bool CService::GetAuthToken(string &tokenOut, int &issuerId)
			{
				rail::IRailPlayer* pPlayer = Helper::Player();
				if (pPlayer == nullptr)
				{
					return false;
				}

				// Response in callback : OnAcquireSessionTicketResponse()
				const rail::RailResult res = pPlayer->AsyncAcquireSessionTicket("");
				if (res != rail::kSuccess)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[Rail][Service] AsyncAcquireSessionTicket() failed : %s", Helper::ErrorString(res));
					return false;
				}

				m_pSessionTicket = &tokenOut;

				return true;
			}


			bool CService::RequestUserInformation(const AccountIdentifier& accountId, UserInformationMask info)
			{
				if (rail::IRailFriends* pRailFriends = Helper::Friends())
				{
					rail::RailArray<rail::RailID> friends;
					friends.push_back(ExtractRailID(accountId));

					// Response in callback : OnRailUsersInfoData()
					const rail::RailResult res = pRailFriends->AsyncGetPersonalInfo(friends, "");
					if (res == rail::kSuccess)
					{
						return true;
					}

					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[Rail][Service] RequestUserInformation('%s') failed : %s", accountId.ToDebugString(), Helper::ErrorString(res));
				}

				return false;
			}

			bool CService::IsLoggedIn() const
			{
				if (rail::IRailPlayer* pPlayer = Helper::Player())
				{
					return pPlayer->AlreadyLoggedIn();
				}

				return false;
			}

			CAccount* CService::FindAccount(rail::RailID id) const
			{
				for (const std::unique_ptr<CAccount>& pAccount : m_accounts)
				{
					if (pAccount->GetRailID() == id)
					{
						return pAccount.get();
					}
				}

				return nullptr;
			}

			CAccount* CService::TryGetAccount(rail::RailID id) const
			{
				if (CAccount* pAccount = FindAccount(id))
				{
					return pAccount;
				}

				m_accounts.emplace_back(stl::make_unique<CAccount>(id));
				NotifyAccountAdded(m_accounts.back().get());

				return m_accounts.back().get();
			}

			CAccount* CService::TryGetAccount(const rail::RailFriendInfo friendInfo) const
			{
				// We don't use TryGetAccount(rail::RailID) to not trigger NotifyAccountAdded() too early
				if (CAccount* pAccount = FindAccount(friendInfo.friend_rail_id))
				{
					pAccount->SetFriendInfo(friendInfo);
					return pAccount;
				}

				m_accounts.emplace_back(stl::make_unique<CAccount>(friendInfo));
				NotifyAccountAdded(m_accounts.back().get());

				return m_accounts.back().get();
			}

			CAccount* CService::TryGetAccount(const rail::PlayerPersonalInfo& playerInfo) const
			{
				// We don't use TryGetAccount(rail::RailID) to not trigger NotifyAccountAdded() too early
				if (CAccount* pAccount = FindAccount(playerInfo.rail_id))
				{
					pAccount->SetPersonalInfo(playerInfo);
					return pAccount;
				}

				m_accounts.emplace_back(stl::make_unique<CAccount>(playerInfo));
				NotifyAccountAdded(m_accounts.back().get());

				return m_accounts.back().get();
			}

			CAccount* CService::TryGetAccount(const AccountIdentifier& accountId) const
			{
				const rail::RailID railId = ExtractRailID(accountId);
				return TryGetAccount(railId);
			}

			std::unique_ptr<CAccount> CService::RemoveAccount(rail::RailID id)
			{
				std::unique_ptr<CAccount> removedAccount;

				auto accPos = std::find_if(m_accounts.begin(), m_accounts.end(), Predicate::SWithRailID(id));
				if (accPos != m_accounts.end())
				{
					removedAccount.swap(*accPos);
					m_accounts.erase(accPos);

					stl::find_and_erase(m_friends, removedAccount.get());
				}

				return removedAccount;
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

			void CService::OnSystemStateChange(const rail::rail_event::RailSystemStateChanged* pChange)
			{
				if (pChange->result != rail::kSuccess)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[Rail][Service] %s failed : %s", __func__, Helper::ErrorString(pChange->result));
					return;
				}

				if (pChange->state == rail::kSystemStatePlatformOffline || pChange->state == rail::kSystemStatePlatformExit)
				{
					// From documentation: "If the user closes the Rail platform and the game verifies that the Rail platform is not running,
					// then the game will quit by itself. Or if the game receives the information in an event message notification from Rail SDK
					// that the Rail platform is not running, then the game should quit by itself."
#if defined(RELEASE)
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR_DBGBRK, "[Rail][Service] Connection to WeGame client lost. Quitting...");
					gEnv->pSystem->Quit();
					return;
#else
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "[Rail][Service] Connection to WeGame client lost. Investigate if not expected.");
#endif
				}

				m_applicationId = pChange->game_id;
				CryComment("[Rail][Service] Current application ID: '%" PRIu64 "'", m_applicationId.get_id());
			}

			// Triggered whenever a notification window is shown.
			void CService::OnShowNotifyWindow(const rail::rail_event::ShowNotifyWindow* pNotify)
			{
				if (pNotify->result != rail::kSuccess)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[Rail][Service] %s failed : %s", __func__, Helper::ErrorString(pNotify->result));
					return;
				}

				CryComment("[Rail][Service] Show '%s' Window : %s", Helper::EnumString(pNotify->window_type), pNotify->json_content.c_str());

				for (IListener* pListener : m_listeners)
				{
					pListener->OnOverlayActivated(RailServiceID, true);
				}
			}

			// Triggered whenever a floating window is shown/hidden.
			// Including, but not limited, to the ones opened using AsyncShowRailFloatingWindow()
			void CService::OnShowFloatingWindowResult(const rail::rail_event::ShowFloatingWindowResult* pResult)
			{
				if (pResult->result != rail::kSuccess)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[Rail][Service] %s failed : %s", __func__, Helper::ErrorString(pResult->result));
					return;
				}

				for (IListener* pListener : m_listeners)
				{
					pListener->OnOverlayActivated(RailServiceID, pResult->is_show);
				}
			}

			// Response to AsyncGetImageData()
			void CService::OnGetImageDataResult(const rail::rail_event::RailGetImageDataResult* pResult)
			{
				if (pResult->result != rail::kSuccess)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[Rail][Service] %s failed : %s", __func__, Helper::ErrorString(pResult->result));
					return;
				}

				CAccount* pAccount = TryGetAccount(pResult->rail_id);
				pAccount->SetAvatarInfo(pResult->image_data, pResult->image_data_descriptor);

				const AccountIdentifier accountId = pAccount->GetIdentifier();
				for (IListener* pListener : m_listeners)
				{
					pListener->OnAvatarImageLoaded(accountId);
				}
			}

			// Triggered if friends' online status change
			void CService::OnFriendsOnlineStateChanged(const rail::rail_event::RailFriendsOnlineStateChanged* pResult)
			{
				if (pResult->result != rail::kSuccess)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[Rail][Service] %s failed : %s", __func__, Helper::ErrorString(pResult->result));
					return;
				}

				switch (pResult->friend_online_state.friend_online_state)
				{
				case rail::kRailOnlineStateUnknown:
				case rail::kRailOnlineStateOffLine:
				{
					CryComment("[Rail][Service] Friend offline: %" PRIu64, pResult->friend_online_state.friend_rail_id.get_id());
					if (std::unique_ptr<CAccount> pRemovedAccount = RemoveAccount(pResult->friend_online_state.friend_rail_id))
					{
						NotifyAccountRemoved(pRemovedAccount.get());
					}
				}
				break;
				case rail::kRailOnlineStateOnLine:
				case rail::kRailOnlineStateBusy:
				case rail::kRailOnlineStateLeave:
				case rail::kRailOnlineStateGameDefinePlayingState:
				{
					CryComment("[Rail][Service] Friend online: %" PRIu64, pResult->friend_online_state.friend_rail_id.get_id());
					TryGetAccount(pResult->friend_online_state.friend_rail_id);
				}
				break;
				}
			}

			// Triggered if local friend list should be queried again
			void CService::OnFriendsListChanged(const rail::rail_event::RailFriendsListChanged* pResult)
			{
				if (pResult->result != rail::kSuccess)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[Rail][Service] %s failed : %s", __func__, Helper::ErrorString(pResult->result));
					return;
				}

				GetFriendAccounts();
			}

			// Response to AsyncAcquireSessionTicket()
 			void CService::OnAcquireSessionTicketResponse(const rail::rail_event::AcquireSessionTicketResponse* pResult)
 			{
				const bool success = pResult->result == rail::kSuccess;
				if (success)
				{
					if (m_pSessionTicket)
					{
						*m_pSessionTicket = pResult->session_ticket.ticket.c_str();
						m_pSessionTicket = nullptr;
					}

					CryComment("[Rail][Service] Authenticated using ticket '%s'", pResult->session_ticket.ticket.c_str());
				}
				else
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[Rail][Service] %s failed : %s", __func__, Helper::ErrorString(pResult->result));
				}

				for (IListener* pListener : m_listeners)
				{
					pListener->OnAuthTokenReceived(success, pResult->session_ticket.ticket.c_str());
				}
  			}

			// Response to AsyncGetPersonalInfo()
			void CService::OnRailUsersInfoData(const rail::rail_event::RailUsersInfoData* pResult)
			{
				if (pResult->result != rail::kSuccess)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[Rail][Service] %s failed : %s", __func__, Helper::ErrorString(pResult->result));
					return;
				}

				CryComment("[Rail][Service] OnRailUsersInfoData() returned %z results:", pResult->user_info_list.size());
				for (size_t infoIdx = 0; infoIdx < pResult->user_info_list.size(); ++infoIdx)
				{
					const rail::PlayerPersonalInfo& info = pResult->user_info_list[infoIdx];

					CryComment("-----------------------------------------------");
					CryComment("[Rail][Service] OnRailUsersInfoData() info #%z:", infoIdx);

					if (info.error_code == rail::kSuccess)
					{
						TryGetAccount(info);
					}
					else
					{
						CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "[Rail][Service] Error in PersonalInfoData : %s", Helper::ErrorString(info.error_code));
					}
				}

				if (rail::IRailUtils* pUtils = Helper::Utils())
				{
					for (size_t infoIdx = 0; infoIdx < pResult->user_info_list.size(); ++infoIdx)
					{
						const rail::PlayerPersonalInfo& info = pResult->user_info_list[infoIdx];
						// Response in callback : OnGetImageDataResult()
						const rail::RailResult res = pUtils->AsyncGetImageData(info.avatar_url, 0, 0, "");
						if (res != rail::kSuccess)
						{
							CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[Rail][Service] AsyncGetImageData('%s') failed : %s", info.avatar_url.c_str(), Helper::ErrorString(info.error_code));
						}
					}
				}
			}

			void CService::OnRailEvent(rail::RAIL_EVENT_ID eventId, rail::EventBase* pEvent)
			{
				if (pEvent->game_id != Helper::GameID())
				{
					return;
				}

				switch (eventId)
				{
				case rail::kRailEventSystemStateChanged:
					OnSystemStateChange(static_cast<const rail::rail_event::RailSystemStateChanged*>(pEvent));
					break;

				case rail::kRailEventShowFloatingNotifyWindow:
					OnShowNotifyWindow(static_cast<rail::rail_event::ShowNotifyWindow*>(pEvent));
					break;

				case rail::kRailEventShowFloatingWindow:
					OnShowFloatingWindowResult(static_cast<rail::rail_event::ShowFloatingWindowResult*>(pEvent));
					break;

				case rail::kRailEventUtilsGetImageDataResult:
					OnGetImageDataResult(static_cast<rail::rail_event::RailGetImageDataResult*>(pEvent));
					break;

				case rail::kRailEventFriendsOnlineStateChanged:
					OnFriendsOnlineStateChanged(static_cast<rail::rail_event::RailFriendsOnlineStateChanged*>(pEvent));
					break;

				case rail::kRailEventFriendsFriendsListChanged:
					OnFriendsListChanged(static_cast<rail::rail_event::RailFriendsListChanged*>(pEvent));
					break;

				case rail::kRailEventSessionTicketGetSessionTicket:
					OnAcquireSessionTicketResponse(static_cast<rail::rail_event::AcquireSessionTicketResponse*>(pEvent));
					break;

				case rail::kRailEventUsersGetUsersInfo:
					OnRailUsersInfoData(static_cast<rail::rail_event::RailUsersInfoData*>(pEvent));
					break;
				}
			}

			CRYREGISTER_SINGLETON_CLASS(CService)
		}
	}
}
