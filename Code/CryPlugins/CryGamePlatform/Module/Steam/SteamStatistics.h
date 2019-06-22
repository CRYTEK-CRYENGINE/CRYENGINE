// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "IPlatformStatistics.h"
#include "SteamTypes.h"

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
				explicit CStatistics(CService& steamService);
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

				virtual IAchievement* GetAchievement(const char* szName, const SAchievementDetails* pDetails) override;
				virtual IAchievement* GetAchievement(int id, const SAchievementDetails* pDetails) override;
				
				// ~IStatistics

			protected:
				IAchievement* GetAchievementInternal(const char* szName);
				IAchievement* GetOrCreateAchievement(const char* szName, const SAchievementDetails* pDetails);

				STEAM_CALLBACK(CStatistics, OnUserStatsReceived, UserStatsReceived_t, m_callbackUserStatsReceived);
				STEAM_CALLBACK(CStatistics, OnUserStatsStored, UserStatsStored_t, m_callbackUserStatsStored);
				STEAM_CALLBACK(CStatistics, OnStatsUnloaded, UserStatsUnloaded_t, m_callbackStatsUnloaded);
				STEAM_CALLBACK(CStatistics, OnAchievementStored, UserAchievementStored_t, m_callbackAchievementStored);

				CService& m_service;

				uint32 m_appId;
				bool m_bInitialized;
				bool m_bRequestedAchievementReset;

				std::vector<std::unique_ptr<IAchievement>> m_achievements;
				std::vector<IListener*> m_listeners;
			};
		}
	}
}