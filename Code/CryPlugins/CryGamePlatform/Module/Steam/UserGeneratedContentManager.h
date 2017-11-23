#pragma once

#include "IPlatformUserGeneratedContent.h"

#include <steam/steam_api.h>

namespace Cry
{
	namespace GamePlatform
	{
		namespace Steam
		{
			struct SItemParameters
			{
				string title;
				string desc;
				IUserGeneratedContent::EVisibility visibility;
				std::vector<string> tags;
				string contentFolderPath;
				string previewPath;
			};

			class CUserGeneratedContentManager
				: public IUserGeneratedContentManager
			{
			public:
				CUserGeneratedContentManager() = default;
				virtual ~CUserGeneratedContentManager() = default;

				// IUserGeneratedContentManager
				virtual void AddListener(IListener& listener) override { m_listeners.push_back(&listener); }
				virtual void RemoveListener(IListener& listener) override { stl::find_and_erase(m_listeners, &listener); }

				virtual void Create(unsigned int appId, IUserGeneratedContent::EType type) override;

				virtual void CreateDirect(unsigned int appId, IUserGeneratedContent::EType type,
					const char* title, const char* desc, IUserGeneratedContent::EVisibility visibility,
					const char* *pTags, int numTags, const char* contentFolderPath, const char* previewPath) override;
				// ~IUserGeneratedContentManager

			protected:
				std::vector<IListener*> m_listeners;

				void OnContentCreated(CreateItemResult_t* pResult, bool bIOFailure);
				CCallResult<CUserGeneratedContentManager, CreateItemResult_t> m_callResultContentCreated;

				std::unique_ptr<SItemParameters> m_pWaitingParameters;

				ApplicationIdentifier m_lastUsedId = 0;
				std::vector<std::unique_ptr<IUserGeneratedContent>> m_content;
			};
		}
	}
}