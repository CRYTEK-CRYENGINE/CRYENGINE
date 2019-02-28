// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "WaveGraphCtrl.h"
#include "Controls/MemDC.h"
#include "Util/GridUtils.h"
#include "Mmsystem.h"
#include "Controls/SharedFonts.h"

IMPLEMENT_DYNAMIC(CWaveGraphCtrl, CWnd)

#define ACTIVE_BKG_COLOR RGB(190, 190, 190)
#define GRID_COLOR       RGB(110, 110, 110)
LINK_SYSTEM_LIBRARY("Winmm.lib")

//////////////////////////////////////////////////////////////////////////
CWaveGraphCtrl::CWaveGraphCtrl()
{
	m_pContext = 0;
	m_editMode = eNothingMode;

	m_nTimer = -1;

	m_nLeftOffset = 40;
	m_fTimeMarker = 0;
	m_timeRange.Set(0, 1);
	m_TimeUpdateRect.SetRectEmpty();

	m_bottomWndHeight = 0;
	m_pBottomWnd = 0;

	m_bScrubbing = false;
	m_bPlaying = false;
	m_fPlaybackSpeed = 1.0f;

	//m_fLastTimeCheck = gEnv->pTimer->GetAsyncTime();
	m_lastTimeCheck = timeGetTime();
}

CWaveGraphCtrl::~CWaveGraphCtrl()
{
	if (m_nTimer != -1)
		KillTimer(m_nTimer);
}

//////////////////////////////////////////////////////////////////////////
int CWaveGraphCtrl::AddWaveform()
{
	int index = m_waveforms.size();
	m_waveforms.resize(m_waveforms.size() + 1);
	m_waveforms[index].itSound = m_soundCache.end();
	return index;
}

//////////////////////////////////////////////////////////////////////////
void CWaveGraphCtrl::DeleteWaveform(int index)
{
	//REINST("do we still need this?");
	std::vector<Waveform>::iterator itWaveform = m_waveforms.begin() + index;
	SoundCache::iterator itSound = (*itWaveform).itSound;
	/*if (itSound != m_soundCache.end())
	   --(*itSound).second.refcount;*/
	m_waveforms.erase(itWaveform);
}

//////////////////////////////////////////////////////////////////////////
int CWaveGraphCtrl::GetWaveformCount()
{
	return m_waveforms.size();
}

//////////////////////////////////////////////////////////////////////////
void CWaveGraphCtrl::SetWaveformTime(int index, float time)
{
	m_waveforms[index].time = time;
}

//////////////////////////////////////////////////////////////////////////
void CWaveGraphCtrl::LoadWaveformSound(int index, const CString& soundFile)
{
	m_bScrubbing = false;

	// Check whether the sound is in the cache.
	bool bSoundLoaded = false;

	//SoundCache::iterator itSound = m_soundCache.find(soundFile);

	//if (itSound == m_soundCache.end())
	//{
	//	CWaveFileReader* pWaveFileReader = new CWaveFileReader;

	//	// [MichaelS 06/03/2007] Temporarily removed FLAG_SOUND_START_PAUSED since new culling rules stop sound from playing if this is set.
	//	// When Tomas returns from GDC I shall confront him about it!
	//	_smart_ptr<ISound> pSound = gEnv->pAudioSystem->CreateSound( soundFile,FLAG_SOUND_EDITOR|FLAG_SOUND_3D|FLAG_SOUND_RELATIVE|FLAG_SOUND_16BITS|FLAG_SOUND_LOAD_SYNCHRONOUSLY/*|FLAG_SOUND_START_PAUSED*/|FLAG_SOUND_LOOP );
	//	if (pSound)
	//	{
	//		// need to set it
	//		pSound->SetPosition(Vec3(1,1,1));
	//		pSound->SetPosition(Vec3(0));
	//		pSound->GetInterfaceExtended()->SetPitch(1000 * m_fPlaybackSpeed);
	//		pSound->Play();
	//		pSound->SetPaused(true);

	//		if ( !pWaveFileReader->LoadFile(soundFile) )
	//		{
	//			pWaveFileReader->GetSoundBufferInfo(pSound->GetSoundBufferInfo());
	//			pWaveFileReader->SetLoaded(false);
	//		}
	//	}
	//	itSound = m_soundCache.insert(std::make_pair(soundFile, SoundCacheEntry(soundFile, pWaveFileReader, pSound, 0))).first;
	//}

	//++(*itSound).second.refcount;
	//m_waveforms[index].itSound = itSound;

	//m_fLastTimeCheck = gEnv->pTimer->GetAsyncTime();
	m_lastTimeCheck = timeGetTime();
}

//////////////////////////////////////////////////////////////////////////
void CWaveGraphCtrl::DeleteUnusedSounds()
{
	//for (SoundCache::iterator itSound = m_soundCache.begin(), itSoundEnd = m_soundCache.end(); itSound != itSoundEnd;)
	//{
	//	if ((*itSound).second.refcount == 0)
	//	{
	//		// Also need to stop the sound before releasing it (since it may not be deleted if it is already playing).
	//		if ((*itSound).second.pSound && (*itSound).second.pSound->IsPlaying())
	//			(*itSound).second.pSound->Stop();
	//		delete (*itSound).second.pWaveFileReader;
	//		m_soundCache.erase(itSound++);
	//	}
	//	else
	//	{
	//		++itSound;
	//	}
	//}
}

//////////////////////////////////////////////////////////////////////////
float CWaveGraphCtrl::GetWaveformLength(int waveformIndex)
{
	return 0.0f;
	/*if (waveformIndex < 0 || waveformIndex >= int(m_waveforms.size()))
	   return 0.0f;
	   CWaveFileReader* pReader = (*m_waveforms[waveformIndex].itSound).second.pWaveFileReader;
	   return (pReader ? pReader->GetLengthMs() / 1000.0f : 0.0f);*/
}

