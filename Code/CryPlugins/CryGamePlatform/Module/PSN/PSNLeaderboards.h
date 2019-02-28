#pragma once

#include "IPlatformLeaderboards.h"

#include <np/np_ranking.h>

namespace Cry
{
	namespace GamePlatform
	{
		namespace PSN
		{
			class CLeaderboards final : public ILeaderboards
			{
			public:
				CLeaderboards();
				virtual ~CLeaderboards();

				// IPlatformLeaderboards
				virtual void AddListener(IListener& listener) override { m_listeners.push_back(&listener); }
				virtual void RemoveListener(IListener& listener) override { stl::find_and_erase(m_listeners, &listener); }

				virtual void DownloadEntries(Identifier leaderboardId, ERequest request = ERequest::Global, int minRange = 1, int maxRange = 10, bool bTimeBased = false) override;
				virtual void UpdateScore(Identifier leaderboardId, int score, EScoreType scoreType, bool bForce = false) override;
				// ~IPlatformLeaderboards

				void Update();

			protected:
				int m_scoreTitleContext;
				std::vector<IListener*> m_listeners;

				struct SLeaderboardRequest
				{
					int requestId;
					Identifier leaderboardIdentifier;

					std::vector<SceNpScoreRankDataA> rankData;
					std::vector<SceNpScoreGameInfo> gameInfo;
					std::vector<SceNpScoreComment> comments;
				};

				struct SGameSpecificInfo
				{
					uint8 version;
					uint8 scoreType;
				};

				std::vector<SLeaderboardRequest> m_leaderboardRequests;
			};
		}
	}
}