// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SequenceTabWidget.h"

#include <QMenu>
#include <QAction>
#include <QToolbar>
#include <QSplitter>
#include <QVBoxLayout>
#include <QToolButton>
#include <QInputDialog>
#include <QTimer>

#include "TrackViewPlugin.h"
#include "TrackViewUndo.h"
#include "IUndoManager.h"
#include "IObjectManager.h"
#include "Objects/EntityObject.h"
#include "Viewport.h"

#include "TrackViewIcons.h"
#include "TrackViewExporter.h"

#include <QMimeData>
#include <QClipboard>
#include <QApplication>
#include <CryIcon.h>

#include "Util/Clipboard.h"
#include "Objects/SelectionGroup.h"

#include "Nodes/TrackViewEntityNode.h"

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
			// Entities are added by "Add selected entities"
			continue;
		}

		if (type == eAnimNodeType_Light)
		{
			const CTrackViewSequence* pSequence = animNode.GetSequence();
			if (!(pSequence->GetFlags() & IAnimSequence::eSeqFlags_LightAnimationSet))
			{
				// Only add Light Animation Nodes to the predefined LightAnimationSet sequence
				continue;
			}
			else
			{
				// if this is a predefined LightAnimationSet sequence, make sure we can *only* add Light Animation Nodes
				addableNode.m_type = type;
				addableNode.m_name = gEnv->pMovieSystem->GetDefaultNodeName(type);

				addableNodes.clear();
				addableNodes.push_back(addableNode);

				break;
			}
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

void ForEachTrack(STimelineTrack& track, std::function<void(STimelineTrack& track)> fun)
{
	fun(track);

	for (size_t i = 0; i < track.tracks.size(); ++i)
	{
		STimelineTrack& subTrack = *track.tracks[i];
		ForEachTrack(subTrack, fun);
	}
}

void ForEachTrack(STimelineTrack& track, std::vector<STimelineTrack*>& parents, std::function<void(STimelineTrack& track)> fun)
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

void ForEachNode(CTrackViewNode& node, std::function<void(CTrackViewNode& node)> fun)
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

namespace
{
typedef std::vector<STimelineTrack*>                               TSelectedTracks;
typedef std::unordered_map<STimelineTrack*, bool>                  TAllTracksWithSelection;
typedef std::vector<std::pair<STimelineTrack*, STimelineElement*>> TSelectedElements;

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

void GetAllTracksWithSelection(STimelineTrack& track, TAllTracksWithSelection& tracks)
{
	tracks[&track] = track.selected;

	for (auto iter = track.tracks.begin(); iter != track.tracks.end(); ++iter)
	{
		GetAllTracksWithSelection(**iter, tracks);
	}
}

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

CTrackViewSequenceTabWidget::CTrackViewSequenceTabWidget(CTrackViewCore* pTrackViewCore)
	: CTrackViewCoreComponent(pTrackViewCore, true)
	, m_pCurrentSelectionDopeSheet(nullptr)
	, m_pCurrentSelectionCurveEditor(nullptr)
	, m_pCurrentSelectedNode(nullptr)
	, m_bDontUpdateDopeSheet(false)
	, m_pToolbar(nullptr)
	, m_bDontUpdateCurveEditor(false)
	, m_showCurveEditor(true)
	, m_showDopeSheet(true)
	, m_timelineLink(true)
	, m_showKeyText(true)
	, m_timeUnit(SAnimTime::EDisplayMode::Time)
{
	GetIEditor()->RegisterNotifyListener(this);

	m_pDopesheetTabs = new QTabWidget();
	m_pDopesheetTabs->setTabsClosable(true);
	// TODO: As soon we have a better way to display sequence properties we don't need to handle tabBarClicked signal anymore.
	connect(m_pDopesheetTabs, SIGNAL(tabBarClicked(int)), this, SLOT(OnDopeSheetTabChanged(int)));
	connect(m_pDopesheetTabs, SIGNAL(currentChanged(int)), this, SLOT(OnDopeSheetTabChanged(int)));
	connect(m_pDopesheetTabs, SIGNAL(tabCloseRequested(int)), this, SLOT(OnDopeSheetTabClose(int)));

	QVBoxLayout* pLayout = new QVBoxLayout(this);
	pLayout->setSpacing(0);
	pLayout->setMargin(0);
	pLayout->addWidget(m_pDopesheetTabs);
	setLayout(pLayout);

	// This is a quick fix solution to avoid critical fps drop when TV is playing even an empty sequence in an empty level.
	m_refreshTimer = new QTimer(this);
	m_refreshTimer->setSingleShot(true);
	connect(m_refreshTimer, &QTimer::timeout, [this]() 
	{
		CTrackViewSequence* pSequence = CTrackViewPlugin::GetAnimationContext()->GetSequence();
		if (pSequence)
		{
			SetSequenceTime(pSequence->GetGUID(), m_time);
		}
	});
}

CTrackViewSequenceTabWidget::~CTrackViewSequenceTabWidget()
{
	GetIEditor()->UnregisterNotifyListener(this);
}

void CTrackViewSequenceTabWidget::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	switch (event)
	{
	case eNotify_OnBeginLoad:
		ClearData();
		break;
	case eNotify_OnEndLoad:
		UpdateSequenceTabs();
		break;
	default:
		break;
	}
}

void CTrackViewSequenceTabWidget::OnDopeSheetTabChanged(int index)
{
	UpdateActiveSequence();

	auto dopeSheet = GetCurrentDopeSheetContainer();
	if (dopeSheet)
		dopeSheet->setVisible(m_showDopeSheet);

	auto curveEditor = GetCurrentCurveEditorContainer();
	if (curveEditor)
		curveEditor->setVisible(m_showCurveEditor);
}

void CTrackViewSequenceTabWidget::OnDopeSheetTabClose(int index)
{
	CryGUID guid = GetSequenceGUIDFromTabIndex(index);

	if (CTrackViewSequence* pSequence = GetTrackViewCore()->GetSequenceByGUID(guid))
	{
		const uint subNodeCount = pSequence->GetChildCount();
		for (uint i = 0; i < subNodeCount; ++i)
		{
			CTrackViewNode* pNode = pSequence->GetChild(i);
			if (pNode)
			{
				RemoveNodeFromDopeSheet(pNode);
			}
		}
	}

	SSequenceData& sequenceData = m_idToSequenceDataMap[guid];
	sequenceData.m_uIdToTimelineTrackMap.erase(guid);
	sequenceData.m_pDopeSheet = nullptr;
	sequenceData.m_pCurveEditor = nullptr;

	m_tabToSequenceUIdMap.erase(m_pDopesheetTabs->widget(index));
	m_pDopesheetTabs->removeTab(index);
	assert(m_tabToSequenceUIdMap.size() == m_pDopesheetTabs->count());

	UpdateActiveSequence();
}

CryGUID CTrackViewSequenceTabWidget::GetSequenceGUIDFromTabIndex(int index) const
{
	return stl::find_in_map(m_tabToSequenceUIdMap, m_pDopesheetTabs->widget(index), CryGUID::Null());
}

CryGUID CTrackViewSequenceTabWidget::GetActiveSequenceGUID()
{
	return stl::find_in_map(m_tabToSequenceUIdMap, m_pDopesheetTabs->widget(m_pDopesheetTabs->currentIndex()), CryGUID::Null());
}

CTrackViewSequenceTabWidget::SSequenceData* CTrackViewSequenceTabWidget::GetActiveSequenceData()
{
	const CryGUID guid = GetActiveSequenceGUID();
	if (guid != CryGUID::Null())
	{
		return &m_idToSequenceDataMap[guid];
	}

	return nullptr;
}

CTrackViewSequence* CTrackViewSequenceTabWidget::GetActiveSequence()
{
	const CryGUID guid = GetActiveSequenceGUID();
	return CTrackViewPlugin::GetSequenceManager()->GetSequenceByGUID(guid);
}

void CTrackViewSequenceTabWidget::ApplyChangedProperties(bool bUpdateProperties)
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

	if (bUpdateProperties)
	{
		GetTrackViewCore()->UpdateProperties(ePropertyDataSource_Timeline);
	}

	CTrackViewSequence* pCurrentSequence = GetActiveSequence();

	if (pCurrentSequence)
	{
		// Must be called to update ranges, time markers and several listeners
		pCurrentSequence->SetPlaybackRange(pCurrentSequence->GetPlaybackRange());
	}
}

