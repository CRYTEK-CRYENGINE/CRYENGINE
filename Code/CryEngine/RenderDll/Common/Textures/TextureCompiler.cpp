// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "TextureCompiler.h"

#if defined(CRY_ENABLE_RC_HELPER)

//////////////////////////////////////////////////////////////////////////
// Provides facilities to asynchronously invoke RC and to capture
// file changes made by invoked RCs
	#include <CryThreading/IJobManager_JobDelegator.h>

DECLARE_JOB("AsyncResourceCompiler", TAsyncResourceCompilerJob, CTextureCompiler::ConsumeQueuedResourceCompiler);

//////////////////////////////////////////////////////////////////////////
CTextureCompiler::CTextureCompiler()
{
}

CTextureCompiler::~CTextureCompiler()
{
}

//////////////////////////////////////////////////////////////////////////
CryMutex CTextureCompiler::g_RCmutex;
CTextureCompiler* CTextureCompiler::g_pRCInstance = nullptr;

void CTextureCompiler::DestroyInstance()
{
	CryAutoLock<CryMutex> rcAutoLock(g_RCmutex);
	if (g_pRCInstance)
	{
		delete g_pRCInstance;
		g_pRCInstance = nullptr;
	}
}

CTextureCompiler& CTextureCompiler::GetInstance()
{
	CryAutoLock<CryMutex> rcAutoLock(g_RCmutex);
	if (!g_pRCInstance)
	{
		g_pRCInstance = new CTextureCompiler();
		assert(g_pRCInstance != nullptr);

		atexit(DestroyInstance);
	}

	return *g_pRCInstance;
}

//////////////////////////////////////////////////////////////////////////
void CTextureCompiler::GetInputFilename(
  const char* filename,
  const unsigned int index,
  char* inputFilename,
  size_t inputFilenameSizeInBytes)
{
	if (inputFilename == 0 || inputFilenameSizeInBytes < sizeof(inputFilename[0]))
	{
		return;
	}

	const char* ext = GetExtension(filename);

	if (ext && stricmp(ext, "dds") == 0)
	{
		switch (index)
		{
		case 0:
			ReplaceExtension(filename, "tif", inputFilename, inputFilenameSizeInBytes);
			return;
		default:
			inputFilename[0] = 0;
			return;
		}
	}

	if (index > 0)
	{
		inputFilename[0] = 0;
		return;
	}

	cry_strcpy(inputFilename, inputFilenameSizeInBytes, filename);
}

//////////////////////////////////////////////////////////////////////////
// choose a specialized imposter, because otherwise file-format
// constraints for specific texture-types may be violated
	#define COMPILE_DELAYED_REGULAR "%ENGINE%/EngineAssets/TextureMsg/TextureCompiling.dds"
	#define COMPILE_DELAYED_CUBEMAP "%ENGINE%/EngineAssets/TextureMsg/TextureCompiling_cm.dds"
	#define COMPILE_DELAYED_CUBEDIF "%ENGINE%/EngineAssets/TextureMsg/TextureCompiling_cm_diff.dds"
	#define COMPILE_DELAYED_NORMAL  "%ENGINE%/EngineAssets/TextureMsg/TextureCompiling_ddn.dds"
	#define COMPILE_DELAYED_NORMALA "%ENGINE%/EngineAssets/TextureMsg/TextureCompiling_ddna.dds"

	#define COMPILE_FAILED_REGULAR  "%ENGINE%/EngineAssets/TextureMsg/RCError.dds"
	#define COMPILE_FAILED_CUBEMAP  "%ENGINE%/EngineAssets/TextureMsg/RCError_cm.dds"
	#define COMPILE_FAILED_CUBEDIF  "%ENGINE%/EngineAssets/TextureMsg/RCError_cm_diff.dds"
	#define COMPILE_FAILED_NORMAL   "%ENGINE%/EngineAssets/TextureMsg/RCError_ddn.dds"
	#define COMPILE_FAILED_NORMALA  "%ENGINE%/EngineAssets/TextureMsg/RCError_ddna.dds"

