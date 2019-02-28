#include "StdAfx.h"

#include <steam/steam_api.h>

#include "SteamStatistics.h"
#include "SteamAchievement.h"
#include "SteamPlatform.h"

namespace Cry
{
	namespace GamePlatform
	{
		namespace Steam
		{
			CStatistics::CStatistics()
				: m_callbackUserStatsReceived(this, &CStatistics::OnUserStatsReceived)
				, m_callbackUserStatsStored(this, &CStatistics::OnUserStatsStored)
				, m_callbackAchievementStored(this, &CStatistics::OnAchievementStored)
				, m_callbackStatsUnloaded(this, &CStatistics::OnStatsUnloaded)
				, m_bInitialized(false)
				, m_bRequestedAchievementReset(false)
			{
				if (!Download())
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "[Steam] Failed to retrieve current stats");
				}

				if (ISteamUtils* pSteamUtils = SteamUtils())
					m_appId = pSteamUtils->GetAppID();
			}

			CStatistics::~CStatistics()
			{
				Upload();
			}

			bool CStatistics::ResetStatistics()
			{
				if (ISteamUserStats* pUserStats = SteamUserStats())
				{
					if (!pUserStats->ResetAllStats(false))
						CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "[Steam] Failed to reset stats!");

					m_bInitialized = false;
					return Download();
				}

				return false;
			}

			bool CStatistics::ResetAchievements()
			{
				if (!m_bInitialized)
				{
					m_bRequestedAchievementReset = true;
					return true;
				}

				if (ISteamUserStats* pUserStats = SteamUserStats())
				{
					int numAchievements = pUserStats->GetNumAchievements();

					for (int i = 0; i < numAchievements; i++)
					{
						pUserStats->ClearAchievement(pUserStats->GetAchievementName(i));
					}

					Upload();

					m_bRequestedAchievementReset = false;
					m_bInitialized = false;

					return Download();
				}

				return false;
			}

			bool CStatistics::Download()
			{
				CPlugin::GetInstance()->SetAwaitingCallback(1);

				if (ISteamUserStats* pUserStats = SteamUserStats())
					return pUserStats->RequestCurrentStats();

				return false;
			}

			bool CStatistics::Upload()
			{
				CPlugin::GetInstance()->SetAwaitingCallback(1);

				if (ISteamUserStats* pUserStats = SteamUserStats())
					return pUserStats->StoreStats();

				return false;
			}

			bool CStatistics::Set(const char* name, int value)
			{
				CRY_ASSERT(m_bInitialized);

				if (ISteamUserStats* pUserStats = SteamUserStats())
				{
					if (pUserStats->SetStat(name, value))
						return pUserStats->StoreStats();
				}

				return false;
			}

			bool CStatistics::Set(const char* name, float value)
			{
				CRY_ASSERT(m_bInitialized);

				if (ISteamUserStats* pUserStats = SteamUserStats())
				{
					if (pUserStats->SetStat(name, value))
						return pUserStats->StoreStats();
				}

				return false;
			}

			bool CStatistics::Get(const char* name, int &value) const
			{
				CRY_ASSERT(m_bInitialized);

				if (ISteamUserStats* pUserStats = SteamUserStats())
					return pUserStats->GetStat(name, &value);

				return false;
			}

			bool CStatistics::Get(const char* name, float &value) const
			{
				CRY_ASSERT(m_bInitialized);

				if (ISteamUserStats* pUserStats = SteamUserStats())
					return pUserStats->GetStat(name, &value);

				return false;
			}

			IAchievement* CStatistics::GetAchievement(const char* szName)
			{
				CRY_ASSERT(m_bInitialized);

				if (ISteamUserStats* pUserStats = SteamUserStats())
				{
					bool bAchieved;
					if (pUserStats->GetAchievement(szName, &bAchieved))
					{
						return TryGetAchievement(szName, bAchieved);
					}
				}

				return nullptr;
			}

			IAchievement* CStatistics::TryGetAchievement(const char* name, bool bAchieved)
			{
				for (const std::unique_ptr<IAchievement>& pAchievement : m_achievements)
				{
					if (!strcmp(pAchievement->GetName(), name))
					{
						return pAchievement.get();
					}
				}

				m_achievements.emplace_back(stl::make_unique<CAchivement>(name, bAchieved));
				return m_achievements.back().get();
			}

			// Steam API callbacks
			void CStatistics::OnUserStatsReceived(UserStatsReceived_t* pCallback)
			{
				CPlugin::GetInstance()->SetAwaitingCallback(-1);

				// we may get callbacks for other games' stats arriving, ignore them
				if (m_appId != pCallback->m_nGameID)
					return;

				if (pCallback->m_eResult == k_EResultOK)
				{
					m_bInitialized = true;

					if (m_bRequestedAchievementReset)
						ResetAchievements();
				}
			}

			void CStatistics::OnUserStatsStored(UserStatsStored_t* pCallback)
			{
				// we may get callbacks for other games' stats arriving, ignore them
				if (pCallback->m_nGameID != m_appId)
					return;
			}

			void CStatistics::OnStatsUnloaded(UserStatsUnloaded_t* pCallback)
			{
				ISteamUser* pSteamUser = SteamUser();
				if (pSteamUser && pCallback->m_steamIDUser == pSteamUser->GetSteamID())
					Download();
			}

			void CStatistics::OnAchievementStored(UserAchievementStored_t* pCallback)
			{
				// we may get callbacks for other games' stats arriving, ignore them
				if (pCallback->m_nGameID != m_appId)
					return;

				// Check if the achievement was fully unlocked
				if (pCallback->m_nCurProgress == 0 && pCallback->m_nMaxProgress == 0)
				{
					IAchievement* pAchievement = TryGetAchievement(pCallback->m_rgchAchievementName, true);
					CRY_ASSERT(pAchievement != nullptr);

					if (pAchievement != nullptr)
					{
						for (IListener* pListener : m_listeners)
						{
							pListener->OnAchievementUnlocked(*pAchievement);
						}
					}
				}
			}
		}
	}
}