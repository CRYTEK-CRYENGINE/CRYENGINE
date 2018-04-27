// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __sequencerkeys_h__
#define __sequencerkeys_h__

#if _MSC_VER > 1000
	#pragma once
#endif

#include "ISequencerSystem.h"
#include "Controls\PropertyCtrl.h"
#include "SequencerNode.h"

class CSequencerTrackPropsDialog;
class CSequencerKeyPropertiesDlg;
class CSequencerNode;

// CSequencerKeys

enum ESequencerActionMode
{
	SEQMODE_MOVEKEY = 1,
	SEQMODE_ADDKEY,
	SEQMODE_SLIDEKEY,
	SEQMODE_SCALEKEY,
};

enum ESequencerSnappingMode
{
	SEQKEY_SNAP_NONE = 0,
	SEQKEY_SNAP_TICK,
	SEQKEY_SNAP_MAGNET,
	SEQKEY_SNAP_FRAME,
};

enum ESequencerTickMode
{
	SEQTICK_INSECONDS = 0,
	SEQTICK_INFRAMES,
};

enum ESequencerKeyBitmaps
{
	SEQBMP_ERROR = 0,
	SEQBMP_TAGS,
	SEQBMP_FRAGMENTID,
	SEQBMP_ANIMLAYER,
	SEQBMP_PROCLAYER,
	SEQBMP_PARAMS,
	SEQBMP_TRANSITION
};

/** Base class for Sequencer key editing dialogs.
 */
class CSequencerDopeSheetBase : public CWnd, public IEditorNotifyListener
{
	DECLARE_DYNAMIC(CSequencerDopeSheetBase)
public:
	enum EDSRenderFlags
	{
		DRAW_BACKGROUND = 1 << 0,
		DRAW_HANDLES    = 1 << 1,
		DRAW_NAMES      = 1 << 2,
	};

	enum EDSSourceControlResponse
	{
		SOURCE_CONTROL_UNAVAILABLE = 0,
		USER_ABORTED_OPERATION,
		FAILED_OPERATION,
		SUCCEEDED_OPERATION,
	};

	struct Item
	{
		int                         nHeight;
		_smart_ptr<CSequencerTrack> track;
		_smart_ptr<CSequencerNode>  node;
		int                         paramId;
		bool                        bSelected;
		//////////////////////////////////////////////////////////////////////////
		Item() { nHeight = 16; track = 0; node = 0; paramId = 0; bSelected = false; }
		Item(int height) { nHeight = height; track = 0; node = 0; paramId = 0; bSelected = false; }
		Item(CSequencerNode* pNode, int nParamId, CSequencerTrack* pTrack)
		{
			nHeight = 16;
			track = pTrack;
			paramId = nParamId;
			node = pNode;
			bSelected = false;
		}
		Item(int height, CSequencerNode* pNode, int nParamId, CSequencerTrack* pTrack)
		{
			nHeight = height;
			track = pTrack;
			paramId = nParamId;
			node = pNode;
			bSelected = false;
		}
		Item(int height, CSequencerNode* pNode)
		{
			nHeight = height;
			track = 0;
			paramId = 0;
			node = pNode;
			bSelected = false;
		}
	};

	CSequencerDopeSheetBase();
	virtual ~CSequencerDopeSheetBase();

	bool         IsDragging() const;

	void         SetSequence(CSequencerSequence* pSequence) { m_pSequence = pSequence; };
	void         SetTimeScale(float timeScale, float fAnchorTime);
	float        GetTimeScale()                             { return m_timeScale; }

	void         SetScrollOffset(int hpos);

	void         SetTimeRange(float start, float end);
	virtual void SetCurrTime(float time, bool bForce = false);
	float        GetCurrTime() const;
	void         SetStartMarker(float fTime);
	void         SetEndMarker(float fTime);

	void         DelSelectedKeys(bool bPrompt, bool bAllowUndo = true, bool bIgnorePermission = false);
	void         SetMouseActionMode(ESequencerActionMode mode);

	bool         CanCopyPasteKeys();
	bool         CopyPasteKeys();