static const char* GetDelayedTexture(const char* szFile)
{
	const char* szImposter;
	szImposter = COMPILE_DELAYED_REGULAR;
	if (strstr(szFile, "_ddna.") != 0)
		szImposter = COMPILE_DELAYED_NORMALA;
	else if (strstr(szFile, "_ddn.") != 0)
		szImposter = COMPILE_DELAYED_NORMAL;
	else if (strstr(szFile, "_cm_diff.") != 0)
		szImposter = COMPILE_DELAYED_CUBEDIF;
	else if (strstr(szFile, "_cm.") != 0)
		szImposter = COMPILE_DELAYED_CUBEMAP;

	return szImposter;
}

static const char* GetFailedTexture(const char* szFile)
{
	const char* szImposter;
	szImposter = COMPILE_FAILED_REGULAR;
	if (strstr(szFile, "_ddna.") != 0)
		szImposter = COMPILE_FAILED_NORMALA;
	else if (strstr(szFile, "_ddn.") != 0)
		szImposter = COMPILE_FAILED_NORMAL;
	else if (strstr(szFile, "_cm_diff.") != 0)
		szImposter = COMPILE_FAILED_CUBEDIF;
	else if (strstr(szFile, "_cm.") != 0)
		szImposter = COMPILE_FAILED_CUBEMAP;

	return szImposter;
}

// Make the dummy destination-file date one tick older than the source-file
// to allow the compilation being re-triggered in case the editor is closed earlier.
// For error-dummies this behavior can be disabled to prevent infinite retry-loops.
	#define COMPILE_DELAYED_DELTA 1
	#define COMPILE_FAILED_DELTA  0

static bool CopyDummy(const char* szImposter, const char* szSrcFile, const char* szDstFile, __int64 timedelta)
{
	string dummyPath = szImposter;
	bool success = true;
	ICryPak::FileTime ft = 0;

	{
		FILE* pSrcFile = gEnv->pCryPak->FOpen(szSrcFile, "rb");
		if (pSrcFile)
		{
			ft = gEnv->pCryPak->GetModificationTime(pSrcFile);
			ft -= timedelta;

			gEnv->pCryPak->FClose(pSrcFile);
		}
	}

	// Copy the dummy file and make it either the same age as the previous destination file,
	// or a bit older than the source file.
	// Take care of copying dummy out of the pak onto disk.
	// Dummy then triggers recompilation in subsequent Sandbox starts as well.
	FILE* pSrcFile = gEnv->pCryPak->FOpen(dummyPath, "rb");
	HANDLE hDestFile = CreateFile(szDstFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);

	success = (pSrcFile && (hDestFile != INVALID_HANDLE_VALUE));
	if (success)
	{
		constexpr size_t chunkSize = 64 * 1024;
		char* buf = new char[chunkSize];
		size_t readBytes = 0;
		DWORD writtenBytes = 0;
		size_t totalBytes = 0;

		while (!gEnv->pCryPak->FEof(pSrcFile))
		{
			readBytes = gEnv->pCryPak->FReadRaw(buf, sizeof(char), chunkSize, pSrcFile);
			success = WriteFile(hDestFile, buf, sizeof(char) * readBytes, &writtenBytes, NULL) != FALSE;

			if (!success || (readBytes != writtenBytes))
			{
				success = false;
				break;
			}

			totalBytes += readBytes;
		}

		delete[] buf;

		// Prevent zero-byte files being returned as valid DDSs.
		success = success && (totalBytes != 0);
	}

	if (pSrcFile)
	{
		gEnv->pCryPak->FClose(pSrcFile);
	}

	if (hDestFile != INVALID_HANDLE_VALUE)
	{
		if (ft)
		{
			FILETIME FT;

			FT.dwLowDateTime = (DWORD)(ft & 0xFFFFFFFF);
			FT.dwHighDateTime = (DWORD)(ft >> 32);

			SetFileTime(hDestFile, 0, 0, &FT);
		}

		CloseHandle(hDestFile);
	}

	if (!success)
	{
		DeleteFile(szDstFile);
	}

	return success;
}

