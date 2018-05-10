// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __WAVEGRAPHCTRL_H__
#define __WAVEGRAPHCTRL_H__
#pragma once

#include <CryAnimation/IFacialAnimation.h>
#include "../FacialEdContext.h"
#include "WaveFileReader.h"
#include "Controls/WndGridHelper.h"

//struct ISound;

// Custom styles for this control.
#define WAVCTRLN_STYLE_NOGRID         0x0001
#define WAVCTRLN_STYLE_NO_TIME_MARKER 0x0002

#define WAVCTRLN_SCROLL_ZOOM          (0x0003)
#define WAVCTRLN_TIME_CHANGE          (0x0004)

struct WaveGraphCtrlWaveformChangeNotification
{
	NMHDR hdr;
	int   waveformIndex;
	float deltaTime;
};

#define WAVECTRLN_BEGIN_MOVE_WAVEFORM (0x0005)
#define WAVECTRLN_MOVE_WAVEFORMS      (0x0006)
#define WAVECTRLN_RESET_CHANGES       (0x0007)
#define WAVECTRLN_END_MOVE_WAVEFORM   (0x0008)

struct WaveGraphCtrlRClickNotification
{
	NMHDR hdr;
	int   waveformIndex;
};
#define WAVECTRLN_RCLICK (0x0009)

//////////////////////////////////////////////////////////////////////////
// Spline control.
//////////////////////////////////////////////////////////////////////////
class CWaveGraphCtrl : public CWnd, public IFacialEdListener
{
public:
	DECLARE_DYNAMIC(CWaveGraphCtrl)

	CWaveGraphCtrl();
	virtual ~CWaveGraphCtrl();

	BOOL  Create(DWORD dwStyle, const CRect& rc, CWnd* pParentWnd, UINT nID);

	void  SetContext(CFacialEdContext* pContext);

	int   AddWaveform();
	void  DeleteWaveform(int index);
	int   GetWaveformCount();
	void  SetWaveformTime(int index, float time);
	void  LoadWaveformSound(int index, const CString& soundFile);
	void  DeleteUnusedSounds();
	float GetWaveformLength(int waveformIndex);
	void  SetWaveformTextString(int waveformIndex, const CString& text);

	void  SetTimeRange(const Range& r) { m_timeRange = r; if (m_hWnd) Invalidate(); }
	void  SetTimeMarker(float fTime);
	float GetTimeMarker();

	float CalculateTimeRange();

	void  StartPlayback(); // starts the sound to play from current marker position
	void  StopPlayback();  // stops the sounds
	void  PausePlayback();

	void  BeginScrubbing();
	void  EndScrubbing();

	void  SetPlaybackSpeed(float fSpeed);
	float GetPlaybackSpeed();

	//////////////////////////////////////////////////////////////////////////
	// Scrolling/Zooming.
	//////////////////////////////////////////////////////////////////////////
	Vec2   ClientToWorld(CPoint point);
	CPoint WorldToClient(Vec2 v);
	void   SetZoom(Vec2 zoom, CPoint center);
	void   SetZoom(Vec2 zoom);
	Vec2   GetZoom() const         { return m_grid.zoom; };
	void   SetScrollOffset(Vec2 ofs);
	Vec2   GetScrollOffset() const { return m_grid.origin; };
	float  SnapTime(float time);
	float  SnapValue(float val);
	void   SetLeftOffset(int nLeft);
	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////

	void  SetBottomWnd(CWnd* pWnd, int nHeight);

	float FindEndOfWaveforms();

	void  UpdatePlayback();

	struct SoundCacheEntry
	{
		/*SoundCacheEntry(const CString& soundFilename, CWaveFileReader* pWaveFileReader, _smart_ptr<ISound> pSound, int refcount)
		   : soundFilename(soundFilename), pWaveFileReader(pWaveFileReader), pSound(pSound), refcount(refcount) {}

		   CString soundFilename;
		   CWaveFileReader* pWaveFileReader;
		   _smart_ptr<ISound> pSound;
		   int refcount;*/
	};
	typedef std::map<CString, SoundCacheEntry> SoundCache;

	struct Waveform
	{
		Waveform() : time(0) {}
		float                time;
		CString              text;
		SoundCache::iterator itSound;
	};

protected:
	DECLARE_MESSAGE_MAP()

	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnPaint();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnTimer(UINT_PTR nIDEvent);

	// Drawing functions
	void         DrawGrid(CDC* pDC);
	void         DrawWaveGraph(CDC* pDC);
	void         DrawTimeMarker(CDC* pDC);

	virtual void OnFacialEdEvent(EFacialEdEvent event, IFacialEffector* pEffector, int nChannelCount, IFacialAnimChannel** ppChannels);

	int          HitTestWaveforms(const CPoint point);

	void         SendNotifyMessage(int code);
	void         SendNotifyMessageStructure(NMHDR* hdr);

	void         StartSoundsAtTime(float fTime, bool bForceStart);
	void         SetTimeMarkerInternal(float fTime);

private:
	enum EditMode
	{
		eNothingMode,
		eClickingMode,
		eScrubbingMode,
		eWaveDragMode
	};

	int                   m_nWaveformBeingDragged;
	CPoint                m_StartClickPoint;

	CRect                 m_rcClient;
	CRect                 m_rcClipRect;
	CRect                 m_rcWaveGraph;

	CBitmap               m_offscreenBitmap;

	EditMode              m_editMode;

	CFacialEdContext*     m_pContext;

	CRect                 m_TimeUpdateRect;
	Range                 m_timeRange;
	float                 m_fTimeMarker;
	//CTimeValue m_fLastTimeCheck;
	DWORD                 m_lastTimeCheck;
	CWndGridHelper        m_grid;
	int                   m_nLeftOffset;

	bool                  m_bScrubbing;

	SoundCache            m_soundCache;

	std::vector<Waveform> m_waveforms;

	int                   m_nTimer;

	CWnd*                 m_pBottomWnd;
	int                   m_bottomWndHeight;
	bool                  m_bPlaying;
	float                 m_fPlaybackSpeed;
};

#endif // __SplineCtrl_h__

