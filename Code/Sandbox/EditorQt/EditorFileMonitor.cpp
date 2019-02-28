// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "EditorFileMonitor.h"
#include <CryCore/ToolsHelpers/ResourceCompilerHelper.h>
#include "CrySystem/IProjectManager.h"
#include "GameEngine.h"
#include "Include/IAnimationCompressionManager.h"
#include <CryString/StringUtils.h>
#include <CrySystem/IProjectManager.h>
#include "CryEdit.h"
#include "FilePathUtil.h"

//////////////////////////////////////////////////////////////////////////
CEditorFileMonitor::CEditorFileMonitor()
{
	GetIEditorImpl()->RegisterNotifyListener(this);
}

//////////////////////////////////////////////////////////////////////////
CEditorFileMonitor::~CEditorFileMonitor()
{
	CFileChangeMonitor::DeleteInstance();
}

//////////////////////////////////////////////////////////////////////////
void CEditorFileMonitor::OnEditorNotifyEvent(EEditorNotifyEvent ev)
{
	if (ev == eNotify_OnInit)
	{
		// Setup file change monitoring
		gEnv->pSystem->SetIFileChangeMonitor(this);

		// We don't want the file monitor to be enabled while
		// in console mode...
		if (!GetIEditorImpl()->IsInConsolewMode())
			MonitorDirectories();

		CFileChangeMonitor::Instance()->Subscribe(this);
	}
	else if (ev == eNotify_OnQuit)
	{
		gEnv->pSystem->SetIFileChangeMonitor(NULL);
		CFileChangeMonitor::Instance()->StopMonitor();
		GetIEditorImpl()->UnregisterNotifyListener(this);
	}
}

//////////////////////////////////////////////////////////////////////////
bool CEditorFileMonitor::RegisterListener(IFileChangeListener* pListener, const char* sMonitorItem)
{
	return RegisterListener(pListener, sMonitorItem, "*");
}

//////////////////////////////////////////////////////////////////////////
static string CanonicalizePath(const char* szPath)
{
	std::vector<char> canonicalizedPath(strlen(szPath) + 1, '\0');
	if (PathCanonicalize(&canonicalizedPath[0], szPath))
		return string(&canonicalizedPath[0]);

	return string(szPath);
}

//////////////////////////////////////////////////////////////////////////
static string GetAbsolutePathOfProjectFolder(const char* szPath)
{
	const char* szProjectRoot = GetISystem()->GetIProjectManager()->GetCurrentProjectDirectoryAbsolute();
	CryPathString path = PathUtil::Make(szProjectRoot, szPath);
	path = PathUtil::AddBackslash(path);
	path = PathUtil::ToDosPath(path);
	return CanonicalizePath(path.c_str());
}

//////////////////////////////////////////////////////////////////////////
// TODO: Change the initialization order to call MonitorDirectories() before any of CEditorFileMonitor::RegisterListener
void CEditorFileMonitor::MonitorDirectories()
{

	// NOTE: Instead of monitoring each sub-directory we monitor the whole root
	// folder. This is needed since if the sub-directory does not exist when
	// we register it it will never get monitored properly.
	CFileChangeMonitor::Instance()->MonitorItem(GetAbsolutePathOfProjectFolder(PathUtil::GetGameFolder()));

	// Add mod paths too
	for (int index = 0;; index++)
	{
		const char* sModPath = gEnv->pCryPak->GetMod(index);
		if (!sModPath)
			break;
		CFileChangeMonitor::Instance()->MonitorItem(GetAbsolutePathOfProjectFolder(sModPath));
	}

	// Add editor directory for scripts
	CFileChangeMonitor::Instance()->MonitorItem(GetAbsolutePathOfProjectFolder("Editor"));

	// Add shader source folder
	CFileChangeMonitor::Instance()->MonitorItem(GetAbsolutePathOfProjectFolder("Engine/Shaders"));
}

//////////////////////////////////////////////////////////////////////////
bool CEditorFileMonitor::RegisterListener(IFileChangeListener* pListener, const char* szFolderRelativeToGame, const char* sExtension)
{
	m_vecFileChangeCallbacks.push_back(SFileChangeCallback(pListener, PathUtil::ToUnixPath(szFolderRelativeToGame), sExtension));

	// NOTE: Here we do not register any CFileChangeMonitor::Instance()->MonitorItem(path).
	// See CEditorFileMonitor::MonitorDirectories().
	return true;
}