static string GetLastErrorString()
{
	const char szMsgBuf[1024]{};

	FormatMessageA(
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		GetLastError(),
		0,
		(LPSTR)&szMsgBuf,
		1024,
		NULL);

	return string(szMsgBuf);
}

static bool MoveAssetFile(const char* szSrcFile, const char* szDstFile)
{
	if (strcmp(szSrcFile, szDstFile) == 0)
	{
		return true;
	}

	const auto dstFileAttributes = GetFileAttributes(szDstFile);
	if (dstFileAttributes != INVALID_FILE_ATTRIBUTES)
	{
		if ((dstFileAttributes & FILE_ATTRIBUTE_READONLY) != 0)
		{
			// CE-12815. Should be able to compile tiff to dds if .cryasset file is write protected.
			if (stricmp(PathUtil::GetExt(szDstFile), "cryasset") == 0)
			{
				DeleteFile(szSrcFile);
				return true;
			}
			else
			{
				iLog->LogError("Can't write to read-only file: \"%s\"\n", szDstFile);
				return false;
			}
		}
	}

	gEnv->pCryPak->MakeDir(PathUtil::GetPathWithoutFilename(szDstFile));

	const string tempFilename = string().Format("%s.$tmp$", szDstFile);

	bool success = false;
	// try spin for 1.5 seconds, there might still be a file lock on either source or target
	// compilation isn't time critical, and this occurs infrequently
	for (int i = 0, stage = 0; i < 5; ++i)
	{
		if (i)
		{
			CrySleep(300);
		}

		switch (stage)
		{
		case 0:
			if (GetFileAttributes(szSrcFile) == INVALID_FILE_ATTRIBUTES)
			{
				continue;
			}
			++stage;
			// falls through
		case 1: 
			if (!MoveFileEx(szSrcFile, tempFilename.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH))
			{
				continue;
			}
			++stage;
			// falls through
		case 2:
			if ((dstFileAttributes != INVALID_FILE_ATTRIBUTES) && !DeleteFile(szDstFile))
			{
				continue;
			}
			++stage;
			// falls through
		case 3:
			// Even if we have already deleted the target file on the previos stage or it did not exist,
			// there is a possibility that AssetGenerator of Sandbox has already created a new one.
			// So we have to overwrite it. 
			// TODO: find a way not to generate cryasset with the Sandbox AssetGenerator until all asset files are copied.
			if (!MoveFileEx(tempFilename.c_str(), szDstFile, MOVEFILE_REPLACE_EXISTING))
			{
				continue;
			}
			++stage;
			// falls through
		default:
			break;
		}

		success = true;
		break;
	}

	if (!success)
	{
		iLog->LogError("Can't copy from \"%s\" to \"%s\": %s\n", szSrcFile, szDstFile, GetLastErrorString().c_str());
		DeleteFile(tempFilename.c_str());
	}

	return success;
}

static bool IsFileReadOnly(const char* szPath)
{
	const auto attributes = GetFileAttributes(szPath);
	return (attributes != INVALID_FILE_ATTRIBUTES) && ((attributes & FILE_ATTRIBUTE_READONLY) != 0);
}


// RAII handler of temporary asset data.
class CTemporaryAsset
{
	struct SAsset
	{
		SAsset(string src, string dst, string tmp) : src(src), dst(dst), tmp(tmp) {}
		string src; // Source file.
		string dst; // Destination file.
		string tmp; // A temporary file for the RC processing.
	};

public:
	CTemporaryAsset(const string& srcFile, const string& dstFile)
		: m_assets(CreateTemp(srcFile, dstFile))
	{
	}

	~CTemporaryAsset()
	{
		CreateDestinationAssets();
		DeleteTemp(GetTmpPath());
	}

	const string& GetTmpPath() const
	{
		return m_assets.front().tmp;
	}

private:

	// Moves asset(s) from the temporary to the destination location.
	void CreateDestinationAssets() const
	{
		for (const auto& asset : m_assets)
		{
			const bool bAssetCreated = MoveAsset(asset.tmp.c_str(), asset.dst.c_str());
			if (!bAssetCreated && (CRenderer::CV_r_texturecompilingIndicator >= 0))
			{
				CopyDummy(GetFailedTexture(asset.dst.c_str()), asset.src.c_str(), asset.dst.c_str(), COMPILE_FAILED_DELTA);
			}

			// Suggest reload of the texture after success or failure.
			if (gEnv && gEnv->pRenderer)
			{
				gEnv->pRenderer->EF_ReloadFile_Request(asset.dst.c_str());
			}
		}
	}