void CTrackViewSequenceTabWidget::TimelineLayoutChanged(bool bUpdateProperties)
{
	const size_t numSelectedElements = m_currentKeySelection.size();
	if (numSelectedElements > 0)
	{
		for (size_t i = 0; i < numSelectedElements; ++i)
		{
			SSelectedKey selectedKey = m_currentKeySelection[i];
			selectedKey.m_key->SetTime(selectedKey.m_pElement->start);

			Serialization::SaveBinaryBuffer(selectedKey.m_pElement->userSideLoad, *selectedKey.m_key);
			selectedKey.m_pTrack->modified = true;
		}
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

	if (bUpdateProperties)
	{
		GetTrackViewCore()->UpdateProperties(ePropertyDataSource_Timeline);
	}

	CTrackViewSequence* pCurrentSequence = GetActiveSequence();

	if (pCurrentSequence)
	{
		// Must be called to update ranges, time markers and several listeners
		pCurrentSequence->SetPlaybackRange(pCurrentSequence->GetPlaybackRange());
	}
}

void CTrackViewSequenceTabWidget::SetToolbar(QToolBar* pToolbar)
{
	m_pToolbar = pToolbar;
}

CTrackViewNode* CTrackViewSequenceTabWidget::GetNodeFromActiveSequence(const STimelineTrack* pDopeSheetTrack)
{
	return GetNodeFromDopeSheetTrack(GetActiveSequence(), pDopeSheetTrack);
}

void CTrackViewSequenceTabWidget::OpenSequenceTab(const CryGUID sequenceGUID)
{
	CTrackViewSequence* pSequence = GetTrackViewCore()->GetSequenceByGUID(sequenceGUID);
	assert(pSequence != nullptr);

	// set time marker to sequence start time
	if (pSequence)
	{
		pSequence->SetTimeRange(pSequence->GetTimeRange());
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

	AddNodeToDopeSheet(pSequence);
	sequenceData.m_timelineContent.userSideLoad.resize(sizeof(CryGUID));
	memcpy(&sequenceData.m_timelineContent.userSideLoad[0], &sequenceGUID, sizeof(CryGUID));

	sequenceData.m_timelineContent.track.endTime = sequenceData.m_endTime;
	sequenceData.m_timelineContent.track.startTime = sequenceData.m_startTime;

	QSplitter* pSplitter = new QSplitter(Qt::Vertical, m_pDopesheetTabs);

	QFrame* pTimelineFrame = new QFrame(m_pDopesheetTabs);
	pTimelineFrame->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
	QVBoxLayout* pTimelineLayout = new QVBoxLayout(pTimelineFrame);
	pTimelineLayout->setMargin(0);
	pTimelineLayout->setSpacing(0);
	pTimelineFrame->setLayout(pTimelineLayout);

	CTimeline* pDopeSheet = new CTimeline(pTimelineFrame);
	pDopeSheet->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
	pDopeSheet->SetUseMainTrackTimeRange(true);
	pDopeSheet->SetTimeUnit(m_timeUnit);
	pDopeSheet->SetAllowOutOfRangeKeys(true);
	pDopeSheet->SetUseInternalRuler(true);
	pDopeSheet->SetCycled(false);
	pDopeSheet->SetSizeToContent(false);
	pDopeSheet->SetKeySize(14);
	pDopeSheet->SetTreeVisible(true);
	pDopeSheet->SetDrawSelectionIndicators(true);
	pDopeSheet->SetVerticalScrollbarVisible(true);
	pDopeSheet->SetDrawTrackTimeMarkers(true);
	pDopeSheet->SetVisibleDistance((sequenceData.m_endTime - sequenceData.m_startTime).ToFloat());
	pDopeSheet->SetKeySnapping(GetTrackViewCore()->GetCurrentSnapMode() == eSnapMode_KeySnapping);
	pDopeSheet->SetTimeSnapping(GetTrackViewCore()->GetCurrentSnapMode() == eSnapMode_TimeSnapping);
	pDopeSheet->SetKeyScaling(GetTrackViewCore()->GetCurrentKeyMoveMode() == eKeyMoveMode_Scale);
	pDopeSheet->SetTime(pSequence->GetTime());
	pDopeSheet->SetContent(&sequenceData.m_timelineContent);

	QObject::connect(pDopeSheet, &CTimeline::SignalSelectionChanged, this, &CTrackViewSequenceTabWidget::OnSelectionChanged);
	QObject::connect(pDopeSheet, &CTimeline::SignalSelectionRefresh, this, &CTrackViewSequenceTabWidget::OnSelectionRefresh);

	QIcon addNodeIcon = CryIcon("icons:General/Element_Add.ico");
	QToolButton* pAddNodeButton = new QToolButton(pDopeSheet);
	pAddNodeButton->setIcon(addNodeIcon);
	pDopeSheet->SetCustomTreeCornerWidget(pAddNodeButton, 20);
	connect(pAddNodeButton, SIGNAL(clicked()), this, SLOT(OnAddNodeButtonClicked()));

	pTimelineLayout->addWidget(pDopeSheet);

	connect(pDopeSheet, SIGNAL(SignalPlay()), this, SLOT(OnDopesheetPlayPause()));
	connect(pDopeSheet, SIGNAL(SignalZoom()), this, SLOT(OnDopesheetZoom()));
	connect(pDopeSheet, SIGNAL(SignalPan()), this, SLOT(OnDopesheetPan()));
	connect(pDopeSheet, SIGNAL(SignalScrub(bool)), this, SLOT(OnDopesheetScrub()));
	connect(pDopeSheet, SIGNAL(SignalContentChanged(bool)), this, SLOT(OnDopesheetContentChanged(bool)));
	connect(pDopeSheet, SIGNAL(SignalSelectionChanged(bool)), this, SLOT(OnDopesheetContentChanged(bool)));
	connect(pDopeSheet, SIGNAL(SignalTrackSelectionChanged()), this, SLOT(OnDopesheetTrackSelectionChanged()));
	connect(pDopeSheet, SIGNAL(SignalTreeContextMenu(const QPoint &)), this, SLOT(OnDopesheetTreeContextMenu(const QPoint &)));
	connect(pDopeSheet, SIGNAL(SignalTracksBeginDrag(STimelineTrack*, bool&)), this, SLOT(OnDopesheetTracksBeginDrag(STimelineTrack*, bool&)));
	connect(pDopeSheet, SIGNAL(SignalTracksDragging(STimelineTrack*, bool&)), this, SLOT(OnDopesheetTracksDragging(STimelineTrack*, bool&)));
	connect(pDopeSheet, SIGNAL(SignalTracksDragged(STimelineTrack*)), this, SLOT(OnDopesheetTracksDragged(STimelineTrack*)));
	connect(pDopeSheet, SIGNAL(SignalCopy(SAnimTime, STimelineTrack*)), this, SLOT(OnDopesheetCopy()));
	connect(pDopeSheet, SIGNAL(SignalPaste(SAnimTime, STimelineTrack*)), this, SLOT(OnDopesheetPaste(SAnimTime, STimelineTrack*)));

	QFrame* pCurveEditorFrame = new QFrame(m_pDopesheetTabs);
	pCurveEditorFrame->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
	QVBoxLayout* pCurveEditorLayout = new QVBoxLayout(pCurveEditorFrame);
	pCurveEditorLayout->setMargin(0);
	pCurveEditorLayout->setSpacing(0);
	pCurveEditorFrame->setLayout(pCurveEditorLayout);

	CCurveEditor* pCurveEditor = new CCurveEditor(pCurveEditorFrame);
	pCurveEditor->SetContent(&sequenceData.m_curveEditorContent);
	pCurveEditor->SetTimeRange(sequenceData.m_startTime, sequenceData.m_endTime);
	pCurveEditor->SetTime(pSequence->GetTime());
	pCurveEditor->SetGridVisible(true);
	pCurveEditor->SetKeySnapping(GetTrackViewCore()->GetCurrentSnapMode() == eSnapMode_KeySnapping);
	pCurveEditor->SetTimeSnapping(GetTrackViewCore()->GetCurrentSnapMode() == eSnapMode_TimeSnapping);

	pCurveEditorLayout->addWidget(pCurveEditor);

	connect(pCurveEditor, SIGNAL(SignalContentChanged()), this, SLOT(OnCurveEditorContentChanged()));
	connect(pCurveEditor, SIGNAL(SignalScrub()), this, SLOT(OnCurveEditorScrub()));
	connect(pCurveEditor, SIGNAL(SignalZoom()), this, SLOT(OnCurveEditorZoom()));
	connect(pCurveEditor, SIGNAL(SignalPan()), this, SLOT(OnCurveEditorPan()));

	connect(pCurveEditor, &CCurveEditor::SignalSelectionChanged, this, &CTrackViewSequenceTabWidget::OnCurveEditorSelectionChanged);

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

	// Content must be set after we added the tab otherwise some layout calculations don't work.
	pDopeSheet->SetContent(&sequenceData.m_timelineContent);

	UpdateActiveSequence();
	UpdatePlaybackRangeMarkers();
	UpdateToolbar();
}

void CTrackViewSequenceTabWidget::OnAddNodeButtonClicked()
{
	CTrackViewSequence* pSequence = GetActiveSequence();
	if (pSequence)
	{
		CTrackViewNode* pSelectedNode = pSequence->GetFirstSelectedNode();

		if (pSelectedNode && pSelectedNode->IsGroupNode())
		{
			m_pContextMenuNode = pSelectedNode;
		}
		else
		{
			m_pContextMenuNode = pSequence;
		}

		std::unique_ptr<QMenu> pContextMenu(new QMenu);
		FillAddNodeMenu(*pContextMenu);
		pContextMenu->exec(QCursor::pos());
	}
}

void CTrackViewSequenceTabWidget::OnAddSelectedEntities()
{
	CTimeline* pDopeSheet = GetActiveDopeSheet();
	CTrackViewSequence* pSequence = GetSequenceFromDopeSheet(pDopeSheet);
	STimelineContent* pContent = pDopeSheet ? pDopeSheet->Content() : nullptr;

	if (pSequence)
	{
		CTrackViewSequenceNotificationContext context(pSequence);

		CUndo undo("Add selected entities to sequence");

		TSelectedTracks selectedTracks;
		GetSelectedTracks(pContent->track, selectedTracks);
		const size_t numSelectedTracks = selectedTracks.size();
		STimelineTrack* pSingleTrack = (numSelectedTracks == 1) ? selectedTracks[0] : nullptr;
		CTrackViewNode* pSelectedNode = GetNodeFromDopeSheetTrack(pSequence, pSingleTrack);

		if (!pSelectedNode || (pSelectedNode && pSelectedNode->IsGroupNode()))
		{
			if (!pSelectedNode)
			{
				pSequence->AddSelectedEntities();
			}
			else
			{
				CTrackViewAnimNode* pSelectedAnimNode = static_cast<CTrackViewAnimNode*>(pSelectedNode);
				pSelectedAnimNode->AddSelectedEntities();
			}
		}
	}
}

void CTrackViewSequenceTabWidget::ClearData()
{
	m_pCurrentSelectedNode = nullptr;
	m_pCurrentSelectionCurveEditor = nullptr;
	m_pCurrentSelectionDopeSheet = nullptr;
	m_currentKeySelection.clear();
	m_tabToSequenceUIdMap.clear();
	m_idToSequenceDataMap.clear();
	m_pDopesheetTabs->clear();
}

void CTrackViewSequenceTabWidget::OnAddTrack(CAnimParamType type)
{
	if (m_pContextMenuNode && m_pContextMenuNode->GetNodeType() == eTVNT_AnimNode)
	{
		CTrackViewAnimNode* pAnimNode = static_cast<CTrackViewAnimNode*>(m_pContextMenuNode);

		string undoMessage;
		undoMessage.Format("Added track to node '%s'", pAnimNode->GetName());
		CUndo undo(undoMessage);
		pAnimNode->CreateTrack(type);
	}
}

void CTrackViewSequenceTabWidget::OnAddNode(const SAddableNode& node)
{
	if (m_pContextMenuNode && m_pContextMenuNode->IsGroupNode())
	{
		CTrackViewAnimNode* pAnimNode = static_cast<CTrackViewAnimNode*>(m_pContextMenuNode);

		string undoMessage;
		undoMessage.Format("Added '%s' to node '%s'", node.m_name, pAnimNode->GetName());
		CUndo undo(undoMessage);

		string availableName = pAnimNode->GetAvailableNodeNameStartingWith(node.m_name);

		// If it is a group node we want to add, take the highest level sequence node and handle the duplicates
		if (node.m_type == eAnimNodeType_Group)
		{
			availableName = GetActiveSequence()->GetAvailableNodeNameStartingWith(node.m_name);
		}

		pAnimNode->CreateSubNode(availableName, node.m_type);
	}
}

void CTrackViewSequenceTabWidget::OnRenameNode()
{
	CTrackViewAnimNode* pAnimNode = static_cast<CTrackViewAnimNode*>(m_pContextMenuNode);
	if (!pAnimNode || !pAnimNode->CanBeRenamed())
		return;

	QString inputText = QInputDialog::getText(this, "Enter new name", "Rename Node", QLineEdit::Normal, pAnimNode->GetName());
	if (!inputText.size())
		return;

	string undoMessage;
	string newNameString = inputText.toUtf8();
	undoMessage.Format("Renamed node '%s' to '%s'", pAnimNode->GetName(), newNameString);
	CUndo undo(undoMessage);

	if (!pAnimNode->SetName(newNameString.c_str()))
	{
		CryLog("Problem with renaming node %s. Does a node with that name already exist?", newNameString);
	}
}

void CTrackViewSequenceTabWidget::OnDeleteNodes()
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

void CTrackViewSequenceTabWidget::OnEnableNode()
{
	CUndo undo("Enable Selected Tracks and Nodes");

	DisableSelectedNodesAndTracks(false);
}

void CTrackViewSequenceTabWidget::OnDisableNode()
{
	CUndo undo("Disable Selected Tracks and Nodes");

	DisableSelectedNodesAndTracks(true);
}

void CTrackViewSequenceTabWidget::DisableSelectedNodesAndTracks(bool disable)
{
	// Process nodes
	CTrackViewAnimNodeBundle selectedNodes = GetActiveSequence()->GetSelectedAnimNodes();

	for (int nodeNumber = 0; nodeNumber < selectedNodes.GetCount(); ++nodeNumber)
	{
		CTrackViewAnimNode* pNode = selectedNodes.GetNode(nodeNumber);
		DisableNode(pNode, disable);
	}

	// Process tracks
	CTrackViewTrackBundle selectedTracks = GetActiveSequence()->GetSelectedTracks();

	for (int trackNumber = 0; trackNumber < selectedTracks.GetCount(); ++trackNumber)
	{
		selectedTracks.GetTrack(trackNumber)->SetDisabled(disable);
	}

}

void CTrackViewSequenceTabWidget::DisableNode(CTrackViewNode* pNode, bool disable)
{
	if (pNode)
	{
		pNode->SetDisabled(disable);
		uint32 count = pNode->GetChildCount();

		for (uint32 i = 0; i < count; ++i)
		{
			DisableNode(pNode->GetChild(i), disable);
		}
	}
}

void CTrackViewSequenceTabWidget::OnSelectEntity()
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
		}
	}
}

