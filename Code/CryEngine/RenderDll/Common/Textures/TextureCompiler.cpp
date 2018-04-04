// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

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
	#define CHUNK_SIZE 64 * 1024

		char* buf = new char[CHUNK_SIZE];
		size_t readBytes = 0;
		DWORD writtenBytes = 0;
		size_t totalBytes = 0;

		while (!gEnv->pCryPak->FEof(pSrcFile))
		{
			readBytes = gEnv->pCryPak->FReadRaw(buf, sizeof(char), CHUNK_SIZE, pSrcFile);
			success = WriteFile(hDestFile, buf, sizeof(char) * readBytes, &writtenBytes, NULL) != FALSE;

			if (!success || (readBytes != writtenBytes))
			{
				success = false;
				break;
			}

			totalBytes += readBytes;
		}

		delete[] buf;

	#undef CHUNK_SIZE

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

static bool MoveAssetFile(const char* szSrcFile, const char* szDstFile)
{
	bool success = true;

	if (strcmp(szSrcFile, szDstFile))
	{
		const auto attributes = GetFileAttributes(szDstFile);
		if (attributes != INVALID_FILE_ATTRIBUTES)
		{
			if ((attributes & FILE_ATTRIBUTE_READONLY) != 0)
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
			success = success && (DeleteFile(szDstFile) != FALSE);
		}

		gEnv->pCryPak->MakeDir(PathUtil::GetPathWithoutFilename(szDstFile));

		success = success && (GetFileAttributes(szSrcFile) != INVALID_FILE_ATTRIBUTES);
		success = success && (MoveFile(szSrcFile, szDstFile) != FALSE);

		if (!success)
		{
			// try spin for 1.5 seconds, there might still be a file lock on either source or target
			// compilation isn't time critical, and this occurs infrequently
			for (int i = 0; (i < 5) && (!success); ++i)
			{
				success = true;

				CrySleep(100);
				if (GetFileAttributes(szDstFile) != INVALID_FILE_ATTRIBUTES)
				{
					CrySleep(100);
					success = success && (DeleteFile(szDstFile) != FALSE);
				}

				CrySleep(100);
				success = success && (GetFileAttributes(szSrcFile) != INVALID_FILE_ATTRIBUTES);
				CrySleep(100);
				success = success && (MoveFile(szSrcFile, szDstFile) != FALSE);
			}

			if (!success)
			{
				iLog->LogError("Can't copy from \"%s\" to \"%s\"\n", szSrcFile, szDstFile);
			}
		}
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
		DeleteTemp(GetTmpPath());
	}

	const string& GetTmpPath() const
	{
		return m_assets.front().tmp;
	}

	// Moves asset(s) from the temporary to the destination location.
	void CreateDestinationAssets(const bool bSuccess) const
	{
		for (const auto& asset : m_assets)
		{
			const bool bAssetCreated = bSuccess && MoveAsset(asset.tmp.c_str(), asset.dst.c_str());
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

private:
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
			CRY_ASSERT_MESSAGE(0, "Cryasset has no data files: %s", cryasset.c_str());
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
	// check if to be generated from the same source
	m_rwLockWatch.RLock();
	TWatchItem queued = m_mWatchList.find(szDstFile);
	bool doadd = (queued == m_mWatchList.end());
	bool exists = (!doadd && (queued->second == szSrcFile));
	m_rwLockWatch.RUnlock();

	return exists;
}

CResourceCompilerHelper::ERcCallResult CTextureCompiler::QueueResourceCompiler(const char* szSrcFile, const char* szDstFile, const bool bWindow, const bool bRefresh)
{
	// check if to be generated from the same source
	if (HasQueuedResourceCompiler(szSrcFile, szDstFile))
	{
		return eRcCallResult_queued;
	}

	// replace/place source
	m_rwLockWatch.WLock();
	TWatchItem queued = m_mWatchList.find(szDstFile);
	bool doadd = (queued == m_mWatchList.end());
	bool exists = (!doadd && (queued->second == szSrcFile));

	if (exists)
	{
		m_rwLockWatch.WUnlock();
		return eRcCallResult_queued;
	}

	if (!doadd)
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
				//	return eRcCallResult_notFound;
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
					//	return eRcCallResult_notFound;
				}
			}
		}
	}
	m_rwLockWatch.WUnlock();

	ForkOffResourceCompiler(szSrcFile, szDstFile, bWindow, bRefresh);
	return eRcCallResult_queued;
}

