// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "TimelineContent.h"

#include <vector>
#include <QWidget>
#include <CryMath/Range.h>
#include <QPoint>
#include <QStaticText>
#include <QStyleOptionViewItem>

#include "DrawingPrimitives/Ruler.h"

class QPainter;
class QPaintEvent;
class QLineEdit;
class QScrollBar;

struct STimelineLayout;

struct STimelineViewState
{
	float  viewOrigin;
	float  visibleDistance;
	int    widthPixels;
	QPoint scrollPixels;
	int    maxScrollX;
	int    treeWidth;
	int    treeLastOpenedWidth;

	STimelineViewState()
		: viewOrigin(0.0f)
		, visibleDistance(1.0f)
		, scrollPixels(0, 0)
		, maxScrollX(0)
		, treeWidth(0)
		, widthPixels(1)
	{
	}

	QPoint LocalToLayout(const QPoint& p) const;
	QPoint LayoutToLocal(const QPoint& p) const;

	int    ScrollOffset(float origin) const;
	int    TimeToLayout(float time) const;
	float  LocalToTime(int x) const;
	int    TimeToLocal(float time) const;
	float  LayoutToTime(int x) const;
};

struct STrackLayout;

enum class ESplitterState
{
	Normal,
	Selected,
	Moving
};

enum class ERenderCacheUpdate
{
	None,
	Once,
	Continuous
};

struct QRenderText
{
	QRect       rect;
	QStaticText text;

	QRenderText();
	QRenderText(QRect _rect, QString _text);
};

struct QRenderIcon
{
	QRect   rect;
	QPixmap icon;

	QRenderIcon();
	QRenderIcon(QRect _rect, QPixmap _icon);
};

struct STreeRenderCache
{
	std::vector<QRect>                descTrackBGRects;
	std::vector<QRect>                compTrackBGRects;
	std::vector<QRect>                seleTrackBGRects;
	std::vector<QRect>                disbTrackBGRects;

	std::vector<QRenderText>          text;
	std::vector<QRenderIcon>          icons;
	std::vector<QLine>                trackLines;

	std::vector<QStyleOptionViewItem> primitives;

	STreeRenderCache();

	void Clear();
};

struct STracksRenderCache
{
	std::vector<QRect>                                   descTrackBGRects;
	std::vector<QRect>                                   compTrackBGRects;
	std::vector<QRect>                                   seleTrackBGRects;
	std::vector<QRect>                                   disbTrackBGRects;
	std::vector<QRect>                                   outsideTrackBGRects;

	std::vector<QRect>                                   toggleRects;
	std::vector<QLine>                                   bottomLines;
	std::vector<QLine>                                   innerTickLines;

	std::vector<QRenderText>                             text;

	std::vector<QRenderIcon>                             defaultKeyIcons;
	std::vector<QRenderIcon>                             selectedKeyIcons;
	std::vector<QRenderIcon>                             highlightedKeyIcons;

	std::vector<QRect>                                   defaultClipRects;
	std::vector<QRect>                                   selectedClipRects;
	std::vector<QLine>                                   unclippedClipLines;

	std::vector<DrawingPrimitives::STick>                timeMarkers;
	const std::vector<DrawingPrimitives::CRuler::STick>* pTimeMarkers;

	STracksRenderCache();

	void Clear();
	void ClearTimeMarkers();
};

using SElementLayoutPtrs = std::vector<struct SElementLayout*>;

class EDITOR_COMMON_API CTimeline : public QWidget
{
	Q_OBJECT

	friend class CTimelineTracks;

public:
	explicit CTimeline(QWidget* parent = nullptr);
	~CTimeline();

	virtual void                 customEvent(QEvent* pEvent) override;

	void                         SetContent(STimelineContent* pContent);
	STimelineContent*            Content() const     { return m_pContent; }

	std::vector<SHeaderElement>& GetHeaderElements() { return m_headerElements; }

	std::vector<SAnimTime>&      GetTickTimePositions() { return m_tickTimePositions; }

	void                         ContentUpdated();

	void                         ShowKeyText(bool bShow);

	bool                         IsDragged() const { return m_mouseHandler.get() != 0; }

