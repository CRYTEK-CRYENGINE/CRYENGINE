#pragma once

namespace Cry
{
	namespace GamePlatform
	{
		//! Allows for retrieving and pushing leaderboard data from / to the platform's servers
		struct ILeaderboards
		{
			//! Type of leaderboard request
			enum class ERequest
			{
				//! Request the global leaderboard, starting from the top user
				Global,
				//! Requests the global leaderboard entries close to the local user's ranking
				GlobalRelativeToUser,
				//! Requests the leaderboard entries of friends
				Friends,
				//! Used when downloading leaderboard entries of specific users
				Users
			};

			//! Determines the type of score that we are uploading to a leaderboard
			enum class EScoreType
			{
				//! Unknown score type, used if the platform does not provide the type of downloaded entries
				//! If this is provided then the gameplay code needs to be aware of leaderboards for the specified game
				Unknown,
				//! Time-based
				Time,
				//! Score / number based
				Score,
			};

			//! Unique platform-specific identifier of a leaderboard
			union Identifier
			{
				//! String identifier for the leaderboard, used for Steam
				const char* szName;
				//! Numeric identifier for the leaderboard, used for PSN
				uint32 id;
			};

			//! Provides callbacks for leaderboard events
			struct IListener
			{
				//! Called when we have downloaded a new leaderboard entry after a call to DownloadEntries
				virtual void OnEntryDownloaded(Identifier leaderboardId, const char* playerName, int rank, int score, EScoreType scoreType, bool bCurrentUser) = 0;
			};

			virtual ~ILeaderboards() {}

			//! Adds a new listener for leaderboard events
			virtual void AddListener(IListener& listener) = 0;
			//! Removes an existing listener for leaderboard events
			virtual void RemoveListener(IListener& listener) = 0;

			//! Asynchronously downloads entries of a specific leaderboard identified by name, triggers IListener::OnEntryDownloaded on download
			virtual void DownloadEntries(Identifier leaderboardId, ERequest request = ERequest::Global, int minRange = 1, int maxRange = 10, bool bTimeBased = false) = 0;
			//! Updates the local user's score on a specific leaderboard
			virtual void UpdateScore(Identifier leaderboardId, int score, EScoreType scoreType, bool bForce = false) = 0;
		};
	}
}