void CTrackViewSequenceTabWidget::OnImportFromFile()
{
	CTrackViewPlugin::GetExporter()->ImportFromFile();
}

void CTrackViewSequenceTabWidget::OnExportToFile()
{
	CTrackViewPlugin::GetExporter()->ExportToFile();
}

void CTrackViewSequenceTabWidget::UpdateCurveEditor(CTimeline* pDopeSheet, CCurveEditor* pCurveEditor)
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
		pCurveEditor->SetTimeRange(pDopeSheetContent->track.startTime, pDopeSheetContent->track.endTime);
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
			if (pNode && pNode->GetNodeType() == eTVNT_Track)
			{
				CTrackViewTrack* pTrack = static_cast<CTrackViewTrack*>(pNode);

				if (!pTrack->IsCompoundTrack() && strcmp(pTrack->GetKeyType(), S2DBezierKey::GetType()) == 0)
				{
					pCurveEditorContent->m_curves.push_back(SCurveEditorCurve());
					SCurveEditorCurve& curve = pCurveEditorContent->m_curves.back();
					curve.m_color = Private_TrackViewWindow::GetCurveColor(pTrack->GetParameterType());
					curve.m_defaultValue = stl::get<float>(pTrack->GetDefaultValue());

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

void CTrackViewSequenceTabWidget::UpdateTabNames()
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

void CTrackViewSequenceTabWidget::SetSelection(CTrackViewNode* pNode)
{
	m_currentKeySelection.clear();
	m_pCurrentSelectedNode = pNode;

	GetTrackViewCore()->UpdateProperties(ePropertyDataSource_Timeline);
}

CTrackViewNode* CTrackViewSequenceTabWidget::GetSelectedNode() const
{
	return m_pCurrentSelectedNode;
}

size_t CTrackViewSequenceTabWidget::GetSelectedElementsCount(STimelineTrack* pTimelineTrack) const
{
	size_t selectedElementCount = 0;

	if (pTimelineTrack == nullptr)
	{
		const CTimeline* pTimeline = GetActiveDopeSheet();
		if (pTimeline)
		{
			pTimelineTrack = &pTimeline->Content()->track;
		}
	}

	if (pTimelineTrack != nullptr)
	{
		TSelectedElements elements;
		GetSelectedElements(*pTimelineTrack, elements);
		selectedElementCount = elements.size();
	}

	return selectedElementCount;
}

void CTrackViewSequenceTabWidget::SetSequenceTime(const CryGUID sequenceGUID, SAnimTime newTime)
{
	CTimeline* pDopeSheet = GetDopeSheetFromSequenceGUID(sequenceGUID);
	if (pDopeSheet)
	{
		pDopeSheet->SetTime(newTime);
	}

	CCurveEditor* pCurveEditor = GetCurveEditorFromSequenceGUID(sequenceGUID);
	if (pCurveEditor)
	{
		pCurveEditor->SetTime(newTime);
	}
}

void CTrackViewSequenceTabWidget::UpdateSequenceTabs()
{
	CTrackViewSequenceManager* pSequenceManager = CTrackViewPlugin::GetSequenceManager();

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

void CTrackViewSequenceTabWidget::UpdateToolbar()
{
	// TODO: We clear here because CurveEditor creates always all items. So should actually
	//			 refactor the curve editor so we just disconnect and connect.
	m_pToolbar->clear();
	if (CCurveEditor* pCurveEditor = GetActiveCurveEditor())
	{
		pCurveEditor->FillWithCurveToolsAndConnect(m_pToolbar);
	}
}

void CTrackViewSequenceTabWidget::UpdatePlaybackRangeMarkers()
{
	CTimeline* pActiveDopeSheet = GetActiveDopeSheet();
	if (pActiveDopeSheet)
	{
		std::vector<SHeaderElement>& elements = pActiveDopeSheet->GetHeaderElements();
		if (elements.empty())
		{
			SHeaderElement startMarker;
			startMarker.description = "playback_start";
			startMarker.pixmap = "icons:Trackview/trackview_time_limit_L";
			startMarker.alignment = Qt::AlignRight | Qt::AlignBottom;

			elements.push_back(startMarker);

			SHeaderElement endMarker;
			endMarker.description = "playback_end";
			endMarker.pixmap = "icons:Trackview/trackview_time_limit_R";
			endMarker.alignment = Qt::AlignLeft | Qt::AlignBottom;

			elements.push_back(endMarker);
		}

		CTrackViewSequence* pSequence = GetActiveSequence();
		auto playbackRange = pSequence->GetPlaybackRange();

		elements[0].time = playbackRange.start;
		elements[0].visible = playbackRange.start >= SAnimTime(0);

		elements[1].time = playbackRange.end;
		elements[1].visible = playbackRange.end >= SAnimTime(0);
	}
}

void CTrackViewSequenceTabWidget::UpdateActiveSequence()
{
	CTimeline* pActiveDopeSheet = GetActiveDopeSheet();
	if (pActiveDopeSheet != nullptr)
	{
		pActiveDopeSheet->ClearElementSelections();

		CTrackViewSequence* pSequence = GetActiveSequence();
		CTrackViewPlugin::GetAnimationContext()->SetSequence(pSequence, false, false);

		if (pSequence)
		{
			GetTrackViewCore()->UpdateProperties(ePropertyDataSource_Sequence);
		}
	}
	else
	{
		CTrackViewPlugin::GetAnimationContext()->SetSequence(nullptr, false, true);
		GetTrackViewCore()->ClearProperties();
	}
}

void CTrackViewSequenceTabWidget::ShowSequenceProperties(CTrackViewSequence* pSequence)
{
	if (pSequence)
	{
		GetTrackViewCore()->UpdateProperties(ePropertyDataSource_Sequence);
	}
	else
	{
		GetTrackViewCore()->ClearProperties();
	}
}

void CTrackViewSequenceTabWidget::FillAddNodeMenu(QMenu& addNodeMenu)
{
	auto nodes = Private_TrackViewWindow::GetAddableNodes(*static_cast<CTrackViewAnimNode*>(m_pContextMenuNode));

	std::sort(nodes.begin(), nodes.end(), [](const SAddableNode& first, const SAddableNode& second)
	{
		return first.m_name < second.m_name;
	});

	for (const SAddableNode& node : nodes)
	{
		QAction* pAction = addNodeMenu.addAction(QString(node.m_name));
		connect(pAction, &QAction::triggered, [this, node]() { OnAddNode(node); });
	}
}

// This creates a recursive menu by splitting track names on '/'
void CTrackViewSequenceTabWidget::CreateAddTrackMenu(QMenu& parentMenu, const CTrackViewAnimNode& animNode)
{
	parentMenu.addSeparator();
	QMenu* pAddTrackMenu = parentMenu.addMenu("Add Track");

	auto addableTracks = Private_TrackViewWindow::GetAddableTracks(animNode);

	std::sort(addableTracks.begin(), addableTracks.end(), [](const SAddableTrack& first, const SAddableTrack& second)
	{
		return first.m_name < second.m_name;
	});

	for (const SAddableTrack& track : addableTracks)
	{
		CAnimParamType type = track.m_type;
		QString path(track.m_name.c_str());
		QStringList pathList = path.split("/");
		QMenu* pParentMenu = pAddTrackMenu;
		for (auto i = 0, subMenuCount = pathList.size() - 1; i < subMenuCount; ++i)
		{
			const QString& section = pathList[i];
			QMenu* pMenu = pParentMenu->findChild<QMenu*>(section);
			if (!pMenu)
			{
				pMenu = pParentMenu->addMenu(section);
				pMenu->setObjectName(section);
			}

			pParentMenu = pMenu;
		}

		QAction* pAction = pParentMenu->addAction(pathList[pathList.size() - 1]);
		connect(pAction, &QAction::triggered, [this, type]() { OnAddTrack(type); });
	}
}

void CTrackViewSequenceTabWidget::RemoveDeletedTracks(STimelineTrack* pTrack)
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

CTrackViewAnimNode* CTrackViewSequenceTabWidget::CheckValidReparenting(CTimeline* pDopeSheet, const TSelectedTracks& selectedTracks, STimelineTrack* pTarget)
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

CTrackViewSequence* CTrackViewSequenceTabWidget::GetSequenceFromDopeSheet(const CTimeline* pDopeSheet)
{
	CryGUID guid;
	memcpy(&guid, &pDopeSheet->Content()->userSideLoad[0], sizeof(CryGUID));
	return CTrackViewPlugin::GetSequenceManager()->GetSequenceByGUID(guid);
}

CTrackViewNode* CTrackViewSequenceTabWidget::GetNodeFromDopeSheetTrack(const CTrackViewSequence* pSequence, const STimelineTrack* pDopeSheetTrack)
{
	return pSequence->GetNodeFromGUID(GetGUIDFromDopeSheetTrack(pDopeSheetTrack));
}

STimelineTrack* CTrackViewSequenceTabWidget::GetDopeSheetTrackFromNode(const CTrackViewNode* pNode)
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

CryGUID CTrackViewSequenceTabWidget::GetGUIDFromDopeSheetTrack(const STimelineTrack* pDopeSheetTrack) const
{
	Private_TrackViewWindow::STrackUserData userData;
	if (pDopeSheetTrack)
	{
		const size_t userLoadSize = pDopeSheetTrack->userSideLoad.size();
		if (userLoadSize > 0 && Serialization::LoadBinaryBuffer(userData, &pDopeSheetTrack->userSideLoad[0], userLoadSize))
		{
			return userData.m_guid;
		}
	}

	return CryGUID::Null();
}

CTimeline* CTrackViewSequenceTabWidget::GetActiveDopeSheet() const
{
	const CryGUID guid = GetSequenceGUIDFromTabIndex(m_pDopesheetTabs->currentIndex());
	return GetDopeSheetFromSequenceGUID(guid);
}

CTimeline* CTrackViewSequenceTabWidget::GetDopeSheetFromSequenceGUID(const CryGUID& guid) const
{
	auto findIter = m_idToSequenceDataMap.find(guid);
	if (findIter != m_idToSequenceDataMap.end())
	{
		const SSequenceData& sequenceData = findIter->second;
		return sequenceData.m_pDopeSheet;
	}

	return nullptr;
}

CCurveEditor* CTrackViewSequenceTabWidget::GetActiveCurveEditor() const
{
	const CryGUID guid = GetSequenceGUIDFromTabIndex(m_pDopesheetTabs->currentIndex());
	return GetCurveEditorFromSequenceGUID(guid);
}

CCurveEditor* CTrackViewSequenceTabWidget::GetCurveEditorFromSequenceGUID(const CryGUID& guid) const
{
	auto findIter = m_idToSequenceDataMap.find(guid);
	if (findIter != m_idToSequenceDataMap.end())
	{
		const SSequenceData& sequenceData = findIter->second;
		return sequenceData.m_pCurveEditor;
	}

	return nullptr;
}

void CTrackViewSequenceTabWidget::OnDopesheetTrackSelectionChanged()
{
	CTimeline* pDopeSheet = static_cast<CTimeline*>(QObject::sender());
	if (pDopeSheet && pDopeSheet->Content())
	{
		// sync dopesheet nodes with track selection from timeline
		TAllTracksWithSelection allTracks;
		GetAllTracksWithSelection(pDopeSheet->Content()->track, allTracks);

		std::vector<STimelineTrack*> selectedTracks;
		GetSelectedTracks(pDopeSheet->Content()->track, selectedTracks);
		const bool bIsSingleNodeSelection = (selectedTracks.size() == 1) && (m_currentKeySelection.size() == 0);

		for (auto iter = allTracks.begin(); iter != allTracks.end(); ++iter)
		{
			CTrackViewNode* pNode = GetNodeFromActiveSequence(iter->first);
			if (pNode)
			{
				pNode->SetSelected(iter->second);

				if (bIsSingleNodeSelection && iter->second)
				{
					SetSelection(pNode);
				}
			}
		}
	}

	CCurveEditor* pCurveEditor = GetActiveCurveEditor();
	if (pDopeSheet && pCurveEditor)
	{
		UpdateCurveEditor(pDopeSheet, pCurveEditor);

		pCurveEditor->OnFitCurvesHorizontally();
		pCurveEditor->OnFitCurvesVertically();
	}

	OnSelectionChanged(false);
}

void CTrackViewSequenceTabWidget::OnDopesheetTreeContextMenu(const QPoint& point)
{
	CTimeline* pDopeSheet = static_cast<CTimeline*>(QObject::sender());
	STimelineContent* pContent = pDopeSheet ? pDopeSheet->Content() : nullptr;
	CTrackViewSequence* pSequence = GetSequenceFromDopeSheet(pDopeSheet);

	if (pContent)
	{
		int tracksEnabledCount = 0;
		int tracksDisabledCount = 0;

		std::vector<STimelineTrack*> selectedTracks;
		GetSelectedTracks(pContent->track, selectedTracks);

		const STimelineTrack* hoverTrack = pDopeSheet->GetTrackFromPos(point);

		const size_t numSelectedTracks = selectedTracks.size();
		STimelineTrack* pSingleTrack = (numSelectedTracks == 1) ? selectedTracks[0] : nullptr;

		std::unique_ptr<QMenu> pContextMenu(new QMenu);

		CTrackViewNode* pSelectedNode = GetNodeFromDopeSheetTrack(pSequence, pSingleTrack);
		IObjectManager* pObjectManager = GetIEditor()->GetObjectManager();
		const CSelectionGroup* pSelectedObjects = pObjectManager->GetSelection();

		int selectedObjectsCount = pSelectedObjects->GetCount();
		bool bShowAddSelectedEntities = (selectedObjectsCount > 0) ? true : false;

		if (!pSelectedNode || (pSelectedNode && pSelectedNode->IsGroupNode()))
		{
			pContextMenu->addAction("Add Selected Entities", this, SLOT(OnAddSelectedEntities()))->setEnabled(bShowAddSelectedEntities);
		}

		if (numSelectedTracks == 0 || !hoverTrack)
		{
			m_pContextMenuNode = pSequence;
			QMenu* addMenu = pContextMenu->addMenu("Add Node");
			FillAddNodeMenu(*addMenu);
		}
		else if (numSelectedTracks > 0 && hoverTrack)
		{
			pContextMenu->addSeparator();

			pContextMenu->addAction("Delete", this, SLOT(OnDeleteNodes()));

			for (STimelineTrack* ploopTrack : selectedTracks)
			{
				if (ploopTrack->disabled)
				{
					++tracksDisabledCount;
				}
				else
				{
					++tracksEnabledCount;
				}
			}

		}

		if (pSingleTrack)
		{
			m_pContextMenuNode = GetNodeFromDopeSheetTrack(pSequence, pSingleTrack);
		}

		if (pSingleTrack)
		{
			if (m_pContextMenuNode->IsDisabled())
			{
				pContextMenu->addAction("Enable", this, SLOT(OnEnableNode()));
			}
			else
			{
				pContextMenu->addAction("Disable", this, SLOT(OnDisableNode()));
			}
		}
		else if (tracksDisabledCount == numSelectedTracks)
		{
			pContextMenu->addAction("Enable", this, SLOT(OnEnableNode()));
		}
		else if (tracksEnabledCount == numSelectedTracks)
		{
			pContextMenu->addAction("Disable", this, SLOT(OnDisableNode()));
		}
		else
		{
			pContextMenu->addAction("Enable", this, SLOT(OnEnableNode()));
			pContextMenu->addAction("Disable", this, SLOT(OnDisableNode()));
		}

		if (pSingleTrack && numSelectedTracks == 1)
		{
			if (m_pContextMenuNode && m_pContextMenuNode->GetNodeType() == eTVNT_AnimNode)
			{
				CTrackViewAnimNode* pAnimNode = static_cast<CTrackViewAnimNode*>(m_pContextMenuNode);
				if (pSingleTrack && pAnimNode && pAnimNode->CanBeRenamed())
				{
					pContextMenu->addAction("Rename", this, SLOT(OnRenameNode()));
				}

				CEntityObject* pEntityObject = pAnimNode->GetNodeEntity(true);
				if (pEntityObject)
				{
					pContextMenu->addSeparator();
					QAction* pAction = pContextMenu->addAction("Select in Viewport", this, SLOT(OnSelectEntity()));
				}

				pAnimNode->RefreshDynamicParams();
				CreateAddTrackMenu(*pContextMenu, *pAnimNode);
			}

			if (m_pContextMenuNode && m_pContextMenuNode->IsGroupNode())
			{
				pContextMenu->addSeparator();
				QMenu* pAddNodeMenu = pContextMenu->addMenu("Add Node");
				FillAddNodeMenu(*pAddNodeMenu);
			}

			if (m_pContextMenuNode && m_pContextMenuNode->GetNodeType() == eTVNT_Track)
			{
				//CClipboard clip;
				//bool bKeyDataInClipboard = !strcmp("Track view keys", clip.GetTitle());
				bool bKeysOnTrack = m_pContextMenuNode->GetAllKeys().GetKeyCount() > 0;
				bool bKeysSelected = m_currentKeySelection.size() > 0;

				pContextMenu->addSeparator();
				pContextMenu->addAction("Copy Keys", this, SLOT(OnCopyKeys()))->setEnabled(bKeysOnTrack);
				pContextMenu->addAction("Copy Selected Keys", this, SLOT(OnCopySelectedKeys()))->setEnabled(bKeysSelected);
				//pContextMenu->addAction("Paste Keys", this, SLOT(OnPasteKeys()))->setEnabled(bKeyDataInClipboard);
				bool bCanExportNode = GetActiveSequence() ? GetActiveSequence()->GetSelectedAnimNodes().GetCount() > 0 : false;
				pContextMenu->addSeparator();
				pContextMenu->addAction("Import Node", this, SLOT(OnImportFromFile()))->setEnabled(bCanExportNode);
				pContextMenu->addAction("Export Node", this, SLOT(OnExportToFile()))->setEnabled(bCanExportNode);
			}

			if (pSelectedNode)
			{
				CTrackViewAnimNode* pSelectedAnimNode = static_cast<CTrackViewAnimNode*>(pSelectedNode);

				if (pSelectedAnimNode->GetType() == eAnimNodeType_Director)
				{
					pContextMenu->addAction("Set As Active Director", this, SLOT(OnSetAsActiveDirector()));
				}
			}
		}

		pContextMenu->exec(point);
	}
}

void CTrackViewSequenceTabWidget::OnSetAsActiveDirector()
{
	CTrackViewAnimNodeBundle selectedNodes = GetActiveSequence()->GetSelectedAnimNodes();

	if (selectedNodes.GetCount() > 0)
	{
		CTrackViewAnimNode* firstNode = selectedNodes.GetNode(0);

		CRY_ASSERT(firstNode);

		if (firstNode && firstNode->GetType() == eAnimNodeType_Director)
		{
			firstNode->SetAsActiveDirector();
		}
	}
}

void CTrackViewSequenceTabWidget::OnDopesheetTracksBeginDrag(STimelineTrack* pTarget, bool& bValid)
{
	CTimeline* pDopeSheet = static_cast<CTimeline*>(QObject::sender());
	CTrackViewSequence* pSequence = GetSequenceFromDopeSheet(pDopeSheet);
	CTrackViewNode* pNode = GetNodeFromDopeSheetTrack(pSequence, pTarget);

	bValid = (pNode->GetNodeType() == eTVNT_AnimNode);
}

void CTrackViewSequenceTabWidget::OnDopesheetTracksDragging(STimelineTrack* pTarget, bool& bValid)
{
	CTimeline* pDopeSheet = static_cast<CTimeline*>(QObject::sender());
	TSelectedTracks selectedTracks;
	GetSelectedTracks(pDopeSheet->Content()->track, selectedTracks);

	bValid = CheckValidReparenting(pDopeSheet, selectedTracks, pTarget) != nullptr;
}

void CTrackViewSequenceTabWidget::OnDopesheetTracksDragged(STimelineTrack* pTarget)
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

void CTrackViewSequenceTabWidget::OnDopesheetZoom()
{
	if (m_timelineLink)
	{
		CCurveEditor* pCurveEditor = GetActiveCurveEditor();
		CTimeline* pDopesheet = GetActiveDopeSheet();
		if (pCurveEditor && pDopesheet)
		{
			Range visibleTimeRange = pDopesheet->GetVisibleTimeRangeFull();
			pCurveEditor->ZoomToTimeRange(visibleTimeRange.start, visibleTimeRange.end);
			pCurveEditor->update();
		}
	}
}

void CTrackViewSequenceTabWidget::OnCurveEditorZoom()
{
	//TODO : implement !
	// TODO: Zooming in dopesheet when zooming in curve editor is very broken
	//			 so we don't allow that for now.
	/*if (m_timelineLink)
	   {
	   CCurveEditor* pCurveEditor = GetActiveCurveEditor();
	   CTimeline* pDopesheet = GetActiveDopeSheet();
	   if (pCurveEditor && pDopesheet)
	   {
	   Range visibleTimeRange = pCurveEditor->GetVisibleTimeRange();
	   pDopesheet->ZoomToTimeRange(visibleTimeRange.start, visibleTimeRange.end);
	   pDopesheet->update();
	   }
	   }*/
}

void CTrackViewSequenceTabWidget::OnDopesheetPan()
{
	if (m_timelineLink)
	{
		CCurveEditor* pCurveEditor = GetActiveCurveEditor();
		CTimeline* pDopesheet = GetActiveDopeSheet();
		if (pCurveEditor && pDopesheet)
		{
			Range visibleTimeRange = pDopesheet->GetVisibleTimeRangeFull();
			pCurveEditor->ZoomToTimeRange(visibleTimeRange.start, visibleTimeRange.end);
			pCurveEditor->update();
		}
	}
}

void CTrackViewSequenceTabWidget::OnCurveEditorPan()
{
	//TODO : implement !
	/*if (m_timelineLink)
	   {
	   CCurveEditor* pCurveEditor = GetActiveCurveEditor();
	   CTimeline* pDopesheet = GetActiveDopeSheet();
	   if (pCurveEditor && pDopesheet)
	   {
	   Range visibleTimeRange = pCurveEditor->GetVisibleTimeRange();
	   pDopesheet->ZoomToTimeRange(visibleTimeRange.start, visibleTimeRange.end);
	   pDopesheet->update();
	   }
	   }*/
}

void CTrackViewSequenceTabWidget::OnCurveEditorContentChanged()
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

	OnSelectionChanged(false);
}

void CTrackViewSequenceTabWidget::OnDopesheetScrub()
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

void CTrackViewSequenceTabWidget::OnCurveEditorScrub()
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

void CTrackViewSequenceTabWidget::EnableTimelineLink(bool enable)
{
	if (m_timelineLink != enable)
	{
		m_timelineLink = enable;

		CTimeline* pDopesheet = GetActiveDopeSheet();
		if (pDopesheet)
		{
			CCurveEditor* pCurveEditor = GetActiveCurveEditor();
			if (enable && pCurveEditor)
			{
				Range visibleTimeRange = pDopesheet->GetVisibleTimeRangeFull();
				pCurveEditor->ZoomToTimeRange(visibleTimeRange.start, visibleTimeRange.end);
				pCurveEditor->update();
			}
		}
	}
}

void CTrackViewSequenceTabWidget::ShowKeyText(bool show)
{
	if (m_showKeyText != show)
	{
		m_showKeyText = show;
		if (CTimeline* pDopesheet = GetActiveDopeSheet())
		{
			pDopesheet->ShowKeyText(show);
		}
	}
}

void CTrackViewSequenceTabWidget::ApplyDopeSheetChanges(CTimeline* pDopeSheet)
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

	GetTrackViewCore()->UpdateProperties(ePropertyDataSource_Timeline);
}

void CTrackViewSequenceTabWidget::ApplyTrackChangesRecursive(const CTrackViewSequence* pSequence, STimelineTrack* pTrack)
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
			}
		}
	}

	for (auto iter = pTrack->tracks.begin(); iter != pTrack->tracks.end(); ++iter)
	{
		ApplyTrackChangesRecursive(pSequence, *iter);
	}
}

