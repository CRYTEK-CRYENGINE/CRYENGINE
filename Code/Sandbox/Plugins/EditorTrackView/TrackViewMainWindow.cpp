// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// CryEngine Source File.
// Copyright (C), Crytek, 1999-2014.

#include "StdAfx.h"
#include "TrackViewMainWindow.h"
#include "TrackViewPlugin.h"
#include "TrackViewUndo.h"
#include "QtUtil.h"
#include "Undo/Undo.h"
#include "Include/IObjectManager.h"
#include "Viewport.h"
#include "Objects/EntityObject.h"

#include <CrySerialization/IArchiveHost.h>
#include <CrySerialization/Enum.h>

#include <CryMovie/AnimKey_impl.h>
#include <CryMath/ISplines.h>

#include <QMenu>
#include <QInputDialog>
#include <QPushButton>
#include <QToolBar>
#include <QVBoxLayout>
#include <QSplitter>
#include <QKeyEvent>

#include <QVBoxLayout>
#include <QDockWidget>

//#include "Controls/PropertyTreeDockWidget.h"
//#include "Controls/SequencesListDockWidget.h"
//#include "Controls/PlaybackControlsToolbar.h"

std::vector<CTrackViewMainWindow*> CTrackViewMainWindow::ms_trackViewWindows;

namespace Private_TrackViewWindow
{
struct STrackUserData
{
	void Serialize(Serialization::IArchive& ar)
	{
		ar(m_nodeType, "type");
		ar(m_guid, "guid");
		ar(m_keyType, "keyType");
	}

	int     m_nodeType;
	CryGUID m_guid;
	string  m_keyType;
};

std::vector<SAddableNode> GetAddableNodes(const CTrackViewAnimNode& animNode)
{
	std::vector<EAnimNodeType> addableNodeTypes = animNode.GetAddableNodeTypes();
	std::vector<SAddableNode> addableNodes;

	// Add tracks to menu, that can be added to animation node.
	const size_t typeCount = addableNodeTypes.size();
	for (size_t i = 0; i < typeCount; ++i)
	{
		SAddableNode addableNode;
		const EAnimNodeType type = addableNodeTypes[i];

		if (type == eAnimNodeType_Alembic)
		{
			// Not supported
			continue;
		}

		if (type == eAnimNodeType_Entity || type == eAnimNodeType_GeomCache || type == eAnimNodeType_Camera)
		{
			// Entities are added by "Add selected entitties"
			continue;
		}

		addableNode.m_type = type;
		addableNode.m_name = gEnv->pMovieSystem->GetDefaultNodeName(type);
		addableNodes.push_back(addableNode);
	}

	return addableNodes;
}

std::vector<SAddableTrack> GetAddableTracks(const CTrackViewAnimNode& animNode)
{
	std::vector<CAnimParamType> addableParams = animNode.GetAddableParams();
	std::vector<SAddableTrack> addableTracks;

	// Add tracks to menu, that can be added to animation node.
	const size_t paramCount = addableParams.size();
	for (size_t i = 0; i < paramCount; ++i)
	{
		SAddableTrack addableTrack;
		addableTrack.m_type = addableParams[i];
		addableTrack.m_name = animNode.GetParamName(addableParams[i]);
		addableTracks.push_back(addableTrack);
	}

	return addableTracks;
}

void ForEachTrack(STimelineTrack& track, std::function<void (STimelineTrack& track)> fun)
{
	fun(track);

	for (size_t i = 0; i < track.tracks.size(); ++i)
	{
		STimelineTrack& subTrack = *track.tracks[i];
		ForEachTrack(subTrack, fun);
	}
}

void ForEachTrack(STimelineTrack& track, std::vector<STimelineTrack*>& parents, std::function<void (STimelineTrack& track)> fun)
{
	fun(track);

	for (size_t i = 0; i < track.tracks.size(); ++i)
	{
		parents.push_back(&track);
		STimelineTrack& subTrack = *track.tracks[i];
		ForEachTrack(subTrack, parents, fun);
		parents.pop_back();
	}
}

void ForEachNode(CTrackViewNode& node, std::function<void (CTrackViewNode& node)> fun)
{
	fun(node);

	const uint numChildNodes = node.GetChildCount();
	for (uint i = 0; i < numChildNodes; ++i)
	{
		CTrackViewNode* pChildNode = node.GetChild(i);
		ForEachNode(*pChildNode, fun);
	}
}

ColorB GetCurveColor(CAnimParamType paramType)
{
	switch (paramType.GetType())
	{
	case eAnimParamType_PositionX:
	case eAnimParamType_RotationX:
	case eAnimParamType_ColorR:
		return ColorB(243, 126, 121);
	case eAnimParamType_PositionY:
	case eAnimParamType_RotationY:
	case eAnimParamType_ColorG:
		return ColorB(187, 243, 121);
	case eAnimParamType_PositionZ:
	case eAnimParamType_RotationZ:
	case eAnimParamType_ColorB:
		return ColorB(121, 152, 243);
	}

	return ColorB(243, 121, 223);
}
}

CTrackViewMainWindow::CTrackViewMainWindow()
	: CDockableWindow()
	, m_activeSequenceUId(CryGUID::Null())
	, m_pCurrentSelectionDopeSheet(nullptr)
	, m_pCurrentSelectionCurveEditor(nullptr)
	, m_pCurrentSelectedNode(nullptr)
	, m_bDontUpdateProperties(false)
	, m_bDontUpdateDopeSheet(false)
	, m_bDontUpdateCurveEditor(false)
	, m_bSnapModeCanUpdate(true)
	, m_SnapMode(eSnapMode_NoSnapping)
{
	// Dopesheet / CurveEditor Widget
	m_pDopesheetTabs = new QTabWidget();
	m_pDopesheetTabs->setTabsClosable(true);
	m_pDopesheetTabs->setStyleSheet(
	  "QTabBar::close-button { image:url(:/Icons/close.png) }"
	  "QTabBar::close-button:hover { image:url(:/Icons/close_hover.png) }"
	  "QTabBar::tab { height: 24px }");
	connect(m_pDopesheetTabs, SIGNAL(currentChanged(int)), this, SLOT(OnDopeSheetTabChanged(int)));
	connect(m_pDopesheetTabs, SIGNAL(tabCloseRequested(int)), this, SLOT(OnDopeSheetTabClose(int)));

	QVBoxLayout* pLayout = new QVBoxLayout(this);
	pLayout->setSpacing(0);
	pLayout->setMargin(0);
	pLayout->addWidget(m_pDopesheetTabs);

	QWidget* pWidget = new QWidget();
	pWidget->setLayout(pLayout);
	setCentralWidget(pWidget);

	// Sequence List Widget
	m_pSequencesListWidget = new CTrackViewSequencesListDockWidget();
	addDockWidget(Qt::DockWidgetArea::RightDockWidgetArea, m_pSequencesListWidget);
	connect(m_pSequencesListWidget->GetAddSequenceButton(), SIGNAL(pressed()), this, SLOT(OnNewSequence()));
	connect(m_pSequencesListWidget->GetListWidget(), SIGNAL(itemPressed(QListWidgetItem*)), this, SLOT(OnSequenceListItemPressed(QListWidgetItem*)));
	connect(m_pSequencesListWidget->GetListWidget(), SIGNAL(itemDoubleClicked(QListWidgetItem*)), this, SLOT(OnSequenceListItemDoubleClicked(QListWidgetItem*)));
	connect(m_pSequencesListWidget->GetFilterLineWidget(), SIGNAL(textChanged(const QString &)), this, SLOT(OnSequencesFilterChanged()));
	m_pSequencesListWidget->installEventFilter(this);

	// Property Tree Widget
	m_pPropertyTreeWidget = new CTrackViewPropertyTreeDockWidget();
	addDockWidget(Qt::DockWidgetArea::RightDockWidgetArea, m_pPropertyTreeWidget);
	connect(m_pPropertyTreeWidget->GetPropertyTree(), SIGNAL(signalPushUndo()), this, SLOT(OnPropertiesUndoPush()));
	connect(m_pPropertyTreeWidget->GetPropertyTree(), SIGNAL(signalChanged()), this, SLOT(OnPropertiesChanged()));

	// Playback Toolbar
	m_pPlaybackToolbar = new CTrackViewPlaybackControlsToolbar();
	addToolBar(m_pPlaybackToolbar);
	connect(m_pPlaybackToolbar->GetButtonPlay(), SIGNAL(toggled(bool)), this, SLOT(OnPlayPause(bool)));
	connect(m_pPlaybackToolbar->GetButtonStop(), SIGNAL(triggered()), this, SLOT(OnStop()));
	connect(m_pPlaybackToolbar->GetButtonGoToStart(), SIGNAL(triggered()), this, SLOT(OnGoToStart()));
	connect(m_pPlaybackToolbar->GetButtonGoToEnd(), SIGNAL(triggered()), this, SLOT(OnGoToEnd()));
	connect(m_pPlaybackToolbar->GetButtonRecord(), SIGNAL(toggled(bool)), this, SLOT(OnRecord(bool)));

	// Sequence Toolbar
	m_pSequenceToolbar = new CTrackViewSequenceToolbar();
	addToolBar(m_pSequenceToolbar);
	connect(m_pSequenceToolbar->GetButtonAddSelectedEntities(), SIGNAL(triggered()), this, SLOT(OnAddSelectedEntities()));
	connect(m_pSequenceToolbar->GetButtonAddDirector(), SIGNAL(triggered()), this, SLOT(OnAddDirectorNode()));

	// Keys Toolbar
	m_pKeysToolbar = new CTrackViewKeysToolbar();
	addToolBar(m_pKeysToolbar);
	connect(m_pKeysToolbar->GetButtonGoToPreviousKey(), SIGNAL(triggered()), this, SLOT(OnGoToPreviousKey()));
	connect(m_pKeysToolbar->GetButtonGoToNextKey(), SIGNAL(triggered()), this, SLOT(OnGoToNextKey()));

	connect(m_pKeysToolbar->GetButtonNoSnapping(), SIGNAL(toggled(bool)), this, SLOT(OnNoSnapping(bool)));
	connect(m_pKeysToolbar->GetButtonTimeSnapping(), SIGNAL(toggled(bool)), this, SLOT(OnSnapTime(bool)));
	connect(m_pKeysToolbar->GetButtonKeySnapping(), SIGNAL(toggled(bool)), this, SLOT(OnSnapKey(bool)));

	connect(m_pKeysToolbar->GetButtonSyncToBase(), SIGNAL(triggered()), this, SLOT(OnSyncSelectedTracksToBase()));
	connect(m_pKeysToolbar->GetButtonSyncFromBase(), SIGNAL(triggered()), this, SLOT(OnSyncSelectedTracksFromBase()));

	// this should go to tab creation
	OnSnapModeChanged(m_SnapMode, true);

	// FrameSettings Toolbar
	m_pFrameSettingsToolbar = new CTrackViewFramerateSettingsToolbar(this);
	addToolBar(m_pFrameSettingsToolbar);

	UpdateSequenceList();
	UpdateActiveSequence();

	CTrackViewPlugin::GetAnimationContext()->AddListener(this);
	CTrackViewPlugin::GetSequenceManager()->AddListener(this);

	CTrackViewSequenceManager* pSequenceManager = CTrackViewPlugin::GetSequenceManager();
	const uint numSequences = pSequenceManager->GetCount();
	for (uint i = 0; i < numSequences; ++i)
	{
		CTrackViewSequence* pSequence = pSequenceManager->GetSequenceByIndex(i);
		pSequence->AddListener(this);
		AddNodeToDopeSheet(pSequence);
	}

	stl::push_back_unique(ms_trackViewWindows, this);
}

