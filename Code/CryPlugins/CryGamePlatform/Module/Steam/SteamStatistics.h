#pragma once

#include "IPlatformStatistics.h"

namespace Cry
{
	namespace GamePlatform
	{
		namespace Steam
		{
			class CStatistics
				: public IStatistics
			{
			public:
				CStatistics();
				virtual ~CStatistics();

				// IStatistics
				virtual void AddListener(IListener& listener) override { m_listeners.push_back(&listener); }
				virtual void RemoveListener(IListener& listener) override { stl::find_and_erase(m_listeners, &listener); }

				virtual bool ResetStatistics() override;
				virtual bool ResetAchievements() override;

				virtual bool Download() override;
				virtual bool Upload() override;

				virtual bool Set(const char* name, int value) override;
				virtual bool Set(const char* name, float value) override;

				virtual bool Get(const char* name, int &value) const override;
				virtual bool Get(const char* name, float &value) const override;

				virtual IAchievement* GetAchievement(int identifier) override { CRY_ASSERT_MESSAGE(false, "Steam only provides support for retrieving achievements by name!"); return nullptr; }
				virtual IAchievement* GetAchievement(const char* szName) override;
				// ~IStatistics

			protected:
				IAchievement* TryGetAchievement(const char* name, bool bAchieved);

				STEAM_CALLBACK(CStatistics, OnUserStatsReceived, UserStatsReceived_t, m_callbackUserStatsReceived);
				STEAM_CALLBACK(CStatistics, OnUserStatsStored, UserStatsStored_t, m_callbackUserStatsStored);
				STEAM_CALLBACK(CStatistics, OnStatsUnloaded, UserStatsUnloaded_t, m_callbackStatsUnloaded);
				STEAM_CALLBACK(CStatistics, OnAchievementStored, UserAchievementStored_t, m_callbackAchievementStored);

				uint32 m_appId;
				bool m_bInitialized;
				bool m_bRequestedAchievementReset;

				std::vector<std::unique_ptr<IAchievement>> m_achievements;
				std::vector<IListener*> m_listeners;
			};
		}
	}
}