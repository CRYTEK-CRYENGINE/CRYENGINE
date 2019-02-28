#include "StdAfx.h"
#include "PSNLeaderboards.h"
#include "PSNUser.h"

namespace Cry
{
	namespace GamePlatform
	{
		namespace PSN
		{
			CLeaderboards::CLeaderboards()
			{
				auto ret = sceSysmoduleLoadModule(SCE_SYSMODULE_NP_SCORE_RANKING);
				if (ret < SCE_OK)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[PSN] Failed to load ranking module");
					return;
				}

				CUser* pLocalUser = static_cast<CUser*>(CPlugin::GetInstance()->GetLocalClient());

				ret = sceNpScoreCreateNpTitleCtxA(0, pLocalUser->GetUserServiceUserId());
				if (ret < 0)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[PSN] Failed to create score np title");
					return;
				}

				m_scoreTitleContext = ret;
			}

			CLeaderboards::~CLeaderboards()
			{
				sceNpScoreDeleteTitleCtx(m_scoreTitleContext);
			}

			void CLeaderboards::Update()
			{
				if (m_leaderboardRequests.size() == 0)
					return;

				CUser* pLocalUser = static_cast<CUser*>(CPlugin::GetInstance()->GetLocalClient());

				for (auto it = m_leaderboardRequests.begin(); it != m_leaderboardRequests.end();)
				{
					int numEntries = it->rankData.size();
					int returnCode = sceNpScorePollAsync(it->requestId, &numEntries);
					if (returnCode < 0 || numEntries < 0)
					{
						sceNpScoreDeleteRequest(it->requestId);

						CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[PSN] Failed to poll for leaderboard update!");

						it = m_leaderboardRequests.erase(it);
					}
					else
					{
						if (returnCode == 1)
						{
							++it;
							continue;
						}

						for (IListener* pListener : m_listeners)
						{
							for(const SceNpScoreRankDataA& rankData : it->rankData)
							{
								bool bIsLocalUser = !strcmp(rankData.onlineId.data, pLocalUser->GetNpId().handle.data);

								pListener->OnEntryDownloaded(it->leaderboardIdentifier, rankData.onlineId.data, rankData.rank, rankData.scoreValue, EScoreType::Unknown, bIsLocalUser);
							}
						}

						sceNpScoreDeleteRequest(it->requestId);
						it = m_leaderboardRequests.erase(it);
					}
				}
			}

			void CLeaderboards::DownloadEntries(Identifier leaderboardIdentifier, ERequest request, int minRange, int maxRange, bool bTimeBased)
			{
				SLeaderboardRequest requestData;

				requestData.requestId = sceNpScoreCreateRequest(m_scoreTitleContext);
				if (requestData.requestId < 0)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[PSN] Failed to create score request");
					return;
				}

				SceRtcTick lastSortDate;
				SceNpScoreRankNumber totalRecord;

				requestData.rankData.resize(maxRange - minRange);
				
				auto ret = sceNpScoreGetRankingByRangeAAsync(requestData.requestId, leaderboardIdentifier.id, minRange + 1,
					requestData.rankData.data(), sizeof(SceNpScoreRankDataA) * requestData.rankData.size(),
					requestData.comments.data(), sizeof(SceNpScoreComment) * requestData.comments.size(),
					requestData.gameInfo.data(), sizeof(SceNpScoreGameInfo) * requestData.gameInfo.size(),
					requestData.rankData.size(), &lastSortDate, &totalRecord, nullptr);

				if (ret < 0)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[PSN] Failed to get ranged rankings for scoreboard");
					sceNpScoreDeleteRequest(requestData.requestId);

					return;
				}

				requestData.leaderboardIdentifier = leaderboardIdentifier;
				m_leaderboardRequests.push_back(requestData);
			}

			void CLeaderboards::UpdateScore(Identifier leaderboardIdentifier, int score, EScoreType scoreType, bool bForce)
			{
				auto requestId = sceNpScoreCreateRequest(m_scoreTitleContext);
				if (requestId < 0)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[PSN] Failed to create score request");
					return;
				}

				auto ret = sceNpScoreRecordScore(requestId, leaderboardIdentifier.id, score, nullptr, nullptr, nullptr, nullptr, nullptr);
				if (ret < 0)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[PSN] Failed to record score!");
					sceNpScoreDeleteRequest(requestId);
				}
			}
		}
	}
}