CTrackViewMainWindow::~CTrackViewMainWindow()
{
	stl::find_and_erase(ms_trackViewWindows, this);

	CTrackViewSequenceManager* pSequenceManager = CTrackViewPlugin::GetSequenceManager();
	const uint numSequences = pSequenceManager->GetCount();
	for (uint i = 0; i < numSequences; ++i)
	{
		CTrackViewSequence* pSequence = pSequenceManager->GetSequenceByIndex(i);
		pSequence->RemoveListener(this);
	}

	CTrackViewPlugin::GetSequenceManager()->RemoveListener(this);
	CTrackViewPlugin::GetAnimationContext()->RemoveListener(this);
}

void CTrackViewMainWindow::ShowNode(CTrackViewAnimNode* pNode)
{
	CTrackViewSequence* pSequence = pNode->GetSequence();
	CryGUID sequenceGUID = pSequence->GetGUID();

	OpenSequenceTab(sequenceGUID);

	const SSequenceData& sequenceData = m_idToSequenceDataMap[sequenceGUID];
	STimelineTrack* pDopeSheetTrack = stl::find_in_map(sequenceData.m_uIdToTimelineTrackMap, pNode->GetGUID(), nullptr);

	assert(pDopeSheetTrack);
	assert(sequenceData.m_pDopeSheet);

	if (pDopeSheetTrack)
	{
		sequenceData.m_pDopeSheet->focusTrack(pDopeSheetTrack);
	}
}

void CTrackViewMainWindow::OnDopeSheetTabChanged(int index)
{
	UpdateActiveSequence();
}

void CTrackViewMainWindow::OnDopeSheetTabClose(int index)
{
	CryGUID guid = GetSequenceGUIDFromTabIndex(index);
	SSequenceData& sequenceData = m_idToSequenceDataMap[guid];
	sequenceData.m_pDopeSheet = nullptr;
	sequenceData.m_pCurveEditor = nullptr;

	m_tabToSequenceUIdMap.erase(m_pDopesheetTabs->widget(index));
	m_pDopesheetTabs->removeTab(index);
	assert(m_tabToSequenceUIdMap.size() == m_pDopesheetTabs->count());

	UpdateActiveSequence();
}

void CTrackViewMainWindow::OnSequenceListItemPressed(QListWidgetItem* pItem)
{
	const QByteArray guidByteArray = pItem->data(Qt::UserRole).toByteArray();
	const CryGUID guid = *reinterpret_cast<const CryGUID*>(guidByteArray.data());

	CTrackViewSequence* pSequence = CTrackViewPlugin::GetSequenceManager()->GetSequenceByGUID(guid);

	m_pPropertyTreeWidget->GetPropertyTree()->detach();
	m_currentKeySelection.clear();
	m_pCurrentSelectedNode = pSequence;

	m_pPropertyTreeWidget->GetPropertyTree()->attach(Serialization::SStruct(*pSequence));
}

void CTrackViewMainWindow::OnSequenceListItemDoubleClicked(QListWidgetItem* pItem)
{
	const QByteArray guidByteArray = pItem->data(Qt::UserRole).toByteArray();
	const CryGUID sequenceGUID = *reinterpret_cast<const CryGUID*>(guidByteArray.data());
	OpenSequenceTab(sequenceGUID);
}

void CTrackViewMainWindow::OpenSequenceTab(const CryGUID sequenceGUID)
{
	CTrackViewSequence* pSequence = CTrackViewPlugin::GetSequenceManager()->GetSequenceByGUID(sequenceGUID);
	assert(pSequence);
	if (!pSequence)
	{
		return;
	}

	SSequenceData& sequenceData = m_idToSequenceDataMap[sequenceGUID];

	bool bOpen = false;
	const int numTabs = m_pDopesheetTabs->count();
	for (int i = 0; i < numTabs; ++i)
	{
		const CryGUID guid = GetSequenceGUIDFromTabIndex(i);
		if (sequenceGUID == guid)
		{
			m_pDopesheetTabs->setCurrentIndex(i);
			UpdateActiveSequence();
			return;
		}
	}

	sequenceData.m_timelineContent.userSideLoad.resize(sizeof(CryGUID));
	memcpy(&sequenceData.m_timelineContent.userSideLoad[0], &sequenceGUID, sizeof(CryGUID));

	QSplitter* pSplitter = new QSplitter(Qt::Vertical, m_pDopesheetTabs);
	pSplitter->setHandleWidth(6);

	QFrame* pTimelineFrame = new QFrame(m_pDopesheetTabs);
	pTimelineFrame->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
	QVBoxLayout* pTimelineLayout = new QVBoxLayout(pTimelineFrame);
	pTimelineLayout->setMargin(0);
	pTimelineLayout->setSpacing(0);
	pTimelineFrame->setLayout(pTimelineLayout);

	CTimeline* pDopeSheet = new CTimeline(pTimelineFrame);
	pDopeSheet->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
	pDopeSheet->SetContent(&sequenceData.m_timelineContent);
	pDopeSheet->SetCycled(false);
	pDopeSheet->SetSizeToContent(false);
	pDopeSheet->SetKeyWidth(3);
	pDopeSheet->SetKeyRadius(0.0f);
	pDopeSheet->SetTreeVisible(true);
	pDopeSheet->SetDrawSelectionIndicators(true);
	pDopeSheet->SetVerticalScrollbarVisible(true);
	pDopeSheet->SetDrawTrackTimeMarkers(true);
	pDopeSheet->SetVisibleDistance((sequenceData.m_endTime - sequenceData.m_startTime).ToFloat());
	//pDopeSheet->SetKeySnapping(m_bSnapTime);
	pDopeSheet->SetTime(pSequence->GetTime());

	QIcon addNodeIcon = QIcon(":/Icons/add_node.png");
	QPushButton* pAddNodeButton = new QPushButton(pDopeSheet);
	pAddNodeButton->setIcon(addNodeIcon);
	pDopeSheet->SetCustomTreeCornerWidget(pAddNodeButton, 20);
	connect(pAddNodeButton, SIGNAL(clicked()), this, SLOT(OnAddNodeButtonClicked()));

	pTimelineLayout->addWidget(pDopeSheet);

	connect(pDopeSheet, SIGNAL(SignalScrub(bool)), this, SLOT(OnDopesheetScrub()));
	connect(pDopeSheet, SIGNAL(SignalContentChanged(bool)), this, SLOT(OnDopesheetContentChanged(bool)));
	connect(pDopeSheet, SIGNAL(SignalSelectionChanged(bool)), this, SLOT(OnDopesheetContentChanged(bool)));
	connect(pDopeSheet, SIGNAL(SignalTrackSelectionChanged()), this, SLOT(OnDopesheetTrackSelectionChanged()));
	connect(pDopeSheet, SIGNAL(SignalTreeContextMenu(const QPoint &)), this, SLOT(OnDopesheetTreeContextMenu(const QPoint &)));
	connect(pDopeSheet, SIGNAL(SignalTracksBeginDrag(STimelineTrack*, bool&)), this, SLOT(OnDopesheetTracksBeginDrag(STimelineTrack*, bool&)));
	connect(pDopeSheet, SIGNAL(SignalTracksDragging(STimelineTrack*, bool&)), this, SLOT(OnDopesheetTracksDragging(STimelineTrack*, bool&)));
	connect(pDopeSheet, SIGNAL(SignalTracksDragged(STimelineTrack*)), this, SLOT(OnDopesheetTracksDragged(STimelineTrack*)));

	QFrame* pCurveEditorFrame = new QFrame(m_pDopesheetTabs);
	pCurveEditorFrame->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
	QVBoxLayout* pCurveEditorLayout = new QVBoxLayout(pCurveEditorFrame);
	pCurveEditorLayout->setMargin(0);
	pCurveEditorLayout->setSpacing(0);
	pCurveEditorFrame->setLayout(pCurveEditorLayout);

	CCurveEditor* pCurveEditor = new CCurveEditor(pCurveEditorFrame);
	pCurveEditor->SetContent(&sequenceData.m_curveEditorContent);
	pCurveEditor->SetCurveType(eCECT_2DBezier);
	pCurveEditor->SetTimeRange(sequenceData.m_startTime, sequenceData.m_endTime);
	pCurveEditor->SetTime(pSequence->GetTime());
	pCurveEditor->SetGridVisible(true);
	QToolBar* pCurveEditorToolBar = new QToolBar(pCurveEditorFrame);
	pCurveEditorToolBar->setIconSize(QSize(24, 24));
	pCurveEditor->FillWithCurveToolsAndConnect(pCurveEditorToolBar);
	//pCurveEditor->SetKeySnapping(m_bSnapTime);

	pCurveEditorLayout->addWidget(pCurveEditorToolBar);
	pCurveEditorLayout->addWidget(pCurveEditor);

	connect(pCurveEditor, SIGNAL(SignalContentChanged()), this, SLOT(OnCurveEditorContentChanged()));
	connect(pCurveEditor, SIGNAL(SignalScrub()), this, SLOT(OnCurveEditorScrub()));

	pSplitter->addWidget(pTimelineFrame);
	pSplitter->addWidget(pCurveEditorFrame);

	QList<int> sizes;
	sizes << 50 << 50;
	pSplitter->setSizes(sizes);

	sequenceData.m_pDopeSheet = pDopeSheet;
	sequenceData.m_pCurveEditor = pCurveEditor;

	m_tabToSequenceUIdMap[pSplitter] = sequenceGUID;

	const string sequenceName = pSequence->GetName();
	m_pDopesheetTabs->setCurrentIndex(m_pDopesheetTabs->addTab(pSplitter, sequenceName.c_str()));

	UpdateActiveSequence();
}