	// make it possible to have actual time in normalized units, but different display units
	void            SetTimeUnitScale(float timeUnitScale);
	void            SetTime(SAnimTime time);
	void            SetCycled(bool cycled);
	void            SetSizeToContent(bool sizeToContent);
	void            SetFrameRate(SAnimTime::EFrameRate frameRate) { m_frameRate = frameRate; }
	void            SetTimeSnapping(bool snapTime)                { m_snapTime = snapTime; }
	void            SetKeySnapping(bool snapKeys)                 { m_snapKeys = snapKeys; }
	void            SetKeyScaling(bool scaling)                   { m_scaleKeys = scaling; }
	void            SetKeySize(uint size)                         { m_keySize = size; UpdateLayout(); update(); }
	void            SetTimelinePadding(uint padding)              { m_timelinePadding = padding; UpdateLayout(); update(); }
	void            SetTreeVisible(bool visible);
	void            SetDrawSelectionIndicators(bool visible)      { m_selIndicators = visible; update(); }
	void            SetCustomTreeCornerWidget(QWidget* pWidget, uint width);
	void            SetVerticalScrollbarVisible(bool bVisible);
	void            SetDrawTrackTimeMarkers(bool bDrawMarkers);
	void            SetVisibleDistance(float distance);
	void            ZoomContinuous(float delta);
	void            ZoomStep(int delta);
	void            SetClampToViewOrigin(float clamp)             { m_clampToViewOrigin = clamp; }
	void            SetAllowOutOfRangeKeys(float allow)           { m_allowOutOfRangeKeys = allow; }
	void            SetUseInternalRuler(bool value);
	void            SetTimeUnit(SAnimTime::EDisplayMode timeUnit) { m_timeUnit = timeUnit; SignalTimeUnitChanged(); }
	void            SetUseMainTrackTimeRange(bool value)          { m_useMainTrackTimeRange = value; }

	SAnimTime       Time() const                                  { return m_time; }

	bool            HandleKeyEvent(int key);

	void            paintEvent(QPaintEvent* ev) override;
	void            mousePressEvent(QMouseEvent* ev) override;
	void            mouseMoveEvent(QMouseEvent* ev) override;
	void            mouseReleaseEvent(QMouseEvent* ev) override;
	void            focusOutEvent(QFocusEvent* ev) override;
	void            mouseDoubleClickEvent(QMouseEvent* ev) override;

	void            AddKeyToTrack(STimelineTrack& subTrack, SAnimTime time);

	void            keyPressEvent(QKeyEvent* ev) override;
	void            keyReleaseEvent(QKeyEvent* ev) override;
	void            resizeEvent(QResizeEvent* ev) override;
	void            wheelEvent(QWheelEvent* ev) override;
	QSize           sizeHint() const override;

	void            focusTrack(STimelineTrack* pTrack);

	Range           GetVisibleTimeRange() const;
	Range           GetVisibleTimeRangeFull() const;

	void            ZoomToTimeRange(const float start, const float end);

	void            ClearElementSelections();
	SAnimTime       GetLastMousePressEventTime() const;
	SAnimTime       GetTimeFromPos(QPoint p) const;
	STimelineTrack* GetTrackFromPos(const QPoint& pos) const;
	QPoint          GetLastKeyPressEventPosition() const { return m_lastMousePressEventPos; }

	void            OnTrackToggled(QPoint pos);
	void            OnSplitterMoved(uint32 newTreeWidth);
	void            OnLayoutChange();

signals:
	void SignalZoom();
	void SignalPan();
	void SignalScrub(bool scrubThrough);
	void SignalContentChanged(bool continuous);
	void SignalSelectionChanged(bool continuous);
	void SignalSelectionRefresh();
	void SignalTrackSelectionChanged();
	void SignalPlay();
	void SignalNumberHotkey(int number);
	void SignalTreeContextMenu(const QPoint& point);
	void SignalTrackToggled(STimelineTrack& track);
	void SignalLayoutChanged();
	void SignalViewOptionChanged();
	void SignalTimeUnitChanged();