bool CEditorFileMonitor::UnregisterListener(IFileChangeListener* pListener)
{
	bool bRet = false;

	// Note that we remove the listener, but we don't currently remove the monitored item
	// from the file monitor. This is fine, but inefficient

	std::vector<SFileChangeCallback>::iterator iter = m_vecFileChangeCallbacks.begin();
	while (iter != m_vecFileChangeCallbacks.end())
	{
		if (iter->pListener == pListener)
		{
			iter = m_vecFileChangeCallbacks.erase(iter);
			bRet = true;
		}
		else
			iter++;
	}

	return bRet;
}

//////////////////////////////////////////////////////////////////////////
static bool IsFilenameEndsWithDotDaeDotZip(const char* fln)
{
	size_t len = strlen(fln);
	if (len < 8)
		return false;

	if (stricmp(fln + len - 8, ".dae.zip") == 0)
		return true;
	else
		return false;
}

//////////////////////////////////////////////////////////////////////////
static bool RecompileColladaFile(const char* path)
{
	string pathWithGameFolder = PathUtil::ToUnixPath(PathUtil::AddSlash(PathUtil::GetGameFolder())) + string(path);
	if (CResourceCompilerHelper::CallResourceCompiler(
	      pathWithGameFolder.c_str(), "/refresh", NULL, false, CResourceCompilerHelper::eRcExePath_editor, true, true, L".")
	    != CResourceCompilerHelper::eRcCallResult_success)
		return true;
	else
		return false;
}

//////////////////////////////////////////////////////////////////////////
static const char* AbsoluteToProjectPath(const char* szAbsolutePath)
{
	if (!GetISystem()->GetIPak()->IsAbsPath(szAbsolutePath))
	{
		return szAbsolutePath;
	}

	const string projectPath = PathUtil::AddSlash(gEnv->pSystem->GetIProjectManager()->GetCurrentProjectDirectoryAbsolute());
	const string fixedPath = PathUtil::ToUnixPath(szAbsolutePath);

	if (cry_strnicmp(fixedPath.c_str(), projectPath.c_str(), projectPath.length()) == 0) // path starts with rootPathStr
	{
		return szAbsolutePath + projectPath.length();
	}
	return "";
}

//////////////////////////////////////////////////////////////////////////
const char* GetPathRelativeToModFolder(const char* szAbsolutePath)
{
	if (szAbsolutePath[0] == '\0')
		return szAbsolutePath;

	if (_strnicmp("engine", szAbsolutePath, 6) == 0 && (szAbsolutePath[6] == '\\' || szAbsolutePath[6] == '/'))
		return szAbsolutePath;

	szAbsolutePath = AbsoluteToProjectPath(szAbsolutePath);
	
	string gameFolder = PathUtil::GetGameFolder();
	string modLocation;
	const char* modFolder = gameFolder.c_str();
	int modIndex = 0;
	do
	{
		if (_strnicmp(modFolder, szAbsolutePath, strlen(modFolder)) == 0)
		{
			const char* result = szAbsolutePath + strlen(modFolder);
			if (*result == '\\' || *result == '/')
				++result;
			return result;
		}

		modFolder = gEnv->pCryPak->GetMod(modIndex);
		++modIndex;
	}
	while (modFolder != 0);

	return "";
}

///////////////////////////////////////////////////////////////////////////

