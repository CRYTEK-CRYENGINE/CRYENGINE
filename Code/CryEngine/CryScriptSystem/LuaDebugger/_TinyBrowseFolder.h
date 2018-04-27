// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _TINY_BROWSE_FOLDER_H_
#define _TINY_BROWSE_FOLDER_H_

#pragma once

#ifndef __TINY_MAIN_H__
	#error "_TinyBrowseFolder require <_TinyMain.h>"
#endif

#if CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT
	#pragma warning(disable: 4090)
#endif

//#include <Shlobj.h>

// TODO
/*
   BOOL _TinyBrowseForFolder(char szPathOut[_MAX_PATH], _TinyWindow *pParent = NULL)
   {
   BROWSEINFO sInfo;
   char szDisplayName[_MAX_PATH];
   ITEMIDLIST *pList = NULL;

   szPathOut[0] = '\0';

   sInfo.hwndOwner = (pParent == NULL) ? NULL : pParent->m_hWnd;
   sInfo.pidlRoot = NULL;
   sInfo.pszDisplayName = szDisplayName;
   sInfo.ulFlags = BIF_RETURNONLYFSDIRS;
   sInfo.lpfn = NULL;
   sInfo.lParam = 0;
   sInfo.iImage = 0;

   __try
   {
    if (FAILED(CoInitialize(NULL)))
      __leave;

    CoInitialize(NULL);

    SHBrowseForFolder(&sInfo);
    if ((pList = SHBrowseForFolder(&sInfo)) == NULL)
      __leave;

    // SHGetPathFromIDList
   }
   __finally
   {
    CoUninitialize();
   }

   return TRUE;
   }

   // char szTmp[_MAX_PATH];
   // extern BOOL g_bTest = _TinyBrowseForFolder(szTmp);
 */
#endif
