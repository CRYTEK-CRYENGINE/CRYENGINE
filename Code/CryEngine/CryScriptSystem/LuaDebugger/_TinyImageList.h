// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __TINY_IMAGE_LIST_H__
#define __TINY_IMAGE_LIST_H__

#pragma once

#ifndef __TINY_MAIN_H__
	#error "_TinyImageList requires <_TinyMain.h>"
#endif

class _TinyImageList
{
public:
	_TinyImageList() { m_hImgLst = NULL; };
	~_TinyImageList()
	{
		if (m_hImgLst)
			_TinyVerify(ImageList_Destroy(m_hImgLst));
	};

	BOOL Create(UINT iFlags = ILC_COLOR, UINT iCX = 16, UINT iCY = 16, UINT iMaxItems = 32)
	{
		_TinyAssert(m_hImgLst == NULL);
		m_hImgLst = ImageList_Create(iCX, iCY, iFlags, 0, iMaxItems);
		if (m_hImgLst == NULL)
		{
			_TINY_CHECK_LAST_ERROR
			return FALSE;
		}
		return TRUE;
	};

	BOOL CreateFromBitmap(const char* pszBitmap, UINT iCX)
	{
		_TinyAssert(m_hImgLst == NULL);
		m_hImgLst = ImageList_LoadBitmap(_Tiny_GetResourceInstance(), pszBitmap, iCX, 32, 0x00FF00FF);
		if (m_hImgLst == NULL)
		{
			_TINY_CHECK_LAST_ERROR
			return FALSE;
		}
		return TRUE;
	};

	BOOL AddImage(DWORD dwResource)
	{
		_TinyAssert(m_hImgLst);
		HBITMAP hBmp = LoadBitmap(_Tiny_GetResourceInstance(), MAKEINTRESOURCE(dwResource));
		if (hBmp == NULL)
		{
			_TINY_CHECK_LAST_ERROR
			return FALSE;
		}
		ImageList_Add(m_hImgLst, hBmp, NULL);
		_TinyVerify(DeleteObject(hBmp));
	};

	HIMAGELIST GetHandle()           { return m_hImgLst; };

	UINT       GetImageCount() const { return ImageList_GetImageCount(m_hImgLst); };

protected:
	HIMAGELIST m_hImgLst;
};

#endif
