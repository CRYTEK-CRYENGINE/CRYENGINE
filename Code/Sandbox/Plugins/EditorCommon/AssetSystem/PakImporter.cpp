// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PakImporter.h"
#include "AssetManagerHelpers.h"
#include "Loader/AssetLoaderHelpers.h"
#include "AssetSystem/AssetManager.h"
#include "Notifications/NotificationCenter.h"
#include "FilePathUtil.h"
#include "ThreadingUtils.h"
#include "QtUtil.h"

#include <CrySystem/File/ICryPak.h>
#include <CryString/CryPath.h>

namespace AssetManagerHelpers
{

void CPakImporter::RegisterFileListener()
{
	static CPakImporter theInstance;
}

// IFileChangeListener implementation.
void CPakImporter::OnFileChange(const char* szFilename, EChangeType changeType)
{
	// Ignore events for files outside of the current game folder.
	if (GetISystem()->GetIPak()->IsAbsPath(szFilename))
	{
		return;
	}

	if (strstri(szFilename, ".cryasset.pak"))
	{
		return;
	}

	// Supports paks only in the game root
	if (szFilename != PathUtil::GetFile(szFilename))
	{
		return;
	}

	// Ignore level's packs
	if (strstri(szFilename, "terraintexture.pak") || strstri(szFilename, "level.pak"))
	{
		return;
	}

	if (changeType != IFileChangeListener::eChangeType_Created
		&& changeType != IFileChangeListener::eChangeType_RenamedNewName
		&& changeType != IFileChangeListener::eChangeType_Modified)
	{
		return;
	}

	const bool isQueuedUp = std::any_of(m_queue.cbegin(), m_queue.cend(), [szFilename](const string& pak)
	{
		return pak.CompareNoCase(szFilename) == 0;
	});

	if (isQueuedUp)
	{
		return;
	}

	if (m_queue.empty())
	{
		m_queue.emplace_front(szFilename);
		ProcessQueue();
	}
	else // When the queue is not empty, a previous call to ProcessQueue() has already triggered the processing.
	{
		m_queue.emplace_front(szFilename);
	}
}

void CPakImporter::ProcessQueue()
{
	if (m_queue.empty())
	{
		return;
	}

	const string pak = m_queue.back();
	m_queue.pop_back();

	if (!TryToProcessPak(pak))
	{
		// Try it again.
		m_queue.push_front(pak);
	}

	if (!m_queue.empty())
	{
		const int timeout = (pak == m_queue.back()) ? 500 : 0;
		QTimer::singleShot(500, [this]()
		{
			ProcessQueue();
		});
	}
}

bool CPakImporter::TryToProcessPak(const string& pak)
{
	const string pakPath(PathUtil::Make(PathUtil::GetGameFolder(), pak));

	if (IsPakOpened(pakPath))
	{
		// Ignore the pak.
		return true;
	}

	// Try again later if the pak file is still being written.
	if (IsFileOpened(pak.c_str()))
	{
		return false;
	}

	if (!GetISystem()->GetIPak()->OpenPack(PathUtil::GetGameFolder(), pakPath))
	{
		// Do not try to import the pak again.
		return true;
	}

	ThreadingUtils::AsyncQueue([pak]()
	{
		ImportAssets(pak);
	});
	return true;
}

// pakPath should be relative to the project root.
bool CPakImporter::IsPakOpened(const string& pakPath)
{
	typedef std::unique_ptr<ICryPak::PakInfo, void(*)(ICryPak::PakInfo*)> PakInfoPtr;
	PakInfoPtr pPakInfo(GetISystem()->GetIPak()->GetPakInfo(), [](ICryPak::PakInfo* p)
	{
		GetISystem()->GetIPak()->FreePakInfo(p);
	});

	for (size_t i = 0, n = pPakInfo->numOpenPaks; i < n; ++i)
	{
		const string path(PathUtil::ToUnixPath(pPakInfo->arrPaks[i].szFilePath));
		if (stricmp(path.c_str(), pakPath.c_str()) == 0)
		{
			return true;
		}
	}
	return false;
}

void CPakImporter::ImportAssets(const string& pak)
{
	// 1. Unpak <archive> to a temp folder.
	// 2. Generate cryasset files.
	// 3. pak cryassets to the <archive>.cryasset.pak
	// 4. Load assets from the newly created cryasset.pak.

	float progress = 0;
	CProgressNotification notif(QObject::tr("Importing assets"), QtUtil::ToQString(pak), true);
	notif.SetProgress(progress);

	const string pakPath(PathUtil::Make(PathUtil::GetGameFolder(), pak));
	const string tempRoot = PathUtil::Make(GetTemporaryDirectoryPath(), pak);

	Unpak(pakPath, tempRoot, [&notif, &progress](float v)
	{
		progress = std::min(0.4f, 0.1f + v * 0.4f);
		notif.SetProgress(progress);
	});

	GenerateCryassets(tempRoot.c_str(), [&notif, &progress](float v)
	{
		progress = std::min(0.8f, 0.4f + v * 0.4f);
		notif.SetProgress(progress);
	});

	RCLogger rcLogger;
	const string pakDirAbsPath(PathUtil::Make(PathUtil::GetGameProjectAssetsPath(), PathUtil::GetDirectory(pak)));
	CResourceCompilerHelper::CallResourceCompiler(
		PathUtil::Make(tempRoot.c_str(), "*.cryasset").c_str(),
		string().Format("/zip=%s.cryasset.pak /zip_compression=0", PathUtil::Make(pakDirAbsPath, PathUtil::GetFileName(pak)).c_str()),
		&rcLogger,
		false, // may show window?
		CResourceCompilerHelper::eRcExePath_editor,
		true,  // silent?
		true);
	notif.SetProgress(0.9f);

	const string cryassetPakPath(PathUtil::Make(PathUtil::GetGameFolder().c_str(), PathUtil::RemoveExtension(pak).c_str(), "cryasset.pak"));
	if (GetISystem()->GetIPak()->OpenPack(PathUtil::GetGameFolder(), cryassetPakPath))
	{
		std::vector<CAsset*> assets = AssetLoader::CAssetFactory::LoadAssetsFromPakFile(cryassetPakPath, [](const char* szAssetRelativePath, uint64 timestamp)
		{
			return true;
		});
		notif.SetProgress(1.0f);

		ThreadingUtils::PostOnMainThread([assets]()
		{
			CAssetManager::GetInstance()->MergeAssets(assets);
		});
	}

	QDir(QtUtil::ToQString(tempRoot)).removeRecursively();
}

string CPakImporter::GetTemporaryDirectoryPath()
{
	char path[ICryPak::g_nMaxPath] = {};
	return gEnv->pCryPak->AdjustFileName("%USER%/temp", path, ICryPak::FLAGS_PATH_REAL | ICryPak::FLAGS_FOR_WRITING | ICryPak::FLAGS_ADD_TRAILING_SLASH);
}

// The pak should be opened.
void CPakImporter::Unpak(const char* szArchivePath, const char* szDestPath, std::function<void(float)> progress)
{
	float progressValue = 0; //(0, 1);
	std::vector<char> buffer(1 << 24);

	std::stack<string> stack;
	stack.push("");
	while (!stack.empty())
	{
		const CryPathString mask = PathUtil::Make(stack.top(), "*");
		const CryPathString folder = stack.top();
		stack.pop();

		GetISystem()->GetIPak()->ForEachArchiveFolderEntry(szArchivePath, mask, [szDestPath, &stack, &folder, &buffer, &progressValue, progress](const ICryPak::ArchiveEntryInfo& entry)
		{
			const CryPathString path(PathUtil::Make(folder.c_str(), entry.szName));
			if (entry.bIsFolder)
			{
				stack.push(path);
				return;
			}

			ICryPak* const pPak = GetISystem()->GetIPak();
			FILE* file = pPak->FOpen(path, "rbx");
			if (!file)
			{
				return;
			}

			if (!pPak->MakeDir(PathUtil::Make(szDestPath, folder)))
			{
				return;
			}

			buffer.resize(pPak->FGetSize(file));
			const size_t numberOfBytesRead = pPak->FReadRawAll(buffer.data(), buffer.size(), file);
			pPak->FClose(file);

			CryPathString tempFilepath(PathUtil::Make(szDestPath, path));
			QFile destFile(QtUtil::ToQString(tempFilepath.c_str()));
			destFile.open(QIODevice::WriteOnly | QIODevice::Truncate);
			destFile.write(buffer.data(), numberOfBytesRead);

			if (progress)
			{
				progressValue = std::min(1.0f, progressValue + 0.01f);
				progress(progressValue);
			}
		});
	}
}

void CPakImporter::GenerateCryassets(const char* szPath, std::function<void(float)> progress)
{
	RCLogger rcLogger;

	const std::vector<CAssetType*>& types = CAssetManager::GetInstance()->GetAssetTypes();
	const float step = 1.0f / types.size();
	float progressValue = 0; //(0, 1);

	for (CAssetType* const pType : types)
	{
		// Ignore fallback asset type.
		if (strcmp(pType->GetTypeName(), "cryasset") == 0)
		{
			continue;
		}

		const string files = string().Format("%s/*.%s", szPath, pType->GetFileExtension());
		const string settings = string().Format("/overwriteextension=cryasset /assettypes=%s,%s", pType->GetFileExtension(), pType->GetTypeName());

		CResourceCompilerHelper::CallResourceCompiler(
			files.c_str(),
			settings.c_str(),
			&rcLogger,
			false, // may show window?
			CResourceCompilerHelper::eRcExePath_editor,
			true,  // silent?
			true);

		if (progress)
		{
			progressValue = std::min(1.0f, progressValue + step);
			progress(progressValue);
		}
	}
}

CPakImporter::CPakImporter()
{
	GetIEditor()->GetFileMonitor()->RegisterListener(this, "", "pak");
}

}
