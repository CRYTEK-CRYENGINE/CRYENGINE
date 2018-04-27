// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "LensFlareManager.h"
#include "LensFlareEditor.h"
#include "LensFlareItem.h"
#include "LensFlareLibrary.h"
#include "LensFlareUtil.h"

#include "Controls/PropertyItem.h"

//////////////////////////////////////////////////////////////////////////
// CLensFlareManager implementation.
//////////////////////////////////////////////////////////////////////////
CLensFlareManager::CLensFlareManager() :
	CBaseLibraryManager()
{
	m_bUniqNameMap = true;
	m_pLevelLibrary = (CBaseLibrary*)AddLibrary("Level");
	m_pLevelLibrary->SetLevelLibrary(true);

	//register deprecated property editor:
	GetIEditorImpl()->RegisterDeprecatedPropertyEditor(ePropertyFlare, WrapMemberFunction(this, &CLensFlareManager::OnPickFlare));
}

//////////////////////////////////////////////////////////////////////////
CLensFlareManager::~CLensFlareManager()
{
}

//////////////////////////////////////////////////////////////////////////

bool CLensFlareManager::OnPickFlare(const string& oldValue, string& newValue)
{
	CWnd* pLensFlareEditor = GetIEditorImpl()->OpenView(CLensFlareEditor::s_pLensFlareEditorClassName);
	if (pLensFlareEditor)
	{
		pLensFlareEditor->PostMessage(WM_FLAREEDITOR_UPDATETREECONTROL, 0, 0);
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////

void CLensFlareManager::ClearAll()
{
	CBaseLibraryManager::ClearAll();

	m_pLevelLibrary = (CBaseLibrary*)AddLibrary("Level");
	m_pLevelLibrary->SetLevelLibrary(true);
}

//////////////////////////////////////////////////////////////////////////
CBaseLibraryItem* CLensFlareManager::MakeNewItem()
{
	return new CLensFlareItem;
}

//////////////////////////////////////////////////////////////////////////
CBaseLibrary* CLensFlareManager::MakeNewLibrary()
{
	return new CLensFlareLibrary(this);
}

//////////////////////////////////////////////////////////////////////////
string CLensFlareManager::GetRootNodeName()
{
	return "FlareLibs";
}

//////////////////////////////////////////////////////////////////////////
string CLensFlareManager::GetLibsPath()
{
	if (m_libsPath.IsEmpty())
		m_libsPath += FLARE_LIBS_PATH;

	return m_libsPath;
}

//////////////////////////////////////////////////////////////////////////
bool CLensFlareManager::LoadFlareItemByName(const string& fullItemName, IOpticsElementBasePtr pDestOptics)
{
	if (pDestOptics == NULL)
		return false;

	CLensFlareItem* pLensFlareItem = (CLensFlareItem*)LoadItemByName(fullItemName);
	if (pLensFlareItem == NULL)
		return false;

	LensFlareUtil::CopyOptics(pLensFlareItem->GetOptics(), pDestOptics, true);
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CLensFlareManager::Modified()
{
	CLensFlareEditor* pEditor = CLensFlareEditor::GetLensFlareEditor();
	if (pEditor == NULL)
		return;
	if (pEditor->GetCurrentLibrary())
		pEditor->GetCurrentLibrary()->SetModified(true);
}

//////////////////////////////////////////////////////////////////////////
IDataBaseLibrary* CLensFlareManager::LoadLibrary(const string& filename, bool bReload)
{
	CLensFlareEditor* pEditor = CLensFlareEditor::GetLensFlareEditor();

	string fileNameWithGameFolder(filename);

	fileNameWithGameFolder.Replace('\\', '/');

	int nGamePathLength = PathUtil::GetGameFolder().size();
	string gamePathName;
	if (nGamePathLength < filename.GetLength())
		gamePathName = filename.Left(nGamePathLength);
	if (gamePathName != PathUtil::GetGameFolder())
	{
		fileNameWithGameFolder.Insert(0, "/");
		fileNameWithGameFolder.Insert(0, PathUtil::GetGameFolder());
	}

	int nLibraryIndex(-1);
	bool bSameAsCurrentLibrary(false);

	for (int i = 0; i < m_libs.size(); i++)
	{
		if (stricmp(fileNameWithGameFolder, m_libs[i]->GetFilename()) == 0)
		{
			IDataBaseLibrary* pExistingLib = m_libs[i];
			for (int j = 0; j < pExistingLib->GetItemCount(); j++)
				UnregisterItem((CBaseLibraryItem*)pExistingLib->GetItem(j));
			pExistingLib->RemoveAllItems();
			nLibraryIndex = i;
			if (pEditor)
				bSameAsCurrentLibrary = pEditor->GetCurrentLibrary() == pExistingLib;
			break;
		}
	}

	TSmartPtr<CBaseLibrary> pLib = MakeNewLibrary();
	if (!pLib->Load(filename))
	{
		Error(_T("Failed to Load Item Library: %s"), filename);
		return NULL;
	}

	if (nLibraryIndex != -1)
	{
		m_libs[nLibraryIndex] = pLib;
		if (bSameAsCurrentLibrary && pEditor)
		{
			pEditor->ResetElementTreeControl();
			pEditor->SelectLibrary(pLib, true);
		}
	}
	else
	{
		m_libs.push_back(pLib);
	}

	pLib->SetFilename(fileNameWithGameFolder);

	return pLib;
}

