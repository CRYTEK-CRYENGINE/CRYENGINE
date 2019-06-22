// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "IPlatformAchievement.h"
#include "SteamTypes.h"

namespace Cry
{
	namespace GamePlatform
	{
		namespace Steam
		{
			class CAchievement
				: public IAchievement
			{
			public:
				enum class Type
				{
					Regular,
					Stat,
				};

				CAchievement(CStatistics& steamStats, const char* szName, Type type, int32 minProgress, int32 maxProgress, int32 curProgress);
				virtual ~CAchievement() = default;

				// IAchievement
				virtual const char* GetName() const override { return m_name.c_str(); }
				virtual bool IsCompleted() const override { return Completed(); }
				virtual bool Reset() override;
				virtual bool Achieve() override;
				virtual bool SetProgress(int32 progress) override;
				virtual int32 GetProgress() const override;
				// ~IAchievement

			protected:
				bool Completed() const { return m_curProgress == m_maxProgress; }
				bool SetAchievement(int32 progress);
				bool UnlockAchievement();
				bool LockAchievement();
				bool SetStat(int32 progress);

				CStatistics& m_stats;
				const string m_name;
				const Type m_type;
				const int32 m_minProgress;
				const int32 m_maxProgress;
				int32 m_curProgress;
			};
		}
	}
}