//////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CWaveGraphCtrl, CWnd)
ON_WM_SIZE()
ON_WM_ERASEBKGND()
ON_WM_PAINT()
ON_WM_LBUTTONDOWN()
ON_WM_RBUTTONDOWN()
ON_WM_MOUSEMOVE()
ON_WM_LBUTTONUP()
ON_WM_SETCURSOR()
ON_WM_RBUTTONDOWN()
ON_WM_TIMER()
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CSplineCtrl message handlers

/////////////////////////////////////////////////////////////////////////////
BOOL CWaveGraphCtrl::Create(DWORD dwStyle, const CRect& rc, CWnd* pParentWnd, UINT nID)
{
	return CreateEx(0, NULL, "SplineCtrl", dwStyle, rc, pParentWnd, nID);
}

//////////////////////////////////////////////////////////////////////////
BOOL CWaveGraphCtrl::OnEraseBkgnd(CDC* pDC)
{
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
void CWaveGraphCtrl::OnSize(UINT nType, int cx, int cy)
{
	__super::OnSize(nType, cx, cy);
	GetClientRect(m_rcClient);
	m_rcWaveGraph = m_rcClient;
	m_rcWaveGraph.left = m_nLeftOffset;

	if (m_pBottomWnd)
	{
		m_rcWaveGraph.bottom -= m_bottomWndHeight;
		CRect rc(m_rcWaveGraph);
		rc.top = m_rcClient.bottom - m_bottomWndHeight;
		rc.bottom = m_rcClient.bottom;
		m_pBottomWnd->MoveWindow(rc);
	}

	m_grid.rect = m_rcWaveGraph;

	m_offscreenBitmap.DeleteObject();
	if (!m_offscreenBitmap.GetSafeHandle())
	{
		CDC* pDC = GetDC();
		m_offscreenBitmap.CreateCompatibleBitmap(pDC, cx, cy);
		ReleaseDC(pDC);
	}
}

//////////////////////////////////////////////////////////////////////////
void CWaveGraphCtrl::SetContext(CFacialEdContext* pContext)
{
	m_pContext = pContext;
	if (m_pContext)
		m_pContext->RegisterListener(this);
}

//////////////////////////////////////////////////////////////////////////
void CWaveGraphCtrl::OnFacialEdEvent(EFacialEdEvent event, IFacialEffector* pEffector, int nChannelCount, IFacialAnimChannel** ppChannels)
{
	switch (event)
	{
	case EFD_EVENT_ADD:
		break;
	case EFD_EVENT_REMOVE:
		break;
	case EFD_EVENT_CHANGE:
		break;
	case EFD_EVENT_SELECT_EFFECTOR:
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
int CWaveGraphCtrl::HitTestWaveforms(const CPoint point)
{
	float time = ClientToWorld(point).x;
	int hitWaveform = -1;
	/*for (int waveFormIndex = 0, waveFormCount = m_waveforms.size(); waveFormIndex < waveFormCount; ++waveFormIndex)
	   {
	   ISound* pSound = (*m_waveforms[waveFormIndex].itSound).second.pSound;
	   CWaveFileReader* pReader = (*m_waveforms[waveFormIndex].itSound).second.pWaveFileReader;
	   float startTime = m_waveforms[waveFormIndex].time;
	   float soundLength = max((pReader ? pReader->GetLengthMs() / 1000.0f : 0.0f), 0.5f);
	   float endTime = startTime + soundLength;
	   if (time >= startTime && time <= endTime)
	    hitWaveform = waveFormIndex;
	   }*/
	return hitWaveform;
}

//////////////////////////////////////////////////////////////////////////
void CWaveGraphCtrl::SendNotifyMessage(int code)
{
	ASSERT(::IsWindow(m_hWnd));
	NMHDR nmh;
	nmh.hwndFrom = m_hWnd;
	nmh.idFrom = ::GetDlgCtrlID(m_hWnd);
	nmh.code = code;
	SendNotifyMessageStructure(&nmh);
}

//////////////////////////////////////////////////////////////////////////
void CWaveGraphCtrl::SendNotifyMessageStructure(NMHDR* hdr)
{
	GetOwner()->SendMessage(WM_NOTIFY, (WPARAM)GetDlgCtrlID(), (LPARAM)hdr);
}

//////////////////////////////////////////////////////////////////////////
void CWaveGraphCtrl::OnPaint()
{
	CPaintDC PaintDC(this);

	CRect rcClient;
	GetClientRect(&rcClient);

	if (!m_offscreenBitmap.GetSafeHandle())
	{
		m_offscreenBitmap.CreateCompatibleBitmap(&PaintDC, rcClient.Width(), rcClient.Height());
	}

	{
		CMemoryDC dc(PaintDC, &m_offscreenBitmap);

		if (m_TimeUpdateRect != CRect(PaintDC.m_ps.rcPaint))
		{
			CBrush bkBrush;
			bkBrush.CreateSolidBrush(RGB(160, 160, 160));
			dc.FillRect(&PaintDC.m_ps.rcPaint, &bkBrush);

			m_grid.CalculateGridLines();
			DrawGrid(&dc);

			DrawWaveGraph(&dc);
		}
		m_TimeUpdateRect.SetRectEmpty();
	}

	DrawTimeMarker(&PaintDC);
}

//////////////////////////////////////////////////////////////////////////
void CWaveGraphCtrl::DrawTimeMarker(CDC* pDC)
{
	if (!(GetStyle() & WAVCTRLN_STYLE_NO_TIME_MARKER))
	{
		CPen timePen;
		timePen.CreatePen(PS_SOLID, 1, RGB(255, 0, 255));
		CPen* pOldPen = pDC->SelectObject(&timePen);
		CPoint pt = WorldToClient(Vec2(m_fTimeMarker, 0));
		if (pt.x >= m_rcWaveGraph.left && pt.x <= m_rcWaveGraph.right)
		{
			pDC->MoveTo(pt.x, m_rcWaveGraph.top + 1);
			pDC->LineTo(pt.x, m_rcWaveGraph.bottom - 1);
		}
		pDC->SelectObject(pOldPen);
	}
}

//////////////////////////////////////////////////////////////////////////
class VerticalLineDrawer
{
public:
	VerticalLineDrawer(CDC& dc, const CRect& rect)
		: rect(rect),
		dc(dc)
	{
	}

	void operator()(int frameIndex, int x)
	{
		dc.MoveTo(x, rect.top);
		dc.LineTo(x, rect.bottom);
	}

	CDC&  dc;
	CRect rect;
};
void CWaveGraphCtrl::DrawGrid(CDC* pDC)
{
	if (GetStyle() & WAVCTRLN_STYLE_NOGRID)
		return;

	CPoint pt0 = WorldToClient(Vec2(m_timeRange.start, 0));
	CPoint pt1 = WorldToClient(Vec2(m_timeRange.end, 0));
	CRect timeRc = CRect(pt0.x - 2, m_rcWaveGraph.top, pt1.x + 2, m_rcWaveGraph.bottom);
	timeRc.IntersectRect(timeRc, m_rcWaveGraph);
	pDC->FillSolidRect(timeRc, ACTIVE_BKG_COLOR);

	CPen penGridSolid;
	penGridSolid.CreatePen(PS_SOLID, 1, GRID_COLOR);
	//////////////////////////////////////////////////////////////////////////
	CPen* pOldPen = pDC->SelectObject(&penGridSolid);

	/// Draw Left Separator.
	CRect leftRect = CRect(m_rcClient.left, m_rcClient.top, m_rcClient.left + m_nLeftOffset - 1, m_rcClient.bottom);
	pDC->FillSolidRect(leftRect, ACTIVE_BKG_COLOR);
	pDC->MoveTo(m_rcClient.left + m_nLeftOffset, m_rcClient.bottom);
	pDC->LineTo(m_rcClient.left + m_nLeftOffset, m_rcClient.top);
	pDC->SetTextColor(RGB(0, 0, 0));
	pDC->SetBkMode(TRANSPARENT);
	pDC->SelectObject(SMFCFonts::GetInstance().hSystemFont);
	pDC->DrawText("WAV", CRect(m_rcClient.left, m_rcWaveGraph.top, m_rcClient.left + m_nLeftOffset - 1, m_rcWaveGraph.bottom), DT_CENTER | DT_VCENTER | DT_SINGLELINE);

	if (m_pBottomWnd)
	{
		pDC->DrawText("Lip\r\nSync", CRect(m_rcClient.left, m_rcWaveGraph.bottom + 4, m_rcClient.left + m_nLeftOffset - 1, m_rcClient.bottom), DT_CENTER | DT_VCENTER);
	}

	//////////////////////////////////////////////////////////////////////////
	int gy;

	LOGBRUSH logBrush;
	logBrush.lbStyle = BS_SOLID;
	logBrush.lbColor = GRID_COLOR;

	CPen pen;
	pen.CreatePen(PS_COSMETIC | PS_ALTERNATE, 1, &logBrush);
	pDC->SelectObject(&pen);

	pDC->SetTextColor(RGB(0, 0, 0));
	pDC->SetBkMode(TRANSPARENT);
	pDC->SelectObject(SMFCFonts::GetInstance().hSystemFont);

	// Draw horizontal grid lines.
	for (gy = m_grid.firstGridLine.y; gy < m_grid.firstGridLine.y + m_grid.numGridLines.y + 1; gy++)
	{
		int y = m_grid.GetGridLineY(gy);
		if (y < 0)
			continue;
		int py = m_rcWaveGraph.bottom - y;
		if (py < m_rcWaveGraph.top || py > m_rcWaveGraph.bottom)
			continue;
		pDC->MoveTo(m_rcWaveGraph.left, py);
		pDC->LineTo(m_rcWaveGraph.right, py);
	}

	// Draw vertical grid lines.
	VerticalLineDrawer verticalLineDrawer(*pDC, m_rcWaveGraph);
	GridUtils::IterateGrid(verticalLineDrawer, 50.0f, m_grid.zoom.x, m_grid.origin.x, FACIAL_EDITOR_FPS, m_grid.rect.left, m_grid.rect.right);

	//////////////////////////////////////////////////////////////////////////
	{
		CPen pen0;
		pen0.CreatePen(PS_SOLID, 2, RGB(110, 100, 100));
		CPoint p = WorldToClient(Vec2(0, 0));

		pDC->SelectObject(&pen0);

		/// Draw X axis.
		pDC->MoveTo(m_rcWaveGraph.left, p.y);
		pDC->LineTo(m_rcWaveGraph.right, p.y);

		// Draw Y Axis.
		if (p.x > m_rcWaveGraph.left && p.y < m_rcWaveGraph.right)
		{
			pDC->MoveTo(p.x, m_rcWaveGraph.top);
			pDC->LineTo(p.x, m_rcWaveGraph.bottom);
		}
	}
	//////////////////////////////////////////////////////////////////////////

	pDC->SelectObject(pOldPen);
}

//////////////////////////////////////////////////////////////////////////
void CWaveGraphCtrl::DrawWaveGraph(CDC* pDC)
{
	//int cx = m_rcWaveGraph.Width();
	//int cy = m_rcWaveGraph.Height();

	//for (int waveFormIndex = 0, waveFormCount = m_waveforms.size(); waveFormIndex < waveFormCount; ++waveFormIndex)
	//{
	//	CString& soundFilename = (*m_waveforms[waveFormIndex].itSound).second.soundFilename;
	//	_smart_ptr<ISound>& pSound = (*m_waveforms[waveFormIndex].itSound).second.pSound;
	//	CWaveFileReader* pWaveFileReader = (*m_waveforms[waveFormIndex].itSound).second.pWaveFileReader;

	//	if ( !pWaveFileReader->IsLoaded() )
	//		continue;

	//	float startTime = m_waveforms[waveFormIndex].time;
	//	CString& text = m_waveforms[waveFormIndex].text;

	//	int minX = WorldToClient( Vec2(startTime,0) ).x;

	//	// Draw sound name.
	//	{
	//		pDC->SetTextColor( RGB(70, 70, 70) );
	//		pDC->SetBkMode( TRANSPARENT );
	//
	//		pDC->TextOut( minX+4,m_rcWaveGraph.top+4,soundFilename );

	//		if (!text.IsEmpty())
	//			pDC->TextOut( minX+4,m_rcWaveGraph.bottom-16,text );
	//	}

	//	//Draw Curve
	//	// create and select a thick, white pen
	//	CPen pen;
	//	pen.CreatePen(PS_SOLID, 1, RGB(128, 255, 128));
	//	CPen* pOldPen = pDC->SelectObject(&pen);

	//	CRect rcClip;
	//	pDC->GetClipBox(rcClip);
	//	rcClip.IntersectRect(rcClip,m_rcWaveGraph);

	//	float fRatio = pWaveFileReader->GetSampleCount()/(float)cx;
	//	uint32 nSamplesPerSec = pWaveFileReader->GetSamplesPerSec();
	//	uint32 nSamplesPerPixel = pWaveFileReader->GetSamplesPerSec() / m_grid.zoom.x;
	//	float fWavLengthSec = pWaveFileReader->GetLengthMs() / 1000.0f;
	//	float fPixelsPerSec = float(nSamplesPerSec) / float(nSamplesPerPixel ? nSamplesPerPixel : 1);
	//	int nWavLengthPixels = fPixelsPerSec * fWavLengthSec;

	//	if (nSamplesPerPixel <= 0)
	//		nSamplesPerPixel = 1;

	//	if (nSamplesPerPixel > 1000)
	//		nSamplesPerPixel = 1000;

	//	int maxX = minX + nWavLengthPixels;

	//	bool bFirst = true;
	//	for (int x = max(minX,(int)rcClip.left), right = min(maxX, (int)rcClip.right); x < right; x++)
	//	{
	//		Vec2 v = ClientToWorld( CPoint(x,0) ) - Vec2(startTime, 0.0f);
	//		if (v.x < 0)
	//			continue;
	//		if (v.x > fWavLengthSec)
	//			break;
	//		float fMinValue = 0.0f;
	//		float fMaxValue = 0.0f;
	//		int nSample = v.x*nSamplesPerSec;
	//		pWaveFileReader->GetSamplesMinMax( nSample,nSamplesPerPixel,fMinValue,fMaxValue );

	//		pDC->MoveTo(x,cy/2+fMaxValue*(cy/2));
	//		pDC->LineTo(x,cy/2+fMinValue*(cy/2));
	//	}

	//	// Put back the old objects
	//	pDC->SelectObject(pOldPen);
	//}
}

//////////////////////////////////////////////////////////////////////////
void CWaveGraphCtrl::SetWaveformTextString(int waveformIndex, const CString& text)
{
	if (waveformIndex < 0 || waveformIndex >= int(m_waveforms.size()))
		return;

	m_waveforms[waveformIndex].text = text;
}

/////////////////////////////////////////////////////////////////////////////
//Mouse Message Handlers
//////////////////////////////////////////////////////////////////////////
void CWaveGraphCtrl::OnLButtonDown(UINT nFlags, CPoint point)
{
	CWnd::OnLButtonDown(nFlags, point);

	m_StartClickPoint = point;

	if (nFlags & MK_CONTROL)
	{
		int waveformIndex = HitTestWaveforms(point);
		if (waveformIndex >= 0)
		{
			m_editMode = eWaveDragMode;
			m_nWaveformBeingDragged = waveformIndex;
			SendNotifyMessage(WAVECTRLN_BEGIN_MOVE_WAVEFORM);
		}
	}
	else
	{
		m_editMode = eClickingMode;

		//m_fLastTimeCheck = gEnv->pTimer->GetAsyncTime();
		m_lastTimeCheck = timeGetTime();
		SetTimeMarkerInternal(max(m_timeRange.start, min(m_timeRange.end, FacialEditorSnapTimeToFrame(ClientToWorld(point).x))));

		SendNotifyMessage(WAVCTRLN_TIME_CHANGE);

		SetCapture();
	}
}

//////////////////////////////////////////////////////////////////////////
void CWaveGraphCtrl::OnRButtonDown(UINT nFlags, CPoint point)
{
	CWnd::OnRButtonDown(nFlags, point);

	WaveGraphCtrlRClickNotification notification;
	notification.hdr.hwndFrom = m_hWnd;
	notification.hdr.idFrom = ::GetDlgCtrlID(m_hWnd);
	notification.hdr.code = WAVECTRLN_RCLICK;

	notification.waveformIndex = HitTestWaveforms(point);

	SendNotifyMessageStructure(&notification.hdr);
}

//////////////////////////////////////////////////////////////////////////
void CWaveGraphCtrl::OnMouseMove(UINT nFlags, CPoint point)
{
	CWnd::OnMouseMove(nFlags, point);

	switch (m_editMode)
	{
	case eWaveDragMode:
		{
			SendNotifyMessage(WAVECTRLN_RESET_CHANGES);

			// Work out how far to drag the waveform in seconds.
			float dt = (point.x - m_StartClickPoint.x) / m_grid.zoom.x;

			// Pass on the changes to our parent.
			ASSERT(m_nWaveformBeingDragged >= 0 && m_nWaveformBeingDragged < int(m_waveforms.size()));
			WaveGraphCtrlWaveformChangeNotification notification;
			notification.hdr.hwndFrom = m_hWnd;
			notification.hdr.idFrom = ::GetDlgCtrlID(m_hWnd);
			notification.hdr.code = WAVECTRLN_MOVE_WAVEFORMS;
			notification.waveformIndex = m_nWaveformBeingDragged;
			notification.deltaTime = dt;
			SendNotifyMessageStructure(&notification.hdr);

		}
		break;

	case eClickingMode:
		{
			if (point.x == m_StartClickPoint.x && point.y == m_StartClickPoint.y)
				return;

			//m_fLastTimeCheck = gEnv->pTimer->GetAsyncTime();
			m_editMode = eScrubbingMode;
			m_bScrubbing = true;
			m_lastTimeCheck = timeGetTime();
			float fNewTime = min(m_timeRange.end, ClientToWorld(point).x);
			SetTimeMarkerInternal(fNewTime);
			StartSoundsAtTime(fNewTime, true);

			NMHDR nmh;
			nmh.hwndFrom = m_hWnd;
			nmh.idFrom = ::GetDlgCtrlID(m_hWnd);
			nmh.code = WAVCTRLN_TIME_CHANGE;

			GetOwner()->SendMessage(WM_NOTIFY, (WPARAM)GetDlgCtrlID(), (LPARAM)&nmh);
		}
		break;
	case eScrubbingMode:
		{
			if (point.x == m_StartClickPoint.x && point.y == m_StartClickPoint.y)
				return;

			float fNewTime = min(m_timeRange.end, ClientToWorld(point).x);
			SetTimeMarkerInternal(fNewTime);
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CWaveGraphCtrl::OnLButtonUp(UINT nFlags, CPoint point)
{
	switch (m_editMode)
	{
	case eScrubbingMode:
		//m_fLastTimeCheck = gEnv->pTimer->GetAsyncTime();
		m_lastTimeCheck = timeGetTime();
		SetTimeMarkerInternal(min(m_timeRange.end, FacialEditorSnapTimeToFrame(ClientToWorld(point).x)));
		ReleaseCapture();
		m_editMode = eNothingMode;
		break;

	case eWaveDragMode:
		SendNotifyMessage(WAVECTRLN_END_MOVE_WAVEFORM);
		m_editMode = eNothingMode;
		break;

	case eClickingMode:
		//SendNotifyMessage(WAVECTRLN_END_MOVE_WAVEFORM);
		m_editMode = eNothingMode;
		break;
	}

	m_bScrubbing = false;

	UpdatePlayback();

	CWnd::OnLButtonUp(nFlags, point);
}

/////////////////////////////////////////////////////////////////////////////
BOOL CWaveGraphCtrl::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
	BOOL b = FALSE;

	CPoint point;
	GetCursorPos(&point);
	ScreenToClient(&point);

	if (!b)
		return CWnd::OnSetCursor(pWnd, nHitTest, message);
	else return TRUE;
}

void CWaveGraphCtrl::StartPlayback()
{
	m_bPlaying = true;
	m_bScrubbing = false;
	//m_fLastTimeCheck = gEnv->pTimer->GetAsyncTime();
	m_lastTimeCheck = timeGetTime();
	UpdatePlayback();
}

void CWaveGraphCtrl::StopPlayback()
{
	m_bPlaying = false;

	/*for (int waveformIndex = 0, waveformCount = m_waveforms.size(); waveformIndex < waveformCount; ++waveformIndex)
	   {
	   ISound* pSound = (*m_waveforms[waveformIndex].itSound).second.pSound;
	   if (pSound && pSound->IsPlaying())
	   {
	    pSound->GetInterfaceExtended()->SetCurrentSamplePos(0.0f, true);
	    pSound->SetPaused(true);
	   }
	   }*/
	m_fTimeMarker = 0.0f;
	Invalidate();
}

void CWaveGraphCtrl::PausePlayback()
{
	m_bPlaying = false;

	/*for (int waveformIndex = 0, waveformCount = m_waveforms.size(); waveformIndex < waveformCount; ++waveformIndex)
	   {
	   ISound* pSound = (*m_waveforms[waveformIndex].itSound).second.pSound;
	   if (pSound && pSound->IsPlaying())
	   {
	    pSound->GetInterfaceExtended()->SetCurrentSamplePos((m_fTimeMarker - m_waveforms[waveformIndex].time) * 1000.0f, true);
	    pSound->SetPaused(true);
	   }
	   }*/
}

void CWaveGraphCtrl::BeginScrubbing()
{
	m_bScrubbing = true;
}

void CWaveGraphCtrl::EndScrubbing()
{
	/*m_bScrubbing = false;
	   for (int waveformIndex = 0, waveformCount = m_waveforms.size(); waveformIndex < waveformCount; ++waveformIndex)
	   {
	   ISound* pSound = (*m_waveforms[waveformIndex].itSound).second.pSound;
	   if (pSound && pSound->IsPlaying() && m_bPlaying)
	   {
	    pSound->GetInterfaceExtended()->SetCurrentSamplePos((m_fTimeMarker - m_waveforms[waveformIndex].time * 1000.0f), true);
	    pSound->SetPaused(false);
	    pSound->GetInterfaceExtended()->SetPitch(1000 * m_fPlaybackSpeed);
	   }
	   }*/
}

void CWaveGraphCtrl::SetPlaybackSpeed(float fSpeed)
{
	/*if (m_fPlaybackSpeed != fSpeed)
	   {
	   m_fPlaybackSpeed = fSpeed;
	   for (int waveformIndex = 0, waveformCount = m_waveforms.size(); waveformIndex < waveformCount; ++waveformIndex)
	   {
	    ISound* pSound = (*m_waveforms[waveformIndex].itSound).second.pSound;
	    if (pSound && pSound->IsPlaying())
	      pSound->GetInterfaceExtended()->SetPitch(1000 * m_fPlaybackSpeed);
	   }
	   }*/
}

float CWaveGraphCtrl::GetPlaybackSpeed()
{
	return m_fPlaybackSpeed;
}

float CWaveGraphCtrl::GetTimeMarker()
{
	return m_fTimeMarker;
}

//////////////////////////////////////////////////////////////////////////
float CWaveGraphCtrl::CalculateTimeRange()
{
	float minT = 0, maxT = 1;
	/*for (int waveformIndex = 0, waveformCount = m_waveforms.size(); waveformIndex < waveformCount; ++waveformIndex)
	   {
	   float startTime = m_waveforms[waveformIndex].time;
	   CWaveFileReader* pWaveFileReader = (*m_waveforms[waveformIndex].itSound).second.pWaveFileReader;
	   float endTime = startTime + (pWaveFileReader->GetLengthMs() / 1000.0f);
	   minT = min(minT, startTime), maxT = max(maxT, endTime);
	   }*/
	return maxT;
}

//////////////////////////////////////////////////////////////////////////
void CWaveGraphCtrl::SetTimeMarker(float fTime)
{
	if (fTime == m_fTimeMarker)
		return;

	SetTimeMarkerInternal(fTime);
	StartSoundsAtTime(fTime, true);
}

//////////////////////////////////////////////////////////////////////////
void CWaveGraphCtrl::SetTimeMarkerInternal(float fTime)
{
	if (fTime == m_fTimeMarker)
		return;

	// Erase old first.
	CPoint pt0 = WorldToClient(Vec2(m_fTimeMarker, 0));
	CPoint pt1 = WorldToClient(Vec2(fTime, 0));
	CRect rc = CRect(pt0.x, m_rcWaveGraph.top, pt1.x, m_rcWaveGraph.bottom);
	rc.NormalizeRect();
	rc.InflateRect(5, 0);
	rc.IntersectRect(rc, m_rcWaveGraph);

	m_TimeUpdateRect = rc;
	InvalidateRect(rc);

	if (m_bScrubbing)
	{
		if (m_nTimer != -1)
			KillTimer(m_nTimer);

		m_nTimer = SetTimer(1, 300, 0);
	}

	m_fTimeMarker = fTime;
	UpdatePlayback();
}

//////////////////////////////////////////////////////////////////////////
void CWaveGraphCtrl::OnTimer(UINT_PTR nIDEvent)
{
	/*for (int waveformIndex = 0, waveformCount = m_waveforms.size(); waveformIndex < waveformCount; ++waveformIndex)
	   {
	   ISound* pSound = (*m_waveforms[waveformIndex].itSound).second.pSound;
	   if (pSound && pSound->IsPlaying() && !m_bPlaying)
	    pSound->SetPaused(true);
	   }*/

	if (m_nTimer != -1)
		KillTimer(m_nTimer);

	m_nTimer = -1;
}

//////////////////////////////////////////////////////////////////////////
CPoint CWaveGraphCtrl::WorldToClient(Vec2 v)
{
	CPoint p = m_grid.WorldToClient(v);
	p.y = m_rcWaveGraph.bottom - p.y;
	return p;
}

//////////////////////////////////////////////////////////////////////////
Vec2 CWaveGraphCtrl::ClientToWorld(CPoint point)
{
	Vec2 v = m_grid.ClientToWorld(CPoint(point.x, m_rcWaveGraph.bottom - point.y));
	return v;
}

//////////////////////////////////////////////////////////////////////////
void CWaveGraphCtrl::SetZoom(Vec2 zoom, CPoint center)
{
	m_grid.SetZoom(zoom, CPoint(center.x, m_rcWaveGraph.bottom - center.y));
	SetScrollOffset(m_grid.origin);
}

//////////////////////////////////////////////////////////////////////////
void CWaveGraphCtrl::SetZoom(Vec2 zoom)
{
	m_grid.zoom = zoom;
	SetScrollOffset(m_grid.origin);
}

//////////////////////////////////////////////////////////////////////////
void CWaveGraphCtrl::SetScrollOffset(Vec2 ofs)
{
	m_grid.origin = ofs;
	if (GetSafeHwnd())
		Invalidate();
}

//////////////////////////////////////////////////////////////////////////
void CWaveGraphCtrl::SetLeftOffset(int nLeft)
{
	m_nLeftOffset = nLeft;
	Invalidate();
}

//////////////////////////////////////////////////////////////////////////
void CWaveGraphCtrl::SetBottomWnd(CWnd* pWnd, int nHeight)
{
	m_pBottomWnd = pWnd;
	m_bottomWndHeight = nHeight;
	RedrawWindow();
}

//////////////////////////////////////////////////////////////////////////
float CWaveGraphCtrl::FindEndOfWaveforms()
{
	// Check whether we need to start playing any sounds at this new time.
	float endTime = 0.0f;
	/*for (int waveFormIndex = 0, waveFormCount = m_waveforms.size(); waveFormIndex < waveFormCount; ++waveFormIndex)
	   {
	   ISound* pSound = (*m_waveforms[waveFormIndex].itSound).second.pSound;
	   CWaveFileReader* pReader = (*m_waveforms[waveFormIndex].itSound).second.pWaveFileReader;
	   float startTime = m_waveforms[waveFormIndex].time;
	   endTime = max(endTime, startTime + (pReader ? (pReader->GetLengthMs() / 1000.0f) : 0.0f));
	   }*/
	return endTime;
}

//////////////////////////////////////////////////////////////////////////
void CWaveGraphCtrl::UpdatePlayback()
{
	if (!m_bPlaying || m_bScrubbing)
		return;

	float fTime = m_fTimeMarker;

	// Update the time marker based on real time.
	//CTimeValue currentRealTime = gEnv->pTimer->GetAsyncTime();
	DWORD currentRealTime = timeGetTime();
	float elapsedTime = float(currentRealTime - m_lastTimeCheck) / 1000.0f;
	fTime = m_fTimeMarker + elapsedTime * m_fPlaybackSpeed;

	// Try to keep the time synched with the sound engine - adjust the time if there are any sounds playing.
	/*for (int waveFormIndex = 0, waveFormCount = m_waveforms.size(); waveFormIndex < waveFormCount; ++waveFormIndex)
	   {
	   ISound* pSound = (*m_waveforms[waveFormIndex].itSound).second.pSound;
	   float startTime = m_waveforms[waveFormIndex].time;
	   float fTimeAccordingToSound = fTime;
	   bool soundFound = false;
	   if (pSound && !pSound->GetPaused())
	    soundFound = true, fTimeAccordingToSound = startTime + pSound->GetInterfaceExtended()->GetCurrentSamplePos(true) / 1000.0f;
	   if (soundFound && fabs(fTimeAccordingToSound - fTime) < 0.1f)
	   {
	    fTime = fTimeAccordingToSound;
	   }
	   }*/

	// Check whether the time is past the end.
	if (fTime > m_timeRange.end)
		fTime = m_timeRange.start;

	StartSoundsAtTime(fTime, false);

	// If the time has changed, update the view.
	if (fTime != m_fTimeMarker)
	{
		// Erase old first.
		CPoint pt0 = WorldToClient(Vec2(m_fTimeMarker, 0));
		CPoint pt1 = WorldToClient(Vec2(fTime, 0));
		CRect rc = CRect(pt0.x, m_rcWaveGraph.top, pt1.x, m_rcWaveGraph.bottom);
		rc.NormalizeRect();
		rc.InflateRect(5, 0);
		rc.IntersectRect(rc, m_rcWaveGraph);

		m_TimeUpdateRect = rc;
		InvalidateRect(rc);

		m_fTimeMarker = fTime;
	}

	//m_fLastTimeCheck = currentRealTime;
	m_lastTimeCheck = currentRealTime;
}

//////////////////////////////////////////////////////////////////////////
struct WaveformSortPredicate
{
	WaveformSortPredicate(const std::vector<CWaveGraphCtrl::Waveform>& waveforms) : waveforms(waveforms) {}
	bool operator()(int left, int right) { return this->waveforms[left].time < this->waveforms[right].time; }
	const std::vector<CWaveGraphCtrl::Waveform>& waveforms;
};

//////////////////////////////////////////////////////////////////////////
void CWaveGraphCtrl::StartSoundsAtTime(float fTime, bool bForceStart)
{
	// Created a sorted list of waveforms.
	//std::vector<int> sortedWaveformIndices(m_waveforms.size());
	//for (int i = 0, count = sortedWaveformIndices.size(); i < count; ++i)
	//	sortedWaveformIndices[i] = i;
	//std::sort(sortedWaveformIndices.begin(), sortedWaveformIndices.end(), WaveformSortPredicate(m_waveforms));

	//// Create a list of waveforms that should be playing.
	//std::vector<int> waveformsThatShouldBePlaying;
	//waveformsThatShouldBePlaying.reserve(m_waveforms.size());
	//for (int waveformIndex = 0, waveformCount = m_waveforms.size(); waveformIndex < waveformCount; ++waveformIndex)
	//{
	//	CWaveFileReader* pReader = (*m_waveforms[sortedWaveformIndices[waveformIndex]].itSound).second.pWaveFileReader;
	//	float startTime = m_waveforms[sortedWaveformIndices[waveformIndex]].time;
	//	float samplesPerSecond = (pReader ? pReader->GetSamplesPerSec() : 0);
	//	float endTime = startTime + (pReader ? float(pReader->GetSampleCount()) : 0.0f) / max(samplesPerSecond, 0.1f);
	//	if (fTime >= startTime && fTime <= endTime)
	//		waveformsThatShouldBePlaying.push_back(waveformIndex);
	//}

	//// Check whether the user only wants to hear one sound at a time - if so, only the sound that starts latest should be playing.
	//bool overlapSounds = (m_pContext ? m_pContext->GetOverlapSounds() : true);
	//if (!overlapSounds)
	//{
	//	int latestEntryIndex = -1;
	//	float latestStartTime = -FLT_MAX;
	//	for (int entryIndex = 0, entryCount = waveformsThatShouldBePlaying.size(); entryIndex < entryCount; ++entryIndex)
	//	{
	//		float startTime = m_waveforms[sortedWaveformIndices[waveformsThatShouldBePlaying[entryIndex]]].time;
	//		if (startTime > latestStartTime)
	//		{
	//			latestStartTime = startTime;
	//			latestEntryIndex = waveformsThatShouldBePlaying[entryIndex];
	//		}
	//	}
	//	waveformsThatShouldBePlaying.clear();
	//	if (latestEntryIndex >= 0)
	//		waveformsThatShouldBePlaying.push_back(latestEntryIndex);
	//}

	//// Create a list of waveforms that are playing.
	//std::vector<int> waveformsThatArePlaying;
	//waveformsThatArePlaying.reserve(m_waveforms.size());
	//for (int waveformIndex = 0, waveformCount = m_waveforms.size(); waveformIndex < waveformCount; ++waveformIndex)
	//{
	//	ISound* pSound = (*m_waveforms[sortedWaveformIndices[waveformIndex]].itSound).second.pSound;
	//	if (pSound && !pSound->GetPaused())
	//		waveformsThatArePlaying.push_back(waveformIndex);
	//}

	//// Check which waveforms need to be started and which need to be stopped. Relies on waveform lists being sorted.
	//std::vector<int> waveformsToStop, waveformsToStart, waveformsToUpdate;
	//waveformsToStart.reserve(m_waveforms.size());
	//waveformsToStop.reserve(m_waveforms.size());
	//waveformsToUpdate.reserve(m_waveforms.size());
	//{
	//	int playingPosition = 0, toPlayPosition = 0, playingCount = waveformsThatArePlaying.size(), toPlayCount = waveformsThatShouldBePlaying.size();
	//	while (playingPosition < playingCount || toPlayPosition < toPlayCount)
	//	{
	//		for (; playingPosition < playingCount && (toPlayPosition >= toPlayCount || waveformsThatArePlaying[playingPosition] < waveformsThatShouldBePlaying[toPlayPosition]); ++playingPosition)
	//			waveformsToStop.push_back(waveformsThatArePlaying[playingPosition]);

	//		for (; toPlayPosition < toPlayCount && (playingPosition >= playingCount || waveformsThatShouldBePlaying[toPlayPosition] < waveformsThatArePlaying[playingPosition]); ++toPlayPosition)
	//			waveformsToStart.push_back(waveformsThatShouldBePlaying[toPlayPosition]);

	//		for (; toPlayPosition < toPlayCount && playingPosition < playingCount && waveformsThatShouldBePlaying[toPlayPosition] == waveformsThatArePlaying[playingPosition]; ++toPlayPosition, ++playingPosition)
	//			waveformsToUpdate.push_back(waveformsThatShouldBePlaying[toPlayPosition]);
	//	}
	//}

	//// Create a list of sounds that need to be playing - this is required in case the same sound is played multiple times.
	//std::set<ISound*> soundsThatShouldBePlaying;
	//for (int entryIndex = 0, entryCount = waveformsThatShouldBePlaying.size(); entryIndex < entryCount; ++entryIndex)
	//{
	//	ISound* pSound = (*m_waveforms[sortedWaveformIndices[waveformsThatShouldBePlaying[entryIndex]]].itSound).second.pSound;
	//	if (pSound)
	//		soundsThatShouldBePlaying.insert(pSound);
	//}

	//// Start all the sounds that should be started.
	//for (int entryIndex = 0, entryCount = waveformsToStart.size(); entryIndex < entryCount; ++entryIndex)
	//{
	//	ISound* pSound = (*m_waveforms[sortedWaveformIndices[waveformsToStart[entryIndex]]].itSound).second.pSound;
	//	float startTime = m_waveforms[sortedWaveformIndices[waveformsToStart[entryIndex]]].time;
	//	if (pSound)
	//	{
	//		pSound->GetInterfaceExtended()->SetCurrentSamplePos((fTime - startTime) * 1000.0f, true);
	//		pSound->SetPaused(false);
	//		pSound->GetInterfaceExtended()->SetPitch(1000 * m_fPlaybackSpeed);
	//	}
	//}

	//// Stop all the sounds that should be stopped.
	//for (int entryIndex = 0, entryCount = waveformsToStop.size(); entryIndex < entryCount; ++entryIndex)
	//{
	//	ISound* pSound = (*m_waveforms[sortedWaveformIndices[waveformsToStop[entryIndex]]].itSound).second.pSound;
	//	if (pSound && soundsThatShouldBePlaying.find(pSound) == soundsThatShouldBePlaying.end())
	//	{
	//		float startTime = m_waveforms[sortedWaveformIndices[waveformsToStop[entryIndex]]].time;
	//		pSound->GetInterfaceExtended()->SetCurrentSamplePos((fTime - startTime) * 1000.0f, true);
	//		pSound->SetPaused(true);
	//		pSound->GetInterfaceExtended()->SetPitch(1000 * m_fPlaybackSpeed);
	//	}
	//}

	//// Update all the sounds that are still playing.
	//if (bForceStart)
	//{
	//	for (int entryIndex = 0, entryCount = waveformsToUpdate.size(); entryIndex < entryCount; ++entryIndex)
	//	{
	//		ISound* pSound = (*m_waveforms[sortedWaveformIndices[waveformsToUpdate[entryIndex]]].itSound).second.pSound;
	//		float startTime = m_waveforms[sortedWaveformIndices[waveformsToUpdate[entryIndex]]].time;
	//		if (pSound)
	//		{
	//			pSound->GetInterfaceExtended()->SetCurrentSamplePos((fTime - startTime) * 1000.0f, true);
	//			pSound->GetInterfaceExtended()->SetPitch(1000 * m_fPlaybackSpeed);
	//		}
	//	}
	//}
}