void CTrackViewSequenceTabWidget::UpdateTrackViewTrack(CTrackViewTrack* pTrack)
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

void CTrackViewSequenceTabWidget::RemoveTrackViewNode(CTrackViewNode* pNode)
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

// Search for correct insert position and insert track into dope sheet
void CTrackViewSequenceTabWidget::InsertDopeSheetTrack(STimelineTrack* pParentTrack, STimelineTrack* pTrack, CTrackViewNode* pNode)
{
	STimelineTracks& siblingTracks = pParentTrack->tracks;
	const uint numSiblingTracks = siblingTracks.size();

	uint insertPosition;
	for (insertPosition = 0; insertPosition < numSiblingTracks; ++insertPosition)
	{
		const STimelineTrack* pSiblingTrack = siblingTracks[insertPosition];
		const CTrackViewNode* pSiblingNode = GetNodeFromDopeSheetTrack(pNode->GetSequence(), pSiblingTrack);
		if (!pSiblingNode || *pNode < *pSiblingNode)
		{
			break;
		}
	}

	siblingTracks.insert(siblingTracks.begin() + insertPosition, pTrack);
}

void CTrackViewSequenceTabWidget::AddNodeToDopeSheet(CTrackViewNode* pNode)
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

			CTrackViewAnimNode* pAnimNode = static_cast<CTrackViewAnimNode*>(pNode);
			CTrackViewIcons::GetIconByNodeType(pDopeSheetTrack->icon, pAnimNode->GetType());

			pDopeSheetTrack->name = pNode->GetName();
			pDopeSheetTrack->height = 22;
			pDopeSheetTrack->caps |= STimelineTrack::CAP_DESCRIPTION_TRACK;

			Serialization::SaveBinaryBuffer(pDopeSheetTrack->userSideLoad, userData);
			InsertDopeSheetTrack(pParentTrack, pDopeSheetTrack, pNode);
		}
		break;
	case eTVNT_Track:
		{
			STimelineTrack* pDopeSheetTrack = new STimelineTrack;
			sequenceData.m_uIdToTimelineTrackMap[nodeGUID] = pDopeSheetTrack;

			CTrackViewTrack* pTrack = static_cast<CTrackViewTrack*>(pNode);
			CTrackViewIcons::GetIconByParamType(pDopeSheetTrack->icon, pTrack->GetParameterType().GetType());
			pDopeSheetTrack->name = pNode->GetName();
			pDopeSheetTrack->height = 22;
			pDopeSheetTrack->startTime = sequenceData.m_startTime;
			pDopeSheetTrack->endTime = sequenceData.m_endTime;
			pDopeSheetTrack->defaultElement.caps = STimelineElement::CAP_SELECT | STimelineElement::CAP_MOVE | STimelineElement::CAP_CLIP_DRAW_KEY;

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
					pDopeSheetTrack->toggleDefaultState = stl::get<bool>(pTrack->GetDefaultValue());
				}

				UpdateDopeSheetNode(pTrack);
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