	bool         CopyKeys(bool bPromptAllowed = true, bool bUseClipboard = true, bool bCopyTrack = false);
	bool         PasteKeys(CSequencerNode* pAnimNode, CSequencerTrack* pAnimTrack, float fTimeOffset);
	void         StartDraggingKeys(CPoint point);
	void         StartPasteKeys();
	void         FinalizePasteKeys();

	void         CopyTrack();

	void         SerializeTracks(XmlNodeRef& destination);
	void         DeserializeTracks(const XmlNodeRef& source);

	void         ShowKeyPropertyCtrlOnSpot(int x, int y, bool bMultipleKeysSelected, bool bKeyChangeInSameTrack);
	void         HideKeyPropertyCtrlOnSpot();

	bool         SelectFirstKey();
	bool         SelectFirstKey(const ESequencerParamType type);

	void         SetKeyPropertiesDlg(CSequencerKeyPropertiesDlg* dlg)
	{ m_keyPropertiesDlg = dlg; }

	//////////////////////////////////////////////////////////////////////////
	// Tracks access.
	//////////////////////////////////////////////////////////////////////////
	int                    GetCount() const { return m_tracks.size(); }
	void                   AddItem(const Item& item);
	const Item&            GetItem(int item) const;
	CSequencerTrack*       GetTrack(int item) const;
	CSequencerNode*        GetNode(int item) const;
	int                    GetHorizontalExtent() const { return m_itemWidth; };

	bool                   GetSelectedTracks(std::vector<CSequencerTrack*>& tracks) const;
	bool                   GetSelectedNodes(std::vector<CSequencerNode*>& nodes) const;

	virtual void           SetHorizontalExtent(int min, int max);
	virtual int            ItemFromPoint(CPoint pnt) const;
	virtual int            GetItemRect(int item, CRect& rect) const;
	virtual void           InvalidateItem(int item);
	virtual void           ResetContent() { m_tracks.clear(); }

	void                   ClearSelection();
	void                   SelectItem(int item);
	bool                   IsSelectedItem(int item);

	void                   SetSnappingMode(ESequencerSnappingMode mode)
	{ m_snappingMode = mode; }
	ESequencerSnappingMode GetSnappingMode() const
	{ return m_snappingMode; }
	void                   SetSnapFPS(UINT fps)
	{ m_snapFrameTime = fps == 0 ? 0.033333f : 1.0f / float(fps); }

	float GetSnapFps() const
	{
		// To address issues of m_snapFrameTime being <FLT_EPSILON or >FLT_MAX
		return 1.0f / CLAMP(m_snapFrameTime, FLT_EPSILON, FLT_MAX);
	}

	ESequencerTickMode GetTickDisplayMode() const
	{ return m_tickDisplayMode; }
	void               SetTickDisplayMode(ESequencerTickMode mode)
	{
		m_tickDisplayMode = mode;
		SetTimeScale(GetTimeScale(), 0);  // for refresh
	}

	//////////////////////////////////////////////////////////////////////////
	virtual void OnEditorNotifyEvent(EEditorNotifyEvent event);
	//////////////////////////////////////////////////////////////////////////

	uint32 GetChangeCount() const
	{
		return m_changeCount;
	}

	const Range& GetMarkedTime() const
	{
		return m_timeMarked;
	}

	//////////////////////////////////////////////////////////////////////////
	// Drag / drop helper
	struct SDropFragmentData
	{
		FragmentID    fragID;
		SFragTagState tagState;
		uint32        option;
	};

	bool                IsPointValidForFragmentInPreviewDrop(const CPoint& point, COleDataObject* pDataObject) const;
	bool                CreatePointForFragmentInPreviewDrop(const CPoint& point, COleDataObject* pDataObject);

	bool                IsPointValidForAnimationInLayerDrop(const CPoint& point, COleDataObject* pDataObject) const;
	bool                CreatePointForAnimationInLayerDrop(const CPoint& point, COleDataObject* pDataObject);

	CSequencerSequence* GetSequence() { return m_pSequence; }

protected:

	DECLARE_MESSAGE_MAP()
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	afx_msg int  OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnDestroy();
	afx_msg void   DrawItem(LPDRAWITEMSTRUCT /*lpDrawItemStruct*/);
	afx_msg void   MeasureItem(LPMEASUREITEMSTRUCT /*lpMeasureItemStruct*/);
	afx_msg HBRUSH CtlColor(CDC* /*pDC*/, UINT /*nCtlColor*/);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnMButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnRawInput(UINT wParam, HRAWINPUT lParam);