void CTrackViewMainWindow::OnDopesheetScrub()
{
	CTimeline* pDopeSheet = static_cast<CTimeline*>(QObject::sender());
	if (pDopeSheet)
	{
		const SAnimTime newTime = pDopeSheet->Time();

		CTrackViewSequence* pSequence = GetActiveSequence();
		CAnimationContext* pAnimationContext = CTrackViewPlugin::GetAnimationContext();
		if (pSequence == pAnimationContext->GetSequence())
		{
			pAnimationContext->SetTime(newTime);
		}

		CCurveEditor* pCurveEditor = GetActiveCurveEditor();
		if (pCurveEditor)
		{
			pCurveEditor->SetTime(newTime);
		}
	}
}

void CTrackViewMainWindow::OnCurveEditorScrub()
{
	CCurveEditor* pCurveEditor = static_cast<CCurveEditor*>(QObject::sender());
	if (pCurveEditor)
	{
		const SAnimTime newTime = pCurveEditor->Time();

		CTrackViewSequence* pSequence = GetActiveSequence();
		CAnimationContext* pAnimationContext = CTrackViewPlugin::GetAnimationContext();
		if (pSequence == pAnimationContext->GetSequence())
		{
			pAnimationContext->SetTime(newTime);
		}

		CTimeline* pDopeSheet = GetActiveDopeSheet();
		if (pDopeSheet)
		{
			pDopeSheet->SetTime(newTime);
		}
	}
}

bool CTrackViewMainWindow::eventFilter(QObject* pWatched, QEvent* pEvent)
{
	if (pWatched == m_pSequencesListWidget && pEvent->type() == QEvent::KeyPress)
	{
		QKeyEvent* pKeyEvent = static_cast<QKeyEvent*>(pEvent);
		if (pKeyEvent->key() == Qt::Key_Backspace || pKeyEvent->key() == Qt::Key_Delete)
		{
			DeleteSelectedSequences();
			return true;
		}
	}

	return QMainWindow::eventFilter(pWatched, pEvent);
}

void CTrackViewMainWindow::ApplyDopeSheetChanges(CTimeline* pDopeSheet)
{
	m_bDontUpdateDopeSheet = true;

	STimelineContent* pContent = pDopeSheet->Content();

	if (pContent)
	{
		CTrackViewSequence* pSequence = GetSequenceFromDopeSheet(pDopeSheet);
		if (pSequence)
		{
			string undoMessage;
			undoMessage.Format("TrackView sequence '%s' changed", pSequence->GetName());
			CUndo undo(undoMessage);

			CTrackViewSequenceNotificationContext notificationContext(pSequence);
			ApplyTrackChangesRecursive(pSequence, &pContent->track);
		}
	}

	m_bDontUpdateDopeSheet = false;

	UpdateProperties();
}

void CTrackViewMainWindow::OnDopesheetContentChanged(bool continuous)
{
	if (continuous)
	{
		return;
	}

	CTimeline* pDopeSheet = static_cast<CTimeline*>(QObject::sender());
	ApplyDopeSheetChanges(pDopeSheet);

	UpdateProperties();

	CTrackViewSequence* pSequence = GetSequenceFromDopeSheet(pDopeSheet);
	if (pSequence)
	{
		CCurveEditor* pCurveEditor = GetCurveEditorFromSequenceGUID(pSequence->GetGUID());
		if (pCurveEditor)
		{
			UpdateCurveEditor(pDopeSheet, pCurveEditor);
		}
	}
}

namespace
{
typedef std::vector<STimelineTrack*> TSelectedTracks;

void GetSelectedTracks(STimelineTrack& track, TSelectedTracks& tracks)
{
	if (track.selected)
	{
		tracks.push_back(&track);
	}

	for (auto iter = track.tracks.begin(); iter != track.tracks.end(); ++iter)
	{
		GetSelectedTracks(**iter, tracks);
	}
}
}

void CTrackViewMainWindow::UpdateCurveEditor(CTimeline* pDopeSheet, CCurveEditor* pCurveEditor)
{
	if (m_bDontUpdateCurveEditor)
	{
		return;
	}

	STimelineContent* pDopeSheetContent = pDopeSheet->Content();
	SCurveEditorContent* pCurveEditorContent = pCurveEditor->Content();
	CTrackViewSequence* pSequence = GetSequenceFromDopeSheet(pDopeSheet);

	if (pDopeSheetContent && pCurveEditorContent)
	{
		pCurveEditorContent->m_curves.clear();

		std::vector<STimelineTrack*> selectedTracks;
		GetSelectedTracks(pDopeSheetContent->track, selectedTracks);

		// If compound tracks are selected also add the child tracks to the list
		std::vector<STimelineTrack*> displayedTracks;
		size_t numSelectedTracks = selectedTracks.size();
		for (uint i = 0; i < numSelectedTracks; ++i)
		{
			STimelineTrack* pSelectedTrack = selectedTracks[i];
			if ((pSelectedTrack->caps & STimelineTrack::CAP_COMPOUND_TRACK) != 0)
			{
				const size_t numSubTracks = pSelectedTrack->tracks.size();
				for (uint j = 0; j < numSubTracks; ++j)
				{
					stl::push_back_unique(displayedTracks, pSelectedTrack->tracks[j]);
				}
			}
			else
			{
				stl::push_back_unique(displayedTracks, pSelectedTrack);
			}
		}

		const size_t numDisplayedTracks = displayedTracks.size();
		for (size_t i = 0; i < numDisplayedTracks; ++i)
		{
			CTrackViewNode* pNode = GetNodeFromDopeSheetTrack(pSequence, displayedTracks[i]);
			if (pNode->GetNodeType() == eTVNT_Track)
			{
				CTrackViewTrack* pTrack = static_cast<CTrackViewTrack*>(pNode);

				if (!pTrack->IsCompoundTrack() && strcmp(pTrack->GetKeyType(), S2DBezierKey::GetType()) == 0)
				{
					pCurveEditorContent->m_curves.push_back(SCurveEditorCurve());
					SCurveEditorCurve& curve = pCurveEditorContent->m_curves.back();
					curve.m_color = Private_TrackViewWindow::GetCurveColor(pTrack->GetParameterType());
					curve.m_defaultValue = boost::get<float>(pTrack->GetDefaultValue());

					const CryGUID trackGUID = pTrack->GetGUID();
					curve.userSideLoad.resize(sizeof(CryGUID));
					memcpy(&curve.userSideLoad[0], &trackGUID, sizeof(CryGUID));

					const uint numKeys = pTrack->GetKeyCount();
					for (uint i = 0; i < numKeys; ++i)
					{
						CTrackViewKeyHandle& keyHandle = pTrack->GetKey(i);

						S2DBezierKey bezierKey;
						keyHandle.GetKey(&bezierKey);

						SCurveEditorKey curveEditorKey;
						curveEditorKey.m_time = bezierKey.m_time;
						curveEditorKey.m_controlPoint = bezierKey.m_controlPoint;
						curveEditorKey.m_bSelected = keyHandle.IsSelected();
						curve.m_keys.push_back(curveEditorKey);
					}
				}
			}
		}
	}

	pCurveEditor->update();
}

void CTrackViewMainWindow::OnDopesheetTrackSelectionChanged()
{
	CTimeline* pDopeSheet = static_cast<CTimeline*>(QObject::sender());
	CCurveEditor* pCurveEditor = GetActiveCurveEditor();

	if (pDopeSheet && pCurveEditor)
	{
		UpdateCurveEditor(pDopeSheet, pCurveEditor);

		pCurveEditor->OnFitCurvesHorizontally();
		pCurveEditor->OnFitCurvesVertically();

		ShowNodeProperties();
	}
}

void CTrackViewMainWindow::OnDopesheetTreeContextMenu(const QPoint& point)
{
	m_contextMenuLocation = point;

	CTimeline* pDopeSheet = static_cast<CTimeline*>(QObject::sender());
	STimelineContent* pContent = pDopeSheet ? pDopeSheet->Content() : nullptr;
	CTrackViewSequence* pSequence = GetSequenceFromDopeSheet(pDopeSheet);

	if (pContent)
	{
		std::vector<STimelineTrack*> selectedTracks;
		GetSelectedTracks(pContent->track, selectedTracks);

		const size_t numSelectedTracks = selectedTracks.size();

		if (numSelectedTracks > 0)
		{
			std::unique_ptr<QMenu> pContextMenu(new QMenu);
			pContextMenu->addAction("Delete", this, SLOT(OnDeleteNodes()));

			STimelineTrack* pSingleTrack = (numSelectedTracks == 1) ? selectedTracks[0] : nullptr;
			if (pSingleTrack)
			{
				m_pContextMenuNode = GetNodeFromDopeSheetTrack(pSequence, pSingleTrack);

				if (m_pContextMenuNode && m_pContextMenuNode->IsGroupNode())
				{
					m_addableNodes = Private_TrackViewWindow::GetAddableNodes(*static_cast<CTrackViewAnimNode*>(m_pContextMenuNode));

					const size_t numAddableNodes = m_addableNodes.size();
					if (numAddableNodes > 0)
					{
						pContextMenu->addSeparator();
						QMenu* pAddTrackMenu = pContextMenu->addMenu("Add Node");

						for (size_t i = 0; i < numAddableNodes; ++i)
						{
							const SAddableNode& addableNode = m_addableNodes[i];
							QAction* pAction = pAddTrackMenu->addAction(QString(addableNode.m_name), this, SLOT(OnAddNode()));
							pAction->setData(i);
						}
					}
				}

				if (m_pContextMenuNode && m_pContextMenuNode->GetNodeType() == eTVNT_AnimNode)
				{
					CTrackViewAnimNode* pAnimNode = static_cast<CTrackViewAnimNode*>(m_pContextMenuNode);
					CEntityObject* pEntityObject = pAnimNode->GetNodeEntity(true);
					if (pEntityObject)
					{
						pContextMenu->addSeparator();
						QAction* pAction = pContextMenu->addAction("Select in Viewport", this, SLOT(OnSelectEntity()));
					}

					m_addableTracks = Private_TrackViewWindow::GetAddableTracks(*static_cast<CTrackViewAnimNode*>(m_pContextMenuNode));

					if (m_addableTracks.size() > 0)
					{
						pContextMenu->addSeparator();
						QMenu* pAddTrackMenu = pContextMenu->addMenu("Add Track");
						FillAddTrackMenu(*pAddTrackMenu, m_addableTracks);
					}
				}
			}

			pContextMenu->exec(m_contextMenuLocation);
			m_addableTracks.clear();
		}
	}
}

