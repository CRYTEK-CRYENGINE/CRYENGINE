// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AssetGenerator.h"

#include "AssetSystem/AssetManager.h"
#include "Loader/AssetLoaderBackgroundTask.h"
#include "Notifications/NotificationCenter.h"
#include "PathUtils.h"
#include "QtUtil.h"
#include "ThreadingUtils.h"
#include <IEditor.h>

#include <CrySystem/IProjectManager.h>
#include <CryString/CryPath.h>

namespace Private_AssetGenerator
{

class CBatchProcess : public CProgressNotification
{
public: 
	CBatchProcess()
		: CProgressNotification(tr("Generating asset metadata"), QString(), true)
		, m_totalItemsCount(0)
		, m_precessedItemsCount(0)
	{
	}

	void PushItem() { ++m_totalItemsCount; }
	void PopItem() { ++m_precessedItemsCount; }
	bool IsDone() {	return m_totalItemsCount == m_precessedItemsCount; }
	virtual void ShowProgress(const string& filePath)
	{
		const QString msg = QtUtil::ToQString(PathUtil::GetFile(filePath));
		const size_t  precessedItemsCount = m_precessedItemsCount.load();
		const size_t  totalItemsCount = m_totalItemsCount.load();
		const float progress = float(precessedItemsCount) / totalItemsCount;
		SetMessage(msg);
		SetProgress(progress);
	}

private:
	std::atomic<size_t> m_totalItemsCount;
	std::atomic<size_t> m_precessedItemsCount;
};

static AssetManagerHelpers::CAssetGenerator* s_pInstance = nullptr;

bool IsGameFolderPath(const char* szSource)
{
	const string gameFolder = PathUtil::GetGameFolder();
	return strnicmp(szSource, gameFolder.c_str(), gameFolder.size()) == 0 &&
		strlen(szSource) > gameFolder.size() && szSource[gameFolder.size()] == '/';
}

}

