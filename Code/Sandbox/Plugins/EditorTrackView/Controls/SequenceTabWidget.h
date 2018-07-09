// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QWidget.h>

#include "Timeline.h"
#include "CurveEditor.h"
#include "TrackViewCoreComponent.h"
#include "Nodes/TrackViewSequence.h"

#include <CryMovie/AnimKey.h>
#include <CryExtension/CryGUID.h>

class QTabWidget;
class QToolBar;
class QTimer;

namespace
{
struct SAddableNode
{
	EAnimNodeType m_type;
	string        m_name;
};

struct SAddableTrack
{
	CAnimParamType m_type;
	string         m_name;
};

struct SSelectedKey
{
	STimelineTrack*             m_pTrack;
	STimelineElement*           m_pElement;
	_smart_ptr<IAnimKeyWrapper> m_key;
};
}

class CTrackViewSequenceTabWidget : public QWidget, public IEditorNotifyListener, public CTrackViewCoreComponent
{
	Q_OBJECT

public:
	typedef std::vector<SSelectedKey> SelectedKeys;

	CTrackViewSequenceTabWidget(CTrackViewCore* pTrackViewCore);
	~CTrackViewSequenceTabWidget();

	virtual void        OnEditorNotifyEvent(EEditorNotifyEvent event) override;

	void                OpenSequenceTab(const CryGUID sequenceGUID);
	void                GetPropertiesToUpdate(Serialization::SStructs& structs, EPropertyDataSource dataSource);
	CTrackViewSequence* GetActiveSequence();
	void                ApplyChangedProperties(bool bUpdateProperties = true);
	void                TimelineLayoutChanged(bool bUpdateProperties = true);

	void                SetToolbar(QToolBar* pToolbar);

	const SelectedKeys& GetCurrentSelectedKeys() const { return m_currentKeySelection; }
	CTrackViewNode*     GetNodeFromActiveSequence(const STimelineTrack* pDopeSheetTrack);

	CTimeline*          GetActiveDopeSheet() const;

	void                ShowDopeSheet(bool show);
	void                ShowCurveEditor(bool show);
	void                EnableTimelineLink(bool enable);
	void                ShowKeyText(bool show);

	bool                IsDopeSheetVisible() const    { return m_showDopeSheet; }
	bool                IsCurveEditorVisible() const  { return m_showCurveEditor; }
	bool                IsTimelineLinkEnabled() const { return m_timelineLink; }
	bool                IsShowingKeyText() const      { return m_showKeyText; }

	void                CloseCurrentTab();

	void                ShowSequenceProperties(CTrackViewSequence* pSequence);

protected:
	virtual void        OnTrackViewEditorEvent(ETrackViewEditorEvent event) override;
	virtual const char* GetComponentTitle() const override { return "Sequence Explorer"; }

	// IAnimationContextListener
	virtual void OnTimeChanged(SAnimTime newTime) override;
	virtual void OnSequenceChanged(CTrackViewSequence* pSequence) override;
	// ~IAnimationContextListener

	// ITrackViewSequenceManagerListener
	virtual void OnSequenceAdded(CTrackViewSequence* pSequence) override;
	virtual void OnSequenceRemoved(CTrackViewSequence* pSequence) override;
	// ~ITrackViewSequenceManagerListener

	// ITrackViewSequenceListener
	virtual void OnSequenceSettingsChanged(CTrackViewSequence* pSequence, SSequenceSettingsChangedEventArgs& args) override;
	virtual void OnNodeChanged(CTrackViewNode* pNode, ENodeChangeType type) override;
	virtual void OnNodeRenamed(CTrackViewNode* pNode, const char* pOldName) override;
	// ~ITrackViewSequenceListener

	void resizeEvent(QResizeEvent* pEvent)
	{
		if (GetActiveDopeSheet())
		{
			GetActiveDopeSheet()->OnLayoutChange();
		}
		QWidget::resizeEvent(pEvent);
	}

private slots:
	void OnDopeSheetTabChanged(int index);
	void OnDopeSheetTabClose(int index);

	void OnAddNodeButtonClicked();
	void OnAddSelectedEntities();
	void OnRenameNode();
	void OnDeleteNodes();
	void OnSelectEntity();
	void OnEnableNode();
	void OnDisableNode();
	void DisableSelectedNodesAndTracks(bool disable);
	void OnImportFromFile();
	void OnExportToFile();
	void OnSetAsActiveDirector();

	void OnDopesheetPlayPause();
	void OnDopesheetScrub();
	void OnDopesheetContentChanged(bool continuous);
	void OnDopesheetTrackSelectionChanged();
	void OnDopesheetTreeContextMenu(const QPoint& point);
	void OnDopesheetTracksBeginDrag(STimelineTrack* pTarget, bool& bValid);
	void OnDopesheetTracksDragging(STimelineTrack* pTarget, bool& bValid);
	void OnDopesheetTracksDragged(STimelineTrack* pTarget);

	void OnDopesheetZoom();
	void OnCurveEditorZoom();
	void OnDopesheetPan();
	void OnCurveEditorPan();

	void OnCurveEditorContentChanged();
	void OnCurveEditorScrub();

	void OnCopyKeys();
	void OnCopySelectedKeys();
	void OnPasteKeys();

	void OnCopyNode();
	void OnPasteNode();