void CTrackViewMainWindow::OnDopesheetTracksBeginDrag(STimelineTrack* pTarget, bool& bValid)
{
	CTimeline* pDopeSheet = static_cast<CTimeline*>(QObject::sender());
	CTrackViewSequence* pSequence = GetSequenceFromDopeSheet(pDopeSheet);
	CTrackViewNode* pNode = GetNodeFromDopeSheetTrack(pSequence, pTarget);

	bValid = (pNode->GetNodeType() == eTVNT_AnimNode);
}

void CTrackViewMainWindow::OnDopesheetTracksDragging(STimelineTrack* pTarget, bool& bValid)
{
	CTimeline* pDopeSheet = static_cast<CTimeline*>(QObject::sender());
	TSelectedTracks selectedTracks;
	GetSelectedTracks(pDopeSheet->Content()->track, selectedTracks);

	bValid = CheckValidReparenting(pDopeSheet, selectedTracks, pTarget) != nullptr;
}

void CTrackViewMainWindow::OnDopesheetTracksDragged(STimelineTrack* pTarget)
{
	CTimeline* pDopeSheet = static_cast<CTimeline*>(QObject::sender());
	TSelectedTracks selectedTracks;
	GetSelectedTracks(pDopeSheet->Content()->track, selectedTracks);

	CTrackViewAnimNode* pTargetNode = CheckValidReparenting(pDopeSheet, selectedTracks, pTarget);
	if (pTargetNode)
	{
		string undoMessage;
		undoMessage.Format("Move nodes to node '%s'", pTargetNode->GetName());
		CUndo undo(undoMessage);

		CTrackViewSequenceNotificationContext context(pTargetNode->GetSequence());
		for (STimelineTrack* pTrack : selectedTracks)
		{
			CTrackViewNode* pSourceNode = GetNodeFromDopeSheetTrack(pTargetNode->GetSequence(), pTrack);
			if (pSourceNode->GetNodeType() == eTVNT_AnimNode)
			{
				CTrackViewAnimNode* pSourceAnimNode = static_cast<CTrackViewAnimNode*>(pSourceNode);
				pSourceAnimNode->SetNewParent(pTargetNode);
			}
		}
	}
}

void CTrackViewMainWindow::OnCurveEditorContentChanged()
{
	CCurveEditor* pCurveEditor = GetActiveCurveEditor();
	SCurveEditorContent* pCurveEditorContent = pCurveEditor->Content();
	CTrackViewSequence* pSequence = GetActiveSequence();
	m_bDontUpdateCurveEditor = true;

	{
		CTrackViewSequenceNotificationContext notificationContext(pSequence);
		CUndo undo("TrackView curve changed");

		CUndo::Record(new CUndoAnimKeySelection(pSequence));
		CTrackViewKeyBundle bundle = pSequence->GetAllKeys();
		bundle.SelectKeys(false);

		const size_t numCurves = pCurveEditorContent->m_curves.size();
		for (size_t i = 0; i < numCurves; ++i)
		{
			SCurveEditorCurve& curve = pCurveEditorContent->m_curves[i];

			CryGUID guid;
			memcpy(&guid, &curve.userSideLoad[0], sizeof(CryGUID));

			CTrackViewTrack* pTrack = static_cast<CTrackViewTrack*>(pSequence->GetNodeFromGUID(guid));
			if (pTrack)
			{
				CUndo::Record(new CUndoTrackObject(pTrack, pSequence));
				pTrack->ClearKeys();

				for (auto iter = curve.m_keys.begin(); iter != curve.m_keys.end(); ++iter)
				{
					if (!iter->m_bDeleted)
					{
						CTrackViewKeyHandle handle = pTrack->CreateKey(SAnimTime(iter->m_time));

						S2DBezierKey bezierKey;
						bezierKey.m_time = SAnimTime(iter->m_time);
						bezierKey.m_controlPoint = iter->m_controlPoint;
						handle.SetKey(&bezierKey);
						handle.Select(iter->m_bSelected);
					}
				}
			}
		}
	}

	m_bDontUpdateCurveEditor = false;
}

namespace
{
typedef std::vector<std::pair<STimelineTrack*, STimelineElement*>> TSelectedElements;

void GetSelectedElements(STimelineTrack& track, TSelectedElements& elements)
{
	for (auto iter = track.elements.begin(); iter != track.elements.end(); ++iter)
	{
		if (iter->selected && !iter->deleted)
		{
			elements.push_back(std::make_pair(&track, &(*iter)));
		}
	}

	for (auto iter = track.tracks.begin(); iter != track.tracks.end(); ++iter)
	{
		GetSelectedElements(**iter, elements);
	}
}
}

// Search for correct insert position and insert track into dope sheet
void CTrackViewMainWindow::InsertDopeSheetTrack(STimelineTrack* pParentTrack, STimelineTrack* pTrack, CTrackViewNode* pNode)
{
	STimelineTracks& siblingTracks = pParentTrack->tracks;
	const uint numSiblingTracks = siblingTracks.size();

	uint insertPosition;
	for (insertPosition = 0; insertPosition < numSiblingTracks; ++insertPosition)
	{
		const STimelineTrack* pSiblingTrack = siblingTracks[insertPosition];
		const CTrackViewNode* pSiblingNode = GetNodeFromDopeSheetTrack(pNode->GetSequence(), pSiblingTrack);
		if (*pNode < *pSiblingNode)
		{
			break;
		}
	}

	siblingTracks.insert(siblingTracks.begin() + insertPosition, pTrack);
}

void CTrackViewMainWindow::AddNodeToDopeSheet(CTrackViewNode* pNode)
{
	if (m_bDontUpdateDopeSheet)
	{
		return;
	}

	CTrackViewSequence* pSequence = pNode->GetSequence();
	SSequenceData& sequenceData = m_idToSequenceDataMap[pSequence->GetGUID()];

	const ETrackViewNodeType nodeType = pNode->GetNodeType();
	STimelineTrack* pParentTrack = (nodeType == eTVNT_Sequence)
	                               ? &sequenceData.m_timelineContent.track
	                               : GetDopeSheetTrackFromNode(pNode->GetParentNode());

	if (!pParentTrack)
	{
		return;
	}

	Private_TrackViewWindow::STrackUserData userData;
	userData.m_guid = pNode->GetGUID();
	userData.m_nodeType = pNode->GetNodeType();

	const CryGUID nodeGUID = pNode->GetGUID();
	assert(sequenceData.m_uIdToTimelineTrackMap.find(nodeGUID) == sequenceData.m_uIdToTimelineTrackMap.end());

	switch (nodeType)
	{
	case eTVNT_Sequence:
		{
			const CTrackViewSequence* pSequence = static_cast<const CTrackViewSequence*>(pNode);
			const TRange<SAnimTime> timeRange = pSequence->GetTimeRange();
			sequenceData.m_uIdToTimelineTrackMap[nodeGUID] = &sequenceData.m_timelineContent.track;
			sequenceData.m_startTime = timeRange.start;
			sequenceData.m_endTime = timeRange.end;
			Serialization::SaveBinaryBuffer(sequenceData.m_timelineContent.track.userSideLoad, userData);
		}
		break;
	case eTVNT_AnimNode:
		{
			STimelineTrack* pDopeSheetTrack = new STimelineTrack;
			sequenceData.m_uIdToTimelineTrackMap[nodeGUID] = pDopeSheetTrack;

			pDopeSheetTrack->name = pNode->GetName();
			pDopeSheetTrack->height = 16;
			pDopeSheetTrack->caps |= STimelineTrack::CAP_DESCRIPTION_TRACK;

			Serialization::SaveBinaryBuffer(pDopeSheetTrack->userSideLoad, userData);
			InsertDopeSheetTrack(pParentTrack, pDopeSheetTrack, pNode);
		}
		break;
	case eTVNT_Track:
		{
			STimelineTrack* pDopeSheetTrack = new STimelineTrack;
			sequenceData.m_uIdToTimelineTrackMap[nodeGUID] = pDopeSheetTrack;

			pDopeSheetTrack->name = pNode->GetName();
			pDopeSheetTrack->height = 16;
			pDopeSheetTrack->startTime = sequenceData.m_startTime;
			pDopeSheetTrack->endTime = sequenceData.m_endTime;
			pDopeSheetTrack->defaultElement.caps = STimelineElement::CAP_SELECT | STimelineElement::CAP_MOVE | STimelineElement::CAP_CLIP_DRAW_KEY;

			CTrackViewTrack* pTrack = static_cast<CTrackViewTrack*>(pNode);
			userData.m_keyType = pTrack->GetKeyType();
			Serialization::SaveBinaryBuffer(pDopeSheetTrack->userSideLoad, userData);

			if (pTrack->KeysHaveDuration())
			{
				pDopeSheetTrack->defaultElement.type = STimelineElement::CLIP;
			}

			if (pTrack->IsCompoundTrack())
			{
				pDopeSheetTrack->caps |= STimelineTrack::CAP_COMPOUND_TRACK;
				pDopeSheetTrack->expanded = false;
			}
			else
			{
				if (pTrack->GetValueType() == eAnimValue_Bool)
				{
					pDopeSheetTrack->caps |= STimelineTrack::CAP_TOGGLE_TRACK;
					pDopeSheetTrack->toggleDefaultState = boost::get<bool>(pTrack->GetDefaultValue());
				}

				UpdateDopeSheetTrack(pTrack);
			}

			InsertDopeSheetTrack(pParentTrack, pDopeSheetTrack, pNode);
		}
		break;
	}

	const uint numChildren = pNode->GetChildCount();
	for (uint i = 0; i < numChildren; ++i)
	{
		CTrackViewNode* pChildNode = pNode->GetChild(i);
		AddNodeToDopeSheet(pChildNode);
	}
}

