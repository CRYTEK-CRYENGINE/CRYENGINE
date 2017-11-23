#include "StdAfx.h"

#include "UserGeneratedContent.h"

#include "SteamPlatform.h"

namespace Cry
{
	namespace GamePlatform
	{
		namespace Steam
		{
			CUserGeneratedContent::CUserGeneratedContent(ApplicationIdentifier appId, IUserGeneratedContent::Identifier id)
				: m_appId(appId)
				, m_id(id)
				, m_updateHandle(0)
			{
			}

			void CUserGeneratedContent::StartPropertyUpdate()
			{
				if (m_updateHandle != 0)
					return;

				m_updateHandle = SteamUGC()->StartItemUpdate(m_appId, m_id);
			}

			bool CUserGeneratedContent::SetTitle(const char* newTitle)
			{
				StartPropertyUpdate();
				return SteamUGC()->SetItemTitle(m_updateHandle, newTitle);
			}

			bool CUserGeneratedContent::SetDescription(const char* newDescription)
			{
				StartPropertyUpdate();
				return SteamUGC()->SetItemDescription(m_updateHandle, newDescription);
			}

			bool CUserGeneratedContent::SetVisibility(IUserGeneratedContent::EVisibility visibility)
			{
				StartPropertyUpdate();
				return SteamUGC()->SetItemVisibility(m_updateHandle, (ERemoteStoragePublishedFileVisibility)visibility);
			}

			bool CUserGeneratedContent::SetTags(const char* *pTags, int numTags)
			{
				StartPropertyUpdate();

				SteamParamStringArray_t stringArray;
				stringArray.m_ppStrings = pTags;
				stringArray.m_nNumStrings = numTags;

				return SteamUGC()->SetItemTags(m_updateHandle, &stringArray);
			}

			bool CUserGeneratedContent::SetContent(const char* contentFolderPath)
			{
				StartPropertyUpdate();
				return SteamUGC()->SetItemContent(m_updateHandle, contentFolderPath);
			}

			bool CUserGeneratedContent::SetPreview(const char* previewFilePath)
			{
				StartPropertyUpdate();
				return SteamUGC()->SetItemPreview(m_updateHandle, previewFilePath);
			}

			void CUserGeneratedContent::CommitChanges(const char* changeNote)
			{
				if (m_updateHandle == 0)
					return;

				SteamAPICall_t result = SteamUGC()->SubmitItemUpdate(m_updateHandle, changeNote);;
				m_callResultContentUpdated.Set(result, this, &CUserGeneratedContent::OnContentUpdated);

				CPlugin::GetInstance()->SetAwaitingCallback(1);

				m_updateHandle = 0;
			}

			void CUserGeneratedContent::OnContentUpdated(SubmitItemUpdateResult_t* pResult, bool bIOError)
			{
				switch (pResult->m_eResult)
				{
					case k_EResultOK:
						break;
					case k_EResultFileNotFound:
					{
						CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "[Steam] File not found!");
					}
					break;
					default:
					{
						CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "[Steam] Failed to update UGC asset!");
					}
					break;
				}

				CPlugin::GetInstance()->SetAwaitingCallback(-1);
			}
		}
	}
}