#pragma once

#include "IPlatformStatistics.h"

#include <np.h>

namespace Cry
{
	namespace GamePlatform
	{
		namespace PSN
		{
			class CStatistics final : public IStatistics
			{
			public:
				virtual ~CStatistics();

				// IPlatformStatistics
				virtual void AddListener(IListener& listener) override {}
				virtual void RemoveListener(IListener& listener) override {}

				virtual bool ResetStatistics() override { CRY_ASSERT_MESSAGE(false, "Not supported by PSN!"); return false; }
				virtual bool ResetAchievements() override { CRY_ASSERT_MESSAGE(false, "Not supported by PSN!"); return false; }

				virtual bool Download() override { CRY_ASSERT_MESSAGE(false, "Not supported by PSN!"); return false; }
				virtual bool Upload() override { CRY_ASSERT_MESSAGE(false, "Not supported by PSN!"); return false; }

				virtual bool Set(const char* name, int value) override { CRY_ASSERT_MESSAGE(false, "Not supported by PSN!"); return false; }
				virtual bool Set(const char* name, float value) override { CRY_ASSERT_MESSAGE(false, "Not supported by PSN!"); return false; }

				virtual bool Get(const char* name, int& value) const override { CRY_ASSERT_MESSAGE(false, "Not supported by PSN!"); return false; }
				virtual bool Get(const char* name, float& value) const override { CRY_ASSERT_MESSAGE(false, "Not supported by PSN!"); return false; }

				virtual IAchievement* GetAchievement(const char* szName) override { CRY_ASSERT_MESSAGE(false, "Retrieving achievements by name is not supported by PSN!"); return nullptr; }
				virtual IAchievement* GetAchievement(int id) override;
				// ~IPlatformStatistics

				void Initialize();

			private:
				void ParseTrophies();

			private:
				std::vector<std::unique_ptr<IAchievement>> m_achievements;

				SceNpTrophyContext m_trophyContext;
				SceNpTrophyGameDetails m_trophyGameDetails;
				SceNpTrophyGameData m_trophyGameData;
			};
		}
	}
}