void CTrackViewSequenceTabWidget::RemoveNodeFromDopeSheet(CTrackViewNode* pNode)
{
	if (m_bDontUpdateDopeSheet)
	{
		return;
	}

	CTrackViewSequence* pSequence = pNode->GetSequence();
	SSequenceData& sequenceData = m_idToSequenceDataMap[pSequence->GetGUID()];
	STimelineTrack* pDopeSheetTrack = GetDopeSheetTrackFromNode(pNode);

	// If the dope sheet track doesn't exist then just return. A common reason for this is
	// editing a trackview node belonging to a sequence that isn't open by our editor
	if (!pDopeSheetTrack)
		return;

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

void CTrackViewSequenceTabWidget::UpdateDopeSheetNode(CTrackViewNode* pNode)
{
	if (m_bDontUpdateDopeSheet)
	{
		return;
	}

	STimelineTrack* pDopeSheetTrack = GetDopeSheetTrackFromNode(pNode);
	if (pDopeSheetTrack && pNode->IsDisabled() != pDopeSheetTrack->disabled)
	{
		pDopeSheetTrack->disabled = pNode->IsDisabled();
		pDopeSheetTrack->modified = true;
	}

	if (pNode->GetNodeType() == eTVNT_Sequence || pNode->GetNodeType() == eTVNT_Track)
	{
		CTrackViewTrack* pTrack = static_cast<CTrackViewTrack*>(pNode);

		if (pTrack->IsCompoundTrack())
		{
			const uint numChildTracks = pTrack->GetChildCount();
			for (uint i = 0; i < numChildTracks; ++i)
			{
				CTrackViewNode* pChild = pTrack->GetChild(i);
				UpdateDopeSheetNode(pChild);
			}
			return;
		}

		CTrackViewSequence* pSequence = pTrack->GetSequence();
		SSequenceData& sequenceData = m_idToSequenceDataMap[pSequence->GetGUID()];

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
					element.clipAnimEnd = keyHandle.GetAnimEnd();
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
}

void CTrackViewSequenceTabWidget::AssignDopeSheetName(STimelineTrack* pDopeSheetTrack, CTrackViewAnimNode* pAnimNode)
{
	assert(pAnimNode);
	assert(pDopeSheetTrack);

	string namealias = pAnimNode->GetName();

	CTrackViewEntityNode* pEntityNode = (pAnimNode->GetType() == eAnimNodeType_Entity) ? static_cast<CTrackViewEntityNode*>(pAnimNode) : nullptr;
	if (!pEntityNode || pEntityNode->GetNodeEntity(false))
	{
		pDopeSheetTrack->name = namealias;
	}
	else
	{
		pDopeSheetTrack->name = namealias + " (Detached)";		
	}
}

void CTrackViewSequenceTabWidget::OnDopesheetContentChanged(bool continuous)
{
	if (continuous)
	{
		return;
	}

	OnSelectionChanged(true);
	TimelineLayoutChanged();
}

void CTrackViewSequenceTabWidget::OnSelectionChanged(bool /*bContinuous*/)
{
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

		const size_t numElements = elements.size();

		for (size_t i = 0; i < numElements; ++i)
		{
			SSelectedKey selectedKey;

			selectedKey.m_pTrack = elements[i].first;
			selectedKey.m_pElement = elements[i].second;

			_smart_ptr<IAnimKeyWrapper> pKeyWrapper = GetWrappedKeyFromElement(*selectedKey.m_pTrack, *selectedKey.m_pElement);
			if (pKeyWrapper)
			{
				selectedKey.m_key = pKeyWrapper;
				m_currentKeySelection.push_back(selectedKey);
			}
		}

		TSelectedTracks selectedTracks;
		GetSelectedTracks(pContent->track, selectedTracks);

		if (selectedTracks.size() == 1)
		{
			CTrackViewNode* pNode = GetNodeFromDopeSheetTrack(GetActiveSequence(), selectedTracks[0]);
			if (pNode)
			{
				m_pCurrentSelectedNode = pNode;
			}
		}
	}

	GetTrackViewCore()->UpdateProperties(ePropertyDataSource_Timeline);
}

void CTrackViewSequenceTabWidget::OnCurveEditorSelectionChanged()
{
	GetTrackViewCore()->ClearProperties();

	// TODO: We need a smarter way to get the current selected key.
	OnCurveEditorContentChanged();

	GetTrackViewCore()->UpdateProperties(ePropertyDataSource_Timeline);
}

void CTrackViewSequenceTabWidget::OnSelectionRefresh()
{
	GetTrackViewCore()->UpdateProperties(ePropertyDataSource_Timeline);
}

void CTrackViewSequenceTabWidget::GetPropertiesToUpdate(Serialization::SStructs& structs, EPropertyDataSource dataSource)
{
	switch (dataSource)
	{
	case ePropertyDataSource_Sequence:
		{
			GetSequenceProperties(structs);
			CTrackViewPlugin::GetAnimationContext()->UpdateTimeRange();
		}
		break;

	case ePropertyDataSource_Timeline:
		{
			if (GetSelectedElementsCount() > 0)
			{
				GetKeyProperties(structs);
			}
			else if (m_pCurrentSelectedNode)
			{
				GetNodeProperties(structs);
			}
		}
		break;

	default:
		break;
	}
}

void CTrackViewSequenceTabWidget::GetKeyProperties(Serialization::SStructs& structs)
{
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
	}
}

