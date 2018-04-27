#pragma once

#include "IPlatformRemoteStorage.h"

namespace Cry
{
	namespace GamePlatform
	{
		namespace Steam
		{
			class CRemoteFile
				: public IRemoteFile
			{
			public:
				CRemoteFile(const char* name);
				virtual ~CRemoteFile() = default;

				// IRemoteFile
				virtual void AddListener(IListener& listener) override { m_listeners.push_back(&listener); }
				virtual void RemoveListener(IListener& listener) override { stl::find_and_erase(m_listeners, &listener); }

				virtual const char* GetName() const override { return m_name.c_str(); }
				virtual int GetFileSize() const override;

				virtual bool Exists() const override;
				virtual bool Write(const char* pData, int length) override;
				virtual bool Read(std::vector<char>& bufferOut) override;

				virtual bool Delete() override;

				virtual bool Share() override;
				virtual ISharedRemoteFile::Identifier GetSharedIdentifier() const override { return m_sharedHandle; }
				// ~IRemoteFile

			private:
				void OnFileShared(RemoteStorageFileShareResult_t* pResult, bool bIOFailure);
				CCallResult<CRemoteFile, RemoteStorageFileShareResult_t> m_callResultFileShared;

			protected:
				std::vector<IListener*> m_listeners;
				string m_name;

				ISharedRemoteFile::Identifier m_sharedHandle;
			};
		}
	}
}