	//////////////////////////////////////////////////////////////////////////
	// Drawing methods.
	//////////////////////////////////////////////////////////////////////////
	virtual void DrawControl(CDC* dc, const CRect& rcUpdate);
	virtual void DrawTrack(int item, CDC* dc, CRect& rcItem);
	virtual void DrawTicks(CDC* dc, CRect& rc, Range& timeRange);

	// Helper functions
	void         ComputeFrameSteps(const Range& VisRange);
	void         DrawTimeLineInFrames(CDC* dc, CRect& rc, COLORREF& lineCol, COLORREF& textCol, double step);
	void         DrawTimeLineInSeconds(CDC* dc, CRect& rc, COLORREF& lineCol, COLORREF& textCol, double step);

	virtual void DrawTimeline(CDC* dc, const CRect& rcUpdate);
	virtual void DrawSummary(CDC* dc, CRect rcUpdate);
	virtual void DrawSelectedKeyIndicators(CDC* dc);
	virtual void DrawKeys(CSequencerTrack* track, CDC* dc, CRect& rc, Range& timeRange, EDSRenderFlags renderFlags);
	virtual void RedrawItem(int item);

	//////////////////////////////////////////////////////////////////////////
	// Must be overriden.
	//////////////////////////////////////////////////////////////////////////
	//! Find a key near this point.
	virtual int  FirstKeyFromPoint(CPoint point, bool exact = false) const;
	virtual int  LastKeyFromPoint(CPoint point, bool exact = false) const;
	//! Select keys inside this client rectangle.
	virtual void SelectKeys(const CRect& rc);
	//! Select all keys within time frame defined by this client rectangle.
	virtual void SelectAllKeysWithinTimeFrame(const CRect& rc);

	//////////////////////////////////////////////////////////////////////////
	//! Return time snapped to timestep,
	double GetTickTime() const;
	float  TickSnap(float time) const;
	float  MagnetSnap(const float time, const CSequencerNode* node) const;
	float  FrameSnap(float time) const;

	//! Returns visible time range.
	Range GetVisibleRange();
	Range GetTimeRange(CRect& rc);

	//! Return client position for given time.
	int   TimeToClient(float time) const;

	float TimeFromPoint(CPoint point) const;
	float TimeFromPointUnsnapped(CPoint point) const;

	//! Unselect all selected keys.
	void UnselectAllKeys(bool bNotify);
	//! Offset all selected keys by this offset.
	void OffsetSelectedKeys(const float timeOffset, const bool bSnapKeys);
	//! Scale all selected keys by this offset.
	void ScaleSelectedKeys(float timeOffset, bool bSnapKeys);
	void CloneSelectedKeys();

	bool FindSingleSelectedKey(CSequencerTrack*& track, int& key);

	//////////////////////////////////////////////////////////////////////////
	void SetKeyInfo(CSequencerTrack* track, int key, bool openWindow = false);

	void UpdateAnimation(int item);
	void UpdateAnimation(CSequencerTrack* animTrack);
	void SetLeftOffset(int ofs) { m_leftOffset = ofs; };

	void SetMouseCursor(HCURSOR crs);

	void RecordTrackUndo(const Item& item);
	void ShowKeyTooltip(CSequencerTrack* pTrack, int nKey, int secondarySelection, CPoint point);

protected:
	//////////////////////////////////////////////////////////////////////////
	// FIELDS.
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	CBrush           m_bkgrBrush;
	CBrush           m_bkgrBrushEmpty;
	CBrush           m_selectedBrush;
	CBrush           m_timeBkgBrush;
	CBrush           m_timeHighlightBrush;
	CBrush           m_visibilityBrush;
	CBrush           m_selectTrackBrush;

	HCURSOR          m_currCursor;
	HCURSOR          m_crsLeftRight;
	HCURSOR          m_crsAddKey;
	HCURSOR          m_crsCross;
	HCURSOR          m_crsMoveBlend;

