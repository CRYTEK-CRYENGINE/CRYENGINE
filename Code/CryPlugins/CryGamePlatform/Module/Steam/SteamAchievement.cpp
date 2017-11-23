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
				return SteamUserStats()->ClearAchievement(m_name);
			}

			bool CAchivement::Achieve()
			{
				bool bResult = SteamUserStats()->SetAchievement(m_name);

				CPlugin::GetInstance()->GetStatistics()->Upload();

				return bResult;
			}
		}
	}
}