	// Sent to check if it's okay to drag a track
	void SignalTracksBeginDrag(STimelineTrack* pTarget, bool& bValid);
	// Sent to check if a track is a valid target
	void SignalTracksDragging(STimelineTrack* pTarget, bool& bValid);
	// Sent when user releases the mouse button after dragging
	void SignalTracksDragged(STimelineTrack* pTarget);

	void SignalUndo();//Note: this is triggered before the undo has been processed
	void SignalRedo();//Note: this is triggered before the undo has been processed

	void SignalCopy(SAnimTime time, STimelineTrack* pTrack);
	void SignalPaste(SAnimTime time, STimelineTrack* pTrack);    

public slots:
	void OnMenuDuplicate();
	void OnMenuDelete();
	void OnMenuCopy();
	void OnMenuCut();
	void OnMenuPaste();

protected slots:
	void OnMenuSelectionToCursor();

	void OnMenuPlay();
	void OnMenuNextKey();
	void OnMenuPreviousKey();
	void OnMenuNextFrame();
	void OnMenuPreviousFrame();

	void OnFilterChanged();
	void OnVerticalScroll(int value);

protected:
	void OnContextMenu(const QPoint& pos);

private:
	struct SMouseHandler;
	struct SSelectionHandler;
	struct SMoveHandler;
	struct SShiftHandler;
	struct SScaleHandler;
	struct SPanHandler;
	struct SScrubHandler;
	struct SSplitterHandler;
	struct STreeMouseHandler;
	struct SBlendInSlideHandler;
	struct SChangeDurationHandler;

	void          ContentChanged(bool continuous);
	void          UpdateLayout(bool forceClamp = false);
	void          UpdateCursor(QMouseEvent* ev, const SElementLayoutPtrs& hitElements);
	void          UpdateHighligted(const SElementLayoutPtrs& hitElements);
	SAnimTime     ClampAndSnapTime(SAnimTime time, bool snapToFrames) const;
	void          ClampAndSetTime(SAnimTime time, bool scrubThrough);
	STrackLayout* GetTrackLayoutFromPos(const QPoint& pos) const;
	void          ResolveHitElements(const QRect& pos, SElementLayoutPtrs& hitElements);
	void          ResolveHitElements(const QPoint& pos, SElementLayoutPtrs& hitElements);

	void          UpdateHightlightedInternal();
	void          UpdateTracksTimeMarkers(int32 width);
	void          InvalidateTracksTimeMarkers()
	{
		m_tracksRenderCache.timeMarkers.clear();
		m_updateTracksRenderCache = ERenderCacheUpdate::Once;
	}
	void InvalidateTree()   { m_updateTreeRenderCache = ERenderCacheUpdate::Once; }
	void InvalidateTracks() { m_updateTracksRenderCache = ERenderCacheUpdate::Once; }

	void ClampViewOrigin(bool force = false);

	void CalculateRulerMarkerTimes();

	// Exposed parameters
	float                   m_timeUnitScale;
	SAnimTime::EFrameRate   m_frameRate;
	SAnimTime::EDisplayMode m_timeUnit;
	bool                    m_cycled                   : 1;
	bool                    m_sizeToContent            : 1;
	bool                    m_snapTime                 : 1;
	bool                    m_scaleKeys                : 1;
	bool                    m_snapKeys                 : 1;
	bool                    m_treeVisible              : 1;
	bool                    m_selIndicators            : 1;
	bool                    m_verticalScrollbarVisible : 1;
	bool                    m_drawMarkers              : 1;
	bool                    m_clampToViewOrigin        : 1;
	bool                    m_allowOutOfRangeKeys      : 1;
	bool                    m_useAdvancedRuler         : 1;
	bool                    m_useMainTrackTimeRange    : 1;
	uint                    m_keySize;
	uint                    m_cornerWidgetWidth;
	uint                    m_timelinePadding;

	// Widgets
	QScrollBar* m_scrollBar;
	QWidget*    m_cornerWidget;

	// State
	STimelineViewState               m_viewState;
	STimelineContent*                m_pContent;
	SAnimTime                        m_time;
	std::unique_ptr<STimelineLayout> m_layout;
	std::unique_ptr<SMouseHandler>   m_mouseHandler;
	std::vector<SHeaderElement>      m_headerElements;