	static string GetTemporaryDirectoryPath()
	{
		char path[ICryPak::g_nMaxPath] = {};
		return gEnv->pCryPak->AdjustFileName("%USER%/TextureCompiler", path, ICryPak::FLAGS_PATH_REAL | ICryPak::FLAGS_FOR_WRITING | ICryPak::FLAGS_ADD_TRAILING_SLASH);
	}

	// Create a temp directory to store texture asset during processing.
	static std::vector<SAsset> CreateTemp(const string& srcFile, const string& dstFile)
	{
		// Temporary files are created outside the asset directory primarily to avoid interference with the editor asset system.
		// Right now, when creating a temporary folder, we copy the folder hierarchy of the asset folder to avoid naming conflicts.
		// Example: The temporary directory for 'textures/defaults/white.dds' will be '%USER%/TextureCompiler/assets/textures/defaults/white'
		// In the future, this should be replaced by some central system that creates temporary folders.
		static const string tempPrefix = GetTemporaryDirectoryPath();

		string dir;
		string filename;
		string ext;
		PathUtil::Split(dstFile, dir, filename, ext);

		// TODO : PathUtil::AbsolutePathToGamePath(dir)
		string tempSuffix = dir;
		tempSuffix.replace(':', '_');

		string tempDir = PathUtil::Make(tempPrefix + tempSuffix, filename);
		gEnv->pCryPak->MakeDir(tempDir.c_str());

		// Several assets may be created by RC from a single source file depends on the source file options.
		std::vector<SAsset> assets;
		std::vector<string> suffixes = { "" };
		const bool bCubemap = strstr(dstFile.c_str(), "_cm.") != nullptr;
		if (bCubemap)
		{
			suffixes.push_back("_diff");
		}
		assets.reserve(suffixes.size());

		for (const string& suffix : suffixes)
		{
			const string asset = filename + suffix;
			const string dstPath = PathUtil::Make(dir, asset, ext);
			const string tmpPath = PathUtil::Make(tempDir, asset, ext);
			assets.emplace_back(srcFile, dstPath, tmpPath);

			// Try to update existing ".cryasset" instead of creating a new one to keep the asset GUID.
			const string dstCryasset = dstPath + ".cryasset";
			const string tmpCryasset = tmpPath + ".cryasset";
			gEnv->pCryPak->CopyFileOnDisk(dstCryasset.c_str(), tmpCryasset.c_str(), false);
		}

		return assets;
	}

	// Delete the temporary asset directory.
	static bool DeleteTemp(const string& filepath)
	{
		const string tmpFolder = PathUtil::GetPathWithoutFilename(filepath);
		return gEnv->pCryPak->RemoveDir(tmpFolder.c_str(), true);
	}

	// Returns list of asset filenames without paths.
	static std::vector<string> GetAssetFiles(const string& srcFile)
	{
		// Try to read the file list form .cryasset
		const string cryasset = srcFile + ".cryasset";

		const XmlNodeRef pXml = (GetFileAttributes(cryasset) != INVALID_FILE_ATTRIBUTES) ? gEnv->pSystem->LoadXmlFromFile(cryasset) : nullptr;
		if (!pXml)
		{
			return{ PathUtil::GetFile(srcFile) };
		}

		std::vector<string> files;

		// Find "AssetMetadata" node. It may be either the root node or a child of the root.
		const XmlNodeRef pMetadata = pXml->isTag("AssetMetadata") ? pXml : pXml->findChild("AssetMetadata");
		if (pMetadata)
		{
			const XmlNodeRef pFiles = pMetadata->findChild("Files");
			if (pFiles)
			{
				files.reserve(pFiles->getChildCount());
				for (int i = 0, n = pFiles->getChildCount(); i < n; ++i)
				{
					const XmlNodeRef pFile = pFiles->getChild(i);
					files.emplace_back(pFile->getAttr("path"));
				}
			}
		}

		// A fallback solution, should never happen.
		if (files.empty())
		{
			iLog->LogError("Cryasset has no data files: %s", cryasset.c_str());
			files.push_back(PathUtil::GetFile(srcFile));
		}

		files.emplace_back(PathUtil::GetFile(cryasset));
		return files;
	}