	void OnDopesheetCopy();
	void OnDopesheetPaste(SAnimTime time, STimelineTrack* pTrack);

private:
	void ClearData();
	void DisableNode(CTrackViewNode* pNode, bool disable);
	void OnAddTrack(CAnimParamType type);
	void OnAddNode(const SAddableNode& node);

	struct SSequenceData
	{
		SSequenceData()
			: m_startTime(0.0f)
			, m_endTime(0.0f)
			, m_bPlaying(false)
			, m_pDopeSheet(nullptr)
			, m_pCurveEditor(nullptr)
		{}

		SAnimTime                          m_startTime;
		SAnimTime                          m_endTime;
		bool                               m_bPlaying;

		CTimeline*                         m_pDopeSheet;
		CCurveEditor*                      m_pCurveEditor;

		STimelineContent                   m_timelineContent;
		SCurveEditorContent                m_curveEditorContent;
		std::map<CryGUID, STimelineTrack*> m_uIdToTimelineTrackMap;
	};

	std::map<CryGUID, SSequenceData> m_idToSequenceDataMap;
	std::map<QWidget*, CryGUID>      m_tabToSequenceUIdMap;

	void            UpdateTabNames();
	void            UpdateSequenceTabs();
	void            UpdateToolbar();
	void            UpdatePlaybackRangeMarkers();
	void            SetSelection(CTrackViewNode* pNode);
	CTrackViewNode* GetSelectedNode() const;
	size_t          GetSelectedElementsCount(STimelineTrack* pTimelineTrack = nullptr) const;
	void            SetSequenceTime(const CryGUID sequenceGUID, SAnimTime newTime);

	QWidget*        GetCurrentDopeSheetContainer() const;
	QWidget*        GetCurrentCurveEditorContainer() const;

	// TrackView Model -> Dopesheet
	void InsertDopeSheetTrack(STimelineTrack* pParentTrack, STimelineTrack* pTrack, CTrackViewNode* pNode);
	void AddNodeToDopeSheet(CTrackViewNode* pNode);
	void RemoveNodeFromDopeSheet(CTrackViewNode* pNode);
	void UpdateDopeSheetNode(CTrackViewNode* pNode);

	void AssignDopeSheetName(STimelineTrack* pDopeSheetTrack, CTrackViewAnimNode* pAnimNode);

	void OnSelectionChanged(bool bContinuous);
	void OnCurveEditorSelectionChanged();
	void OnSelectionRefresh();

	void GetKeyProperties(Serialization::SStructs& structs);
	void GetNodeProperties(Serialization::SStructs& structs);
	void GetSequenceProperties(Serialization::SStructs& structs);

	// Dopesheet -> TrackView Model
	void ApplyDopeSheetChanges(CTimeline* pDopeSheet);
	void ApplyTrackChangesRecursive(const CTrackViewSequence* pSequence, STimelineTrack* pTrack);
	void UpdateTrackViewTrack(CTrackViewTrack* pTrack);
	void RemoveTrackViewNode(CTrackViewNode* pNode);

	// TrackView Model -> curve editor
	void                        UpdateCurveEditor(CTimeline* pDopeSheet, CCurveEditor* pCurveEditor);

	void                        UpdateActiveSequence();
	void                        FillAddNodeMenu(QMenu& addNodeMenu);
	void                        CreateAddTrackMenu(QMenu& parentMenu, const CTrackViewAnimNode& animNode);
	void                        RemoveDeletedTracks(STimelineTrack* pTrack);
	CTrackViewAnimNode*         CheckValidReparenting(CTimeline* pDopeSheet, const std::vector<STimelineTrack*>& selectedTracks, STimelineTrack* pTarget);
	CTrackViewSequence*         GetSequenceFromDopeSheet(const CTimeline* pDopeSheet);
	CTrackViewNode*             GetNodeFromDopeSheetTrack(const CTrackViewSequence* pSequence, const STimelineTrack* pDopeSheetTrack);
	STimelineTrack*             GetDopeSheetTrackFromNode(const CTrackViewNode* pNode);
	CryGUID                     GetGUIDFromDopeSheetTrack(const STimelineTrack* pDopeSheetTrack) const;

	_smart_ptr<IAnimKeyWrapper> GetWrappedKeyFromElement(const STimelineTrack& track, const STimelineElement& element);

	CTimeline*                  GetDopeSheetFromSequenceGUID(const CryGUID& guid) const;

	CCurveEditor*               GetActiveCurveEditor() const;
	CCurveEditor*               GetCurveEditorFromSequenceGUID(const CryGUID& guid) const;

	CryGUID                     GetSequenceGUIDFromTabIndex(int index) const;
	CryGUID                     GetActiveSequenceGUID();
	SSequenceData*              GetActiveSequenceData();

	bool                    m_timelineLink;

	bool                    m_bDontUpdateDopeSheet;
	bool                    m_bDontUpdateCurveEditor;

	bool                    m_showDopeSheet;
	bool                    m_showCurveEditor;
	bool                    m_showKeyText;

	SAnimTime::EDisplayMode m_timeUnit;

	// Node context menu
	CTrackViewNode* m_pContextMenuNode;

	QTabWidget*     m_pDopesheetTabs;
	CTrackViewNode* m_pCurrentSelectedNode;
	CTimeline*      m_pCurrentSelectionDopeSheet;
	CCurveEditor*   m_pCurrentSelectionCurveEditor;
	QToolBar*       m_pToolbar;
	SelectedKeys    m_currentKeySelection;

	QTimer*         m_refreshTimer;
	SAnimTime       m_time;
};

