// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AssetLoader.h"
#include "AssetLoaderHelpers.h"
#include "AssetSystem/Asset.h"
#include "FileReader.h"
#include "CompletionPortFileReader.h"
#include "Metadata.h"
#include "FilePathUtil.h"
#include <CrySystem/ISystem.h>
#include <Cry3DEngine/I3DEngine.h>
#include <Cry3DEngine/CGF/IChunkFile.h>  
#include <CryString/CryPath.h>
#include <CrySystem/IProjectManager.h>
#include <QtUtil.h>
#include <QDirIterator> 
#include <condition_variable>
#include <unordered_map>

namespace Loader_Private
{
	template <typename T>
	class CBlockingCollection
	{
	public:
		typedef std::deque<T> UnderlyingContainer;

	public:
		void Push(const T& value)
		{
			{
				std::unique_lock<std::mutex> lock(m_mutex);
				assert(!m_bCompleted);
				m_container.push_front(value);
			}
			m_condition.notify_one();
		}

		void Complete()
		{
			{
				std::unique_lock<std::mutex> lock(m_mutex);
				m_bCompleted = true;
			}
			m_condition.notify_one();
		}

		bool Pop(T& value)
		{
			std::unique_lock<std::mutex> lock(m_mutex);
			m_condition.wait(lock, [this]() { return !m_container.empty() || IsCompleted(); });
			if (IsCompleted())
			{
				return false;
			}
			value = std::move(m_container.back());
			m_container.pop_back();
			return true;
		}

	private:
		bool IsCompleted() const { return m_bCompleted && m_container.empty(); };

	private:
		std::mutex              m_mutex;
		std::condition_variable m_condition;
		UnderlyingContainer		m_container;
		bool					m_bCompleted = false;
	};

	uint64 GetTimestamp(const char* szAssetFilepath)
	{
		WIN32_FIND_DATAA FindFileData;
		HANDLE hFind = FindFirstFileA(szAssetFilepath, &FindFileData);
		if (hFind == INVALID_HANDLE_VALUE)
		{
			return 0;
		}
		FindClose(hFind);

		ULARGE_INTEGER time = { FindFileData.ftLastWriteTime.dwLowDateTime, FindFileData.ftLastWriteTime.dwHighDateTime };
		return time.QuadPart;
	}

	static void Load(
		CBlockingCollection<CAsset*>& items, 
		const string& assetsRootPath, 
		const string& alias,
		bool bSingleThread,
		AssetLoader::Predicate& predicate)
	{
		struct SAssetInfo
		{
			const string path;
			const uint64 timestamp;
		};

		std::unique_ptr<AssetLoader::IFileReader> pReader(bSingleThread ? AssetLoader::CreateFileReader() : AssetLoader::CreateCompletionPortFileReader());

		// A handler for consuming the result of [asynchronous] request. 
		const auto xmlCompletionHandler = [&items](const char* pBuffer, size_t numberOfBytes, void* pUserData) -> void
		{
			std::unique_ptr<SAssetInfo> pAssetInfo = std::unique_ptr<SAssetInfo>(static_cast<SAssetInfo*>(pUserData));

			CAsset* pAsset = AssetLoader::CAssetFactory::LoadAssetFromInMemoryXmlFile(pAssetInfo->path, pAssetInfo->timestamp, pBuffer, numberOfBytes);
			if (!pAsset)
			{
				CryWarning(EValidatorModule::VALIDATOR_MODULE_EDITOR, EValidatorSeverity::VALIDATOR_ERROR, "Unable to load asset: \"%s\".", pAssetInfo->path.c_str());
				return;
			}
			items.Push(pAsset);
		};

		const size_t offsetToRelativePath = assetsRootPath.size() + 1;

		QDirIterator iterator(QtUtil::ToQString(assetsRootPath.c_str()), QStringList() << "*.cryasset", QDir::Files, QDirIterator::Subdirectories);
		while (iterator.hasNext())
		{
			const string filePath = QtUtil::ToString(iterator.next());
			const char* szRelativeFilePath = filePath.c_str() + offsetToRelativePath;
			const uint64 timestamp = GetTimestamp(filePath);

			if (!predicate(szRelativeFilePath, timestamp))
			{
				continue;
			}

			std::unique_ptr<SAssetInfo> pAssetInfo = std::unique_ptr<SAssetInfo>(new SAssetInfo{ alias.empty() ? szRelativeFilePath : PathUtil::Make(alias, szRelativeFilePath), timestamp });
			if (!pReader->ReadFileAsync(filePath, xmlCompletionHandler, pAssetInfo.get()))
			{
				CryWarning(EValidatorModule::VALIDATOR_MODULE_EDITOR, EValidatorSeverity::VALIDATOR_ERROR, "Failed to read asset file: \"%s\".", filePath.c_str());
				continue;
			}
			pAssetInfo.release();
		}
		pReader->WaitForCompletion();
	};