	// Moves asset files.
	static bool MoveAsset(const string& srcFile, const string& dstFile)
	{
		// Does not support rename.
		CRY_ASSERT(stricmp(PathUtil::GetFile(srcFile), PathUtil::GetFile(dstFile)) == 0);

		const string srcPath = PathUtil::GetPathWithoutFilename(srcFile);
		const string dstPath = PathUtil::GetPathWithoutFilename(dstFile);
		const auto files = GetAssetFiles(srcFile);
		for (const string& file : files)
		{
			const string srcFile = srcPath + file;
			const string dstFile = dstPath + file;
			if (!MoveAssetFile(srcFile.c_str(), dstFile.c_str()))
			{
				return false;
			}
		}
		return true;
	}

private:
	std::vector<SAsset> m_assets;

};

/* Cases for sequence of events:
 *
 * A) tif is written to, no dds exist or dds is older
 *  1) tif-monitor is triggered
 *  2) corresponding dds is requested via the streaming system
 *  3) RCHelper is called via mfInvokeRC
 *  4) RCHelper checks dates and request enters QueueResourceCompiler
 *  5) dummy dds is copied and request put into the queue
 *   a) dds-monitor is triggered
 *  6) streamer loads dummy dds
 *
 *  7) background process compiles dds and writes it out
 *   a) dds-monitor is triggered
 *  8) task is removed from queue
 *  9) loop until queue empty
 *
 * B) tif is written to, while RC is compiling it
 *  1) access denied on tif to writer
 *   VERIFY: does RC hold a lock on the file for the whole processing? It should, to have this work
 *   TODO: the queue should be updated, the RC killed and restarted
 *
 * C) tif is written to again, before RC is compiling it (double/tripple/... saving)
 *  1) tif-monitor is triggered
 *  2) corresponding dds is requested via the streaming system
 *  3) RCHelper is called via mfInvokeRC
 *  4) RCHelper checks dates and request enters QueueResourceCompiler
 *  5) file is found in watch-list and queuing is skipped
 *
 * D) dds is written to, while RC is compiling it
 *  1) dds is overwritten by RC, or RC get's access denied for writing
 *   a) dds-monitor is triggered
 *
 * E) dds is written to, before RC is compiling it
 *  1) dds is overwritten by RC
 *   a) dds-monitor is triggered
 *
 * In case of an error a texture with an error-message is placed for the requested outfile.
 */
bool CTextureCompiler::HasQueuedResourceCompiler(const char* szSrcFile, const char* szDstFile)
{
	CryAutoReadLock<CryRWLock> lock(m_rwLockWatch);

	// check if to be generated from the same source
	TWatchItem queued = m_mWatchList.find(szDstFile);
	const bool doAdd = (queued == m_mWatchList.end());
	const bool exists = (!doAdd && (queued->second == szSrcFile));
	return exists;
}

CResourceCompilerHelper::ERcCallResult CTextureCompiler::QueueResourceCompiler(const char* szSrcFile, const char* szDstFile, const bool bWindow, const bool bRefresh)
{
	if (AddToWatchList(szDstFile, szSrcFile))
	{
		ForkOffResourceCompiler(szSrcFile, szDstFile, bWindow, bRefresh);
	}

	return eRcCallResult_queued;
}