void CTrackViewMainWindow::RemoveNodeFromDopeSheet(CTrackViewNode* pNode)
{
	if (m_bDontUpdateDopeSheet)
	{
		return;
	}

	CTrackViewSequence* pSequence = pNode->GetSequence();
	SSequenceData& sequenceData = m_idToSequenceDataMap[pSequence->GetGUID()];
	STimelineTrack* pDopeSheetTrack = GetDopeSheetTrackFromNode(pNode);

	int findCount = 0;
	STimelineTrack* pParentTrack = nullptr;

	std::vector<STimelineTrack*> parents;
	Private_TrackViewWindow::ForEachTrack(sequenceData.m_timelineContent.track, parents, [&](STimelineTrack& track)
	{
		if (&track == pDopeSheetTrack && parents.size() > 0)
		{
		  ++findCount;
		  pParentTrack = parents.back();
		}
	});
	assert(findCount == 1);

	assert(stl::find(pParentTrack->tracks, pDopeSheetTrack));
	stl::find_and_erase(pParentTrack->tracks, pDopeSheetTrack);

	// Remove all GUIDs for this node and its children from the map
	Private_TrackViewWindow::ForEachNode(*pNode, [&](CTrackViewNode& node)
	{
		CryGUID nodeGUID = node.GetGUID();
		assert(sequenceData.m_uIdToTimelineTrackMap.find(nodeGUID) != sequenceData.m_uIdToTimelineTrackMap.end());
		sequenceData.m_uIdToTimelineTrackMap.erase(nodeGUID);
	});
}

void CTrackViewMainWindow::UpdateDopeSheetTrack(CTrackViewTrack* pTrack)
{
	if (m_bDontUpdateDopeSheet)
	{
		return;
	}

	if (pTrack->IsCompoundTrack())
	{
		const uint numChildTracks = pTrack->GetChildCount();
		for (uint i = 0; i < numChildTracks; ++i)
		{
			CTrackViewNode* pChild = pTrack->GetChild(i);
			UpdateDopeSheetTrack(static_cast<CTrackViewTrack*>(pChild));
		}
		return;
	}

	CTrackViewSequence* pSequence = pTrack->GetSequence();
	SSequenceData& sequenceData = m_idToSequenceDataMap[pSequence->GetGUID()];
	STimelineTrack* pDopeSheetTrack = GetDopeSheetTrackFromNode(pTrack);

	if (pDopeSheetTrack)
	{
		pDopeSheetTrack->elements.clear();

		const bool bKeysHaveDuration = pTrack->KeysHaveDuration();
		const STimelineElement::EType keyType = bKeysHaveDuration ? STimelineElement::CLIP : STimelineElement::KEY;

		const uint numKeys = pTrack->GetKeyCount();
		for (uint i = 0; i < numKeys; ++i)
		{
			CTrackViewKeyHandle& keyHandle = pTrack->GetKey(i);

			STimelineElement element = pDopeSheetTrack->defaultElement;
			element.start = keyHandle.GetTime();
			element.end = element.start + max(SAnimTime(0), keyHandle.GetDuration());
			element.selected = keyHandle.IsSelected();
			element.description = keyHandle.GetDescription();
			element.type = keyType;

			if (bKeysHaveDuration)
			{
				element.clipAnimDuration = keyHandle.GetCycleDuration();
				element.clipAnimStart = keyHandle.GetAnimStart();
				element.clipLoopable = keyHandle.IsAnimLoopable();
			}

			const _smart_ptr<IAnimKeyWrapper> pWrappedKey = keyHandle.GetWrappedKey();
			if (pWrappedKey)
			{
				Serialization::SaveBinaryBuffer(element.userSideLoad, *pWrappedKey);
			}

			pDopeSheetTrack->elements.push_back(element);
		}
	}
}

void CTrackViewMainWindow::ApplyTrackChangesRecursive(const CTrackViewSequence* pSequence, STimelineTrack* pTrack)
{
	CTrackViewNode* pTrackViewNode = pSequence->GetNodeFromGUID(GetGUIDFromDopeSheetTrack(pTrack));

	if (pTrackViewNode)
	{
		if (pTrack->deleted)
		{
			RemoveTrackViewNode(pTrackViewNode);
		}
		else if (pTrackViewNode->GetNodeType() == eTVNT_Track)
		{
			if (pTrack->modified || pTrack->keySelectionChanged)
			{
				CTrackViewTrack* pTrackViewTrack = static_cast<CTrackViewTrack*>(pTrackViewNode);
				UpdateTrackViewTrack(pTrackViewTrack);

				// Need to call UpdateDopeSheetTrack to update key durations and descriptions
				const bool bOldDontUpdateDopeSheet = m_bDontUpdateDopeSheet;
				m_bDontUpdateDopeSheet = false;
				UpdateDopeSheetTrack(pTrackViewTrack);
				m_bDontUpdateDopeSheet = bOldDontUpdateDopeSheet;
			}
		}
	}

	for (auto iter = pTrack->tracks.begin(); iter != pTrack->tracks.end(); ++iter)
	{
		ApplyTrackChangesRecursive(pSequence, *iter);
	}
}

void CTrackViewMainWindow::UpdateTrackViewTrack(CTrackViewTrack* pTrack)
{
	CTrackViewSequence* pSequence = pTrack->GetSequence();
	SSequenceData& sequenceData = m_idToSequenceDataMap[pSequence->GetGUID()];
	STimelineTrack* pDopeSheetTrack = GetDopeSheetTrackFromNode(pTrack);
	CTrackViewSequenceNotificationContext notificationContext(pSequence);

	if (pDopeSheetTrack)
	{
		CUndo::Record(new CUndoTrackObject(pTrack, pSequence));
		pTrack->ClearKeys();

		// Do two passes: First add all keys that were already there and not deleted, then add deleted keys.
		// This allows CTrackViewSplineTrack::CreateKey to initialize the value of the new key to the
		// correct interpolated value on the Bezier segment
		for (uint i = 0; i < 2; ++i)
		{
			for (auto iter = pDopeSheetTrack->elements.begin(); iter != pDopeSheetTrack->elements.end(); ++iter)
			{
				if (iter->deleted || (iter->added && i == 0) || (!iter->added && i == 1))
				{
					continue;
				}

				const SAnimTime time(iter->start);
				CTrackViewKeyHandle handle = pTrack->CreateKey(time);

				if (!iter->added)
				{
					_smart_ptr<IAnimKeyWrapper> pWrappedKey = GetWrappedKeyFromElement(*pDopeSheetTrack, *iter);
					if (pWrappedKey)
					{
						handle.SetKey(pWrappedKey->Key());
					}
				}

				handle.SetDuration(SAnimTime(iter->end - iter->start));
				handle.Select(iter->selected);

				Serialization::SaveBinaryBuffer(iter->userSideLoad, *handle.GetWrappedKey());
			}
		}
	}
}

void CTrackViewMainWindow::RemoveTrackViewNode(CTrackViewNode* pNode)
{
	if (pNode)
	{
		CTrackViewAnimNode* pParentNode = static_cast<CTrackViewAnimNode*>(pNode->GetParentNode());
		if (pParentNode)
		{
			if (pNode->GetNodeType() == eTVNT_AnimNode)
			{
				pParentNode->RemoveSubNode(static_cast<CTrackViewAnimNode*>(pNode));
			}
			else if (pNode->GetNodeType() == eTVNT_Track)
			{
				pParentNode->RemoveTrack(static_cast<CTrackViewTrack*>(pNode));
			}
		}
	}
}

void CTrackViewMainWindow::UpdateProperties()
{
	if (m_bDontUpdateProperties)
	{
		return;
	}

	if (m_currentKeySelection.size() > 0)
	{
		ShowKeyProperties();
	}
	else if (m_pCurrentSelectedNode)
	{
		if (m_pCurrentSelectedNode->GetNodeType() == eTVNT_Sequence)
		{
			m_pPropertyTreeWidget->GetPropertyTree()->detach();
			Serialization::SStructs structs;
			structs.push_back(Serialization::SStruct(*m_pCurrentSelectedNode));
			m_pPropertyTreeWidget->GetPropertyTree()->attach(structs);
		}
		else
		{
			ShowNodeProperties();
		}
	}
	else
	{
		m_pPropertyTreeWidget->GetPropertyTree()->detach();
	}
}

void CTrackViewMainWindow::ShowKeyProperties()
{
	m_pPropertyTreeWidget->GetPropertyTree()->detach();
	m_currentKeySelection.clear();
	m_pCurrentSelectedNode = nullptr;
	m_pCurrentSelectionDopeSheet = nullptr;
	m_pCurrentSelectionCurveEditor = nullptr;

	CTimeline* pDopeSheet = GetActiveDopeSheet();
	CCurveEditor* pCurveEditor = GetActiveCurveEditor();
	STimelineContent* pContent = pDopeSheet ? pDopeSheet->Content() : nullptr;

	if (pContent)
	{
		m_pCurrentSelectionDopeSheet = pDopeSheet;
		m_pCurrentSelectionCurveEditor = pCurveEditor;

		TSelectedElements elements;
		GetSelectedElements(pContent->track, elements);
		Serialization::SStructs structs;

		const size_t numElements = elements.size();

		for (size_t i = 0; i < numElements; ++i)
		{
			SSelectedKey selectedKey;

			selectedKey.m_pTrack = elements[i].first;
			selectedKey.m_pElement = elements[i].second;

			_smart_ptr<IAnimKeyWrapper> pKeyWrapper = GetWrappedKeyFromElement(*selectedKey.m_pTrack, *selectedKey.m_pElement);
			if (pKeyWrapper)
			{
				structs.push_back(Serialization::SStruct(*pKeyWrapper));
				selectedKey.m_key = pKeyWrapper;
				m_currentKeySelection.push_back(selectedKey);
			}
		}

		m_pPropertyTreeWidget->GetPropertyTree()->attach(structs);
	}
}

void CTrackViewMainWindow::ShowNodeProperties()
{
	m_pPropertyTreeWidget->GetPropertyTree()->detach();
	m_currentKeySelection.clear();
	m_pCurrentSelectedNode = nullptr;

	CTimeline* pDopeSheet = GetActiveDopeSheet();
	STimelineContent* pContent = pDopeSheet ? pDopeSheet->Content() : nullptr;
	CTrackViewSequence* pSequence = GetSequenceFromDopeSheet(pDopeSheet);

	if (pContent)
	{
		TSelectedTracks selectedTracks;
		GetSelectedTracks(pContent->track, selectedTracks);

		if (selectedTracks.size() == 1)
		{
			CTrackViewNode* pNode = GetNodeFromDopeSheetTrack(pSequence, selectedTracks[0]);
			if (pNode)
			{
				m_pPropertyTreeWidget->GetPropertyTree()->attach(Serialization::SStruct(*pNode));
				m_pCurrentSelectedNode = pNode;
			}
		}
	}
}