namespace AssetManagerHelpers
{

void CAssetGenerator::RegisterFileListener()
{
	static CAssetGenerator theInstance;
	Private_AssetGenerator::s_pInstance = &theInstance;
}

// IFileChangeListener implementation.
void CAssetGenerator::OnFileChange(const char* szFilename, EChangeType changeType)
{
	// Ignore events for files outside of the current game folder.
	if (GetISystem()->GetIPak()->IsAbsPath(szFilename))
	{
		return;
	}

	if (changeType != IFileChangeListener::eChangeType_RenamedNewName
	    && changeType != IFileChangeListener::eChangeType_Modified)
	{
		return;
	}

	// Ignore files that start with a dot.
	if (szFilename[0] == '.')
	{
		return;
	}

	// Ignore auto backup folders.
	if (strstr(szFilename, "/_autobackup/"))
	{
		return;
	}

	// Ignore invalid paths.
	string reasonToReject;
	if (!CAssetType::IsValidAssetPath(string().Format("%s.cryasset", szFilename), reasonToReject))
	{
		return;
	}

	// Refresh cryasset files for the following types even if exists. 
	// These asset types do not have true asset editors to update cryasset files.
	static const char* const update[] = { "cdf", "dds" };
	const char* szExt = PathUtil::GetExt(szFilename);
	const bool updateExisting = std::any_of(std::begin(update), std::end(update), [szExt](const char* szUpdatable)
	{
		return stricmp(szExt, szUpdatable) == 0;
	});

	const char* const szAssetDirectory = GetIEditor()->GetProjectManager()->GetCurrentAssetDirectoryAbsolute();
	const string filePath = PathUtil::Make(szAssetDirectory, szFilename);

	m_fileQueue.ProcessItemUniqueAsync(filePath, [this, updateExisting](const string& path)
	{
		// It can be that the file is still being opened for writing.
		if (m_pTextureCompilerProgress || IsFileOpened(path))
		{
			// Try again
			return false;
		}

		const string cryasset = string().Format("%s.cryasset", path.c_str()).MakeLower();
		if (!updateExisting && GetISystem()->GetIPak()->IsFileExist(cryasset))
		{
			return true;
		}

		GenerateCryasset(path);
		return true;
	});
}

bool CAssetGenerator::GenerateCryassets()
{
	const string jobFile = PathUtil::Make(PathUtil::GetEnginePath(), "tools/cryassets/rcjob_cryassets.xml");
	const string options = string().Format("/job=\"%s\" /src=\"%s\"", jobFile.c_str(), PathUtil::GetGameProjectAssetsPath().c_str());

	RCLogger rcLogger;

	return CResourceCompilerHelper::ERcCallResult::eRcCallResult_success == CResourceCompilerHelper::CallResourceCompiler(
		nullptr,
		options.c_str(),
		&rcLogger,
		false, // may show window?
		CResourceCompilerHelper::eRcExePath_editor,
		true,  // silent?
		true); // no user dialog?
}

CAssetGenerator::CAssetGenerator()
{
	GetIEditor()->GetSystem()->GetIRenderer()->AddAsyncTextureCompileListener(this);

	const std::vector<CAssetType*>& types = CAssetManager::GetInstance()->GetAssetTypes();

	m_rcSettings.reserve(types.size() * 20);
	m_rcSettings.Append("/overwriteextension=cryasset /assettypes=\"");
	for (CAssetType* pType : types)
	{
		if (!pType->CanAutoRepairMetadata())
		{
			continue;
		}

		m_rcSettings.AppendFormat("%s,%s;", pType->GetFileExtension(), pType->GetTypeName());
		GetIEditor()->GetFileMonitor()->RegisterListener(this, "", pType->GetFileExtension());
	}

	// TODO: There are .wav.cryasset and .ogg.cryasset for the CSoundType. Remove the following scoped lines when this is fixed.
	{
		m_rcSettings.Append("ogg,Sound;");
		GetIEditor()->GetFileMonitor()->RegisterListener(this, "", "ogg");
	}

	m_rcSettings.Append("\" ");

	m_rcSettings.shrink_to_fit();
}

// The method must be called only from the main thread. 
void CAssetGenerator::GenerateCryasset(const string& filePath, const string& destFolder /* = "" */)
{
	using namespace Private_AssetGenerator;

	if (!m_pProgress)
	{
		m_pProgress.reset(new CBatchProcess());
	}
	static_cast<CBatchProcess*>(m_pProgress.get())->PushItem();

	ThreadingUtils::AsyncQueue([filePath, destFolder, this]()
	{
		RCLogger rcLogger;

		CBatchProcess* pProgress = static_cast<CBatchProcess*>(m_pProgress.get());
		CRY_ASSERT(pProgress);

		pProgress->ShowProgress(filePath);

		if (GetISystem()->GetIPak()->IsFileExist(filePath))
		{
			// TODO: Move the implementation to a virtual function of CAssetType. Thus, each asset would override the default implementation.

			string extendedSettings;
			if (!destFolder.empty())
			{
				extendedSettings = m_rcSettings + "/targetroot=\"" + destFolder + "\" ";
			}
			const string& currentSettings = destFolder.empty() ? m_rcSettings : extendedSettings;

			CResourceCompilerHelper::CallResourceCompiler(
				filePath.c_str(),
				currentSettings.c_str(),
				&rcLogger,
				false, // may show window?
				CResourceCompilerHelper::eRcExePath_editor,
				true,  // silent?
				true);
		}

		ThreadingUtils::PostOnMainThread([this]() 
		{
			// It mustn't be null here by design.
			CBatchProcess* pProgress = static_cast<CBatchProcess*>(m_pProgress.get());
			CRY_ASSERT(pProgress); 

			pProgress->PopItem();
			if (pProgress->IsDone())
			{
				m_pProgress.reset(nullptr);
			}
		});
	});
}

void CAssetGenerator::OnCompilationStarted(const char* szSource, const char* szTarget, int nPending)
{
	std::unique_lock<std::mutex> lock(m_textureCompilerMutex);
	if (m_pTextureCompilerProgress)
	{
		m_pTextureCompilerProgress->SetMessage(QtUtil::ToQString(PathUtil::GetFile(szSource)));
	}
}

void CAssetGenerator::OnCompilationFinished(const char* szSource, const char* szTarget, ERcExitCode eReturnCode)
{
	using namespace Private_AssetGenerator;
	if (!IsGameFolderPath(szSource))
	{
		return;
	}

	auto const absRoot = PathUtil::GetCurrentProjectDirectoryAbsolute();
	auto const destFolder = PathUtil::Make(absRoot, PathUtil::GetDirectory(PathUtil::ToUnixPath(szSource)));
	auto const targetFile = PathUtil::Make(absRoot, PathUtil::ToUnixPath(szTarget));
	ThreadingUtils::PostOnMainThread([this, targetFile, destFolder]()
	{
		GenerateCryasset(targetFile, destFolder);
	});
}

void CAssetGenerator::OnCompilationQueueTriggered(int nPending)
{
	std::unique_lock<std::mutex> lock(m_textureCompilerMutex);

	if (!m_pTextureCompilerProgress)
	{
		m_pTextureCompilerProgress.reset(new CProgressNotification(QObject::tr("Compiling textures"), QString()));
	}
}

void CAssetGenerator::OnCompilationQueueDepleted()
{
	std::unique_lock<std::mutex> lock(m_textureCompilerMutex);

	if (m_pTextureCompilerProgress)
	{
		m_pTextureCompilerProgress.reset();
	}
}

void GenerateCryasset(const string& filePath)
{
	if (Private_AssetGenerator::s_pInstance)
	{
		if (PathUtil::IsRelativePath(filePath))
		{ 
			const char* const szAssetDirectory = GetIEditor()->GetProjectManager()->GetCurrentAssetDirectoryAbsolute();
			Private_AssetGenerator::s_pInstance->GenerateCryasset(PathUtil::Make(szAssetDirectory, filePath.c_str()));
		}
		else
		{
			Private_AssetGenerator::s_pInstance->GenerateCryasset(filePath);
		}
	}
}

}