bool CTextureCompiler::AddToWatchList(const char* szDstFile, const char* szSrcFile)
{
	// check if to be generated from the same source
	if (HasQueuedResourceCompiler(szSrcFile, szDstFile))
	{
		return false;
	}

	CryAutoWriteLock<CryRWLock> lock(m_rwLockWatch);

	// replace/place source
	TWatchItem queued = m_mWatchList.find(szDstFile);
	const bool doAdd = (queued == m_mWatchList.end());
	const bool exists = (!doAdd && (queued->second == szSrcFile));

	if (exists)
	{
		return false;
	}

	if (!doAdd)
	{
		queued->second = szSrcFile;
	}
	else
	{
		m_mWatchList[szDstFile] = szSrcFile;

		if (CRenderer::CV_r_texturecompilingIndicator > 0)
		{
			const char* szImposter = GetDelayedTexture(szDstFile);
			if (!CopyDummy(szImposter, szSrcFile, szDstFile, COMPILE_DELAYED_DELTA))
			{
				// If this fails it's not critical, although it'll show
				// the "ReplaceMe" texture instead of a proper one.
			}

			// Provide cubemap's diffuse texture dummy as well if it is necessary
			if (strstr(szSrcFile, "_cm."))
			{
				string szDstDiff = AddSuffix(szDstFile, "_diff");

				const char* szImposter = GetDelayedTexture(szDstDiff.c_str());
				if (!CopyDummy(szImposter, szSrcFile, szDstDiff.c_str(), COMPILE_DELAYED_DELTA))
				{
					// If this fails it's not critical, although it'll show
					// the "ReplaceMe" texture instead of a proper one.
				}
			}
		}
	}
	return true;
}

void CTextureCompiler::ForkOffResourceCompiler(const char* szSrcFile, const char* szDstFile, const bool bWindow, const bool bRefresh)
{
	CryAutoWriteLock<CryRWLock> lock(m_rwLockProcessing);

	bool empty = !m_qProcessingList.size();
#if defined(STLPORT)
	m_qProcessingList.push_back(TProcItem());
#else
	m_qProcessingList.emplace_back(TProcItem());
#endif
	TProcItem& addedrc = m_qProcessingList.back();
	addedrc.src = szSrcFile;
	addedrc.dst = szDstFile;
	addedrc.windowed = bWindow;
	addedrc.refresh = bRefresh;

	// This approach spawns 1 new thread every time we had an empty list.
	// The thread automatically consumes the list for as long as it's not empty
	// and then terminates. When the editor is closed while the thread is running
	// it's handled by the job-system, no need to manually handle it here.
	if (empty)
	{
		TAsyncResourceCompilerJob compileJob(&addedrc);

		compileJob.SetClassInstance(&CTextureCompiler::GetInstance());
		compileJob.Run();
	}
}

bool CTextureCompiler::IsFileOpened(const char* szPath)
{
	if (!gEnv->pCryPak->IsFileExist(szPath, ICryPak::eFileLocation_OnDisk))
	{
		return false;
	}

	FILE* pFile = gEnv->pCryPak->FOpen(szPath, "r", ICryPak::FOPEN_ONDISK | ICryPak::FOPEN_LOCKED_OPEN);
	if (!pFile)
	{
		return true;
	}
	gEnv->pCryPak->FClose(pFile);
	return false;
}

void CTextureCompiler::ConsumeQueuedResourceCompiler(TProcItem* item)
{
	// no need to protect
	int pending = m_qProcessingList.size();
	NotifyCompilationQueueTriggered(pending);

	while (item)
	{
		// Try to spin for some time if there is a file lock on the source
		for( int i = 0; IsFileOpened(item->src.c_str()) && (i < 5); ++i)
		{
			CrySleep(300);
			continue;
		}

		NotifyCompilationStarted(item, pending);

		iLog->Log("Compile texture from \"%s\", to \"%s\"\n", item->src.c_str(), item->dst.c_str());

			// Always use a temporary file as outfile, otherwise RC may write to the
			// file before it's even loaded as a dummy.
			{
				CTemporaryAsset tmpAsset(item->src, item->dst);
				item->returnval = InvokeResourceCompiler(item->src.c_str(), tmpAsset.GetTmpPath().c_str(), item->windowed, true);
			}

		NotifyCompilationFinished(item);
		GetNextItem(item, pending);
	}

	NotifyCompilationQueueDepleted();
}

