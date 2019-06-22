// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "SteamStatistics.h"
#include "SteamAchievement.h"
#include "SteamService.h"

namespace Cry
{
	namespace GamePlatform
	{
		namespace Steam
		{
			CStatistics::CStatistics(CService& steamService)
				: m_service(steamService)
				, m_callbackUserStatsReceived(this, &CStatistics::OnUserStatsReceived)
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
					uint32 numAchievements = pUserStats->GetNumAchievements();

					for (uint32 i = 0; i < numAchievements; i++)
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
				if (ISteamUserStats* pUserStats = SteamUserStats())
					return pUserStats->RequestCurrentStats();

				return false;
			}

			bool CStatistics::Upload()
			{
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

			IAchievement* CStatistics::GetAchievement(const char* szName, const SAchievementDetails* pDetails)
			{
				CRY_ASSERT(m_bInitialized);

				if (ISteamUserStats* pUserStats = SteamUserStats())
				{
					return GetOrCreateAchievement(szName, pDetails);
				}

				return nullptr;
			}

			IAchievement* CStatistics::GetAchievement(int id, const SAchievementDetails* pDetails)
			{
				CRY_ASSERT(m_bInitialized);

				if (ISteamUserStats* pUserStats = SteamUserStats())
				{
					const char* szName = pUserStats->GetAchievementName(static_cast<uint32>(id));
					if (szName && *szName)
					{
						return GetOrCreateAchievement(szName, pDetails);
					}
				}

				return nullptr;
			}

			IAchievement* CStatistics::GetAchievementInternal(const char* szName)
			{
				for (const std::unique_ptr<IAchievement>& pAchievement : m_achievements)
				{
					if (strcmp(pAchievement->GetName(), szName) == 0)
					{
						return pAchievement.get();
					}
				}

				return nullptr;
			}

			IAchievement* CStatistics::GetOrCreateAchievement(const char* szName, const SAchievementDetails* pDetails)
			{
				// Check if steam achievement was already created
				if (IAchievement* pAchievement = GetAchievementInternal(szName))
				{
					return pAchievement;
				}

				ISteamUserStats* pUserStats = SteamUserStats();
				CRY_ASSERT(pUserStats != nullptr);

				// Check if steam achievement exists
				CAchievement::Type achievementType;
				int32 minProgress = 0;
				int32 maxProgress = 1;
				int32 curProgress;
				bool hasAchievement = false;
				bool achieved;
				if (pUserStats->GetAchievement(szName, &achieved))
				{
					achievementType = CAchievement::Type::Regular;
					hasAchievement = true;
					curProgress = achieved ? maxProgress : minProgress;
				}
				else
				{
					// Steam achievement was not found with given name
					// Check if steam stat exists for the given name which may be linked to a steam achievement as progress stat
					if (pUserStats->GetStat(szName, &curProgress))
					{
						if (pDetails == nullptr)
						{
							stack_string str;
							str.Format("[Steam] Achievement details for stat '%s' are missing", szName);
							CRY_ASSERT_MESSAGE(false, str.c_str());
							CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, str.c_str());
							return nullptr;
						}
						achievementType = CAchievement::Type::Stat;
						hasAchievement = true;
						minProgress = pDetails->minProgress;
						maxProgress = pDetails->maxProgress;
					}
				}

				if (hasAchievement)
				{
					m_achievements.emplace_back(stl::make_unique<CAchievement>(*this, szName, achievementType, minProgress, maxProgress, curProgress));
					return m_achievements.back().get();
				}

				return nullptr;
			}

			// Steam API callbacks
			void CStatistics::OnUserStatsReceived(UserStatsReceived_t* pCallback)
			{
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
					IAchievement* pAchievement = GetAchievementInternal(pCallback->m_rgchAchievementName);
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