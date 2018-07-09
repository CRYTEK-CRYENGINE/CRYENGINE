// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __FILE_TREE_H__
#define __FILE_TREE_H__
#pragma once

#if CRY_PLATFORM_WINDOWS

	#include <CryCore/Platform/CryWindows.h>
	#include "_TinyMain.h" // includes <windows.h>
	#include "_TinyWindow.h"
	#include "_TinyFileEnum.h"
	#include "_TinyImageList.h"

	#include <vector>

	#include "..\LuaDebuggerResource.h"
	#include <CrySystem/File/ICryPak.h>

class CFileTree : public _TinyTreeView
{
public:
	CFileTree() { m_iID = 0; };
	virtual ~CFileTree() {};

	BOOL Create(const _TinyRect& rcWnd, _TinyWindow* pParent = NULL, UINT iID = NULL)
	{
		m_iID = iID;
		_TinyVerify(_TinyTreeView::Create(iID, WS_CHILD | TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT |
		                                  WS_VISIBLE, WS_EX_CLIENTEDGE, &rcWnd, pParent));
		m_cTreeIcons.CreateFromBitmap(MAKEINTRESOURCE(IDB_TREE_VIEW), 16);
		SetImageList(m_cTreeIcons.GetHandle());

		return TRUE;
	};

	static int CALLBACK SortItems(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
	{
		CFileTree* parent = static_cast<CFileTree*>((CFileTree*)lParamSort);
		int diff = (parent->m_isDir[lParam2] ? 1 : 0) - (parent->m_isDir[lParam1] ? 1 : 0);
		if (diff)
			return diff;
		else
			return stricmp(parent->m_vFileNameTbl[lParam1], parent->m_vFileNameTbl[lParam2]);
	}

	virtual HTREEITEM AddItemToTree(LPTSTR lpszItem, LPARAM ud = NULL, HTREEITEM hParent = NULL, UINT iImage = 0)
	{
		HTREEITEM res = _TinyTreeView::AddItemToTree(lpszItem, ud, hParent, iImage);
		TVSORTCB sort;
		sort.hParent = hParent;
		sort.lParam = (LPARAM)this;
		sort.lpfnCompare = &SortItems;
		TreeView_SortChildrenCB(m_hWnd, &sort, FALSE);
		return res;
	}
	virtual void RemoveItemFromTree(HTREEITEM itm)
	{
		TreeView_DeleteItem(m_hWnd, itm);
	}

	void ScanFiles(char* pszRootPath)
	{
		HTREEITEM hRoot = _TinyTreeView::AddItemToTree(pszRootPath, NULL, NULL, 2);
		EnumerateScripts(hRoot, pszRootPath);
		Expand(hRoot);
	};

	const char* GetCurItemFileName()
	{
		HTREEITEM hCurSel = NULL;
		LPARAM nIdx;
		hCurSel = _TinyTreeView::GetSelectedItem();
		if (!hCurSel)
			return NULL;
		nIdx = _TinyTreeView::GetItemUserData(hCurSel);

		size_t sz = m_vFileNameTbl.size();  // size_t is unsigned, so don't subtract 1
		if ((0 == sz) || ((size_t)nIdx > (sz - 1)))
			return NULL;
		return m_vFileNameTbl[nIdx].c_str();
	};

protected:
	_TinyImageList      m_cTreeIcons;
	INT                 m_iID;
	std::vector<string> m_vFileNameTbl;
	std::vector<bool>   m_isDir;

	int EnumerateScripts(HTREEITEM hRoot, char* pszEnumPath)
	{
		int num_added = 0;
		_finddata_t c_file;
		char szPath[_MAX_PATH];
		char szFullFilePath[_MAX_PATH];
		HTREEITEM hItem = NULL;

		intptr_t hFile;

		if ((hFile = gEnv->pCryPak->FindFirst((string(pszEnumPath) + "/*.*").c_str(), &c_file)) == -1L)
			return 0;

		while (gEnv->pCryPak->FindNext(hFile, &c_file) == 0)
		{
			if (_stricmp(c_file.name, "..") == 0)
				continue;
			if (_stricmp(c_file.name, ".") == 0)
				continue;

			cry_sprintf(szFullFilePath, "%s%s", pszEnumPath, c_file.name);
			m_vFileNameTbl.push_back(szFullFilePath);
			m_isDir.push_back((c_file.attrib & _A_SUBDIR) != 0);

			if (c_file.attrib & _A_SUBDIR)
			{
				HTREEITEM hItem = AddItemToTree(c_file.name, (LPARAM) m_vFileNameTbl.size() - 1, hRoot, 0);
				cry_sprintf(szPath, "%s%s/", pszEnumPath, c_file.name);
				if (EnumerateScripts(hItem, szPath))
				{
					++num_added;
				}
				else
				{
					RemoveItemFromTree(hItem);
				}
			}
			else
			{
				char fname[_MAX_PATH];
				cry_strcpy(fname, c_file.name);
				_strlwr(fname);
				if (!strchr(fname, '.'))
					continue;
				if (strcmp(strchr(fname, '.'), ".lua") == 0)
				{
					hItem = AddItemToTree(c_file.name, (LPARAM) m_vFileNameTbl.size() - 1, hRoot, 1);
					++num_added;
				}
			}
		}
		return num_added;
	};
};

#endif // CRY_PLATFORM_WINDOWS

#endif
