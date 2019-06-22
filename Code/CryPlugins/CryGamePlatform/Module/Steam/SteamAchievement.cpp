// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "SteamAchievement.h"
#include "SteamStatistics.h"

namespace Cry
{
	namespace GamePlatform
	{
		namespace Steam
		{
			CAchievement::CAchievement(CStatistics& steamStats, const char* szName, Type type, int32 minProgress, int32 maxProgress, int32 curProgress)
				: m_stats(steamStats)
				, m_name(szName)
				, m_type(type)
				, m_minProgress(minProgress)
				, m_maxProgress(maxProgress)
				, m_curProgress(curProgress)
			{
			}

			bool CAchievement::Reset()
			{
				if (Completed())
				{
					switch (m_type)
					{
					case Type::Regular:
					{
						return LockAchievement();
					}
					case Type::Stat:
					{
						return SetStat(m_minProgress);
					}
					}
				}
				return false;
			}

			bool CAchievement::Achieve()
			{
				if (!Completed())
				{
					switch (m_type)
					{
					case Type::Regular:
					{
						return UnlockAchievement();
					}
					case Type::Stat:
					{
						return SetStat(m_maxProgress);
					}
					}
				}
				return false;
			}

			bool CAchievement::SetProgress(int32 progress)
			{
				progress = crymath::clamp(progress, m_minProgress, m_maxProgress);
				if (progress != m_curProgress)
				{
					switch (m_type)
					{
					case Type::Regular:
					{
						return SetAchievement(progress);
					}
					case Type::Stat:
					{
						return SetStat(progress);
					}
					}
				}
				return false;
			}

			int32 CAchievement::GetProgress() const
			{
				return m_curProgress;
			}

			bool CAchievement::SetAchievement(int32 progress)
			{
				if (progress == m_maxProgress)
				{
					return UnlockAchievement();
				}
				else
				{
					return LockAchievement();
				}
			}

			bool CAchievement::UnlockAchievement()
			{
				bool success = false;
				if (ISteamUserStats* pSteamUserStats = SteamUserStats())
				{
					if (success = pSteamUserStats->SetAchievement(m_name.c_str()))
					{
						if (success = pSteamUserStats->StoreStats())
						{
							m_curProgress = m_maxProgress;
						}
					}
				}
				return success;
			}

			bool CAchievement::LockAchievement()
			{
				bool success = false;
				if (ISteamUserStats* pSteamUserStats = SteamUserStats())
				{
					if (success = pSteamUserStats->ClearAchievement(m_name.c_str()))
					{
						if (success = pSteamUserStats->StoreStats())
						{
							m_curProgress = m_minProgress;
						}
					}
				}
				return success;
			}

			bool CAchievement::SetStat(int32 progress)
			{
				// If a steam stat, that is linked to a steam achievement, reaches its max value, then Steam will automatically unlock the achievement
				// However: resetting a steam stat will not lock the linked steam achievement again
				if (m_stats.Set(m_name.c_str(), progress))
				{
					m_curProgress = progress;
					return true;
				}

				return false;
			}
		}
	}
}