	CRect            m_rcClient;
	CPoint           m_scrollOffset;
	CRect            m_rcSelect;
	CRect            m_rcTimeline;
	CRect            m_rcSummary;

	CPoint           m_lastTooltipPos;
	CPoint           m_mouseDownPos;
	CPoint           m_mouseOverPos;
	float            m_mouseMoveStartTimeOffset;
	int              m_mouseMoveStartTrackOffset;
	static const int SNumOfBmps = 7;
	CImageList       m_imageSeqKeyBody[SNumOfBmps];
	CImageList       m_imageList;
	CImageList       m_imgMarker;

	CBitmap          m_offscreenBitmap;

	bool             m_bZoomDrag;
	bool             m_bMoveDrag;

	bool             m_bMouseOverKey;

	//////////////////////////////////////////////////////////////////////////
	// Time.
	float m_timeScale;
	float m_currentTime;
	float m_storedTime;
	Range m_timeRange;
	Range m_realTimeRange;
	Range m_timeMarked;

	//! This is how often to place ticks.
	//! value of 10 means place ticks every 10 second.
	double m_ticksStep;

	//////////////////////////////////////////////////////////////////////////
	int   m_mouseMode;
	int   m_mouseActionMode;
	int   m_actionMode;
	bool  m_bAnySelected;
	float m_keyTimeOffset;
	float m_grabOffset;
	int   m_secondarySelection;

	// Drag / Drop
	bool                        m_bUseClipboard;
	XmlNodeRef                  m_dragClipboard;
	XmlNodeRef                  m_prePasteSheetData;
	COleDropTarget*             m_pDropTarget;

	CSequencerKeyPropertiesDlg* m_keyPropertiesDlg;
	CPropertyCtrl               m_wndPropsOnSpot;
	CSequencerTrack*            m_pLastTrackSelectedOnSpot;

	CFont*                      m_descriptionFont;

	//////////////////////////////////////////////////////////////////////////
	// Track list related.
	//////////////////////////////////////////////////////////////////////////
	std::vector<Item>      m_tracks;

	int                    m_itemWidth;

	int                    m_leftOffset;
	int                    m_scrollMin, m_scrollMax;

	CToolTipCtrl           m_tooltip;

	CSequencerSequence*    m_pSequence;

	bool                   m_bCursorWasInKey;

	float                  m_fJustSelected;

	bool                   m_bMouseMovedAfterRButtonDown;

	uint32                 m_changeCount;

	ESequencerSnappingMode m_snappingMode;
	double                 m_snapFrameTime;

	ESequencerTickMode     m_tickDisplayMode;
	double                 m_fFrameTickStep;
	double                 m_fFrameLabelStep;

	void                     OffsetKey(CSequencerTrack* pTrack, const int keyIndex, const float timeOffset) const;
	void                     NotifyKeySelectionUpdate();
	bool                     IsOkToAddKeyHere(const CSequencerTrack* pTrack, float time) const;

	void                     MouseMoveSelect(CPoint point);
	void                     MouseMoveMove(CPoint point, UINT nFlags);

	void                     MouseMoveDragTime(CPoint point, UINT nFlags);
	void                     MouseMoveOver(CPoint point);
	void                     MouseMovePaste(CPoint point, UINT nFlags);
	void                     MouseMoveDragEndMarker(CPoint point, UINT nFlags);
	void                     MouseMoveDragStartMarker(CPoint point, UINT nFlags);

	int                      GetAboveKey(CSequencerTrack*& track);
	int                      GetBelowKey(CSequencerTrack*& track);
	int                      GetRightKey(CSequencerTrack*& track);
	int                      GetLeftKey(CSequencerTrack*& track);

	bool                     AddOrCheckoutFile(const string& filename);
	void                     TryOpenFile(const CString& relativePath, const CString& fileName, const CString& extension) const;
	EDSSourceControlResponse TryGetLatestOnFiles(const std::vector<CString>& paths, bool bPromptUser = TRUE) const;
	EDSSourceControlResponse TryCheckoutFiles(const std::vector<CString>& paths, bool bPromptUser = TRUE) const;

private:
	int						 m_startDragMouseOverItemID; //!< Keeps item id, when dragging starts; is used as default item, if dropping is not allowed on current item.
};

#endif // __sequencerkeys_h__