void CTrackViewSequenceTabWidget::GetNodeProperties(Serialization::SStructs& structs)
{
	m_currentKeySelection.clear();
	m_pCurrentSelectedNode = nullptr;

	CTimeline* pDopeSheet = GetActiveDopeSheet();
	STimelineContent* pContent = pDopeSheet ? pDopeSheet->Content() : nullptr;
	CTrackViewSequence* pSequence = pDopeSheet ? GetSequenceFromDopeSheet(pDopeSheet) : nullptr;

	if (pContent)
	{
		TSelectedTracks selectedTracks;
		GetSelectedTracks(pContent->track, selectedTracks);

		if (selectedTracks.size() == 1)
		{
			CTrackViewNode* pNode = GetNodeFromDopeSheetTrack(pSequence, selectedTracks[0]);
			if (pNode)
			{
				m_pCurrentSelectedNode = pNode;
				structs.push_back(Serialization::SStruct(*m_pCurrentSelectedNode));
			}
		}
	}
}

void CTrackViewSequenceTabWidget::GetSequenceProperties(Serialization::SStructs& structs)
{
	CTrackViewSequence* pSequence = GetActiveSequence();
	if (pSequence)
	{
		structs.push_back(Serialization::SStruct(*pSequence));
	}
}

_smart_ptr<IAnimKeyWrapper> CTrackViewSequenceTabWidget::GetWrappedKeyFromElement(const STimelineTrack& track, const STimelineElement& element)
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

