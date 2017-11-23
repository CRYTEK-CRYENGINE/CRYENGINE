#pragma once

#include "IPlatformRemoteStorage.h"

namespace Cry
{
	namespace GamePlatform
	{
		namespace Steam
		{
			class CRemoteStorage
				: public IRemoteStorage
			{
			public:
				CRemoteStorage();
				virtual ~CRemoteStorage() = default;

				// IRemoteStorage
				virtual bool IsEnabled() const override;

				virtual IUserGeneratedContentManager* GetUserGeneratedContentManager() const override { return m_pUGCManager.get(); }

				virtual std::shared_ptr<IRemoteFile> GetFile(const char* name) override;
				virtual std::shared_ptr<ISharedRemoteFile> GetSharedFile(ISharedRemoteFile::Identifier id) override;
				// ~IRemoteStorage

			protected:
				std::unique_ptr<IUserGeneratedContentManager> m_pUGCManager;
			};
		}
	}
}