void CTrackViewMainWindow::OnPropertiesUndoPush()
{
	GetIEditor()->GetUndoManager()->Begin();
	m_bDontUpdateProperties = true;
}

void CTrackViewMainWindow::OnPropertiesChanged()
{
	const size_t numSelectedElements = m_currentKeySelection.size();
	if (numSelectedElements > 0)
	{
		for (size_t i = 0; i < numSelectedElements; ++i)
		{
			SSelectedKey selectedKey = m_currentKeySelection[i];
			Serialization::SaveBinaryBuffer(selectedKey.m_pElement->userSideLoad, *selectedKey.m_key);
			selectedKey.m_pTrack->modified = true;
		}

		if (m_pCurrentSelectionDopeSheet)
		{
			ApplyDopeSheetChanges(m_pCurrentSelectionDopeSheet);
			m_pCurrentSelectionDopeSheet->ContentUpdated();

			if (m_pCurrentSelectionCurveEditor)
			{
				UpdateCurveEditor(m_pCurrentSelectionDopeSheet, m_pCurrentSelectionCurveEditor);
			}
		}
	}

	m_bDontUpdateProperties = false;
	GetIEditor()->GetUndoManager()->Accept("Changed sequence properties");
}

void CTrackViewMainWindow::UpdateSequenceList()
{
	m_pSequencesListWidget->GetListWidget()->clear();

	CTrackViewSequenceManager* pSequenceManager = CTrackViewPlugin::GetSequenceManager();
	const uint numSequences = pSequenceManager->GetCount();
	for (uint i = 0; i < numSequences; ++i)
	{
		CTrackViewSequence* pSequence = pSequenceManager->GetSequenceByIndex(i);

		const string filterString = QtUtil::ToString(m_pSequencesListWidget->GetFilterLineWidget()->text());
		if (!filterString.empty())
		{
			const bool bNameMatchesFilter = CryStringUtils::stristr(pSequence->GetName(), filterString) != nullptr;
			if (!bNameMatchesFilter)
			{
				continue;
			}
		}

		QListWidgetItem* pNewItem = new QListWidgetItem(pSequence->GetName());

		const CryGUID guid = pSequence->GetGUID();
		const QByteArray guidByteArray(reinterpret_cast<const char*>(&guid), sizeof(CryGUID));
		pNewItem->setData(Qt::UserRole, guidByteArray);
		m_pSequencesListWidget->GetListWidget()->addItem(pNewItem);
	}

	// Remove sequence data of removed sequences
	for (auto iter = m_idToSequenceDataMap.begin(); iter != m_idToSequenceDataMap.end(); )
	{
		if (!pSequenceManager->GetSequenceByGUID(iter->first))
		{
			iter = m_idToSequenceDataMap.erase(iter);
		}
		else
		{
			++iter;
		}
	}

	// Close tabs with sequences that don't exist anymore
	int numTabs = m_pDopesheetTabs->count();
	for (int i = 0; i < numTabs; )
	{
		const CryGUID guid = GetSequenceGUIDFromTabIndex(i);
		CTrackViewSequence* pSequence = CTrackViewPlugin::GetSequenceManager()->GetSequenceByGUID(guid);
		if (!pSequence)
		{
			OnDopeSheetTabClose(i);
			numTabs -= 1;
		}
		else
		{
			i += 1;
		}
	}
}

void CTrackViewMainWindow::UpdateTabNames()
{
	const int numTabs = m_pDopesheetTabs->count();
	for (int i = 0; i < numTabs; ++i)
	{
		const CryGUID guid = GetSequenceGUIDFromTabIndex(i);
		CTrackViewSequence* pSequence = CTrackViewPlugin::GetSequenceManager()->GetSequenceByGUID(guid);
		if (pSequence)
		{
			m_pDopesheetTabs->setTabText(i, pSequence->GetName());
		}
	}
}

// This creates a recursive menu by splitting track names on '/'
void CTrackViewMainWindow::FillAddTrackMenu(QMenu& addTrackMenu, const std::vector<SAddableTrack>& addableTracks) const
{
	std::map<std::vector<string>, QMenu*> pathToSubMenuMap;
	pathToSubMenuMap[std::vector < string > ()] = &addTrackMenu;

	const size_t numAddableTracks = addableTracks.size();
	for (size_t i = 0; i < numAddableTracks; ++i)
	{
		const SAddableTrack& type = addableTracks[i];

		std::vector<string> path;
		string segment;

		int currentPos = 0;
		for (segment = type.m_name.Tokenize("/", currentPos); !segment.empty(); segment = type.m_name.Tokenize("/", currentPos))
		{
			path.push_back(segment);
		}

		if (path.size() > 0)
		{
			QMenu* pMenu = &addTrackMenu;

			std::vector<string> subPath;
			auto secondLastElement = --path.end();

			for (auto iter = path.begin(); iter != secondLastElement; ++iter)
			{
				QMenu* pParentMenu = pathToSubMenuMap[subPath];

				subPath.push_back(*iter);
				auto findIter = pathToSubMenuMap.find(subPath);
				if (findIter == pathToSubMenuMap.end())
				{
					pMenu = pParentMenu->addMenu(QString(*iter));
					pathToSubMenuMap[subPath] = pMenu;
				}
				else
				{
					pMenu = findIter->second;
				}
			}

			QAction* pAction = pMenu->addAction(QString(path.back()), this, SLOT(OnAddTrack()));
			pAction->setData(i);
		}
	}
}

void CTrackViewMainWindow::OnAddNodeButtonClicked()
{
	CTrackViewSequence* pSequence = GetActiveSequence();
	m_pContextMenuNode = pSequence;

	if (pSequence)
	{
		std::unique_ptr<QMenu> pContextMenu(new QMenu);

		m_addableNodes = Private_TrackViewWindow::GetAddableNodes(*pSequence);
		const size_t numAddableNodes = m_addableNodes.size();
		for (size_t i = 0; i < numAddableNodes; ++i)
		{
			const SAddableNode& addableNode = m_addableNodes[i];
			QAction* pAction = pContextMenu->addAction(QString(addableNode.m_name), this, SLOT(OnAddNode()));
			pAction->setData(i);
		}

		pContextMenu->exec(QCursor::pos());
	}

	m_addableNodes.clear();
}

void CTrackViewMainWindow::OnAddTrack()
{
	QAction* pAction = static_cast<QAction*>(QObject::sender());
	size_t index = pAction->data().toULongLong();

	assert(m_addableTracks.size() > index);
	if (m_addableTracks.size() > index)
	{
		const SAddableTrack& track = m_addableTracks[index];
		if (m_pContextMenuNode && m_pContextMenuNode->GetNodeType() == eTVNT_AnimNode)
		{
			CTrackViewAnimNode* pAnimNode = static_cast<CTrackViewAnimNode*>(m_pContextMenuNode);

			string undoMessage;
			undoMessage.Format("Added track to node '%s'", pAnimNode->GetName());
			CUndo undo(undoMessage);
			pAnimNode->CreateTrack(track.m_type);
		}
	}
}

void CTrackViewMainWindow::OnAddNode()
{
	QAction* pAction = static_cast<QAction*>(QObject::sender());
	size_t index = pAction->data().toULongLong();

	assert(m_addableNodes.size() > index);
	if (m_addableNodes.size() > index)
	{
		const SAddableNode& node = m_addableNodes[index];
		if (m_pContextMenuNode && m_pContextMenuNode->IsGroupNode())
		{
			CTrackViewAnimNode* pAnimNode = static_cast<CTrackViewAnimNode*>(m_pContextMenuNode);

			string undoMessage;
			undoMessage.Format("Added '%s' to node '%s'", node.m_name, pAnimNode->GetName());
			CUndo undo(undoMessage);

			string availableName = pAnimNode->GetAvailableNodeNameStartingWith(node.m_name);
			pAnimNode->CreateSubNode(availableName, node.m_type);
		}
	}
}

void CTrackViewMainWindow::OnDeleteNodes()
{
	QAction* pAction = static_cast<QAction*>(QObject::sender());

	CTimeline* pDopeSheet = GetActiveDopeSheet();
	CTrackViewSequence* pSequence = GetSequenceFromDopeSheet(pDopeSheet);
	STimelineContent* pContent = pDopeSheet ? pDopeSheet->Content() : nullptr;

	string undoMessage;
	undoMessage.Format("Remove nodes from sequence '%s'", pSequence->GetName());
	CUndo undo(undoMessage);

	if (pContent)
	{
		TSelectedTracks selectedTracks;
		GetSelectedTracks(pContent->track, selectedTracks);
		const size_t numSelectedTracks = selectedTracks.size();

		std::vector<CryGUID> trackGUIDs;
		for (size_t i = 0; i < numSelectedTracks; ++i)
		{
			STimelineTrack* pDopeSheetTrack = selectedTracks[i];
			trackGUIDs.push_back(GetGUIDFromDopeSheetTrack(pDopeSheetTrack));
		}

		const size_t numGUIDs = trackGUIDs.size();
		for (size_t i = 0; i < numGUIDs; ++i)
		{
			CTrackViewNode* pTrackViewNode = pSequence->GetNodeFromGUID(trackGUIDs[i]);
			if (pTrackViewNode)
			{
				CTrackViewNode* pParentNode = pTrackViewNode->GetParentNode();
				if (pParentNode && pParentNode->GetNodeType() != eTVNT_Track)
				{
					CTrackViewAnimNode* pParentAnimNode = static_cast<CTrackViewAnimNode*>(pParentNode);

					if (pTrackViewNode->GetNodeType() == eTVNT_AnimNode)
					{
						pParentAnimNode->RemoveSubNode(static_cast<CTrackViewAnimNode*>(pTrackViewNode));
					}
					else if (pTrackViewNode->GetNodeType() == eTVNT_Track)
					{
						pParentAnimNode->RemoveTrack(static_cast<CTrackViewTrack*>(pTrackViewNode));
					}
				}
			}
		}
	}
}