void CTrackViewSequenceTabWidget::OnTimeChanged(SAnimTime newTime)
{
	if (newTime == m_time)
	{
		return;
	}

	m_time = newTime;

	if (!m_refreshTimer->isActive())
	{
		m_refreshTimer->start(GetUiRefreshRateMilliseconds());
	}
}

void CTrackViewSequenceTabWidget::OnSequenceChanged(CTrackViewSequence* pSequence)
{
	UpdateSequenceTabs();
	UpdateToolbar();

	if (CTimeline* pDopesheet = GetActiveDopeSheet())
	{
		pDopesheet->ShowKeyText(m_showKeyText);
	}
}

void CTrackViewSequenceTabWidget::OnSequenceAdded(CTrackViewSequence* pSequence)
{
	CTrackViewCoreComponent::OnSequenceAdded(pSequence);
}

void CTrackViewSequenceTabWidget::OnSequenceRemoved(CTrackViewSequence* pSequence)
{
	CTrackViewCoreComponent::OnSequenceRemoved(pSequence);

	if (GetSelectedNode() == pSequence)
	{
		SetSelection(nullptr);
	}

	UpdateSequenceTabs();
	GetTrackViewCore()->UpdateProperties(ePropertyDataSource_Timeline);
}

void CTrackViewSequenceTabWidget::OnSequenceSettingsChanged(CTrackViewSequence* pSequence, SSequenceSettingsChangedEventArgs& args)
{
	UpdatePlaybackRangeMarkers();

	TRange<SAnimTime> newTimeRange = pSequence->GetTimeRange();

	SSequenceData& sequenceData = m_idToSequenceDataMap[pSequence->GetGUID()];
	if (sequenceData.m_startTime != newTimeRange.start || sequenceData.m_endTime != newTimeRange.end)
	{
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

		CCurveEditor* pCurveEditor = GetCurveEditorFromSequenceGUID(pSequence->GetGUID());
		if (pCurveEditor)
		{
			UpdateCurveEditor(pDopeSheet, pCurveEditor);
		}
	}

	// This code will try and update the current properties being displayed in the property tree. Previously
	// it was checking if the change came from user input and made some awful assumptions on what properties
	// it should update. That being said this is still not correct. The complete solution would be to have
	// selection be an undoable action and that way as things are selected and properties changed the
	// properties belonging to whatever is being changed will always be what's displayed.
	Serialization::SStructs structs;
	GetTrackViewCore()->GetCurrentProperties(structs);
	for (auto propStruct : structs)
	{
		if (propStruct.type() == Serialization::TypeID::get<CTrackViewSequence>())
		{
			GetTrackViewCore()->UpdateProperties(ePropertyDataSource_Sequence);
			return;
		}
	}
	GetTrackViewCore()->UpdateProperties(ePropertyDataSource_Timeline);
}

void CTrackViewSequenceTabWidget::OnNodeChanged(CTrackViewNode* pNode, ENodeChangeType type)
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
	case ITrackViewSequenceListener::eNodeChangeType_Disabled:
	case ITrackViewSequenceListener::eNodeChangeType_Enabled:
	case ITrackViewSequenceListener::eNodeChangeType_KeysChanged:
	// Fall through
	case ITrackViewSequenceListener::eNodeChangeType_KeySelectionChanged:
		{
			const bool bOldDontUpdateDopeSheet = m_bDontUpdateDopeSheet;
			m_bDontUpdateDopeSheet = false;
			UpdateDopeSheetNode(pNode);
			m_bDontUpdateDopeSheet = bOldDontUpdateDopeSheet;
		}
		break;
		case ITrackViewSequenceListener::eNodeChangeType_NodeOwnerChanged:
		{
			SSequenceData& sequenceData = m_idToSequenceDataMap[pNode->GetSequence()->GetGUID()];
			if (sequenceData.m_uIdToTimelineTrackMap.find(pNode->GetGUID()) != sequenceData.m_uIdToTimelineTrackMap.end())
			{
				AssignDopeSheetName(sequenceData.m_uIdToTimelineTrackMap[pNode->GetGUID()], static_cast<CTrackViewAnimNode*>(pNode));
			}
		}
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
}