// Called when file monitor message is received
void CEditorFileMonitor::OnFileMonitorChange(const SFileChangeInfo& rChange)
{
	CCryEditApp* app = CCryEditApp::GetInstance();
	if (app == NULL || app->IsExiting())
		return;

	// skip folders!
	if (QDir(QString::fromLocal8Bit(rChange.filename)).exists())
	{
		return;
	}

	// Process updated file.
	// Make file relative to MasterCD folder.
	string filename = rChange.filename;

	// Make sure there is no leading slash
	if (!filename.IsEmpty() && filename[0] == '\\' || filename[0] == '/')
		filename = filename.Mid(1);

	if (!filename.IsEmpty())
	{
		// Make it relative to the game folder
		const string filenameRelGame = GetPathRelativeToModFolder(filename.GetString());

		const string ext = filename.Right(filename.GetLength() - filename.ReverseFind('.') - 1);

		// Check for File Monitor callback
		std::vector<SFileChangeCallback>::iterator iter;
		for (iter = m_vecFileChangeCallbacks.begin(); iter != m_vecFileChangeCallbacks.end(); ++iter)
		{
			SFileChangeCallback& sCallback = *iter;

			// We compare against length of callback string, so we get directory matches as well as full filenames
			if (sCallback.pListener)
			{
				if (sCallback.extension == "*" || stricmp(ext, sCallback.extension) == 0)
				{
					if (!filenameRelGame.empty() && _strnicmp(filenameRelGame, sCallback.item, sCallback.item.GetLength()) == 0)
					{
						sCallback.pListener->OnFileChange(filenameRelGame, IFileChangeListener::EChangeType(rChange.changeType));
					}
					else if (_strnicmp(filename, sCallback.item, sCallback.item.GetLength()) == 0)
					{
						sCallback.pListener->OnFileChange(filename, IFileChangeListener::EChangeType(rChange.changeType));
					}
				}
			}
		}

		//TODO:  have all these file types encapsulated in some IFileChangeFileTypeHandler to deal with each of them in a more generic way
		bool isCAF = stricmp(ext, "caf") == 0;
		bool isLMG = (stricmp(ext, "lmg") == 0) || (stricmp(ext, "bspace") == 0) || (stricmp(ext, "comb") == 0);

		bool isExportLog = stricmp(ext, "exportlog") == 0;
		bool isRCDone = stricmp(ext, "rcdone") == 0;
		bool isCOLLADA = (stricmp(ext, "dae") == 0 || IsFilenameEndsWithDotDaeDotZip(filename.GetString()));

		if (!isCAF && !isLMG && !isCOLLADA && !isExportLog && !isRCDone)
		{
			// Updating an existing CGF file is often implemented as a DeleteFile + MoveFile pair.
			// If the file listener triggers a refresh of a CStatObj after deletion and before moving,
			// it will not find the CGF file, overwrite its path with a default file, and lose the
			// reference to the original path. This causes objects in the level to disappear.
			// Therefore, we ignore deletions of CGF objects.
			if (rChange.changeType != IFileChangeListener::eChangeType_Deleted)
			{
				// Apart from reloading resources it also may trigger importing of tif files.
				if (rChange.changeType != IFileChangeListener::eChangeType_RenamedOldName)
				{
					GetIEditorImpl()->GetGameEngine()->ReloadResourceFile(filenameRelGame);
				}

				if (stricmp(ext, "cgf") == 0)
				{
					CBaseObjectsArray objects;
					GetIEditorImpl()->GetObjectManager()->GetObjects(objects);
					for (CBaseObject* pObj : objects)
					{
						pObj->InvalidateGeometryFile(filenameRelGame);
					}
				}
			}
		}
		else if (isCOLLADA)
		{
			// Make a corresponding .cgf path.
			string cgfFileName;
			int nameLength = filename.GetLength();

			cgfFileName = filename;
			if (stricmp(ext, "dae") == 0)
			{
				cgfFileName.SetAt(nameLength - 3, 'c');
				cgfFileName.SetAt(nameLength - 2, 'g');
				cgfFileName.SetAt(nameLength - 1, 'f');
			}
			else
			{
				cgfFileName.SetAt(nameLength - 7, 'c');
				cgfFileName.SetAt(nameLength - 6, 'g');
				cgfFileName.SetAt(nameLength - 5, 'f');
				cgfFileName.SetAt(nameLength - 4, 0);
			}
			IStatObj* pStatObjectToReload = GetIEditorImpl()->Get3DEngine()->FindStatObjectByFilename(cgfFileName.GetBuffer());

			// If the corresponding .cgf file exists, recompile the changed COLLADA file.
			if (pStatObjectToReload)
			{
				CryLog("Recompile DAE file: %s", (LPCTSTR)filename);
				RecompileColladaFile(filename.GetString());
			}
		}
		else
		{
			ICharacterManager* pICharacterManager = GetISystem()->GetIAnimationSystem();
			stack_string strPath = filenameRelGame;
			PathUtil::UnifyFilePath(strPath);

			if (isLMG)
			{
				CryLog("Reload blendspace file: %s", (LPCTSTR)strPath);
				pICharacterManager->ReloadLMG(strPath.c_str());
			}

		}
		// Set this flag to make sure that the viewport will update at least once,
		// so that the changes will be shown, even if the app does not have focus.
		CCryEditApp::GetInstance()->ForceNextIdleProcessing();
	}
}