	template<typename F>
	void EnumerateAssetFiles(const char* szAssetsRootPath, const bool bPakFilesOnly,  F f)
	{
		std::stack<string> stack;
		stack.push(PathUtil::RemoveSlash(szAssetsRootPath));

		ICryPak* const pPak = GetISystem()->GetIPak();

		// We want to keep aliases, e.g. %engine%/textures/a.dds
		const char* szAlias = pPak->GetAlias(szAssetsRootPath, false);
		const size_t offsetToRelativePath = szAlias ? 0 : stack.top().size() + 1;

		while (stack.size())
		{
			// Needs an optimized solution to iterate .cryassets and directories only.
			const string mask = PathUtil::Make(stack.top(), "*");
			const string path = stack.top();
			stack.pop();

			_finddata_t fd;

			intptr_t handle = pPak->FindFirst(mask.c_str(), &fd);
			if (handle != -1)
			{
				do
				{
					if (!strcmp(fd.name, ".") || !strcmp(fd.name, ".."))
						continue;

					if (bPakFilesOnly && ((fd.attrib & _A_IN_CRYPAK) == 0))
					{
						continue;
					}

					// Iterate sub-directories.
					if ((fd.attrib & _A_SUBDIR) != 0)
					{
						stack.push(PathUtil::Make(path.c_str(), fd.name));
						continue;
					}

					if (stricmp(PathUtil::GetExt(fd.name), "cryasset") != 0)
					{
						continue;
					}

					const string entryParth = PathUtil::Make(path, fd.name);
					f(entryParth.c_str() + offsetToRelativePath, fd.time_write);

				} while (pPak->FindNext(handle, &fd) >= 0);
				pPak->FindClose(handle);
			}
		}
	}

	// Iterates OS files only.
	std::vector<CAsset*> LoadAssetsOsFileSystem(const std::vector<string>& assetRootPaths, AssetLoader::Predicate& predicate)
	{
		using namespace AssetLoader;

		// Producer-consumer pattern.
		CBlockingCollection<CAsset*> items;

		struct SRoot
		{
			string path;
			string alias;
		};

		// get absolute paths
		std::vector<SRoot> roots;
		std::transform(assetRootPaths.begin(), assetRootPaths.end(), std::back_inserter(roots), [](const string& path)
		{
			ICryPak* const pPak = GetISystem()->GetIPak();
			const char* szAlias = pPak->GetAlias(path.c_str(), false);
			if (szAlias)
			{
				return SRoot{ szAlias, path };
			}

			if (pPak->IsAbsPath(path.c_str()))
			{
				return SRoot{ path, "" };
			}

			// We expect that the relative path is a child of the current project root.
			return SRoot{ PathUtil::Make(GetISystem()->GetIProjectManager()->GetCurrentProjectDirectoryAbsolute(), path.c_str()), "" };
		});

		std::thread producerThread([&items, &roots, &predicate]()
		{
			const bool bSingleThread = false;
			for (const auto& root : roots)
			{
				Load(items, root.path, root.alias, bSingleThread, predicate);
			}

			// Let consumer know we are done.
			items.Complete();
		});

		std::vector<CAsset*> assets;
		assets.reserve(10000);

		CAsset* pAsset;
		// Consumer. Wait for next or break if done. 
		while (items.Pop(pAsset))
		{
			assets.push_back(pAsset);
		}
		producerThread.join();
		assets.shrink_to_fit();
		return assets;
	}

	// Iterates OS and paks files.
	std::vector<CAsset*> LoadAssetsPakFileSystem(const std::vector<string>& assetRootPaths, AssetLoader::Predicate& predicate)
	{
		using namespace AssetLoader;

		std::vector<char> buffer(1 << 24);
		auto createAsset = [&buffer](const char* szFilename, uint64 timestamp) -> CAsset*
		{
			ICryPak* const pPak = GetISystem()->GetIPak();

			FILE* file = pPak->FOpen(szFilename, "rbx");
			if (!file)
			{
				CryWarning(EValidatorModule::VALIDATOR_MODULE_EDITOR, EValidatorSeverity::VALIDATOR_ERROR, "Failed to open asset file: \"%s\".", szFilename);
				return nullptr;
			}

			buffer.resize(pPak->FGetSize(file));
			const size_t numberOfBytesRead = pPak->FReadRawAll(buffer.data(), buffer.size(), file);

			pPak->FClose(file);

			return AssetLoader::CAssetFactory::LoadAssetFromInMemoryXmlFile(szFilename, timestamp, buffer.data(), numberOfBytesRead);

		};

		std::vector<CAsset*> assets;
		assets.reserve(10000);

		const bool bPakFilesOnly = false;
		for (const auto& root : assetRootPaths)
		{
			EnumerateAssetFiles(root, bPakFilesOnly, [&predicate, &assets, &createAsset](const char* szRelativeFilePath, uint64 timestamp)
			{
				if (!predicate(szRelativeFilePath, timestamp))
				{
					return;
				}

				CAsset* pAsset = createAsset(szRelativeFilePath, timestamp);
				if (!pAsset)
				{
					CryWarning(EValidatorModule::VALIDATOR_MODULE_EDITOR, EValidatorSeverity::VALIDATOR_ERROR, "Unable to load asset: \"%s\".", szRelativeFilePath);
					return;
				}
				assets.push_back(pAsset);
			});
		}

		assets.shrink_to_fit();
		return assets;
	}

}


namespace AssetLoader
{
	std::vector<CAsset*> LoadAssets(const std::vector<string>& assetRootPaths, bool bIgnorePaks, Predicate predicate)
	{
		using namespace Loader_Private;

		// For the moment LoadAssetsOsFileSystem and LoadAssetsPakFileSystem are independent subsystems.
		// It seems that for OS files LoadAssetsOsFileSystem works a bit faster then LoadAssetsPakFileSystem. 
		// TODO : Performance measurement.
		// TODO : Consider a composite solution LoadAssetsOsFileSystem + LoadAssetsPakFileSystem(bPaksOnly).

		if (bIgnorePaks)
		{
			return LoadAssetsOsFileSystem(assetRootPaths, predicate);
		}
		else
		{
			return LoadAssetsPakFileSystem(assetRootPaths, predicate);
		}
	}
}


