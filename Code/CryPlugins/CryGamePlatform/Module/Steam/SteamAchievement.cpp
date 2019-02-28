#include "StdAfx.h"

#include <steam/steam_api.h>

#include "SteamPlatform.h"
#include "SteamAchievement.h"

namespace Cry
{
	namespace GamePlatform
	{
		namespace Steam
		{
			CAchivement::CAchivement(const char* name, bool bAchieved)
				: m_name(name)
				, m_bAchieved(bAchieved)
			{

			}

			bool CAchivement::Reset()
			{
				ISteamUserStats* pSteamUserStats = SteamUserStats();
				if (!pSteamUserStats)
				{
					return false;
				}
				return pSteamUserStats->ClearAchievement(m_name);
			}

			bool CAchivement::Achieve()
			{
				ISteamUserStats* pSteamUserStats = SteamUserStats();
				if (!pSteamUserStats)
				{
					return false;
				}
				bool bResult = pSteamUserStats->SetAchievement(m_name);

				CPlugin::GetInstance()->GetStatistics()->Upload();

				return bResult;
			}
		}
	}
}