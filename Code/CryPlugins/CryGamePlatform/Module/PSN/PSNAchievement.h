#pragma once

#include "IPlatformAchievement.h"

#include <np.h>

namespace Cry
{
	namespace GamePlatform
	{
		namespace PSN
		{
			class CAchievement final : public IAchievement
			{
			public:
				CAchievement(SceNpTrophyContext* pContext, SceNpTrophyId id, SceNpTrophyDetails trophyDetails, SceNpTrophyData trophyData);
				virtual ~CAchievement() = default;

				// IAchievement
				virtual const char* GetName() const override { return m_details.name; }
				virtual bool IsCompleted() const override { return m_data.unlocked; }

				virtual bool Reset() override { CRY_ASSERT_MESSAGE(false, "The method or operation is not implemented."); return false; }
				virtual bool Achieve() override;
				// ~IAchievement

				SceNpTrophyId GetIdentifier() const { return m_id; }

			protected:
				SceNpTrophyId m_id;
				SceNpTrophyDetails m_details;
				SceNpTrophyData m_data;
				SceNpTrophyContext* m_pContext;
			};
		}
	}
}