void CTextureCompiler::GetNextItem(TProcItem* &item, int &pending)
{
	CryAutoWriteLock<CryRWLock> lockWatch(m_rwLockWatch);
	CryAutoWriteLock<CryRWLock> lockProcessing(m_rwLockProcessing);

	TProcQueue::iterator foundrc;
	for (foundrc = m_qProcessingList.begin(); (&(*foundrc) != item) && (foundrc != m_qProcessingList.end()); ++foundrc)
	{
		// empty by intention
	}

	if (foundrc == m_qProcessingList.end())
	{
		CRY_ASSERT_MESSAGE(foundrc != m_qProcessingList.end(), "Severe container-damage, should not happen.");
		return;
	}

	m_mWatchList.erase(item->dst);
	m_qProcessingList.erase(foundrc);
	if (m_qProcessingList.size())
		item = &m_qProcessingList.front();
	else
		item = NULL;
	pending = m_qProcessingList.size();
}

void CTextureCompiler::NotifyCompilationQueueDepleted()
{
	CryAutoReadLock<CryRWLock> lock(m_rwLockNotify);
	for (IAsyncTextureCompileListener* notify : m_sNotifyList)
	{
		notify->OnCompilationQueueDepleted();
	};
}

void CTextureCompiler::NotifyCompilationFinished(TProcItem* item)
{
	CryAutoReadLock<CryRWLock> lock(m_rwLockNotify);
	for (IAsyncTextureCompileListener* notify : m_sNotifyList)
	{
		notify->OnCompilationFinished(item->src.c_str(), item->dst.c_str(), item->returnval);
	};
}

void CTextureCompiler::NotifyCompilationStarted(TProcItem* item, int pending)
{
	CryAutoReadLock<CryRWLock> lock(m_rwLockNotify);
	for (IAsyncTextureCompileListener* notify : m_sNotifyList)
	{
		notify->OnCompilationStarted(item->src.c_str(), item->dst.c_str(), pending);
	};
}

void CTextureCompiler::NotifyCompilationQueueTriggered(int pending)
{
	CryAutoReadLock<CryRWLock> lock(m_rwLockNotify);
	for (IAsyncTextureCompileListener* notify : m_sNotifyList)
	{
		notify->OnCompilationQueueTriggered(pending);
	};
}

