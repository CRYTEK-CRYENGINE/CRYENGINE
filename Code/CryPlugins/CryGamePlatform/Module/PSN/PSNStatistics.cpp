#include "StdAfx.h"

#include "PSNStatistics.h"
#include "PSNAchievement.h"

#include <np.h>

namespace Cry
{
	namespace GamePlatform
	{
		namespace PSN
		{
			CStatistics::~CStatistics()
			{
				int retVal = SCE_OK;

				retVal = sceNpTrophyDestroyContext(m_trophyContext);
				if (retVal != SCE_OK)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[PSN] Failed to destroy trophy context: %x\n", retVal);
				}
			}

			void CStatistics::Initialize()
			{
				CUser* pLocalUser = static_cast<CUser*>(CPlugin::GetInstance()->GetLocalClient());

				int retVal = SCE_OK;

				retVal = sceSysmoduleLoadModule(SCE_SYSMODULE_NP_TROPHY);
				if (retVal < SCE_OK)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[PSN] Failed to initialize Trophy module: %x\n", retVal);
					return;
				}

				retVal = sceNpTrophyCreateContext(&m_trophyContext, pLocalUser->GetUserServiceUserId(), 0, 0);
				if (retVal != SCE_OK)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[PSN] Failed to create trophy context (Init): %x\n", retVal);
					return;
				}

				SceNpTrophyHandle trophyHandle;
				retVal = sceNpTrophyCreateHandle(&trophyHandle);
				if (retVal != SCE_OK)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[PSN] Failed to create trophy handle (Init): %x\n", retVal);
					return;
				}

				retVal = sceNpTrophyRegisterContext(m_trophyContext, trophyHandle, 0);
				if (retVal != SCE_OK)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[PSN] Failed to register trophy context (Init): %x\n", retVal);
					return;
				}

				memset(&m_trophyGameDetails, 0x00, sizeof(m_trophyGameDetails));
				memset(&m_trophyGameData, 0x00, sizeof(m_trophyGameData));

				m_trophyGameDetails.size = sizeof(m_trophyGameDetails);
				m_trophyGameData.size = sizeof(m_trophyGameData);

				retVal = sceNpTrophyGetGameInfo(m_trophyContext, trophyHandle, &m_trophyGameDetails, &m_trophyGameData);
				if (retVal != SCE_OK)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[PSN] Failed to get trophy game info (Init): %x\n", retVal);
					return;
				}

				m_achievements.reserve(m_trophyGameDetails.numTrophies);

				retVal = sceNpTrophyDestroyHandle(trophyHandle);
				if (retVal != SCE_OK)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[PSN] Failed to destroy trophy handle (Init): %x\n", retVal);
					return;
				}

				ParseTrophies();
			}

			void CStatistics::ParseTrophies()
			{
				SceNpTrophyId trophyId;
				SceNpTrophyDetails trophyDetails;
				SceNpTrophyData trophyData;

				int retValue = SCE_OK;

				SceNpTrophyHandle trophyHandle;
				retValue = sceNpTrophyCreateHandle(&trophyHandle);
				if (retValue != SCE_OK)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[PSN] Failed to create trophy handle (ParseTrophies): %x\n", retValue);
				}

				for (int i = 0; i < m_trophyGameDetails.numTrophies; i++)
				{
					trophyId = (SceNpTrophyId)i;

					memset(&trophyDetails, 0x00, sizeof(trophyDetails));
					memset(&trophyData, 0x00, sizeof(trophyData));

					trophyDetails.size = sizeof(trophyDetails);
					trophyData.size = sizeof(trophyData);

					retValue = sceNpTrophyGetTrophyInfo(m_trophyContext, trophyHandle, trophyId, &trophyDetails, &trophyData);
					if (retValue != SCE_OK)
					{
						CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[PSN] Failed to get trophy info (ParseTrophies): %x\n", retValue);
					}
					else
					{
						if (trophyDetails.trophyGrade != SCE_NP_TROPHY_GRADE_PLATINUM && trophyDetails.trophyGrade != SCE_NP_TROPHY_GRADE_UNKNOWN)
						{
							m_achievements.emplace_back(stl::make_unique<CAchievement>(&m_trophyContext, trophyId, trophyDetails, trophyData));
						}
						else if (trophyDetails.trophyGrade == SCE_NP_TROPHY_GRADE_UNKNOWN)
						{
							CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[PSN] Got a corrupt trophy with an unknown grade. (ParseTrophies)");
						}
					}
				}

				retValue = sceNpTrophyDestroyHandle(trophyHandle);
				if (retValue != SCE_OK)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[PSN] Failed to destroy trophy handle (ParseTrophies): %x\n", retValue);
				}
			}

			IAchievement* CStatistics::GetAchievement(int id)
			{
				for (const std::unique_ptr<IAchievement>& pAchievement : m_achievements)
				{
					if (static_cast<CAchievement*>(pAchievement.get())->GetIdentifier() == id)
					{
						return pAchievement.get();
					}
				}

				return nullptr;
			}
		}
	}
}