void CTextureCompiler::ForkOffResourceCompiler(const char* szSrcFile, const char* szDstFile, const bool bWindow, const bool bRefresh)
{
	m_rwLockProcessing.WLock();
	bool empty = !m_qProcessingList.size();
	#if defined(STLPORT)
	m_qProcessingList.push_back(TProcItem());
	#else
	m_qProcessingList.emplace_back(TProcItem());
	#endif
	TProcItem& addedrc = m_qProcessingList.back();

	// have to be valid after the unlock
	addedrc.src = szSrcFile;
	addedrc.dst = szDstFile;
	addedrc.windowed = bWindow;
	addedrc.refresh = bRefresh;
	m_rwLockProcessing.WUnlock();

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

void CTextureCompiler::ConsumeQueuedResourceCompiler(TProcItem* item)
{
	// no need to protect
	int pending = m_qProcessingList.size();

	m_rwLockNotify.RLock();
	std::for_each(m_sNotifyList.begin(), m_sNotifyList.end(), [=](IAsyncTextureCompileListener* notify)
	{
		notify->OnCompilationQueueTriggered(pending);
	});
	m_rwLockNotify.RUnlock();

	while (item)
	{
		{
			m_rwLockNotify.RLock();
			std::for_each(m_sNotifyList.begin(), m_sNotifyList.end(), [=](IAsyncTextureCompileListener* notify)
			{
				notify->OnCompilationStarted(item->src.c_str(), item->dst.c_str(), pending);
			});
			m_rwLockNotify.RUnlock();

			// Always use a temporary file as outfile, otherwise RC may write to the
			// file before it's even loaded as a dummy.
			{
				CTemporaryAsset tmpAsset(item->src, item->dst);

				iLog->Log("Compile texture from \"%s\", to \"%s\"\n", item->src.c_str(), tmpAsset.GetTmpPath().c_str());
				item->returnval = InvokeResourceCompiler(item->src.c_str(), tmpAsset.GetTmpPath().c_str(), item->windowed, true);

				bool bSuccess = (item->returnval == eRcExitCode_Success);
				tmpAsset.CreateDestinationAssets(bSuccess);
			}

			m_rwLockNotify.RLock();
			std::for_each(m_sNotifyList.begin(), m_sNotifyList.end(), [=](IAsyncTextureCompileListener* notify)
			{
				notify->OnCompilationFinished(item->src.c_str(), item->dst.c_str(), item->returnval);
			});
			m_rwLockNotify.RUnlock();
		}

		TProcQueue::iterator foundrc;

		m_rwLockWatch.WLock();
		m_rwLockProcessing.WLock();

		for (foundrc = m_qProcessingList.begin(); (&(*foundrc) != item) && (foundrc != m_qProcessingList.end()); ++foundrc)
			;
		if (foundrc != m_qProcessingList.end())
		{
			m_mWatchList.erase(item->dst);
			m_qProcessingList.erase(foundrc);
			if (m_qProcessingList.size())
				item = &m_qProcessingList.front();
			else
				item = NULL;
			pending = m_qProcessingList.size();
		}
		// Severe container-damage, should not happen.
		else
		{
			assert(0);
		}

		m_rwLockWatch.WUnlock();
		m_rwLockProcessing.WUnlock();
	}

	m_rwLockNotify.RLock();
	std::for_each(m_sNotifyList.begin(), m_sNotifyList.end(), [=](IAsyncTextureCompileListener* notify)
	{
		notify->OnCompilationQueueDepleted();
	});
	m_rwLockNotify.RUnlock();
}

//////////////////////////////////////////////////////////////////////////
bool CTextureCompiler::ProcessTextureIfNeeded(
  const char* originalFilename,
  char* processedFilename,
  size_t processedFilenameSizeInBytes)
{
	// allocates 1k upto 4k on the stack
	char sSrcFile[MAX_PATH];
	char sDestFile[MAX_PATH];

	char sFullSrcFilename[MAX_PATH];
	char sFullDestFilename[MAX_PATH];

	GetOutputFilename(originalFilename, sDestFile, sizeof(sDestFile));

	// Adjust filename so that it is global.
	gEnv->pCryPak->AdjustFileName(sDestFile, sFullDestFilename, 0);

	for (uint32 dwIndex = 0;; ++dwIndex)    // check for all input files
	{
		GetInputFilename(originalFilename, dwIndex, sSrcFile, sizeof(sSrcFile));

		if (sSrcFile[0] == 0)
		{
			break;          // last input file
		}

		// Adjust filename so that it is global.
		gEnv->pCryPak->AdjustFileName(sSrcFile, sFullSrcFilename, 0);

	#if defined(_RENDERER)
		// Prevent a race-condition of date-checker and file-writer
		// by doing an early queue-check (files are in the queue until
		// they are completely processed)
		if (HasQueuedResourceCompiler(sFullSrcFilename, sFullDestFilename))
		{
			break;
		}
	#endif

		// compile if there is no destination
		// compare date of destination and source , recompile if needed
		// load dds header, check hash-value of the compile settings in the dds file, recompile if needed (not done yet)

		FILE* pDestFile = nullptr;
		FILE* pSrcFile = nullptr;
		{
			SCOPED_ALLOW_FILE_ACCESS_FROM_THIS_THREAD();
			pDestFile = gEnv->pCryPak->FOpen(sDestFile, "rb");
			pSrcFile = gEnv->pCryPak->FOpen(sSrcFile, "rb");
		}

		// files from the pak file do not count as date comparison do not seem to work there
		if (pDestFile)
		{
			if (gEnv->pCryPak->IsInPak(pDestFile))
			{
				gEnv->pCryPak->FClose(pDestFile);
				pDestFile = 0;
			}
		}

		if (pSrcFile)
		{
			if (gEnv->pCryPak->IsInPak(pSrcFile))
			{
				gEnv->pCryPak->FClose(pSrcFile);
				pSrcFile = 0;
			}
		}

		bool bInvokeResourceCompiler = false;

		// is there no destination file?
		if (pSrcFile && !pDestFile)
		{
			bInvokeResourceCompiler = true;
		}

		// if both files exist, is the source file newer?
		if (pSrcFile && pDestFile && !IsFileReadOnly(sFullDestFilename))
		{
			ICryPak::FileTime timeSrc = gEnv->pCryPak->GetModificationTime(pSrcFile);
			ICryPak::FileTime timeDest = gEnv->pCryPak->GetModificationTime(pDestFile);

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

		if (pDestFile)
		{
			gEnv->pCryPak->FClose(pDestFile);
			pDestFile = 0;
		}

		if (pSrcFile)
		{
			gEnv->pCryPak->FClose(pSrcFile);
			pSrcFile = 0;
		}
		else
		{
			continue;
		}

		if (bInvokeResourceCompiler && gEnv->IsEditor())
		{
			// call rc.exe
			//
			// All requests are time-check already, force the RC to refresh the
			// target file in any case
	#if defined(_RENDERER)
			if (QueueResourceCompiler(sFullSrcFilename, sFullDestFilename, false, true) != eRcCallResult_queued)    // false=no window, true=force compile
	#else
			if (InvokeResourceCompiler(sFullSrcFilename, sFullDestFilename, false, true) != eRcExitCode_Success)    // false=no window, true=force compile
	#endif
			{
				cry_strcpy(processedFilename, processedFilenameSizeInBytes, originalFilename);

				// rc failed
				return false;
			}
		}

		break;
	}

	// load without using RC (e.g. TGA)
	cry_strcpy(processedFilename, processedFilenameSizeInBytes, sDestFile);

	// rc didn't fail
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CTextureCompiler::AddAsyncTextureCompileListener(IAsyncTextureCompileListener* pListener)
{
	m_rwLockNotify.WLock();
	m_sNotifyList.insert(pListener);
	m_rwLockNotify.WUnlock();
}

void CTextureCompiler::RemoveAsyncTextureCompileListener(IAsyncTextureCompileListener* pListener)
{
	m_rwLockNotify.WLock();
	m_sNotifyList.erase(pListener);
	m_rwLockNotify.WUnlock();
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
