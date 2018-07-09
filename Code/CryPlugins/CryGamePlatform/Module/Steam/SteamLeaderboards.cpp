#include "StdAfx.h"

#include <steam/steam_api.h>

#include "SteamLeaderboards.h"
#include "SteamPlatform.h"

namespace Cry
{
	namespace GamePlatform
	{
		namespace Steam
		{
			void CLeaderboards::FindOrCreateLeaderboard(const char* name, ELeaderboardSortMethod sortMethod, ELeaderboardDisplayType displayType)
			{
				ISteamUserStats* pSteamUserStats = SteamUserStats();
				if (!pSteamUserStats)
				{
					return;
				}
				SteamAPICall_t hSteamAPICall = pSteamUserStats->FindOrCreateLeaderboard(name, sortMethod, displayType);
				m_callResultFindLeaderboard.Set(hSteamAPICall, this, &CLeaderboards::OnFindLeaderboard);

				CPlugin::GetInstance()->SetAwaitingCallback(1);
			}

			void CLeaderboards::OnFindLeaderboard(LeaderboardFindResult_t* pResult, bool bIOFailure)
			{
				CPlugin::GetInstance()->SetAwaitingCallback(-1);
				ISteamUserStats* pSteamUserStats = SteamUserStats();

				if (!pResult->m_bLeaderboardFound || bIOFailure || !pSteamUserStats)
				{
					return;
				}

				if (m_pQueuedEntryRequest != nullptr)
				{
					SteamAPICall_t hSteamAPICall = pSteamUserStats->DownloadLeaderboardEntries(pResult->m_hSteamLeaderboard, m_pQueuedEntryRequest->request, m_pQueuedEntryRequest->minRange, m_pQueuedEntryRequest->maxRange);
					m_callResultEntriesDownloaded.Set(hSteamAPICall, this, &CLeaderboards::OnEntriesDownloaded);

					CPlugin::GetInstance()->SetAwaitingCallback(1);

					m_pQueuedEntryRequest.reset();
				}

				if (m_pQueuedUpdateRequest != nullptr)
				{
					int scoreDetails[1] = { static_cast<int>(m_pQueuedUpdateRequest->scoreType) };
					SteamAPICall_t hSteamAPICall = pSteamUserStats->UploadLeaderboardScore(pResult->m_hSteamLeaderboard, m_pQueuedUpdateRequest->method, m_pQueuedUpdateRequest->score, scoreDetails, 1);

					m_pQueuedUpdateRequest.reset();
				}
			}

			void CLeaderboards::DownloadEntries(Identifier leaderboardId, ERequest request, int minRange, int maxRange, bool bTimeBased)
			{
				if (SteamUserStats() == nullptr)
					return;

				m_pQueuedEntryRequest = stl::make_unique<SQueuedEntryRequest>();
				m_pQueuedEntryRequest->request = (ELeaderboardDataRequest)request;
				m_pQueuedEntryRequest->minRange = minRange;
				m_pQueuedEntryRequest->maxRange = maxRange;

				// If score is time based then sort leaderboard ascending and display in milliseconds, otherwise descending and numeric.
				ELeaderboardSortMethod sortMethod = bTimeBased ? k_ELeaderboardSortMethodAscending : k_ELeaderboardSortMethodDescending;
				ELeaderboardDisplayType displayType = bTimeBased ? k_ELeaderboardDisplayTypeTimeMilliSeconds : k_ELeaderboardDisplayTypeNumeric;

				FindOrCreateLeaderboard(leaderboardId.szName, sortMethod, displayType);
			}

			void CLeaderboards::OnEntriesDownloaded(LeaderboardScoresDownloaded_t* pResult, bool bIOFailure)
			{
				CPlugin::GetInstance()->SetAwaitingCallback(-1);

				LeaderboardEntry_t leaderboardEntry;
				int32 details[3];

				auto* pUser = CPlugin::GetInstance()->GetLocalClient();

				ISteamUserStats* pSteamUserStats = SteamUserStats();
				if (!pSteamUserStats)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "Steam user stats service not available");
					return;
				}
				ISteamFriends* pSteamFriends = SteamFriends();
				if (!pSteamFriends)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "Steam friends service not available");
					return;
				}

				for (int i = 0; i < pResult->m_cEntryCount; i++)
				{
					if (pSteamUserStats->GetDownloadedLeaderboardEntry(pResult->m_hSteamLeaderboardEntries, i, &leaderboardEntry, details, 3))
					{
						for (IListener* pListener : m_listeners)
						{
							Identifier leaderboardIdentifier;
							leaderboardIdentifier.szName = pSteamUserStats->GetLeaderboardName(pResult->m_hSteamLeaderboard);

							pListener->OnEntryDownloaded(leaderboardIdentifier, pSteamFriends->GetFriendPersonaName(leaderboardEntry.m_steamIDUser), leaderboardEntry.m_nGlobalRank, leaderboardEntry.m_nScore, (EScoreType)details[0], leaderboardEntry.m_steamIDUser.ConvertToUint64() == pUser->GetIdentifier());
						}
					}
				}
			}

			void CLeaderboards::UpdateScore(Identifier leaderboardId, int score, EScoreType scoreType, bool bForce)
			{
				if (SteamUserStats() == nullptr)
				{
					return;
				}

				m_pQueuedUpdateRequest = stl::make_unique<SQueuedUpdateRequest>();
				m_pQueuedUpdateRequest->method = bForce ? k_ELeaderboardUploadScoreMethodForceUpdate : k_ELeaderboardUploadScoreMethodKeepBest;
				m_pQueuedUpdateRequest->score = score;
				m_pQueuedUpdateRequest->scoreType = scoreType;

				// If score is time based then sort leaderboard ascending and display in milliseconds, otherwise descending and numeric.
				ELeaderboardSortMethod sortMethod = (scoreType == EScoreType::Time) ? k_ELeaderboardSortMethodAscending : k_ELeaderboardSortMethodDescending;
				ELeaderboardDisplayType displayType = (scoreType == EScoreType::Time) ? k_ELeaderboardDisplayTypeTimeMilliSeconds : k_ELeaderboardDisplayTypeNumeric;

				FindOrCreateLeaderboard(leaderboardId.szName, sortMethod, displayType);
			}
		}
	}
}