#include "StdAfx.h"
#include "PSNAchievement.h"

namespace Cry
{
	namespace GamePlatform
	{
		namespace PSN
		{
			CAchievement::CAchievement(SceNpTrophyContext* pContext, SceNpTrophyId id, SceNpTrophyDetails trophyDetails, SceNpTrophyData trophyData)
				: m_pContext(pContext)
				, m_id(id)
				, m_details(trophyDetails)
				, m_data(trophyData)
			{

			}

			bool CAchievement::Achieve()
			{
				CRY_ASSERT(m_pContext != nullptr);

				SceNpTrophyId platinumId = SCE_NP_TROPHY_INVALID_TROPHY_ID;
				SceNpTrophyHandle handle;

				int retValue = SCE_OK;

				retValue = sceNpTrophyCreateHandle(&handle);
				if (retValue != SCE_OK)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[PSN] Failed to create trophy handle: %x\n", retValue);
					return false;
				}

				retValue = sceNpTrophyUnlockTrophy(*m_pContext, handle, m_id, &platinumId);
				if (retValue != SCE_OK)
				{
					if (retValue != SCE_NP_TROPHY_ERROR_TROPHY_ALREADY_UNLOCKED && retValue != SCE_NP_TROPHY_ERROR_PLATINUM_CANNOT_UNLOCK)
					{
						CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[PSN] Failed to unlock trophy: %x\n", retValue);
						return false;
					}
				}

				if (platinumId != SCE_NP_TROPHY_INVALID_TROPHY_ID)
				{
					// TODO: implement extra processing for platinum trophy
				}

				retValue = sceNpTrophyDestroyHandle(handle);
				if (retValue != SCE_OK)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[PSN] Failed to destroy trophy handle: %x\n", retValue);
				}

				return true;
			}
		}
	}
}