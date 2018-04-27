// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "FacialVideoFrameDialog.h"
#include <atlimage.h>
#include "Controls/MemDC.h"

IMPLEMENT_DYNAMIC(CFacialVideoFrameDialog, CToolbarDialog)

BEGIN_MESSAGE_MAP(CFacialVideoFrameDialog, CToolbarDialog)
ON_WM_SIZE()
ON_WM_ACTIVATE()
ON_WM_PAINT()
END_MESSAGE_MAP()

class CImage2 : public ATL::CImage
{

};

CFacialVideoFrameDialog::CFacialVideoFrameDialog()
	: m_pContext(0),
	m_hAccelerators(0)
{
	m_image = new CImage2();
	SetResolution(100, 100, 24);
}

CFacialVideoFrameDialog::~CFacialVideoFrameDialog()
{
	SAFE_DELETE(m_image);
}

void CFacialVideoFrameDialog::SetContext(CFacialEdContext* pContext)
{
	m_pContext = pContext;
}

BOOL CFacialVideoFrameDialog::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message >= WM_KEYFIRST && pMsg->message <= WM_KEYLAST && m_hAccelerators)
		return ::TranslateAccelerator(m_hWnd, m_hAccelerators, pMsg);
	return CDialog::PreTranslateMessage(pMsg);
}

BOOL CFacialVideoFrameDialog::OnInitDialog()
{
	BOOL bValue = __super::OnInitDialog();

	//CRect rcIn;
	//GetClientRect(rcIn);
	//m_static.Create("", WS_CHILD|WS_VISIBLE|SS_BITMAP, rcIn, this, 0);

	//if (m_static.m_hWnd)
	//	m_static.SetBitmap(*m_image);

	//m_hAccelerators = LoadAccelerators(AfxGetApp()->m_hInstance, MAKEINTRESOURCE(IDR_FACED_MENU));

	return bValue;
}

void CFacialVideoFrameDialog::DoDataExchange(CDataExchange* pDX)
{
	__super::DoDataExchange(pDX);
}

void CFacialVideoFrameDialog::OnSize(UINT nType, int cx, int cy)
{
	m_offscreenBitmap.DeleteObject();
	if (!m_offscreenBitmap.GetSafeHandle())
	{
		CDC* pDC = GetDC();
		m_offscreenBitmap.CreateCompatibleBitmap(pDC, cx, cy);
		ReleaseDC(pDC);
	}

	__super::OnSize(nType, cx, cy);
	Invalidate(FALSE);

	/*
	   if (m_static.m_hWnd)
	   {
	   CRect rcClient;
	   GetClientRect(rcClient);
	   m_static.MoveWindow(rcClient);
	   m_static.Invalidate(FALSE);
	   }
	 */
}

void CFacialVideoFrameDialog::OnActivate(UINT nState, CWnd* pWndOther, BOOL bMinimized)
{
	__super::OnActivate(nState, pWndOther, bMinimized);

	/*
	   if (m_static.m_hWnd)
	   {
	   CRect rcClient;
	   GetClientRect(rcClient);
	   m_static.MoveWindow(rcClient);
	   }
	 */
}

void CFacialVideoFrameDialog::OnPaint()
{
	CPaintDC paintDC(this);

	HDC imageDC = m_image->GetDC();

	CRect client;
	GetClientRect(&client);

	if (!m_offscreenBitmap.GetSafeHandle())
	{
		m_offscreenBitmap.CreateCompatibleBitmap(&paintDC, client.Width(), client.Height());
	}

	float imageWidth = m_image->GetWidth();
	float imageHeight = m_image->GetHeight();
	float aspectRatio = imageWidth / max(imageHeight, 1.0f);
	float viewportWidth = client.right - client.left;
	float viewportHeight = client.bottom - client.top;
	float adjustedViewportWidth = min(viewportWidth, viewportHeight * aspectRatio);
	float adjustedViewportHeight = min(adjustedViewportWidth, adjustedViewportWidth / max(0.0001f, aspectRatio));

	{
		CMemoryDC dc(paintDC, &m_offscreenBitmap);

		CBrush bgBrush(RGB(0, 0, 0));
		dc.FillRect(&client, &bgBrush);
		dc.StretchBlt(
		  (client.left + client.right - int(adjustedViewportWidth)) / 2,
		  (client.top + client.bottom - int(adjustedViewportHeight)) / 2,
		  adjustedViewportWidth,
		  adjustedViewportHeight,
		  CDC::FromHandle(imageDC),
		  0,
		  0,
		  imageWidth,
		  imageHeight,
		  SRCCOPY);
	}

	m_image->ReleaseDC();
}

void CFacialVideoFrameDialog::SetResolution(int width, int height, int bpp)
{
	if (!*m_image || width != m_image->GetWidth() || height != -m_image->GetHeight())
	{
		m_image->Destroy();
		m_image->Create(width, -height, bpp, 0);
	}
}

void* CFacialVideoFrameDialog::GetBits()
{
	return m_image->GetBits();
}

int CFacialVideoFrameDialog::GetPitch()
{
	return m_image->GetPitch();
}