	QPoint                           m_lastMousePressEventPos;
	QPoint                           m_lastMouseMoveEventPos;
	QPoint                           m_mouseMenuStartPos;

	// Filtering
	QLineEdit* m_pFilterLineEdit;
	QWidget* m_searchWidget;

	// Track selection
	STrackLayout* m_pLastSelectedTrack;

	// Rendering
	ESplitterState            m_splitterState;
	ERenderCacheUpdate        m_updateTracksRenderCache;
	ERenderCacheUpdate        m_updateTreeRenderCache;
	bool                      m_bShowKeyText;

	QTimer                    m_highlightedTimer;
	QMetaObject::Connection   m_highlightedConnection;

	STracksRenderCache        m_tracksRenderCache;
	STreeRenderCache          m_treeRenderCache;
	SElementLayoutPtrs        m_highlightedElements;
	
	DrawingPrimitives::CRuler m_timelineDrawer;

	std::vector<SAnimTime>    m_tickTimePositions;

};

inline void CTimeline::SetUseInternalRuler(bool value)
{
	m_useAdvancedRuler = value;
	if (value)
	{
		m_tracksRenderCache.pTimeMarkers = &m_timelineDrawer.GetTicks();
	}
	else
	{
		m_tracksRenderCache.pTimeMarkers = nullptr;
	}
}

class CTimelineTracks : public QWidget
{
	Q_OBJECT
public:
	CTimelineTracks(QWidget* widget) : QWidget(widget) {}
	void ConnectToTimeline(CTimeline* timeline) { m_timeline = timeline; }

private:
	CTimeline* m_timeline;
};

inline QRenderText::QRenderText()
{}

inline QRenderText::QRenderText(QRect _rect, QString _text)
	: rect(_rect), text(_text)
{}

inline QRenderIcon::QRenderIcon()
{}

inline QRenderIcon::QRenderIcon(QRect _rect, QPixmap _icon)
	: rect(_rect), icon(_icon)
{}

inline STreeRenderCache::STreeRenderCache()
{
	descTrackBGRects.reserve(10);
	compTrackBGRects.reserve(10);
	seleTrackBGRects.reserve(10);

	text.reserve(10);
	icons.reserve(10);
	trackLines.reserve(10);

	primitives.reserve(10);
}

inline void STreeRenderCache::Clear()
{
	descTrackBGRects.clear();
	compTrackBGRects.clear();
	seleTrackBGRects.clear();

	text.clear();
	icons.clear();
	trackLines.clear();

	primitives.clear();
}

inline STracksRenderCache::STracksRenderCache()
	: pTimeMarkers(nullptr)
{
	descTrackBGRects.reserve(10);
	compTrackBGRects.reserve(10);
	seleTrackBGRects.reserve(10);
	disbTrackBGRects.reserve(10);
	outsideTrackBGRects.reserve(10);

	toggleRects.reserve(10);
	bottomLines.reserve(10);
	innerTickLines.reserve(100);
	text.reserve(100);
	defaultKeyIcons.reserve(100);
	selectedKeyIcons.reserve(10);
	highlightedKeyIcons.reserve(4);

	defaultClipRects.reserve(10);
	selectedClipRects.reserve(10);
	unclippedClipLines.reserve(60);
}

inline void STracksRenderCache::Clear()
{
	descTrackBGRects.clear();
	compTrackBGRects.clear();
	seleTrackBGRects.clear();
	disbTrackBGRects.clear();
	outsideTrackBGRects.clear();

	toggleRects.clear();
	bottomLines.clear();
	innerTickLines.clear();
	text.clear();
	defaultKeyIcons.clear();
	selectedKeyIcons.clear();
	highlightedKeyIcons.clear();

	defaultClipRects.clear();
	selectedClipRects.clear();
	unclippedClipLines.clear();
}

inline void STracksRenderCache::ClearTimeMarkers()
{
	// We only need to clear them if the user
	// zoomed/panned or the content changed.
	timeMarkers.clear();
}