//////////////////////////////////////////////////////////////////////////
CTextureCompiler::EResult CTextureCompiler::ProcessTextureIfNeeded(
  const char* originalFilename,
  char* processedFilename,
  size_t processedFilenameSizeInBytes,
  bool immediate)
{
	// allocates 1k upto 4k on the stack
	char sSrcFile[MAX_PATH];
	char sDestFile[MAX_PATH];

	char sFullSrcFilename[MAX_PATH];
	char sFullDestFilename[MAX_PATH];

	GetOutputFilename(originalFilename, sDestFile, sizeof(sDestFile));

	// Adjust filename so that it is global.
	gEnv->pCryPak->AdjustFileName(sDestFile, sFullDestFilename, 0);

	EResult result = EResult::AlreadyCompiled;

	for (uint32 dwIndex = 0;; ++dwIndex)    // check for all input files
	{
		GetInputFilename(originalFilename, dwIndex, sSrcFile, sizeof(sSrcFile));

		if (sSrcFile[0] == 0)
		{
			break;          // last input file
		}

		// Adjust filename so that it is global.
		gEnv->pCryPak->AdjustFileName(sSrcFile, sFullSrcFilename, 0);

		// Prevent a race-condition of date-checker and file-writer
		// by doing an early queue-check (files are in the queue until
		// they are completely processed)
		if (!immediate && HasQueuedResourceCompiler(sFullSrcFilename, sFullDestFilename))
		{
			break;
		}

		bool bInvokeResourceCompiler = false;

		// compile if there is no destination
		// compare date of destination and source , recompile if needed
		// load dds header, check hash-value of the compile settings in the dds file, recompile if needed (not done yet)

		CCryFile destinationFile, sourceFile;
		{
			SCOPED_ALLOW_FILE_ACCESS_FROM_THIS_THREAD();
			destinationFile.Open(sDestFile, "rb");

			// It can be that the file is still being opened for writing.
			if (IsFileOpened(sFullSrcFilename))
			{
				// Force the texture-compiling, the compilation queue will try to wait for the end of the file operation.
				// see CTextureCompiler::ConsumeQueuedResourceCompiler
				bInvokeResourceCompiler = true;
			}
			sourceFile.Open(sFullSrcFilename, "rb");
		}

		// files from the pak file do not count as date comparison do not seem to work there
		if (destinationFile.IsInPak())
		{
			destinationFile.Close();
		}

		if (sourceFile.IsInPak())
		{
			sourceFile.Close();
		}

		// is there no destination file?
		if (sourceFile.GetHandle() != nullptr && destinationFile.GetHandle() == nullptr)
		{
			bInvokeResourceCompiler = true;
		}

		// if both files exist, is the source file newer?
		if (sourceFile.GetHandle() != nullptr && destinationFile.GetHandle() != nullptr && !IsFileReadOnly(sFullDestFilename))
		{
			ICryPak::FileTime timeSrc = gEnv->pCryPak->GetModificationTime(sourceFile.GetHandle());
			ICryPak::FileTime timeDest = gEnv->pCryPak->GetModificationTime(destinationFile.GetHandle());

			// if the timestamp is identical, then it might be:
			//  1) a valid compiled target                          -> don't try
			//  2) a critically failed target (error file)          -> don't try again
			if (timeDest == (timeSrc - COMPILE_FAILED_DELTA))
			{
				bInvokeResourceCompiler = false;
			}
			// if the timestamp is one less than the source, then it might be:
			//  1) a genuine outdated target                        -> do try
			//  2) a queued target which was never run (dummy file) -> do try again
			else if (timeDest == (timeSrc - COMPILE_DELAYED_DELTA))
			{
				bInvokeResourceCompiler = true;
			}
			// otherwise, it might be
			//  1) older timestamp and the target is outdated       -> do try
			//  2) newer timestamp because the source was reverted  -> do try
			else
			{
				bInvokeResourceCompiler = (timeDest != timeSrc);
			}
		}

		destinationFile.Close();
		
		if (sourceFile.GetHandle() != nullptr)
		{
			sourceFile.Close();
		}
		else if (!bInvokeResourceCompiler)
		{
			continue;
		}

		if (bInvokeResourceCompiler)
		{
			// call rc.exe
			//
			// All requests are time-check already, force the RC to refresh the
			// target file in any case
			bool processed;

			if (immediate)
			{
				processed = InvokeResourceCompiler(sFullSrcFilename, sFullDestFilename, false, true) == eRcExitCode_Success;    // false=no window, true=force compile

				// Mark result as having been processed, unless another part of this texture was queued
				if (result != EResult::Queued)
				{
					result = EResult::Available;
				}
			}
			else
			{
				processed = QueueResourceCompiler(sFullSrcFilename, sFullDestFilename, false, true) == eRcCallResult_queued;    // false=no window, true=force compile

				result = EResult::Queued;
			}

			if(!processed)
			{
				cry_strcpy(processedFilename, processedFilenameSizeInBytes, originalFilename);

				// rc failed
				return EResult::Failed;
			}
		}

		break;
	}

	// load without using RC (e.g. TGA)
	cry_strcpy(processedFilename, processedFilenameSizeInBytes, sDestFile);

	// rc didn't fail
	return result;
}

//////////////////////////////////////////////////////////////////////////
void CTextureCompiler::AddAsyncTextureCompileListener(IAsyncTextureCompileListener* pListener)
{
	CryAutoWriteLock<CryRWLock> lock(m_rwLockNotify);
	m_sNotifyList.insert(pListener);

}

void CTextureCompiler::RemoveAsyncTextureCompileListener(IAsyncTextureCompileListener* pListener)
{
	CryAutoWriteLock<CryRWLock> lock(m_rwLockNotify);
	m_sNotifyList.erase(pListener);
}

#else

//////////////////////////////////////////////////////////////////////////
void CTextureCompiler::AddAsyncTextureCompileListener(IAsyncTextureCompileListener* pListener)
{
}

void CTextureCompiler::RemoveAsyncTextureCompileListener(IAsyncTextureCompileListener* pListener)
{
}

#endif