void CTrackViewMainWindow::OnSelectEntity()
{
	if (m_pContextMenuNode && m_pContextMenuNode->GetNodeType() == eTVNT_AnimNode)
	{
		CTrackViewAnimNode* pAnimNode = static_cast<CTrackViewAnimNode*>(m_pContextMenuNode);
		CEntityObject* pEntity = pAnimNode->GetNodeEntity();
		if (pEntity)
		{
			IObjectManager* pObjectManager = GetIEditor()->GetObjectManager();
			pObjectManager->ClearSelection();
			pObjectManager->SelectObject(pEntity);
			CViewport* pViewport = GetIEditor()->GetActiveView();
			if (pViewport)
			{
				pViewport->CenterOnSelection();
			}
		}
	}
}

void CTrackViewMainWindow::OnPlayPause(bool bState)
{
	CTrackViewPlugin::GetAnimationContext()->SetPlaying(bState);
}

void CTrackViewMainWindow::OnStop()
{
	CAnimationContext* pAnimationContext = CTrackViewPlugin::GetAnimationContext();
	pAnimationContext->SetPlaying(false);
	pAnimationContext->SetTime(SAnimTime(0));
}

void CTrackViewMainWindow::OnGoToStart()
{
	CAnimationContext* pAnimationContext = CTrackViewPlugin::GetAnimationContext();
	const TRange<SAnimTime> sequenceTime = pAnimationContext->GetTimeRange();
	pAnimationContext->SetTime(sequenceTime.start);
}

void CTrackViewMainWindow::OnGoToEnd()
{
	CAnimationContext* pAnimationContext = CTrackViewPlugin::GetAnimationContext();
	const TRange<SAnimTime> sequenceTime = pAnimationContext->GetTimeRange();
	pAnimationContext->SetTime(sequenceTime.end);
}

void CTrackViewMainWindow::OnRecord(bool bState)
{
	CTrackViewPlugin::GetAnimationContext()->SetRecording(bState);
}

void CTrackViewMainWindow::OnNoSnapping(bool bState)
{
	if (m_bSnapModeCanUpdate)
	{
		OnSnapModeChanged(eSnapMode_NoSnapping, bState);
	}
}

void CTrackViewMainWindow::OnSnapTime(bool bState)
{
	if (m_bSnapModeCanUpdate)
	{
		OnSnapModeChanged(eSnapMode_TimeSnapping, bState);
	}
}

void CTrackViewMainWindow::OnSnapKey(bool bState)
{
	if (m_bSnapModeCanUpdate)
	{
		OnSnapModeChanged(eSnapMode_KeySnapping, bState);
	}
}

void CTrackViewMainWindow::OnSnapModeChanged(eSnapMode newSnapMode, bool bEnabled)
{
	m_bSnapModeCanUpdate = false;

	QAction* pButtonNoSnap = m_pKeysToolbar->GetButtonNoSnapping();
	QAction* pButtonKeySnap = m_pKeysToolbar->GetButtonKeySnapping();
	QAction* pButtonTimeSnap = m_pKeysToolbar->GetButtonTimeSnapping();

	if (!bEnabled)
	{
		pButtonNoSnap->setChecked(true);
		pButtonKeySnap->setChecked(false);
		pButtonTimeSnap->setChecked(false);
		m_SnapMode = eSnapMode_NoSnapping;
	}
	else
	{
		m_SnapMode = newSnapMode;

		pButtonNoSnap->setChecked(m_SnapMode == eSnapMode_NoSnapping);
		pButtonKeySnap->setChecked(m_SnapMode == eSnapMode_KeySnapping);
		pButtonTimeSnap->setChecked(m_SnapMode == eSnapMode_TimeSnapping);
	}

	for (auto iter = m_idToSequenceDataMap.begin(); iter != m_idToSequenceDataMap.end(); ++iter)
	{
		CTimeline* pDopeSheet = iter->second.m_pDopeSheet;
		if (pDopeSheet)
		{
			pDopeSheet->SetKeySnapping(m_SnapMode == eSnapMode_KeySnapping);
			pDopeSheet->SetTimeSnapping(m_SnapMode == eSnapMode_TimeSnapping);
		}

		CCurveEditor* pCurveEditor = iter->second.m_pCurveEditor;
		if (pCurveEditor)
		{
			pCurveEditor->SetKeySnapping(m_SnapMode == eSnapMode_KeySnapping);
			pCurveEditor->SetTimeSnapping(m_SnapMode == eSnapMode_TimeSnapping);
		}
	}

	m_bSnapModeCanUpdate = true;
}

void CTrackViewMainWindow::OnSyncSelectedTracksToBase()
{
	CTrackViewSequence* pSequence = GetSequenceFromDopeSheet(GetActiveDopeSheet());
	if (pSequence)
	{
		pSequence->SyncSelectedTracksToBase();
	}
}

void CTrackViewMainWindow::OnSyncSelectedTracksFromBase()
{
	CTrackViewSequence* pSequence = GetSequenceFromDopeSheet(GetActiveDopeSheet());
	if (pSequence)
	{
		pSequence->SyncSelectedTracksFromBase();
	}
}

void CTrackViewMainWindow::DeleteSelectedSequences()
{
	if (m_pSequencesListWidget->hasFocus())
	{
		QList<QListWidgetItem*> selectedItems = m_pSequencesListWidget->GetListWidget()->selectedItems();

		if (selectedItems.size() > 0)
		{
			CUndo undo("Delete sequences");

			for (QListWidgetItem* pItem : selectedItems)
			{
				const QByteArray guidByteArray = pItem->data(Qt::UserRole).toByteArray();
				const CryGUID guid = *reinterpret_cast<const CryGUID*>(guidByteArray.data());
				CTrackViewSequence* pSequence = CTrackViewPlugin::GetSequenceManager()->GetSequenceByGUID(guid);
				CTrackViewPlugin::GetSequenceManager()->DeleteSequence(pSequence);
			}
		}
	}
}

void CTrackViewMainWindow::OnAddSelectedEntities()
{
	CTrackViewSequence* pSequence = GetSequenceFromDopeSheet(GetActiveDopeSheet());
	CTrackViewSequenceNotificationContext context(pSequence);

	CUndo undo("Add selected entities to sequence");
	pSequence->AddSelectedEntities();
}

void CTrackViewMainWindow::OnAddDirectorNode()
{
	CTrackViewSequence* pSequence = GetSequenceFromDopeSheet(GetActiveDopeSheet());
	CTrackViewSequenceNotificationContext context(pSequence);

	if (pSequence)
	{
		CUndo undo("Create TrackView Director Node");
		const string name = pSequence->GetAvailableNodeNameStartingWith("Director");
		pSequence->CreateSubNode(name, eAnimNodeType_Director);
	}
}

void CTrackViewMainWindow::OnGoToPreviousKey()
{
	CAnimationContext* pAnimationContext = CTrackViewPlugin::GetAnimationContext();
	CTrackViewSequence* pSequence = pAnimationContext->GetSequence();

	if (pSequence)
	{
		SAnimTime time = pAnimationContext->GetTime();
		CTrackViewNode* pNode = pSequence->GetFirstSelectedNode();
		pNode = pNode ? pNode : pSequence;

		if (pNode->SnapTimeToPrevKey(time))
		{
			pAnimationContext->SetTime(time);
		}
	}
}

void CTrackViewMainWindow::OnGoToNextKey()
{
	CAnimationContext* pAnimationContext = CTrackViewPlugin::GetAnimationContext();
	CTrackViewSequence* pSequence = pAnimationContext->GetSequence();

	if (pSequence)
	{
		SAnimTime time = pAnimationContext->GetTime();
		CTrackViewNode* pNode = pSequence->GetFirstSelectedNode();
		pNode = pNode ? pNode : pSequence;

		if (pNode->SnapTimeToNextKey(time))
		{
			pAnimationContext->SetTime(time);
		}
	}
}

void CTrackViewMainWindow::OnNewSequence()
{
	bool bOk;
	const string name = QtUtil::ToString(QInputDialog::getText(this, "New sequence", "Sequence name:", QLineEdit::Normal, "", &bOk));

	if (bOk && !name.empty())
	{
		CUndo undo("Create TrackView sequence");
		CTrackViewPlugin::GetSequenceManager()->CreateSequence(name);
	}
}

void CTrackViewMainWindow::OnSequencesFilterChanged()
{
	UpdateSequenceList();
}

void CTrackViewMainWindow::RemoveDeletedTracks(STimelineTrack* pTrack)
{
	for (auto iter = pTrack->tracks.begin(); iter != pTrack->tracks.end(); )
	{
		if ((*iter)->deleted)
		{
			iter = pTrack->tracks.erase(iter);
		}
		else
		{
			RemoveDeletedTracks(*iter);
			++iter;
		}
	}
}

CTrackViewAnimNode* CTrackViewMainWindow::CheckValidReparenting(CTimeline* pDopeSheet, const TSelectedTracks& selectedTracks, STimelineTrack* pTarget)
{
	CTrackViewSequence* pSequence = GetSequenceFromDopeSheet(pDopeSheet);

	CTrackViewNode* pTargetNode = GetNodeFromDopeSheetTrack(pSequence, pTarget);
	if (!pTargetNode)
	{
		return nullptr;
	}

	const ETrackViewNodeType targetNodeType = pTargetNode->GetNodeType();

	if (pTargetNode && (targetNodeType == eTVNT_Sequence || targetNodeType == eTVNT_AnimNode))
	{
		CTrackViewAnimNode* pTargetAnimNode = static_cast<CTrackViewAnimNode*>(pTargetNode);

		for (STimelineTrack* pTrack : selectedTracks)
		{
			CTrackViewNode* pSourceNode = GetNodeFromDopeSheetTrack(pSequence, pTrack);
			if (pSourceNode && (pSourceNode->GetNodeType() == eTVNT_AnimNode))
			{
				CTrackViewAnimNode* pSourceAnimNode = static_cast<CTrackViewAnimNode*>(pSourceNode);
				if (!pSourceAnimNode->IsValidReparentingTo(pTargetAnimNode))
				{
					return nullptr;
				}
			}
		}

		return pTargetAnimNode;
	}

	return nullptr;
}

CryGUID CTrackViewMainWindow::GetSequenceGUIDFromTabIndex(int index)
{
	return stl::find_in_map(m_tabToSequenceUIdMap, m_pDopesheetTabs->widget(index), CryGUID::Null());
}