void CTrackViewSequenceTabWidget::OnNodeRenamed(CTrackViewNode* pNode, const char* pOldName)
{
	if (pNode->GetNodeType() == eTVNT_Sequence)
	{
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
}

void CTrackViewSequenceTabWidget::OnTrackViewEditorEvent(ETrackViewEditorEvent event)
{
	switch (event)
	{
	case eTrackViewEditorEvent_OnSnapModeChanged:
		{
			ETrackViewSnapMode newSnapMode = GetTrackViewCore()->GetCurrentSnapMode();
			for (auto iter = m_idToSequenceDataMap.begin(); iter != m_idToSequenceDataMap.end(); ++iter)
			{
				CTimeline* pDopeSheet = iter->second.m_pDopeSheet;
				if (pDopeSheet)
				{
					pDopeSheet->SetKeySnapping(newSnapMode == eSnapMode_KeySnapping);
					pDopeSheet->SetTimeSnapping(newSnapMode == eSnapMode_TimeSnapping);
				}

				CCurveEditor* pCurveEditor = iter->second.m_pCurveEditor;
				if (pCurveEditor)
				{
					pCurveEditor->SetKeySnapping(newSnapMode == eSnapMode_KeySnapping);
					pCurveEditor->SetTimeSnapping(newSnapMode == eSnapMode_TimeSnapping);
				}
			}
			break;
		}

	case eTrackViewEditorEvent_OnKeyMoveModeChanged:
		{
			ETrackViewKeyMoveMode newMoveMode = GetTrackViewCore()->GetCurrentKeyMoveMode();
			for (auto iter = m_idToSequenceDataMap.begin(); iter != m_idToSequenceDataMap.end(); ++iter)
			{
				CTimeline* pDopeSheet = iter->second.m_pDopeSheet;
				if (pDopeSheet)
				{
					pDopeSheet->SetKeyScaling(newMoveMode == eKeyMoveMode_Scale);
				}
			}
			break;
		}

	case eTrackViewEditorEvent_OnFramerateChanged:
		{
			SAnimTime::EFrameRate newFramerate = GetTrackViewCore()->GetCurrentFramerate();
			CTimeline* pTimeline = GetActiveDopeSheet();
			if (pTimeline)
			{
				pTimeline->SetFrameRate(newFramerate);
			}

			CCurveEditor* pCurveEditor = GetActiveCurveEditor();
			if (pCurveEditor)
			{
				pCurveEditor->SetFrameRate(newFramerate);
			}
			break;
		}

	case eTrackViewEditorEvent_OnDisplayModeChanged:
		{
			SAnimTime::EDisplayMode newDisplayMode = GetTrackViewCore()->GetCurrentDisplayMode();
			CTimeline* pTimeline = GetActiveDopeSheet();
			if (pTimeline)
			{
				pTimeline->SetTimeUnit(newDisplayMode);
			}
			m_timeUnit = newDisplayMode;

			// TODO: CurveEditor doesn't support time units yet.
			/*CCurveEditor* pCurveEditor = GetActiveCurveEditor();
			   if (pCurveEditor)
			   {
			   pCurveEditor->SetFrameRate(newFramerate);
			   }*/
			break;
		}
	}
}

QWidget* CTrackViewSequenceTabWidget::GetCurrentDopeSheetContainer() const
{
	QSplitter* pSplitter = static_cast<QSplitter*>(m_pDopesheetTabs->widget(m_pDopesheetTabs->currentIndex()));
	return pSplitter ? pSplitter->widget(0) : nullptr;
}

QWidget* CTrackViewSequenceTabWidget::GetCurrentCurveEditorContainer() const
{
	QSplitter* pSplitter = static_cast<QSplitter*>(m_pDopesheetTabs->widget(m_pDopesheetTabs->currentIndex()));
	return pSplitter ? pSplitter->widget(1) : nullptr;
}

void CTrackViewSequenceTabWidget::ShowDopeSheet(bool show)
{
	if (show != m_showDopeSheet)
	{
		m_showDopeSheet = show;
		auto dopeSheet = GetCurrentDopeSheetContainer();
		if (dopeSheet)
			dopeSheet->setVisible(show);
	}
}

void CTrackViewSequenceTabWidget::ShowCurveEditor(bool show)
{
	if (show != m_showCurveEditor)
	{
		m_showCurveEditor = show;
		auto curveEditor = GetCurrentCurveEditorContainer();
		if (curveEditor)
			curveEditor->setVisible(show);
	}
}

void CTrackViewSequenceTabWidget::CloseCurrentTab()
{
	if (m_pDopesheetTabs && m_pDopesheetTabs->currentIndex() != -1)
	{
		OnDopeSheetTabClose(m_pDopesheetTabs->currentIndex());
	}
}

void CTrackViewSequenceTabWidget::OnCopyKeys()
{
	if (m_pContextMenuNode && m_pContextMenuNode->GetNodeType() == eTVNT_Track)
	{
		CTrackViewTrack* pTrack = static_cast<CTrackViewTrack*>(m_pContextMenuNode);
		XmlNodeRef copyNode = XmlHelpers::CreateXmlNode("CopyKeysNode");

		pTrack->CopyKeysToClipboard(copyNode, false, true);

		CClipboard clip;
		clip.Put(copyNode, "Track view keys");
	}
}

void CTrackViewSequenceTabWidget::OnCopySelectedKeys()
{
	if (m_pContextMenuNode && m_pContextMenuNode->GetNodeType() == eTVNT_Track)
	{
		CTrackViewTrack* pTrack = static_cast<CTrackViewTrack*>(m_pContextMenuNode);
		XmlNodeRef copyNode = XmlHelpers::CreateXmlNode("CopyKeysNode");

		pTrack->CopyKeysToClipboard(copyNode, true, true);

		CClipboard clip;
		clip.Put(copyNode, "Track view keys");
	}
}

void CTrackViewSequenceTabWidget::OnPasteKeys()
{
	if (m_pContextMenuNode && m_pContextMenuNode->GetNodeType() == eTVNT_Track)
	{
		CClipboard clipboard;
		if (clipboard.IsEmpty())
		{
			return;
		}

		XmlNodeRef trackKeysRoot = clipboard.Get();
		if (trackKeysRoot == NULL || strcmp(trackKeysRoot->getTag(), "CopyKeysNode") != 0)
		{
			return;
		}

		CTrackViewTrack* pTrack = static_cast<CTrackViewTrack*>(m_pContextMenuNode);
		pTrack->PasteKeys(trackKeysRoot, SAnimTime(0));
	}
}

void CTrackViewSequenceTabWidget::OnCopyNode()
{
	if (m_pContextMenuNode)
	{
		CTrackViewAnimNode* pAnimNode = static_cast<CTrackViewAnimNode*>(m_pContextMenuNode);
		pAnimNode->CopyNodesToClipboard(true);
	}
}

void CTrackViewSequenceTabWidget::OnPasteNode()
{
	OnDopesheetPaste(SAnimTime(0), nullptr);
}

void CTrackViewSequenceTabWidget::OnDopesheetPlayPause()
{
	if (!GetTrackViewCore()->IsPlaying())
		GetTrackViewCore()->OnPlayPause(true);
	else
		GetTrackViewCore()->OnPlayPause(GetTrackViewCore()->IsPaused());
}

void CTrackViewSequenceTabWidget::OnDopesheetCopy()
{
	const bool bIsKeySelected = (m_currentKeySelection.size() > 0);
	if (bIsKeySelected)
	{
		GetActiveSequence()->CopyKeysToClipboard(true, false);
	}
	else
	{
		GetActiveSequence()->CopyNodesToClipboard(true);
	}
}

void CTrackViewSequenceTabWidget::OnDopesheetPaste(SAnimTime time, STimelineTrack* pTrack)
{
	CClipboard clip;
	if (!strcmp("Track view entity nodes", clip.GetTitle()))
	{
		CTimeline* pDopesheet = GetActiveDopeSheet();
		if (pDopesheet)
		{
			TSelectedTracks selectedTracks;
			GetSelectedTracks(pDopesheet->Content()->track, selectedTracks);

			CTrackViewNode* pTrackViewNode = GetActiveSequence();
			if (selectedTracks.size() > 0)
			{
				const CryGUID currentTrackGUID = GetGUIDFromDopeSheetTrack(selectedTracks[0]);
				pTrackViewNode = GetActiveSequence()->GetNodeFromGUID(currentTrackGUID);
			}

			CTrackViewAnimNode* pAnimNode = static_cast<CTrackViewAnimNode*>(pTrackViewNode);
			if (pAnimNode)
			{
				pAnimNode->PasteNodesFromClipboard();
			}
		}
	}
	else if (!strcmp("Track view keys", clip.GetTitle()))
	{
		CTimeline* pDopesheet = GetActiveDopeSheet();
		if (pDopesheet)
		{
			const CryGUID currentTrackGUID = GetGUIDFromDopeSheetTrack(pTrack);
			CTrackViewNode* pTrackViewNode = GetActiveSequence()->GetNodeFromGUID(currentTrackGUID);
			if (pTrackViewNode)
			{
				CTrackViewTrack* pTrackViewTrack = static_cast<CTrackViewTrack*>(pTrackViewNode);
				CTrackViewAnimNode* pAnimNode = pTrackViewTrack->GetAnimNode();
				GetActiveSequence()->PasteKeysFromClipboard(pAnimNode, pTrackViewTrack, time);
			}
		}
	}
}

