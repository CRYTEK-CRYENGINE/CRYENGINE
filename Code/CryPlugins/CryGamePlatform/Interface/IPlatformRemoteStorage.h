#pragma once

#include <CrySystem/ISystem.h>
#include <CrySystem/File/ICryPak.h>

namespace Cry
{
	namespace GamePlatform
	{
		struct IRemoteFile;
		struct ISharedRemoteFile;

		struct IUserGeneratedContentManager;

		//! Interface to a file available on the game platform's servers
		struct IRemoteFileBase
		{
			//! Provides callbacks for remote file events
			struct IListener
			{
				virtual bool OnFileShared(IRemoteFile* pFile) = 0;
				virtual bool OnDownloadedSharedFile(ISharedRemoteFile* pFile) = 0;
			};

			//! Adds a listener to remote file events
			virtual void AddListener(IListener& listener) = 0;
			//! Removes a listener to remote file events
			virtual void RemoveListener(IListener& listener) = 0;
			//! Reads the data contained in the file
			//! \param bufferOut Vector to which the data contained in the file will be read to
			//! \returns True if the read was successful, otherwise false
			virtual bool Read(std::vector<char>& bufferOut) = 0;
			
			//! Retrieves the size of the remote file
			virtual int GetFileSize() const = 0;

			//! Helper function to read the remote file's contents to a local file on disk
			bool ReadToFile(const char* szPath)
			{
				std::vector<char> data;
				if (!Read(data))
					return false;

				FILE* f = gEnv->pCryPak->FOpen(szPath, "wb");
				bool bSuccess = f != nullptr;

				if (bSuccess)
				{
					gEnv->pCryPak->FWrite(data.data(), data.size(), f);
					gEnv->pCryPak->FClose(f);
				}

				return bSuccess;
			}
		};

		//! Interface to a remote file that was shared to us by another user
		struct ISharedRemoteFile : public IRemoteFileBase
		{
			//! Unique identifier of a shared file
			using Identifier = uint64;

			//! Starts downloading the shared file, will result in IListener::OnDownloadedSharedFile being called on completion
			//! \param downloadPriority Specifies the priority of the download, a value of 0 being immediate - otherwise the lowest value is prioritized
			virtual void Download(int downloadPriority) = 0;
			//! Gets the unique identifier of this shared file
			virtual Identifier GetIdentifier() const = 0;
		};

		//! Interface to a remote file owned by the local user
		struct IRemoteFile : public IRemoteFileBase
		{
			virtual ~IRemoteFile() {}

			//! Retrieves the name of the remote file
			virtual const char* GetName() const = 0;

			//! Checks whether the remote file exists on the remote server
			virtual bool Exists() const = 0;
			//! Writes the specified data to the remote file
			virtual bool Write(const char* pData, int length) = 0;
			//! Helper function to write the contents of a local file on disk to the remote file
			bool WriteFromFile(const char* path)
			{
				FILE* f = gEnv->pCryPak->FOpen(path, "rb");
				if (f == nullptr)
					return false;

				std::vector<char> data;
				data.resize(gEnv->pCryPak->FGetSize(f));

				gEnv->pCryPak->FRead(data.data(), data.size(), f);
				gEnv->pCryPak->FClose(f);

				return Write(data.data(), data.size());
			}

			//! Deletes the remote file
			virtual bool Delete() = 0;
			//! Commences an asynchronous operation to share the remote file with other users
			//! IListener::OnFileShared will be called on success
			virtual bool Share() = 0;
			//! Gets the shared file identifier, if the file was shared
			//! This can be shared with other users and combined with IRemoteStorage::GetSharedFile to get data from other users
			virtual ISharedRemoteFile::Identifier GetSharedIdentifier() const = 0;
		};

		//! Interface to the remote storage on the platform's servers
		struct IRemoteStorage
		{
			virtual ~IRemoteStorage() {}

			//! Checks whether remote storage is available
			virtual bool IsEnabled() const = 0;
			//! Gets the UGC manager, allowing for direct creation and upload of local files
			virtual IUserGeneratedContentManager* GetUserGeneratedContentManager() const = 0;
			//! Gets remote file by name, even if it does not exist on the remote server yet
			virtual std::shared_ptr<IRemoteFile> GetFile(const char* name) = 0;
			//! Gets a file shared with us by another user, allowing for sharing of data over the network without direct connections
			virtual std::shared_ptr<ISharedRemoteFile> GetSharedFile(ISharedRemoteFile::Identifier id) = 0;
		};
	}
}