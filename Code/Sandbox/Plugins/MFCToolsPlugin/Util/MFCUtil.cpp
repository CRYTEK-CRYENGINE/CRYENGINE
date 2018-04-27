// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MFCUtil.h"

#include "Util/Variable.h"
#include "FileDialogs/SystemFileDialog.h"
#include "QtUtil.h"


//////////////////////////////////////////////////////////////////////////
BOOL CMFCUtils::LoadTrueColorImageList(CImageList& imageList, UINT nIDResource, int nIconWidth, COLORREF colMaskColor)
{
	CBitmap bitmap;
	BITMAP bmBitmap;
	if (!bitmap.Attach(LoadImage(AfxGetResourceHandle(), MAKEINTRESOURCE(nIDResource), IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE | LR_CREATEDIBSECTION)))
		return FALSE;
	if (!bitmap.GetBitmap(&bmBitmap))
		return FALSE;
	CSize cSize(bmBitmap.bmWidth, bmBitmap.bmHeight);
	RGBTRIPLE* rgb = (RGBTRIPLE*)(bmBitmap.bmBits);
	int nCount = cSize.cx / nIconWidth;
	if (!imageList)
	{
		if (!imageList.Create(nIconWidth, cSize.cy, ILC_COLOR32 /*ILC_COLOR24*/ | ILC_MASK, nCount, 0))
			return FALSE;
	}

	if (imageList.Add(&bitmap, colMaskColor) == -1)
		return FALSE;
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
void CMFCUtils::TransparentBlt(HDC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HBITMAP hBitmap, int nXSrc, int nYSrc,
	COLORREF colorTransparent)
{
	CDC dc, memDC, maskDC, tempDC;
	dc.Attach(hdcDest);
	maskDC.CreateCompatibleDC(&dc);
	CBitmap maskBitmap;

	//add these to store return of SelectObject() calls
	CBitmap* pOldMemBmp = NULL;
	CBitmap* pOldMaskBmp = NULL;
	HBITMAP hOldTempBmp = NULL;

	memDC.CreateCompatibleDC(&dc);
	tempDC.CreateCompatibleDC(&dc);
	CBitmap bmpImage;
	bmpImage.CreateCompatibleBitmap(&dc, nWidth, nHeight);
	pOldMemBmp = memDC.SelectObject(&bmpImage);

	hOldTempBmp = (HBITMAP)::SelectObject(tempDC.m_hDC, hBitmap);

	memDC.BitBlt(0, 0, nWidth, nHeight, &tempDC, nXSrc, nYSrc, SRCCOPY);

	// Create monochrome bitmap for the mask
	maskBitmap.CreateBitmap(nWidth, nHeight, 1, 1, NULL);
	pOldMaskBmp = maskDC.SelectObject(&maskBitmap);
	memDC.SetBkColor(colorTransparent);

	// Create the mask from the memory DC
	maskDC.BitBlt(0, 0, nWidth, nHeight, &memDC,
		0, 0, SRCCOPY);

	// Set the background in memDC to black. Using SRCPAINT with black
	// and any other color results in the other color, thus making
	// black the transparent color
	memDC.SetBkColor(RGB(0, 0, 0));
	memDC.SetTextColor(RGB(255, 255, 255));
	memDC.BitBlt(0, 0, nWidth, nHeight, &maskDC, 0, 0, SRCAND);

	// Set the foreground to black. See comment above.
	dc.SetBkColor(RGB(255, 255, 255));
	dc.SetTextColor(RGB(0, 0, 0));
	dc.BitBlt(nXDest, nYDest, nWidth, nHeight, &maskDC, 0, 0, SRCAND);

	// Combine the foreground with the background
	dc.BitBlt(nXDest, nYDest, nWidth, nHeight, &memDC,
		0, 0, SRCPAINT);

	if (hOldTempBmp)
		::SelectObject(tempDC.m_hDC, hOldTempBmp);
	if (pOldMaskBmp)
		maskDC.SelectObject(pOldMaskBmp);
	if (pOldMemBmp)
		memDC.SelectObject(pOldMemBmp);

	dc.Detach();
}

//////////////////////////////////////////////////////////////////////////
void CMFCUtils::LoadShortcuts(CXTPCommandBars* pCommandBars, UINT nMenuIDResource, const char* pSectionNameForLoading)
{
	string shortcutsSection = "Shortcuts";
	shortcutsSection += "\\";
	shortcutsSection += pSectionNameForLoading;
	pCommandBars->GetShortcutManager()->SetAccelerators(nMenuIDResource);
	pCommandBars->GetShortcutManager()->LoadShortcuts(shortcutsSection);
}

//////////////////////////////////////////////////////////////////////////
void CMFCUtils::ShowShortcutsCustomizeDlg(CXTPCommandBars* pCommandBars, UINT nMenuIDResource, const char* pSectionNameForSaving)
{
	if (pCommandBars != NULL)
	{
		CXTPCustomizeSheet dlg(pCommandBars);

		// Remove unnecessary pages.
		for (int i = 0; i < dlg.GetPageCount(); ++i)
			dlg.RemovePage(i);
		dlg.RemovePage(dlg.GetCommandsPage());

		CXTPCustomizeKeyboardPage pageKeyboard(&dlg);
		dlg.AddPage(&pageKeyboard);
		pageKeyboard.AddCategories(nMenuIDResource, TRUE);
		dlg.SetActivePage(&pageKeyboard);

		dlg.DoModal();

		string shortcutsSection = "Shortcuts";
		shortcutsSection += "\\";
		shortcutsSection += pSectionNameForSaving;
		pCommandBars->GetShortcutManager()->SaveShortcuts(shortcutsSection);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMFCUtils::ExportShortcuts(CXTPShortcutManager* pShortcutMgr)
{
	CSystemFileDialog::RunParams params;
	params.extensionFilters = CExtensionFilter::Parse("HOT Files (*.hot)|*.hot||");

	auto filepath = CSystemFileDialog::RunExportFile(params);

	if (!filepath.isEmpty())
	{
		CStdioFile cFile;
		if (cFile.Open(filepath.toStdString().c_str(), CFile::modeCreate | CFile::modeWrite | CFile::typeText) == FALSE)
		{
			string errMsg;
			errMsg.Format("Writing to the file, %s failed!", filepath);
			CQuestionDialog::SCritical(QObject::tr(""), QObject::tr(errMsg));
			return;
		}

		CArchive archive(&cFile, CArchive::store);
		pShortcutMgr->SerializeShortcuts(archive);
		archive.Close();
		cFile.Close();
	}
}

//////////////////////////////////////////////////////////////////////////
void CMFCUtils::ImportShortcuts(CXTPShortcutManager* pShortcutMgr, const char* pSectionNameForSaving)
{
	CSystemFileDialog::RunParams params;
	params.extensionFilters = CExtensionFilter::Parse("HOT Files (*.hot)|*.hot||");

	auto filepath = CSystemFileDialog::RunImportFile(params);
	if (!filepath.isEmpty())
	{
		CStdioFile cFile;
		if (cFile.Open(filepath.toStdString().c_str(), CFile::modeRead | CFile::typeText) == FALSE)
		{
			string errMsg;
			errMsg.Format("Reading the file, %s failed!", filepath);
			CQuestionDialog::SCritical(QObject::tr(""), QObject::tr(errMsg));
			return;
		}

		CArchive archive(&cFile, CArchive::load);
		pShortcutMgr->SerializeShortcuts(archive);
		archive.Close();
		cFile.Close();

		if (pSectionNameForSaving)
		{
			string shortcutsSection = "Shortcuts";
			shortcutsSection += "\\";
			shortcutsSection += pSectionNameForSaving;
			pShortcutMgr->SaveShortcuts(shortcutsSection);
		}
	}
}

IVariable* CMFCUtils::GetChildVar(const IVariable* array, const char* name, bool recursive /*= false*/)
{
	if (array == 0 || array->IsEmpty())
		return 0;

	// first search top level
	for (int i = 0; i < array->GetNumVariables(); ++i)
	{
		IVariable* var = array->GetVariable(i);
		if (0 == strcmp(name, var->GetName()))
			return var;
	}
	if (recursive)
	{
		for (int i = 0; i < array->GetNumVariables(); ++i)
		{
			if (IVariable* pVar = GetChildVar(array->GetVariable(i), name, recursive))
				return pVar;
		}
	}
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
COLORREF CMFCUtils::ColorLinearToGamma(ColorF col)
{
	float r = clamp_tpl(col.r, 0.0f, 1.0f);
	float g = clamp_tpl(col.g, 0.0f, 1.0f);
	float b = clamp_tpl(col.b, 0.0f, 1.0f);

	r = (float)(r <= 0.0031308 ? (12.92 * r) : (1.055 * pow((double)r, 1.0 / 2.4) - 0.055));
	g = (float)(g <= 0.0031308 ? (12.92 * g) : (1.055 * pow((double)g, 1.0 / 2.4) - 0.055));
	b = (float)(b <= 0.0031308 ? (12.92 * b) : (1.055 * pow((double)b, 1.0 / 2.4) - 0.055));

	return RGB(FtoI(r * 255.0f), FtoI(g * 255.0f), FtoI(b * 255.0f));
}

//////////////////////////////////////////////////////////////////////////
ColorF CMFCUtils::ColorGammaToLinear(COLORREF col)
{
	float r = (float)GetRValue(col) / 255.0f;
	float g = (float)GetGValue(col) / 255.0f;
	float b = (float)GetBValue(col) / 255.0f;

	return ColorF((float)(r <= 0.04045 ? (r / 12.92) : pow(((double)r + 0.055) / 1.055, 2.4)),
		(float)(g <= 0.04045 ? (g / 12.92) : pow(((double)g + 0.055) / 1.055, 2.4)),
		(float)(b <= 0.04045 ? (b / 12.92) : pow(((double)b + 0.055) / 1.055, 2.4)));
}

//////////////////////////////////////////////////////////////////////////

QPoint CMFCUtils::CPointToQPointLocal(const QWidget* widget, const CPoint& point)
{
	return QPoint(QtUtil::QtScale(widget, point.x), QtUtil::QtScale(widget, point.y));
}

bool CMFCUtils::ExecuteConsoleApp(const string& CommandLine, string& OutputText, bool bNoTimeOut /*= false*/, bool bShowWindow /*= false*/)
{
	// Execute a console application and redirect its output to the console window
	SECURITY_ATTRIBUTES sa = { 0 };
	STARTUPINFO si = { 0 };
	PROCESS_INFORMATION pi = { 0 };
	HANDLE hPipeOutputRead = NULL;
	HANDLE hPipeOutputWrite = NULL;
	HANDLE hPipeInputRead = NULL;
	HANDLE hPipeInputWrite = NULL;
	BOOL bTest = FALSE;
	bool bReturn = true;
	DWORD dwNumberOfBytesRead = 0;
	DWORD dwStartTime = 0;
	char szCharBuffer[65];
	char szOEMBuffer[65];

	CryLog("Executing console application '%s'", (const char*)CommandLine);
	// Initialize the SECURITY_ATTRIBUTES structure
	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.bInheritHandle = TRUE;
	sa.lpSecurityDescriptor = NULL;
	// Create a pipe for standard output redirection
	VERIFY(CreatePipe(&hPipeOutputRead, &hPipeOutputWrite, &sa, 0));
	// Create a pipe for standard inout redirection
	VERIFY(CreatePipe(&hPipeInputRead, &hPipeInputWrite, &sa, 0));
	// Make a child process useing hPipeOutputWrite as standard out. Also
	// make sure it is not shown on the screen
	si.cb = sizeof(STARTUPINFO);
	si.dwFlags = STARTF_USESHOWWINDOW;

	if (bShowWindow == false)
	{
		si.dwFlags |= STARTF_USESTDHANDLES;
		si.hStdInput = hPipeInputRead;
		si.hStdOutput = hPipeOutputWrite;
		si.hStdError = hPipeOutputWrite;
	}

	si.wShowWindow = bShowWindow ? SW_SHOW : SW_HIDE;
	// Save the process start time
	dwStartTime = GetTickCount();

	// Launch the console application
	char cmdLine[1024];
	cry_strcpy(cmdLine, CommandLine);

	if (!CreateProcess(NULL, cmdLine, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi))
		return false;

	// If no process was spawned
	if (!pi.hProcess)
		bReturn = false;

	// Now that the handles have been inherited, close them
	CloseHandle(hPipeOutputWrite);
	CloseHandle(hPipeInputRead);

	if (bShowWindow == false)
	{
		// Capture the output of the console application by reading from hPipeOutputRead
		while (true)
		{
			// Read from the pipe
			bTest = ReadFile(hPipeOutputRead, &szOEMBuffer, 64, &dwNumberOfBytesRead, NULL);

			// Break when finished
			if (!bTest)
				break;

			// Break when timeout has been exceeded
			if (bNoTimeOut == false && GetTickCount() - dwStartTime > 5000)
				break;

			// Null terminate string
			szOEMBuffer[dwNumberOfBytesRead] = '\0';

			// Translate into ANSI
			VERIFY(OemToChar(szOEMBuffer, szCharBuffer));

			// Add it to the output text
			OutputText += szCharBuffer;
		}
	}

	// Wait for the process to finish
	WaitForSingleObject(pi.hProcess, bNoTimeOut ? INFINITE : 1000);

	return bReturn;
}

