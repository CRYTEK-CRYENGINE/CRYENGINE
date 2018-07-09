// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Clipboard.h"
#include "Util/CryMemFile.h"        // CCryMemFile
#include "Controls/QuestionDialog.h"

XmlNodeRef CClipboard::m_node;
string CClipboard::m_title;

//////////////////////////////////////////////////////////////////////////
// Clipboard implementation.
//////////////////////////////////////////////////////////////////////////
void CClipboard::Put(XmlNodeRef& node, const string& title)
{
	m_title = title;
	if (m_title.IsEmpty())
	{
		m_title = node->getTag();
	}
	m_node = node;

	PutString(m_node->getXML().c_str(), title);

	/*
	   COleDataSource	Source;
	   CSharedFile	sf(GMEM_MOVEABLE|GMEM_DDESHARE|GMEM_ZEROINIT);
	   string text = node->getXML();

	   sf.Write(text, text.GetLength());

	   HGLOBAL hMem = sf.Detach();
	   if (!hMem)
	   return;
	   Source.CacheGlobalData(CF_TEXT, hMem);
	   Source.SetClipboard();
	 */
}

//////////////////////////////////////////////////////////////////////////
string CClipboard::GetTitle() const 
{
	return m_title; 
}

//////////////////////////////////////////////////////////////////////////
XmlNodeRef CClipboard::Get() const
{
	string str = GetString();

	XmlNodeRef node = XmlHelpers::LoadXmlFromBuffer(str.GetBuffer(), str.GetLength());
	return node;

	/*
	   COleDataObject	obj;

	   if (obj.AttachClipboard()) {
	   if (obj.IsDataAvailable(CF_TEXT)) {
	    HGLOBAL hmem = obj.GetGlobalData(CF_TEXT);
	    CCryMemFile sf((BYTE*) ::GlobalLock(hmem), ::GlobalSize(hmem));
	    string buffer;

	    LPSTR str = buffer.GetBufferSetLength(::GlobalSize(hmem));
	    sf.Read(str, ::GlobalSize(hmem));
	    ::GlobalUnlock(hmem);

	    XmlNodeRef node = XmlHelpers::LoadXmlFromBuffer(buffer.GetBuffer(), buffer.GetLength());
	    return node;
	   }
	   }
	   return 0;

	   XmlNodeRef node = m_node;
	   //m_node = 0;
	   return node;
	 */
}

//////////////////////////////////////////////////////////////////////////
void CClipboard::PutString(const string& text, const string& title /* = ""  */)
{
	if (!OpenClipboard(NULL))
	{
		CQuestionDialog::SCritical(QObject::tr(""), QObject::tr("Cannot open the Clipboard"));
		return;
	}
	// Remove the current Clipboard contents
	if (!EmptyClipboard())
	{
		CQuestionDialog::SCritical(QObject::tr(""), QObject::tr("Cannot empty the Clipboard"));
		return;
	}

	CSharedFile sf(GMEM_MOVEABLE | GMEM_DDESHARE | GMEM_ZEROINIT);

	sf.Write(text, text.GetLength());

	HGLOBAL hMem = sf.Detach();
	if (!hMem)
		return;

	// For the appropriate data formats...
	if (::SetClipboardData(CF_TEXT, hMem) == NULL)
	{
		CQuestionDialog::SCritical(QObject::tr(""), QObject::tr("Unable to set Clipboard data"));
		CloseClipboard();
		return;
	}
	CloseClipboard();

	/*
	   COleDataSource	Source;
	   Source.CacheGlobalData(CF_TEXT, hMem);
	   Source.SetClipboard();
	 */
}

//////////////////////////////////////////////////////////////////////////
string CClipboard::GetString() const
{
	string buffer;
	if (OpenClipboard(NULL))
	{
		buffer = (char*)GetClipboardData(CF_TEXT);
	}
	CloseClipboard();

	return buffer;
}

//////////////////////////////////////////////////////////////////////////
bool CClipboard::IsEmpty() const
{
	return GetString().IsEmpty();

	//if (Get() != NULL)
	//return false;
	/*
	   COleDataObject	obj;
	   if (obj.AttachClipboard())
	   {
	   if (obj.IsDataAvailable(CF_TEXT))
	   {
	    return true;
	   }
	   }
	 */

	//return true;
}

//////////////////////////////////////////////////////////////////////////
void CClipboard::PutImage(const CImageEx& img)
{
	HBITMAP hBm = CreateBitmap(img.GetWidth(), img.GetHeight(), 1, 32, img.GetData());
	if (!hBm)
	{
		CQuestionDialog::SCritical(QObject::tr(""), QObject::tr("Cannot create Bitmap"));
		return;
	}

	if (!OpenClipboard(NULL))
	{
		CQuestionDialog::SCritical(QObject::tr(""), QObject::tr("Cannot open the Clipboard"));
		return;
	}
	// Remove the current Clipboard contents
	if (!EmptyClipboard())
	{
		CQuestionDialog::SCritical(QObject::tr(""), QObject::tr("Cannot empty the Clipboard"));
		return;
	}

	SetClipboardData(CF_BITMAP, hBm);
	CloseClipboard();

	DeleteObject(hBm);
}

//////////////////////////////////////////////////////////////////////////
bool CClipboard::GetImage(CImageEx& img)
{
	if (!OpenClipboard(NULL))
	{
		CQuestionDialog::SCritical(QObject::tr(""), QObject::tr("Cannot open the Clipboard"));
		return false;
	}

	HANDLE hDib = GetClipboardData(CF_DIB);
	if (!hDib)
	{
		CQuestionDialog::SCritical(QObject::tr(""), QObject::tr("No Bitmap in clipboard"));
		return false;
	}

	BITMAPINFO* pbi = (BITMAPINFO*)GlobalLock(hDib);

	img.Allocate(pbi->bmiHeader.biWidth, pbi->bmiHeader.biHeight);

	unsigned char* pSrc = (unsigned char*)&pbi->bmiColors;
	unsigned char* pDst = (unsigned char*)img.GetData();

	if (pbi->bmiHeader.biCompression == BI_BITFIELDS)
		pSrc += 3 * sizeof(DWORD);

	int stSrc = (pbi->bmiHeader.biBitCount == 24) ? 3 : 4;

	for (int y = 0; y < pbi->bmiHeader.biHeight; y++)
		for (int x = 0; x < pbi->bmiHeader.biWidth; x++)
		{
			pDst[x * 4 + (pbi->bmiHeader.biHeight - y - 1) * pbi->bmiHeader.biWidth * 4] = pSrc[x * stSrc + y * pbi->bmiHeader.biWidth * stSrc];
			pDst[x * 4 + (pbi->bmiHeader.biHeight - y - 1) * pbi->bmiHeader.biWidth * 4 + 1] = pSrc[x * stSrc + y * pbi->bmiHeader.biWidth * stSrc + 1];
			pDst[x * 4 + (pbi->bmiHeader.biHeight - y - 1) * pbi->bmiHeader.biWidth * 4 + 2] = pSrc[x * stSrc + y * pbi->bmiHeader.biWidth * stSrc + 2];
			pDst[x * 4 + (pbi->bmiHeader.biHeight - y - 1) * pbi->bmiHeader.biWidth * 4 + 3] = 0;
		}

	GlobalUnlock(pbi);
	CloseClipboard();

	return true;
}

