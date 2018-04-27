// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "IPlugin.h"

class CXTPCommandBars;
class CXTPShortcutManager;
struct IVariable;

/*! Collection of Utility MFC functions.
*/
struct PLUGIN_API CMFCUtils
{
	//////////////////////////////////////////////////////////////////////////
	// Load true color image into image list.
	static BOOL LoadTrueColorImageList(CImageList& imageList, UINT nIDResource, int nIconWidth, COLORREF colMaskColor);

	//////////////////////////////////////////////////////////////////////////
	static void TransparentBlt(HDC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HBITMAP hBitmap, int nXSrc, int nYSrc,
		COLORREF colorTransparent);

	static void LoadShortcuts(CXTPCommandBars* pCommandBars, UINT nMenuIDResource, const char* pSectionNameForLoading);
	static void ShowShortcutsCustomizeDlg(CXTPCommandBars* pCommandBars, UINT nMenuIDResource, const char* pSectionNameForSaving);
	static void ExportShortcuts(CXTPShortcutManager* pShortcutMgr);
	static void ImportShortcuts(CXTPShortcutManager* pShortcutMgr, const char* pSectionNameForSaving);


	static IVariable* GetChildVar(const IVariable* array, const char* name, bool recursive = false);


	//////////////////////////////////////////////////////////////////////////
	// Color utils
	//////////////////////////////////////////////////////////////////////////

	inline static Vec3 Rgb2Vec(COLORREF color)
	{
		return Vec3(GetRValue(color) / 255.0f, GetGValue(color) / 255.0f, GetBValue(color) / 255.0f);
	}

	inline static int Vec2Rgb(const Vec3& color)
	{
		return RGB(color.x * 255, color.y * 255, color.z * 255);
	}

	static COLORREF ColorLinearToGamma(ColorF col);
	static ColorF   ColorGammaToLinear(COLORREF col);

	//////////////////////////////////////////////////////////////////////////

	//! Converts a CPoint to a QPoint, using local space of the provided widget.
	static QPoint CPointToQPointLocal(const QWidget* widget, const CPoint& point);

	//////////////////////////////////////////////////////////////////////////

	static bool ExecuteConsoleApp(const string& CommandLine, string& OutputText, bool bNoTimeOut = false, bool bShowWindow = false);
};

//Do not use anymore, only kept for compatibility with old MFC based tools
namespace Deprecated
{
	inline bool CheckVirtualKey(int virtualKey)
	{
		GetAsyncKeyState(virtualKey);
		if (GetAsyncKeyState(virtualKey) & (1 << 15))
			return true;
		return false;
	}
}