CryGUID CTrackViewMainWindow::GetActiveSequenceGUID()
{
	return stl::find_in_map(m_tabToSequenceUIdMap, m_pDopesheetTabs->widget(m_pDopesheetTabs->currentIndex()), CryGUID::Null());
}

CTrackViewMainWindow::SSequenceData* CTrackViewMainWindow::GetActiveSequenceData()
{
	const CryGUID guid = GetActiveSequenceGUID();
	if (guid != CryGUID::Null())
	{
		return &m_idToSequenceDataMap[guid];
	}

	return nullptr;
}

CTrackViewSequence* CTrackViewMainWindow::GetActiveSequence()
{
	const CryGUID guid = GetActiveSequenceGUID();
	return CTrackViewPlugin::GetSequenceManager()->GetSequenceByGUID(guid);
}

CTimeline* CTrackViewMainWindow::GetActiveDopeSheet()
{
	const CryGUID guid = GetSequenceGUIDFromTabIndex(m_pDopesheetTabs->currentIndex());
	return GetDopeSheetFromSequenceGUID(guid);
}

CTimeline* CTrackViewMainWindow::GetDopeSheetFromSequenceGUID(const CryGUID& guid)
{
	auto findIter = m_idToSequenceDataMap.find(guid);
	if (findIter != m_idToSequenceDataMap.end())
	{
		const SSequenceData& sequenceData = findIter->second;
		return sequenceData.m_pDopeSheet;
	}

	return nullptr;
}

CCurveEditor* CTrackViewMainWindow::GetActiveCurveEditor()
{
	const CryGUID guid = GetSequenceGUIDFromTabIndex(m_pDopesheetTabs->currentIndex());
	return GetCurveEditorFromSequenceGUID(guid);
}

CCurveEditor* CTrackViewMainWindow::GetCurveEditorFromSequenceGUID(const CryGUID& guid)
{
	auto findIter = m_idToSequenceDataMap.find(guid);
	if (findIter != m_idToSequenceDataMap.end())
	{
		const SSequenceData& sequenceData = findIter->second;
		return sequenceData.m_pCurveEditor;
	}

	return nullptr;
}

CTrackViewSequence* CTrackViewMainWindow::GetSequenceFromDopeSheet(const CTimeline* pDopeSheet)
{
	CryGUID guid;
	memcpy(&guid, &pDopeSheet->Content()->userSideLoad[0], sizeof(CryGUID));
	return CTrackViewPlugin::GetSequenceManager()->GetSequenceByGUID(guid);
}

CTrackViewNode* CTrackViewMainWindow::GetNodeFromDopeSheetTrack(const CTrackViewSequence* pSequence, const STimelineTrack* pDopeSheetTrack)
{
	return pSequence->GetNodeFromGUID(GetGUIDFromDopeSheetTrack(pDopeSheetTrack));
}

STimelineTrack* CTrackViewMainWindow::GetDopeSheetTrackFromNode(const CTrackViewNode* pNode)
{
	if (pNode)
	{
		const auto findIter = m_idToSequenceDataMap.find(pNode->GetSequence()->GetGUID());
		if (findIter != m_idToSequenceDataMap.end())
		{
			const SSequenceData& sequenceData = findIter->second;
			return stl::find_in_map(sequenceData.m_uIdToTimelineTrackMap, pNode->GetGUID(), nullptr);
		}
	}

	return nullptr;
}

CryGUID CTrackViewMainWindow::GetGUIDFromDopeSheetTrack(const STimelineTrack* pDopeSheetTrack) const
{
	Private_TrackViewWindow::STrackUserData userData;
	if (pDopeSheetTrack->userSideLoad.size() > 0 && Serialization::LoadBinaryBuffer(userData, &pDopeSheetTrack->userSideLoad[0], pDopeSheetTrack->userSideLoad.size()))
	{
		return userData.m_guid;
	}

	return CryGUID::Null();
}

void CTrackViewMainWindow::UpdateActiveSequence()
{
	CTimeline* pActiveDopeSheet = GetActiveDopeSheet();
	bool bDopeSheetOpen = pActiveDopeSheet != NULL;

	SSequenceData* pSequenceData = GetActiveSequenceData();
	if (pSequenceData)
	{
		//m_pActionPlay->setChecked(pSequenceData->m_bPlaying);
	}
	else
	{
		//m_pActionPlay->setChecked(false);
		//m_pActionRecord->setChecked(false);
	}

	CTrackViewSequence* pSequence = GetActiveSequence();
	CTrackViewPlugin::GetAnimationContext()->SetSequence(pSequence, false, false);

	//m_pActionPlay->setEnabled(bDopeSheetOpen);
	//m_pActionStop->setEnabled(bDopeSheetOpen);
}

void CTrackViewMainWindow::OnSequenceChanged(CTrackViewSequence* pSequence)
{
	m_activeSequenceUId = pSequence ? pSequence->GetGUID() : CryGUID::Null();

	UpdateSequenceList();
}

void CTrackViewMainWindow::OnTimeChanged(SAnimTime newTime)
{
	CTrackViewSequence* pSequence = CTrackViewPlugin::GetAnimationContext()->GetSequence();
	if (pSequence)
	{
		const CryGUID sequenceGUID = pSequence->GetGUID();
		const SAnimTime sequenceTime = pSequence->GetTime();

		CTimeline* pDopeSheet = GetDopeSheetFromSequenceGUID(sequenceGUID);
		if (pDopeSheet)
		{
			pDopeSheet->SetTime(sequenceTime);
		}

		CCurveEditor* pCurveEditor = GetCurveEditorFromSequenceGUID(sequenceGUID);
		if (pCurveEditor)
		{
			pCurveEditor->SetTime(sequenceTime);
		}

		m_pFrameSettingsToolbar->UpdateTime(newTime);
	}
}

void CTrackViewMainWindow::OnPlaybackStateChanged(bool bPlaying)
{
	m_pPlaybackToolbar->GetButtonPlay()->setChecked(bPlaying);
}

void CTrackViewMainWindow::OnRecordingStateChanged(bool bRecording)
{
	m_pPlaybackToolbar->GetButtonRecord()->setChecked(bRecording);
}

void CTrackViewMainWindow::OnSequenceAdded(CTrackViewSequence* pSequence)
{
	pSequence->AddListener(this);
	AddNodeToDopeSheet(pSequence);
	UpdateSequenceList();
}

void CTrackViewMainWindow::OnSequenceRemoved(CTrackViewSequence* pSequence)
{
	pSequence->RemoveListener(this);
	UpdateSequenceList();
	if (m_pCurrentSelectedNode = pSequence)
	{
		m_pCurrentSelectedNode = nullptr;
	}
	UpdateProperties();
}

void CTrackViewMainWindow::OnSequenceSettingsChanged(CTrackViewSequence* pSequence)
{
	TRange<SAnimTime> newTimeRange = pSequence->GetTimeRange();

	SSequenceData& sequenceData = m_idToSequenceDataMap[pSequence->GetGUID()];
	sequenceData.m_startTime = newTimeRange.start;
	sequenceData.m_endTime = newTimeRange.end;

	Private_TrackViewWindow::ForEachTrack(sequenceData.m_timelineContent.track, [=](STimelineTrack& track)
	{
		track.startTime = newTimeRange.start;
		track.endTime = newTimeRange.end;
	});

	CTimeline* pDopeSheet = GetDopeSheetFromSequenceGUID(pSequence->GetGUID());
	if (pDopeSheet)
	{
		pDopeSheet->ContentUpdated();
	}

	UpdateProperties();
}

void CTrackViewMainWindow::OnNodeChanged(CTrackViewNode* pNode, ENodeChangeType type)
{
	switch (type)
	{
	case ITrackViewSequenceListener::eNodeChangeType_Added:
		AddNodeToDopeSheet(pNode);
		break;
	case ITrackViewSequenceListener::eNodeChangeType_Removed:
		{
			if (m_pCurrentSelectedNode == pNode)
			{
				m_pCurrentSelectedNode = nullptr;
			}
			RemoveNodeFromDopeSheet(pNode);
		}
		break;
	case ITrackViewSequenceListener::eNodeChangeType_KeysChanged:
	// Fall through
	case ITrackViewSequenceListener::eNodeChangeType_KeySelectionChanged:
		UpdateDopeSheetTrack(static_cast<CTrackViewTrack*>(pNode));
		break;
	}

	const CryGUID sequenceGUID = pNode->GetSequence()->GetGUID();
	CTimeline* pDopeSheet = GetDopeSheetFromSequenceGUID(sequenceGUID);
	if (pDopeSheet)
	{
		pDopeSheet->ContentUpdated();

		CCurveEditor* pCurveEditor = GetCurveEditorFromSequenceGUID(sequenceGUID);
		if (pCurveEditor)
		{
			UpdateCurveEditor(pDopeSheet, pCurveEditor);
		}
	}

	if (type == ITrackViewSequenceListener::eNodeChangeType_KeySelectionChanged)
	{
		ShowKeyProperties();
	}
	else
	{
		UpdateProperties();
	}
}

void CTrackViewMainWindow::OnNodeRenamed(CTrackViewNode* pNode, const char* pOldName)
{
	if (pNode->GetNodeType() == eTVNT_Sequence)
	{
		UpdateSequenceList();
		UpdateTabNames();
	}
	else
	{
		RemoveNodeFromDopeSheet(pNode);
		AddNodeToDopeSheet(pNode);

		CTimeline* pDopeSheet = GetDopeSheetFromSequenceGUID(pNode->GetSequence()->GetGUID());
		if (pDopeSheet)
		{
			pDopeSheet->ContentUpdated();
		}
	}

	UpdateProperties();
}

_smart_ptr<IAnimKeyWrapper> CTrackViewMainWindow::GetWrappedKeyFromElement(const STimelineTrack& track, const STimelineElement& element)
{
	Private_TrackViewWindow::STrackUserData userData;
	if (track.userSideLoad.size() > 0 && Serialization::LoadBinaryBuffer(userData, &track.userSideLoad[0], track.userSideLoad.size()))
	{
		_smart_ptr<IAnimKeyWrapper> pKeyWrapper = CIntrusiveFactory<IAnimKeyWrapper>::Instance().Create(userData.m_keyType);
		if (element.userSideLoad.size() > 0 && Serialization::LoadBinaryBuffer(*pKeyWrapper, &element.userSideLoad[0], element.userSideLoad.size()))
		{
			return pKeyWrapper;
		}
	}

	return nullptr;
}

