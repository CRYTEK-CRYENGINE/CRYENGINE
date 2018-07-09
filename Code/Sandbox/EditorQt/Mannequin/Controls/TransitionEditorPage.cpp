// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "TransitionEditorPage.h"

#include <ICryMannequinEditor.h>
#include <CryGame/IGameFramework.h>

#include "../FragmentEditor.h"
#include "../MannKeyPropertiesDlgFE.h"
#include "../MannequinDialog.h"
#include "../MannequinModelViewport.h"
#include "../MannequinNodes.h"
#include "../SequencerSequence.h"
#include "../MannequinPlayback.h"
#include "../FragmentTrack.h"
#include "../SequenceAnalyzerNodes.h"
#include "../MannDebugOptionsDialog.h"

namespace
{
static const float TRANSITION_MIN_TIME_RANGE = 1.0f;
static const int TRANSITION_CHAR_BUFFER_SIZE = 512;
};

IMPLEMENT_DYNAMIC(CTransitionEditorPage, CMannequinEditorPage)

BEGIN_MESSAGE_MAP(CTransitionEditorPage, CMannequinEditorPage)
ON_COMMAND(ID_TV_JUMPSTART, OnGoToStart)
ON_COMMAND(ID_TV_JUMPEND, OnGoToEnd)
ON_COMMAND(ID_TV_PLAY, OnPlay)
ON_COMMAND(ID_PLAY_LOOP, OnLoop)
ON_COMMAND(ID_BTN_TOGGLE_1P, OnToggle1P)
ON_COMMAND(ID_SHOW_DEBUG_OPTIONS, OnShowDebugOptions)
ON_COMMAND(ID_TOGGLE_PREVIEW_UNITS, OnToggleTimelineUnits)
ON_COMMAND(ID_MANN_RELOAD_ANIMS, OnReloadAnimations)
ON_COMMAND(ID_MANN_LOOKAT_TOGGLE, OnToggleLookat)
ON_COMMAND(ID_MANN_ROTATE_LOCATOR, OnClickRotateLocator)
ON_COMMAND(ID_MANN_TRANSLATE_LOCATOR, OnClickTranslateLocator)
ON_COMMAND(ID_MANN_ATTACH_TO_ENTITY, OnToggleAttachToEntity)
ON_COMMAND(ID_MANN_SHOW_SCENE_ROOTS, OnToggleShowSceneRoots)
ON_UPDATE_COMMAND_UI(ID_TV_JUMPSTART, OnUpdateGoToStart)
ON_UPDATE_COMMAND_UI(ID_TV_JUMPEND, OnUpdateGoToEnd)
ON_UPDATE_COMMAND_UI(ID_TV_PLAY, OnUpdatePlay)
ON_UPDATE_COMMAND_UI(ID_PLAY_LOOP, OnUpdateLoop)
ON_NOTIFY(TBN_DROPDOWN, AFX_IDW_TOOLBAR, OnToolbarDropDown)
ON_NOTIFY_EX_RANGE(TTN_NEEDTEXTA, 0, 0xFFFF, OnToolTipText)
ON_WM_SETFOCUS()
END_MESSAGE_MAP()

#define SEQUENCE_FILE_FILTER "Sequence XML Files (*.xml)|*.xml"

//////////////////////////////////////////////////////////////////////////

CTransitionEditorPage::CTransitionEditorPage(CWnd* pParent)
	: CMannequinEditorPage(CTransitionEditorPage::IDD)
	, m_contexts(NULL)
	, m_modelViewport(NULL)
	, m_sequence(NULL)
	, m_seed(0)
	, m_fromID(FRAGMENT_ID_INVALID)
	, m_toID(FRAGMENT_ID_INVALID)
	, m_fromFragTag()
	, m_toFragTag()
	, m_scopeMaskFrom(ACTION_SCOPES_NONE)
	, m_scopeMaskTo(ACTION_SCOPES_NONE)
	, m_hAccelerators(0)
	, m_pViewportWidget(NULL)
{
}

CTransitionEditorPage::~CTransitionEditorPage()
{
	const CString strSection = _T("Settings\\Mannequin\\TransitionEditorLayout");
	CXTRegistryManager regMgr;
	regMgr.WriteProfileInt(strSection, "TimelineUnits", m_wndTrackPanel.GetTickDisplayMode());
	delete m_pViewportWidget;
}

BOOL CTransitionEditorPage::OnInitDialog()
{
	__super::OnInitDialog();

	m_contexts = CMannequinDialog::GetCurrentInstance()->Contexts();

	m_hAccelerators = LoadAccelerators(AfxGetApp()->m_hInstance, MAKEINTRESOURCE(IDR_MANN_ACCELERATOR));

	m_fTime = 0.0f;
	m_fMaxTime = 0.0f;
	m_playSpeed = 1.0f;
	m_bPlay = false;
	m_bLoop = false;
	m_draggingTime = false;
	m_bRefreshingTagCtrl = false;

	m_sequencePlayback = NULL;

	m_tagVars.reset(new CVarBlock());

	ModifyStyleEx(0, WS_EX_CLIENTEDGE);

	SetWindowText("TransitionEditor Page");

	const char* SEQUENCE_NAME = "Transition Preview";
	m_sequence = new CSequencerSequence();
	m_sequence->SetName(SEQUENCE_NAME);

	// Main splitters
	m_wndSplitterVert.CreateStatic(this, 2, 1, WS_CHILD | WS_VISIBLE, AFX_IDW_PANE_FIRST);
	m_wndSplitterVert.ModifyStyle(0, WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
	m_wndSplitterVert.SetRowInfo(0, 600, CMannequinDialog::s_minPanelSize);
	m_wndSplitterVert.SetRowInfo(1, 600, CMannequinDialog::s_minPanelSize);

	m_wndSplitterHorz.CreateStatic(&m_wndSplitterVert, 1, 2, WS_CHILD | WS_VISIBLE, AFX_IDW_PANE_FIRST);
	m_wndSplitterHorz.ModifyStyle(0, WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
	m_wndSplitterHorz.SetColumnInfo(0, 600, CMannequinDialog::s_minPanelSize);
	m_wndSplitterHorz.SetColumnInfo(1, 600, CMannequinDialog::s_minPanelSize);
	m_wndSplitterHorz.SetDlgCtrlID(m_wndSplitterVert.IdFromRowCol(0, 0));

	// Model viewport
	m_modelViewport = new CMannequinModelViewport(eMEM_TransitionEditor);
	m_viewportHost.Create(NULL, NULL, WS_VISIBLE | WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_HSCROLL, CRect(200, 0, 500, 300), this, NULL);
	m_viewportHost.ModifyStyle(WS_POPUP, WS_VISIBLE | WS_CHILD, 0);
	m_viewportHost.SetDlgCtrlID(m_wndSplitterHorz.IdFromRowCol(0, 0));
	m_viewportHost.SetParent(&m_wndSplitterHorz);
	m_viewportHost.SetOwner(this);
	m_modelViewport->SetType(ET_ViewportModel);

	CRect viewRect;
	m_viewportHost.GetClientRect(viewRect);
	m_pViewportWidget = new QMfcViewportHost(&m_viewportHost, m_modelViewport, viewRect);
	m_viewportHost.SetWidgetHost(m_pViewportWidget->GetHostWidget());
	m_viewportHost.ShowWindow(SW_SHOW);

	// Track view
	//m_wndTrackPanel.SetDragAndDrop(false);
	m_wndTrackPanel.Create(NULL, NULL, WS_VISIBLE | WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_HSCROLL, CRect(200, 0, 500, 300), this, NULL);
	m_wndTrackPanel.SetTimeRange(0, m_fMaxTime);
	m_wndTrackPanel.SetTimeScale(100, 0);
	m_wndTrackPanel.SetCurrTime(0);
	m_wndTrackPanel.SetOwner(this);
	m_wndTrackPanel.SetSequence(m_sequence.get());

	// Nodes tree view
	m_wndNodes.Create(WS_VISIBLE | WS_CHILD | WS_TABSTOP, CRect(0, 0, 200, 300), this, NULL);
	m_wndNodes.SetKeyListCtrl(&m_wndTrackPanel);
	m_wndNodes.SetSequence(m_sequence.get());

	// Properties panel
	m_wndKeyProperties.Create(CMannKeyPropertiesDlgFE::IDD, this);
	m_wndKeyProperties.SetKeysCtrl(&m_wndTrackPanel);
	m_wndKeyProperties.SetSequence(m_sequence.get());
	m_wndKeyProperties.ShowWindow(SW_SHOW);

	// 3-way sequencer splitter
	m_wndSplitterTracks.CreateStatic(&m_wndSplitterVert, 1, 3, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, 59748);
	m_wndSplitterTracks.SetColumnInfo(0, 100, CMannequinDialog::s_minPanelSize);
	m_wndSplitterTracks.SetColumnInfo(1, 100, CMannequinDialog::s_minPanelSize);
	m_wndSplitterTracks.SetColumnInfo(2, 100, CMannequinDialog::s_minPanelSize);
	m_wndSplitterTracks.SetPane(0, 0, &m_wndNodes, CSize(200, 200));
	m_wndSplitterTracks.SetPane(0, 1, &m_wndTrackPanel, CSize(800, 200));
	m_wndSplitterTracks.SetPane(0, 2, &m_wndKeyProperties, CSize(200, 200));
	m_wndSplitterTracks.SetDlgCtrlID(m_wndSplitterVert.IdFromRowCol(1, 0));

	// Tags panel
	m_tagsPanel.SetParent(&m_wndSplitterHorz);
	m_tagsPanel.SetTitle("Preview Filter Tags");
	m_tagsPanel.SetDlgCtrlID(m_wndSplitterHorz.IdFromRowCol(0, 1));
	m_tagsPanel.ShowWindow(SW_SHOW);

	// Toolbar
	InitToolbar();

	// Calculate toolbar size
	CRect rcToolbar;
	RepositionBars(AFX_IDW_CONTROLBAR_FIRST, AFX_IDW_CONTROLBAR_LAST, 0, reposQuery, rcToolbar);

	// Position and set resizing properties
	CRect rc;
	GetClientRect(&rc);
	rc.bottom -= rcToolbar.top;
	SetResize(&m_wndSplitterVert, SZ_RESIZE(1), rc);
	GetClientRect(&rc);
	rc.top = rc.bottom - rcToolbar.top;
	SetResize(&m_cDlgToolBar, CXTResizeRect(0, 1, 1, 1), rc);

	return TRUE;
}

void CTransitionEditorPage::InitToolbar()
{
	m_cDlgToolBar.CreateEx(this, TBSTYLE_FLAT,
	                       WS_CHILD | WS_VISIBLE | CBRS_TOP | CBRS_TOOLTIPS | CBRS_FLYBY | CBRS_SIZE_DYNAMIC);
	m_cDlgToolBar.LoadToolBar24(IDR_MANN_TRANSITIONEDITOR_TOOLBAR);
	m_cDlgToolBar.GetToolBarCtrl().SetExtendedStyle(TBSTYLE_EX_DRAWDDARROWS);

	m_cDlgToolBar.InitToolbar();
	m_cDlgToolBar.SetTime(m_fTime, m_wndTrackPanel.GetSnapFps());

	m_cDlgToolBar.SetButtonStyle(m_cDlgToolBar.CommandToIndex(ID_TV_PLAY), TBBS_CHECKBOX | TBSTYLE_DROPDOWN);
	m_cDlgToolBar.SetButtonStyle(m_cDlgToolBar.CommandToIndex(ID_BTN_TOGGLE_1P), TBBS_CHECKBOX);

	m_cDlgToolBar.SetButtonStyle(m_cDlgToolBar.CommandToIndex(ID_MANN_LOOKAT_TOGGLE), TBBS_CHECKBOX);
	m_cDlgToolBar.SetButtonStyle(m_cDlgToolBar.CommandToIndex(ID_MANN_ROTATE_LOCATOR), TBBS_CHECKBOX);
	m_cDlgToolBar.SetButtonStyle(m_cDlgToolBar.CommandToIndex(ID_MANN_TRANSLATE_LOCATOR), TBBS_CHECKBOX);
	m_cDlgToolBar.SetButtonStyle(m_cDlgToolBar.CommandToIndex(ID_MANN_ATTACH_TO_ENTITY), TBBS_CHECKBOX);
	m_cDlgToolBar.SetButtonStyle(m_cDlgToolBar.CommandToIndex(ID_MANN_SHOW_SCENE_ROOTS), TBBS_CHECKBOX);

	ValidateToolbarButtonsState();

	m_cDlgToolBar.CalcFixedLayout(0, TRUE);
}

void CTransitionEditorPage::ValidateToolbarButtonsState()
{
	m_cDlgToolBar.GetToolBarCtrl().CheckButton(ID_MANN_LOOKAT_TOGGLE, m_modelViewport->IsLookingAtCamera());
	m_cDlgToolBar.GetToolBarCtrl().CheckButton(ID_MANN_ROTATE_LOCATOR, !m_modelViewport->IsTranslateLocatorMode());
	m_cDlgToolBar.GetToolBarCtrl().CheckButton(ID_MANN_TRANSLATE_LOCATOR, m_modelViewport->IsTranslateLocatorMode());
	m_cDlgToolBar.GetToolBarCtrl().CheckButton(ID_MANN_ATTACH_TO_ENTITY, m_modelViewport->IsAttachedToEntity());
	m_cDlgToolBar.GetToolBarCtrl().CheckButton(ID_MANN_SHOW_SCENE_ROOTS, m_modelViewport->IsShowingSceneRoots());
	m_cDlgToolBar.GetToolBarCtrl().CheckButton(ID_BTN_TOGGLE_1P, m_modelViewport->IsCameraAttached());
}

BOOL CTransitionEditorPage::PreTranslateMessage(MSG* pMsg)
{
	bool bHandled = false;
	if (pMsg->message >= WM_KEYFIRST && pMsg->message <= WM_KEYLAST && m_hAccelerators)
		bHandled = ::TranslateAccelerator(m_hWnd, m_hAccelerators, pMsg);

	if (!bHandled)
		return CMannequinEditorPage::PreTranslateMessage(pMsg);

	return bHandled;
}

void CTransitionEditorPage::SaveLayout(const XmlNodeRef& xmlLayout)
{
	MannUtils::SaveSplitterToXml(xmlLayout, _T("SequencerSplitter"), m_wndSplitterTracks);
	MannUtils::SaveSplitterToXml(xmlLayout, _T("VerticalSplitter"), m_wndSplitterVert);
	MannUtils::SaveSplitterToXml(xmlLayout, _T("HorizontalSplitter"), m_wndSplitterHorz);
}

void CTransitionEditorPage::LoadLayout(const XmlNodeRef& xmlLayout)
{
	MannUtils::LoadSplitterFromXml(xmlLayout, _T("SequencerSplitter"), m_wndSplitterTracks, CMannequinDialog::s_minPanelSize);
	MannUtils::LoadSplitterFromXml(xmlLayout, _T("VerticalSplitter"), m_wndSplitterVert, CMannequinDialog::s_minPanelSize);
	MannUtils::LoadSplitterFromXml(xmlLayout, _T("HorizontalSplitter"), m_wndSplitterHorz, CMannequinDialog::s_minPanelSize);
}

void CTransitionEditorPage::OnSetFocus(CWnd* pOldWnd)
{
	__super::OnSetFocus(pOldWnd);

	if (auto pMannequinDialog = CMannequinDialog::GetCurrentInstance())
	{
		m_wndNodes.SyncKeyCtrl();

		if (auto pDockingPaneManager = pMannequinDialog->GetDockingPaneManager())
		{
			pDockingPaneManager->ShowPane(CMannequinDialog::IDW_TRANSITIONS_PANE, FALSE);
		}
	}
}

float CTransitionEditorPage::GetMarkerTimeStart() const
{
	return m_wndTrackPanel.GetMarkedTime().start;
}

float CTransitionEditorPage::GetMarkerTimeEnd() const
{
	return m_wndTrackPanel.GetMarkedTime().end;
}

void CTransitionEditorPage::Update()
{
	bool isDraggingTime = m_wndTrackPanel.IsDraggingTime();
	float dsTime = m_wndTrackPanel.GetTime();

	float effectiveSpeed = m_playSpeed;
	if (isDraggingTime)
	{
		m_sequencePlayback->SetTime(dsTime);
		m_fTime = dsTime;
		effectiveSpeed = 0.0f;
	}
	else if (m_draggingTime)
	{
	}

	if (!m_bPlay)
	{
		effectiveSpeed = 0.0f;
	}

	m_cDlgToolBar.SetTime(m_fTime, m_wndTrackPanel.GetSnapFps());

	m_modelViewport->SetPlaybackMultiplier(effectiveSpeed);

	m_draggingTime = isDraggingTime;

	float endTime = GetMarkerTimeEnd();
	if (endTime <= 0.0f)
	{
		endTime = m_fMaxTime;
	}

	m_sequencePlayback->Update(gEnv->pTimer->GetFrameTime() * effectiveSpeed, m_modelViewport);

	m_fTime = m_sequencePlayback->GetTime();

	if (m_fTime > endTime)
	{
		if (m_bLoop)
		{
			const float startTime = GetMarkerTimeStart();

			m_fTime = startTime;
			m_sequencePlayback->SetTime(startTime);

			// Set silent mode off.
			m_bRestartingSequence = false;
		}
		else
		{
			m_bPlay = false;
		}
	}

	m_wndTrackPanel.SetCurrTime(m_fTime);

	if (MannUtils::IsSequenceDirty(m_contexts, m_sequence.get(), m_lastChange, eMEM_TransitionEditor))
	{
		if (!TrackPanel()->IsDragging())
		{
			OnUpdateTV();
		}
	}

	m_wndKeyProperties.OnUpdate();
	m_bRestartingSequence = false;

	// update the button logic
	m_cDlgToolBar.GetToolBarCtrl().CheckButton(ID_MANN_LOOKAT_TOGGLE, m_modelViewport->IsLookingAtCamera());
	m_cDlgToolBar.GetToolBarCtrl().CheckButton(ID_MANN_ROTATE_LOCATOR, !m_modelViewport->IsTranslateLocatorMode());
	m_cDlgToolBar.GetToolBarCtrl().CheckButton(ID_MANN_TRANSLATE_LOCATOR, m_modelViewport->IsTranslateLocatorMode());
	m_cDlgToolBar.GetToolBarCtrl().CheckButton(ID_MANN_ATTACH_TO_ENTITY, m_modelViewport->IsAttachedToEntity());
}

//////////////////////////////////////////////////////////////////////////
void CTransitionEditorPage::OnGoToStart()
{
	m_fTime = 0.0f;
	m_bPlay = false;

	m_sequencePlayback->SetTime(0.0f);
}

//////////////////////////////////////////////////////////////////////////
void CTransitionEditorPage::OnGoToEnd()
{
	m_fTime = m_fMaxTime;
	m_bPlay = false;

	m_sequencePlayback->SetTime(m_sequence->GetTimeRange().end);
}

//////////////////////////////////////////////////////////////////////////
void CTransitionEditorPage::OnPlay()
{
	m_bPlay = !m_bPlay;

	if (m_bPlay)
	{
		float endTime = GetMarkerTimeEnd();
		if (endTime <= 0.0f)
		{
			endTime = m_fMaxTime;
		}
		if (m_fTime > endTime)
		{
			const float startTime = GetMarkerTimeStart();

			m_fTime = startTime;
			m_sequencePlayback->SetTime(startTime);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CTransitionEditorPage::OnLoop()
{
	m_bLoop = !m_bLoop;
}

//////////////////////////////////////////////////////////////////////////
void CTransitionEditorPage::OnToolbarDropDown(NMHDR* pnhdr, LRESULT* plr)
{
	CRect rc;
	CPoint pos;
	GetCursorPos(&pos);
	NMTOOLBAR* pnmtb = (NMTOOLBAR*)pnhdr;
	m_cDlgToolBar.GetToolBarCtrl().GetRect(pnmtb->iItem, rc);
	m_cDlgToolBar.ClientToScreen(rc);
	pos.x = rc.left;
	pos.y = rc.bottom;

	// Switch on button command ids.
	switch (pnmtb->iItem)
	{
	case ID_TV_PLAY:
		OnPlayMenu(pos);
		break;
	default:
		return;
	}
	*plr = TBDDRET_DEFAULT;
}

//////////////////////////////////////////////////////////////////////////
void CTransitionEditorPage::OnPlayMenu(CPoint pos)
{
	struct SScales
	{
		const char* pszText;
		float       fScale;
	};
	SScales Scales[] = {
		{ " 2 ", 2.0f   },
		{ " 1 ", 1.0f   },
		{ "1/2", 0.5f   },
		{ "1/4", 0.25f  },
		{ "1/8", 0.125f },
	};

	int nScales = sizeof(Scales) / sizeof(SScales);
	CMenu menu;
	menu.CreatePopupMenu();
	for (int i = 0; i < nScales; i++)
	{
		int flags = 0;
		if (m_playSpeed == Scales[i].fScale)
			flags |= MF_CHECKED;
		menu.AppendMenu(MF_STRING | flags, i + 1, Scales[i].pszText);
	}

	int cmd = CXTPCommandBars::TrackPopupMenu(&menu,
	                                          TPM_NONOTIFY | TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON, pos.x, pos.y, this, NULL);
	if (cmd >= 1 && cmd <= nScales)
	{
		m_playSpeed = Scales[cmd - 1].fScale;
	}
}

//////////////////////////////////////////////////////////////////////////
void CTransitionEditorPage::OnUpdateGoToStart(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(true);
}

//////////////////////////////////////////////////////////////////////////
void CTransitionEditorPage::OnUpdateGoToEnd(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(true);
}

//////////////////////////////////////////////////////////////////////////
void CTransitionEditorPage::OnUpdatePlay(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(true);
	pCmdUI->SetCheck(m_bPlay ? TRUE : FALSE);
}

//////////////////////////////////////////////////////////////////////////
void CTransitionEditorPage::OnUpdateLoop(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(true);
	pCmdUI->SetCheck(m_bLoop ? TRUE : FALSE);
}

//////////////////////////////////////////////////////////////////////////
void CTransitionEditorPage::OnToggle1P()
{
	m_modelViewport->ToggleCamera();
}

//////////////////////////////////////////////////////////////////////////
void CTransitionEditorPage::OnShowDebugOptions()
{
	CMannequinDebugOptionsDialog dialog(m_modelViewport, this);
	dialog.DoModal();
	m_cDlgToolBar.GetToolBarCtrl().CheckButton(ID_BTN_TOGGLE_1P, m_modelViewport->IsCameraAttached());
}

//////////////////////////////////////////////////////////////////////////
void CTransitionEditorPage::OnToggleTimelineUnits()
{
	// Enum has 2 states, so (oldMode + 1) % 2
	m_wndTrackPanel.SetTickDisplayMode((ESequencerTickMode)((m_wndTrackPanel.GetTickDisplayMode() + 1) & 1));
	if (m_modelViewport)
		m_modelViewport->SetTimelineUnits(m_wndTrackPanel.GetTickDisplayMode());
}

//////////////////////////////////////////////////////////////////////////
void CTransitionEditorPage::OnReloadAnimations()
{
	if (m_modelViewport == NULL)
		return;

	CSequencerUtils::SelectedKeys selectedKeys;
	CSequencerUtils::GetAllKeys(m_wndTrackPanel.GetSequence(), selectedKeys);
	for (int i = 0; i < selectedKeys.keys.size(); ++i)
	{
		CSequencerUtils::SelectedKey& selectedKey = selectedKeys.keys[i];

		const ESequencerParamType paramType = selectedKey.pTrack->GetParameterType();
		if (paramType == SEQUENCER_PARAM_ANIMLAYER)
		{
			IEntity* pEntity = selectedKey.pNode->GetEntity();
			CRY_ASSERT(pEntity != NULL);

			ICharacterInstance* pCharInstance = pEntity->GetCharacter(0);
			CRY_ASSERT(pCharInstance != NULL);

			pCharInstance->GetISkeletonAnim()->StopAnimationsAllLayers();
			pCharInstance->ReloadCHRPARAMS();
		}
	}
	//QT_TODO("Animation Browser needed here for Mannequin")
	/*
	   CAnimationBrowser *pAnimationBrowser = m_modelViewport->GetAnimBrowserPanel();
	   if( pAnimationBrowser != NULL )
	   {
	   pAnimationBrowser->ReloadAnimations();
	   }
	 */

	OnSequenceRestart(0.0f);
	SetTime(0.0f);
}

//////////////////////////////////////////////////////////////////////////
void CTransitionEditorPage::OnToggleLookat()
{
	m_cDlgToolBar.GetToolBarCtrl().CheckButton(ID_MANN_LOOKAT_TOGGLE, m_modelViewport->ToggleLookingAtCamera());
}

//////////////////////////////////////////////////////////////////////////
void CTransitionEditorPage::OnClickRotateLocator()
{
	m_modelViewport->SetLocatorRotateMode();
	m_cDlgToolBar.GetToolBarCtrl().CheckButton(ID_MANN_ROTATE_LOCATOR, !m_modelViewport->IsTranslateLocatorMode());
	m_cDlgToolBar.GetToolBarCtrl().CheckButton(ID_MANN_TRANSLATE_LOCATOR, m_modelViewport->IsTranslateLocatorMode());
}

//////////////////////////////////////////////////////////////////////////
void CTransitionEditorPage::OnClickTranslateLocator()
{
	m_modelViewport->SetLocatorTranslateMode();
	m_cDlgToolBar.GetToolBarCtrl().CheckButton(ID_MANN_ROTATE_LOCATOR, !m_modelViewport->IsTranslateLocatorMode());
	m_cDlgToolBar.GetToolBarCtrl().CheckButton(ID_MANN_TRANSLATE_LOCATOR, m_modelViewport->IsTranslateLocatorMode());
}

//////////////////////////////////////////////////////////////////////////
void CTransitionEditorPage::OnToggleShowSceneRoots()
{
	m_modelViewport->SetShowSceneRoots(!m_modelViewport->IsShowingSceneRoots());
	m_cDlgToolBar.GetToolBarCtrl().CheckButton(ID_MANN_SHOW_SCENE_ROOTS, m_modelViewport->IsShowingSceneRoots());
}

//////////////////////////////////////////////////////////////////////////
void CTransitionEditorPage::OnToggleAttachToEntity()
{
	if (m_modelViewport->IsAttachedToEntity())
	{
		m_modelViewport->DetachFromEntity();
	}
	else
	{
		m_modelViewport->AttachToEntity();
	}
	m_cDlgToolBar.GetToolBarCtrl().CheckButton(ID_MANN_ATTACH_TO_ENTITY, m_modelViewport->IsAttachedToEntity());
}

//////////////////////////////////////////////////////////////////////////
float CTransitionEditorPage::PopulateClipTracks(CSequencerNode* node, const int scopeID)
{
	struct SSelectedKey
	{
		SSelectedKey(ESequencerParamType trackParamType, int layerTrackIdx, int keyIdx)
			: m_trackParamType(trackParamType)
			, m_layerTrackIdx(layerTrackIdx)
			, m_keyIdx(keyIdx)
		{}

		ESequencerParamType m_trackParamType;
		int                 m_layerTrackIdx; // index of track of type m_trackParamType (indexing of each type is separated)
		int                 m_keyIdx;
	};

	typedef std::vector<SSelectedKey> TSelectedKeys;
	TSelectedKeys selectedKeys;
	int layerTrackCount[SEQUENCER_PARAM_TOTAL];
	memset(layerTrackCount, 0, sizeof(layerTrackCount));

	IActionController* pActionController = m_contexts->viewData[eMEM_TransitionEditor].m_pActionController;
	IScope* scope = pActionController->GetScope(scopeID);

	for (uint32 l = 0; l < node->GetTrackCount(); )
	{
		CSequencerTrack* track = node->GetTrackByIndex(l);
		switch (track->GetParameterType())
		{
		case SEQUENCER_PARAM_ANIMLAYER:
		case SEQUENCER_PARAM_PROCLAYER:
			for (int keyIdx = track->GetNumKeys() - 1; keyIdx >= 0; --keyIdx)
			{
				if (track->IsKeySelected(keyIdx))
				{
					selectedKeys.push_back(SSelectedKey(track->GetParameterType(), layerTrackCount[track->GetParameterType()], keyIdx));
				}
			}
			layerTrackCount[track->GetParameterType()]++;

			node->RemoveTrack(track);
			break;
		default:
			l++;
			break;
		}
	}

	CFragmentTrack* fragTrack = (CFragmentTrack*)node->GetTrackForParameter(SEQUENCER_PARAM_FRAGMENTID);
	CTransitionPropertyTrack* propsTrack = (CTransitionPropertyTrack*)node->GetTrackForParameter(SEQUENCER_PARAM_TRANSITIONPROPS);

	//--- Strip transition properties track
	const int numPropKeysInitial = propsTrack->GetNumKeys();
	for (int i = numPropKeysInitial - 1; i >= 0; i--)
	{
		propsTrack->RemoveKey(i);
	}

	const ActionScopes scopeFlag = BIT64(scopeID);

	float maxTime = TRANSITION_MIN_TIME_RANGE;
	SClipTrackContext trackContext;
	trackContext.tagState.globalTags = m_tagState;
	trackContext.context = m_contexts->m_scopeData[scopeID].context[eMEM_TransitionEditor];

	const TagState additionalTags = m_contexts->m_controllerDef->m_scopeDef[scopeID].additionalTags;
	const CTagDefinition& tagDef = m_contexts->m_controllerDef->m_tags;

	const uint32 contextID = scope->GetContextID();

	FragmentID lastfragment = FRAGMENT_ID_INVALID;
	SFragTagState lastTagState;
	float lastTime = 0.0f;           // last fragment's insertion time, in 'track time'
	float lastFragmentDuration = 0.0f;
	bool lastPrimaryInstall = false;

	float motionParams[eMotionParamID_COUNT];
	memset(motionParams, 0, sizeof(motionParams));

	for (uint32 h = 0; h < m_fragmentHistory->m_history.m_items.size(); h++)
	{
		CFragmentHistory::SHistoryItem& item = m_fragmentHistory->m_history.m_items[h];

		if (item.type == CFragmentHistory::SHistoryItem::Tag)
		{
			trackContext.tagState.globalTags = tagDef.GetUnion(m_tagState, item.tagState.globalTags);

			trackContext.context = m_contexts->GetContextDataForID(trackContext.tagState.globalTags, contextID, eMEM_TransitionEditor);
		}
		else if (item.type == CFragmentHistory::SHistoryItem::Param)
		{
			EMotionParamID paramID = MannUtils::GetMotionParam(item.paramName);
			if (paramID != eMotionParamID_COUNT)
			{
				motionParams[paramID] = item.param.value.q.v.x;
			}
		}
		else if (item.type == CFragmentHistory::SHistoryItem::Fragment)
		{
			SFragmentQuery fragQuery(tagDef, item.fragment, SFragTagState(trackContext.tagState.globalTags, item.tagState.fragmentTags), additionalTags, item.fragOptionIdxSel);

			ActionScopes scopeMask = m_contexts->potentiallyActiveScopes & item.installedScopeMask;

			if (scopeMask & BIT64(scopeID))
			{
				const int numFragKeys = fragTrack->GetNumKeys();
				for (int i = 0; i < numFragKeys; i++)
				{
					CFragmentKey fragKey;
					fragTrack->GetKey(i, &fragKey);
					if (fragKey.historyItem == h)
					{
						fragKey.scopeMask = item.installedScopeMask;
						fragTrack->SetKey(i, &fragKey);
					}
				}

				if (trackContext.context && trackContext.context->database)
				{
					float nextTime = -1.0f;
					for (uint32 n = h + 1; n < m_fragmentHistory->m_history.m_items.size(); n++)
					{
						const CFragmentHistory::SHistoryItem& nextItem = m_fragmentHistory->m_history.m_items[n];
						if (nextItem.installedScopeMask & item.installedScopeMask)
						{
							//--- This fragment collides with the current one and so will force a flush of it
							nextTime = nextItem.time - m_fragmentHistory->m_history.m_firstTime;
							break;
						}
					}

					trackContext.historyItem = h;
					trackContext.scopeID = scopeID;

					const float time = item.time - m_fragmentHistory->m_history.m_firstTime;

					uint32 rootScope = 0;
					for (uint32 i = 0; i <= scopeID; i++)
					{
						if ((BIT64(1) & scopeMask) != 0)
						{
							rootScope = i;
							break;
						}
					}

					bool primaryInstall = true;
					//--- If this is not the root scope on a cross-scope fragment
					if (rootScope != scopeID)
					{
						if (additionalTags == TAG_STATE_EMPTY)
						{
							//--- Check to see if it should go on an earlier context
							const uint32 numScopes = pActionController->GetTotalScopes();
							const uint32 contextID = pActionController->GetScope(scopeID)->GetContextID();
							for (uint32 i = 0; (i < scopeID) && primaryInstall; i++)
							{
								const uint32 altContextID = pActionController->GetScope(i)->GetContextID();
								primaryInstall = (contextID != altContextID);
							}
						}
					}
					else
					{
						item.fragOptionIdxSel = item.fragOptionIdx;
						if (item.fragOptionIdx == OPTION_IDX_RANDOM)
						{
							item.fragOptionIdxSel = m_contexts->viewData[eMEM_TransitionEditor].m_pAnimContext->randGenerator.GenerateUint32();
						}
					}

					trackContext.tagState.fragmentTags = item.tagState.fragmentTags;

					CClipKey lastClipKey;
					CSequencerTrack* pSeqTrack = node->GetTrackForParameter(SEQUENCER_PARAM_ANIMLAYER, 0);
					if (pSeqTrack)
					{
						const int numKeys = pSeqTrack->GetNumKeys();
						if (numKeys > 0)
						{
							pSeqTrack->GetKey(numKeys - 1, &lastClipKey);
						}
					}

					FragmentID installFragID = item.fragment;
					const float fragmentTime = item.time - lastTime; // how much time has passed relative to the last fragment's insertion time, so 'fragment time'
					SBlendQuery blendQuery;
					blendQuery.fragmentFrom = lastfragment;
					blendQuery.fragmentTo = installFragID;
					blendQuery.tagStateFrom = lastTagState;
					blendQuery.tagStateTo = fragQuery.tagState;
					blendQuery.fragmentTime = fragmentTime;
					blendQuery.prevNormalisedTime = blendQuery.normalisedTime = lastClipKey.ConvertTimeToNormalisedTime(time);
					blendQuery.additionalTags = additionalTags;
					blendQuery.SetFlag(SBlendQuery::fromInstalled, lastPrimaryInstall);
					blendQuery.SetFlag(SBlendQuery::toInstalled, primaryInstall);
					blendQuery.SetFlag(SBlendQuery::higherPriority, item.trumpsPrevious);
					blendQuery.SetFlag(SBlendQuery::noTransitions, !lastPrimaryInstall);

					blendQuery.forceBlendUid = m_forcedBlendUid;

					SFragmentSelection fragmentSelection;
					SFragmentData fragData;
					const bool hasFragment = trackContext.context->database->Query(fragData, blendQuery, item.fragOptionIdxSel, trackContext.context->animSet, &fragmentSelection);
					MannUtils::AdjustFragDataForVEGs(fragData, *trackContext.context, eMEM_TransitionEditor, motionParams);
					SBlendQueryResult blend1, blend2;
					if (lastPrimaryInstall)
					{
						trackContext.context->database->FindBestBlends(blendQuery, blend1, blend2);
					}
					const SFragmentBlend* pFirstBlend = blend1.pFragmentBlend;
					const bool hasTransition = (pFirstBlend != NULL);
					//ADD STUFF

					float blendStartTime = 0.0f;  // in 'track time'
					float blendSelectTime = 0.0f; // in 'track time'
					float transitionStartTime = m_fragmentHistory->m_history.m_startTime;
					float cycleIncrement = 0.0f;
					if (rootScope == scopeID)
					{
						if (!item.trumpsPrevious)
						{
							// by default the blendStartTime is at the 'end' of the previous fragment
							// (for oneshots lastFragmentDuration is at the end of the fragment,
							// for looping frags it's at the end of the next-to-last clip)
							blendStartTime = lastTime + lastFragmentDuration;
							if (hasTransition)
							{
								if (pFirstBlend->flags & SFragmentBlend::Cyclic)
								{
									// In a cyclic transition treat selectTime as normalised time
									// (we assume it's in the first cycle here and adjust it lateron in the CycleLocked case)
									blendSelectTime = lastClipKey.ConvertUnclampedNormalisedTimeToTime(pFirstBlend->selectTime);

									if (pFirstBlend->flags & SFragmentBlend::CycleLocked)
									{
										// Treat the startTime as normalised time.
										// Figure out which cycle we are currently in, and move
										// startTime & selectTime into the same cycle.

										blendStartTime = lastClipKey.ConvertUnclampedNormalisedTimeToTime(pFirstBlend->startTime);
										const float lastPrimeClipDuration = lastClipKey.GetOneLoopDuration();
										for (; blendStartTime + cycleIncrement <= time; cycleIncrement += lastPrimeClipDuration)
											;

										if (blendSelectTime + cycleIncrement > time)
										{
											cycleIncrement = max(cycleIncrement - lastPrimeClipDuration, 0.0f);
										}

										blendStartTime += cycleIncrement;
										blendSelectTime += cycleIncrement;
									}
								}
								else
								{
									blendStartTime += pFirstBlend->startTime;
									blendSelectTime = lastTime + pFirstBlend->selectTime;
								}
								transitionStartTime = pFirstBlend->startTime;
							}
						}

						item.startTime = blendStartTime + fragData.duration[0]; // in 'track time' (duration[0] should probably be the total time of the transition instead)
					}
					else
					{
						blendStartTime = item.startTime - fragData.duration[0];
					}

					const float insertionTime = max(time, blendStartTime); // in 'track time'

					float lastInsertedClipEndTime = 0.0f;
					const bool isLooping = MannUtils::InsertClipTracksToNode(node, fragData, insertionTime, nextTime, lastInsertedClipEndTime, trackContext);
					maxTime = max(maxTime, lastInsertedClipEndTime);

					float firstBlendDuration = 0.0f;
					bool foundReferenceBlend = false;
					if (fragData.animLayers.size() > 0)
					{
						const uint32 numClips = fragData.animLayers[0].size();
						for (uint32 c = 0; c < numClips; c++)
						{
							const SAnimClip& animClip = fragData.animLayers[0][c];
							if (animClip.part != animClip.blendPart)
							{
								firstBlendDuration = animClip.blend.duration;
								foundReferenceBlend = true;
							}
						}
					}
					if ((fragData.procLayers.size() > 0) && !foundReferenceBlend)
					{
						const uint32 numClips = fragData.procLayers[0].size();
						for (uint32 c = 0; c < numClips; c++)
						{
							const SProceduralEntry& procClip = fragData.procLayers[0][c];
							if (procClip.part != procClip.blendPart)
							{
								firstBlendDuration = procClip.blend.duration;
								foundReferenceBlend = true;
							}
						}
					}

					float transitionTime = insertionTime;
					const float transitionClipEffectiveStartTime = lastClipKey.GetEffectiveAssetStartTime();
					const float transitionLastClipDuration = lastClipKey.GetOneLoopDuration();
					int partID = 0;
					if (blend1.pFragmentBlend)
					{
						const float duration = fragData.duration[partID];
						const int id = propsTrack->CreateKey(transitionTime);
						CTransitionPropertyKey transitionKey;
						propsTrack->GetKey(id, &transitionKey);
						transitionKey.duration = duration;
						if (!blend2.pFragmentBlend)
						{
							transitionKey.duration += firstBlendDuration;
						}
						transitionKey.blend = blend1;
						transitionKey.prop = CTransitionPropertyKey::eTP_Transition;
						transitionKey.prevClipStart = lastTime;
						transitionKey.prevClipDuration = transitionLastClipDuration;
						transitionKey.tranFlags = blend1.pFragmentBlend->flags;
						transitionKey.toHistoryItem = h;
						transitionKey.toFragIndex = partID;
						transitionKey.sharedId = 0;
						propsTrack->SetKey(id, &transitionKey);

						const int idSelect = propsTrack->CreateKey(blendSelectTime);
						CTransitionPropertyKey selectKey;
						propsTrack->GetKey(idSelect, &selectKey);
						selectKey.blend = blend1;
						selectKey.prop = CTransitionPropertyKey::eTP_Select;
						selectKey.refTime = lastTime;
						selectKey.prevClipStart = lastTime;
						selectKey.prevClipDuration = transitionLastClipDuration;
						selectKey.tranFlags = blend1.pFragmentBlend->flags;
						selectKey.toHistoryItem = h;
						selectKey.toFragIndex = partID;
						selectKey.sharedId = 0;
						propsTrack->SetKey(idSelect, &selectKey);

						CTransitionPropertyKey startKey;
						const int idStart = propsTrack->CreateKey(cycleIncrement + lastTime + lastFragmentDuration + transitionStartTime);
						propsTrack->GetKey(idStart, &startKey);
						startKey.blend = blend1;
						startKey.prop = CTransitionPropertyKey::eTP_Start;
						startKey.refTime = lastTime + lastFragmentDuration;
						startKey.duration = transitionTime - startKey.m_time;
						startKey.prevClipStart = lastTime;
						startKey.prevClipDuration = transitionLastClipDuration;
						startKey.tranFlags = blend1.pFragmentBlend->flags;
						startKey.toHistoryItem = h;
						startKey.toFragIndex = partID;
						startKey.sharedId = 0;
						propsTrack->SetKey(idStart, &startKey);
						transitionTime += duration;
						++partID;
					}
					if (blend2.pFragmentBlend)
					{
						const float duration = fragData.duration[partID];
						const int id = propsTrack->CreateKey(transitionTime);
						CTransitionPropertyKey transitionKey;
						propsTrack->GetKey(id, &transitionKey);
						transitionKey.duration = duration + firstBlendDuration;
						transitionKey.blend = blend2;
						transitionKey.prop = CTransitionPropertyKey::eTP_Transition;
						transitionKey.prevClipStart = lastTime;
						transitionKey.prevClipDuration = transitionLastClipDuration;
						transitionKey.tranFlags = blend2.pFragmentBlend->flags;
						transitionKey.toHistoryItem = h;
						transitionKey.toFragIndex = partID;
						transitionKey.sharedId = 1;
						propsTrack->SetKey(id, &transitionKey);

						const int idSelect = propsTrack->CreateKey(blendSelectTime);
						CTransitionPropertyKey selectKey;
						propsTrack->GetKey(idSelect, &selectKey);
						selectKey.blend = blend2;
						selectKey.prop = CTransitionPropertyKey::eTP_Select;
						selectKey.refTime = lastTime;
						selectKey.prevClipStart = lastTime;
						selectKey.prevClipDuration = transitionLastClipDuration;
						selectKey.tranFlags = blend2.pFragmentBlend->flags;
						selectKey.toHistoryItem = h;
						selectKey.toFragIndex = partID;
						selectKey.sharedId = 1;
						propsTrack->SetKey(idSelect, &selectKey);

						const int idStart = propsTrack->CreateKey(cycleIncrement + lastTime + lastFragmentDuration + transitionStartTime);
						CTransitionPropertyKey startKey;
						propsTrack->GetKey(idStart, &startKey);
						startKey.blend = blend2;
						startKey.prop = CTransitionPropertyKey::eTP_Start;
						startKey.refTime = lastTime + lastFragmentDuration;
						startKey.duration = transitionTime - startKey.m_time;
						startKey.prevClipStart = lastTime;
						startKey.prevClipDuration = transitionLastClipDuration;
						startKey.tranFlags = blend2.pFragmentBlend->flags;
						startKey.toHistoryItem = h;
						startKey.toFragIndex = partID;
						startKey.sharedId = 1;
						propsTrack->SetKey(idStart, &startKey);
						transitionTime += duration;
						++partID;
					}

					lastFragmentDuration = ((fragData.duration[partID] + transitionTime) - insertionTime);

					const float fragmentClipDuration = lastInsertedClipEndTime - insertionTime;
					for (int i = 0; i < numFragKeys; i++)
					{
						CFragmentKey fragKey;
						fragTrack->GetKey(i, &fragKey);
						if (fragKey.historyItem == trackContext.historyItem)
						{
							const float duration = fragData.duration[partID];

							fragKey.transitionTime = firstBlendDuration;
							fragKey.tagState = fragmentSelection.tagState;
							fragKey.tagStateFull = fragQuery.tagState;
							fragKey.context = trackContext.context;
							fragKey.clipDuration = fragmentClipDuration;
							fragKey.tranStartTime = blendStartTime;
							fragKey.hasFragment = hasFragment;
							fragKey.isLooping = isLooping;
							fragKey.scopeMask = item.installedScopeMask;
							fragKey.fragIndex = partID;
							fragKey.tranStartTimeValue = transitionStartTime;
							fragTrack->SetKey(i, &fragKey);
						}
					}

					lastPrimaryInstall = primaryInstall;
					lastfragment = installFragID;
					lastTagState = fragQuery.tagState;
					lastTime = insertionTime;
				}
			}
		}
	}

	for (TSelectedKeys::const_iterator it = selectedKeys.begin(); it != selectedKeys.end(); ++it)
	{
		int layerTrackCount = 0;
		for (int l = 0; l < node->GetTrackCount(); ++l)
		{
			CSequencerTrack* pTrack = node->GetTrackByIndex(l);
			if (pTrack->GetParameterType() != it->m_trackParamType)
			{
				continue;
			}

			if (layerTrackCount == it->m_layerTrackIdx)
			{
				if (it->m_keyIdx < pTrack->GetNumKeys())
				{
					pTrack->SelectKey(it->m_keyIdx, true);
				}
			}

			layerTrackCount++;
		}
	}

	return maxTime;
}

//////////////////////////////////////////////////////////////////////////
void CTransitionEditorPage::PopulateAllClipTracks()
{
	if (!m_fragmentHistory.get())
		return;

	m_contexts->viewData[eMEM_TransitionEditor].m_pAnimContext->randGenerator.seed(m_seed);

	uint32 numScopes = m_contexts->m_scopeData.size();
	float maxTime = TRANSITION_MIN_TIME_RANGE;
	for (uint32 i = 0; i < numScopes; i++)
	{
		CSequencerNode* animNode = m_contexts->m_scopeData[i].animNode[eMEM_TransitionEditor];
		if (animNode)
		{
			const float maxTimeCur = PopulateClipTracks(animNode, i);
			maxTime = max(maxTime, maxTimeCur);
		}
	}
	maxTime += 1.0f;
	maxTime = max(maxTime, (m_fragmentHistory->m_history.m_endTime - m_fragmentHistory->m_history.m_firstTime));

	Range timeRange(0.0f, maxTime);
	m_sequence->SetTimeRange(timeRange);
	m_wndTrackPanel.SetTimeRange(0.0f, maxTime);
	m_fMaxTime = maxTime;

	//--- Create locators
	m_modelViewport->ClearLocators();

	const CFragmentHistory& history = m_fragmentHistory->m_history;
	const uint32 numHistoryItems = history.m_items.size();
	for (uint32 h = 0; h < numHistoryItems; h++)
	{
		const CFragmentHistory::SHistoryItem& item = history.m_items[h];

		if ((item.type == CFragmentHistory::SHistoryItem::Param) && item.isLocation)
		{
			m_modelViewport->AddLocator(h, item.paramName, item.param.value);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CTransitionEditorPage::InsertContextHistoryItems(const SScopeData& scopeData, const float keyTime)
{
	for (uint32 h = 0; h < m_contextHistory->m_history.m_items.size(); h++)
	{
		const CFragmentHistory::SHistoryItem& item = m_contextHistory->m_history.m_items[h];

		if (item.type == CFragmentHistory::SHistoryItem::Fragment)
		{
			ActionScopes scopeMask = item.installedScopeMask;
			scopeMask &= scopeData.mannContexts->potentiallyActiveScopes;

			ActionScopes rootScope = ACTION_SCOPES_ALL;
			for (uint32 i = 0; i <= scopeData.scopeID; i++)
			{
				if ((BIT64(i) & scopeMask) != 0)
				{
					rootScope = i;
					break;
				}
			}

			if (scopeData.scopeID == rootScope)
			{
				CFragmentHistory::SHistoryItem newItem(item);
				newItem.time = keyTime;

				m_fragmentHistory->m_history.m_items.push_back(newItem);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CTransitionEditorPage::SetUIFromHistory()
{
	if (!m_fragmentHistory.get())
		return;

	float transitionStartTime = 3.0f; // Default transition start time

	uint32 numScopes = m_contexts->m_scopeData.size();
	m_sequence->RemoveAll();
	m_fragmentHistory->m_tracks.clear();
	if (m_contexts->m_controllerDef)
	{
		CRootNode* rootNode = new CRootNode(m_sequence.get(), *m_contexts->m_controllerDef);
		rootNode->SetName("Globals");
		CTagTrack* tagTrack = (CTagTrack*)rootNode->CreateTrack(SEQUENCER_PARAM_TAGS);
		m_sequence->AddNode(rootNode);

		CParamTrack* paramTrack = (CParamTrack*)rootNode->CreateTrack(SEQUENCER_PARAM_PARAMS);

		m_fragmentHistory->m_tracks.push_back(tagTrack);
		m_fragmentHistory->m_tracks.push_back(paramTrack);

		const uint32 numHistoryItems = m_fragmentHistory->m_history.m_items.size();
		for (uint32 h = 0; h < numHistoryItems; h++)
		{
			const CFragmentHistory::SHistoryItem& item = m_fragmentHistory->m_history.m_items[h];

			if (item.type == CFragmentHistory::SHistoryItem::Tag)
			{
				float time = item.time - m_fragmentHistory->m_history.m_firstTime;
				int newKeyID = tagTrack->CreateKey(time);
				CTagKey newKey;
				newKey.m_time = time;
				newKey.tagState = item.tagState.globalTags;
				tagTrack->SetKey(newKeyID, &newKey);

				if (newKeyID > 0)
				{
					// Store this tag's time as the start of the transition, there should only ever be two tags
					transitionStartTime = time;
				}
			}
			else if (item.type == CFragmentHistory::SHistoryItem::Param)
			{
				float time = item.time - m_fragmentHistory->m_history.m_firstTime;
				int newKeyID = paramTrack->CreateKey(time);
				CParamKey newKey;
				newKey.m_time = time;
				newKey.name = item.paramName;
				newKey.parameter = item.param;
				newKey.isLocation = item.isLocation;
				newKey.historyItem = h;
				paramTrack->SetKey(newKeyID, &newKey);
			}
		}
	}
	for (uint32 i = 0; i < numScopes; i++)
	{
		SScopeData& scopeData = m_contexts->m_scopeData[i];

		if (!(m_scopeMaskFrom & BIT64(i)))
		{
			InsertContextHistoryItems(scopeData, 0.0f);
		}
		else if (!(m_scopeMaskTo & BIT64(i))) // If scope was in "from" then it is already applied - we don't want duplicates
		{
			InsertContextHistoryItems(scopeData, transitionStartTime);
		}

		CScopeNode* animNode = new CScopeNode(m_sequence.get(), &scopeData, eMEM_TransitionEditor);
		m_sequence->AddNode(animNode);

		if (scopeData.context[eMEM_TransitionEditor])
		{
			animNode->SetName(scopeData.name + " (" + scopeData.context[eMEM_TransitionEditor]->name + ")");
		}
		else
		{
			animNode->SetName(scopeData.name + " (none)");
		}

		float maxTime = TRANSITION_MIN_TIME_RANGE;
		MannUtils::InsertFragmentTrackFromHistory(animNode, *m_fragmentHistory, 0.0f, 0.0f, maxTime, scopeData);
		scopeData.fragTrack[eMEM_TransitionEditor] = (CFragmentTrack*)animNode->GetTrackForParameter(SEQUENCER_PARAM_FRAGMENTID);
		scopeData.animNode[eMEM_TransitionEditor] = animNode;
	}

	PopulateAllClipTracks();

	m_wndNodes.SetSequence(m_sequence.get());

	if (m_sequencePlayback)
	{
		m_sequencePlayback->SetTime(m_fTime);
	}
}

//////////////////////////////////////////////////////////////////////////
void CTransitionEditorPage::SetHistoryFromUI()
{
	if (!m_fragmentHistory.get())
		return;
	MannUtils::GetHistoryFromTracks(*m_fragmentHistory);
	m_fragmentHistory->m_history.UpdateScopeMasks(m_contexts, m_tagState);

	Range range = m_sequence->GetTimeRange();
	m_fragmentHistory->m_history.m_startTime = range.start;
	m_fragmentHistory->m_history.m_endTime = range.end;
}

//////////////////////////////////////////////////////////////////////////
void CTransitionEditorPage::OnUpdateTV(bool forceUpdate)
{
	IMannequin& mannequinSys = gEnv->pGameFramework->GetMannequinInterface();
	bool anyChanges = false;

	const uint32 numScopes = m_contexts->m_scopeData.size();
	for (uint32 i = 0; i < numScopes; i++)
	{
		uint32 clipChangeCount = 0;
		uint32 fragChangeCount = 0;
		uint32 propChangeCount = 0;

		const SScopeData& scopeData = m_contexts->m_scopeData[i];
		CFragmentTrack* pFragTrack = NULL;
		CTransitionPropertyTrack* pPropsTrack = NULL;
		if (scopeData.animNode[eMEM_TransitionEditor])
		{
			const uint32 numTracks = scopeData.animNode[eMEM_TransitionEditor]->GetTrackCount();

			for (uint32 t = 0; t < numTracks; t++)
			{
				CSequencerTrack* seqTrack = scopeData.animNode[eMEM_TransitionEditor]->GetTrackByIndex(t);
				switch (seqTrack->GetParameterType())
				{
				case SEQUENCER_PARAM_ANIMLAYER:
				case SEQUENCER_PARAM_PROCLAYER:
					clipChangeCount += seqTrack->GetChangeCount();
					break;
				case SEQUENCER_PARAM_FRAGMENTID:
					pFragTrack = (CFragmentTrack*)seqTrack;
					fragChangeCount = pFragTrack->GetChangeCount();
					break;
				case SEQUENCER_PARAM_TRANSITIONPROPS:
					pPropsTrack = (CTransitionPropertyTrack*)seqTrack;
					propChangeCount = pPropsTrack->GetChangeCount();
					break;
				}
			}
		}

		// If there have been changes this time, take note
		if (forceUpdate || clipChangeCount || fragChangeCount || propChangeCount)
		{
			anyChanges = true;
		}

		//--- Transition properties have changed, rebuild any transitions
		if ((clipChangeCount || propChangeCount) && pPropsTrack)
		{
			std::vector<int> overriddenBlends;

			const int numKeys = pPropsTrack->GetNumKeys();
			for (uint32 k = 0; k < numKeys; ++k)
			{
				CTransitionPropertyKey key;
				pPropsTrack->GetKey(k, &key);
				if (std::find(overriddenBlends.begin(), overriddenBlends.end(), key.sharedId) != overriddenBlends.end())
				{
					// Have hit a key pointing at this blend which is overriding the other properties
					continue;
				}
				const SFragmentBlend* blend = pPropsTrack->GetBlendForKey(k);
				if (blend)
				{
					bool updated = false;

					SFragmentBlend blendCopy = *blend;

					if (propChangeCount)
					{
						if (key.overrideProps)
						{
							// This key was updated from the properties panel
							// Copy properties from it then ignore other keys for the same blend
							updated = true;
							overriddenBlends.push_back(key.sharedId);
							key.overrideProps = false;

							// Copy values directly from the key
							blendCopy.flags = key.tranFlags;
							blendCopy.selectTime = max(0.f, key.overrideSelectTime);
							if (key.tranFlags & SFragmentBlend::Cyclic)
							{
								blendCopy.startTime = key.overrideStartTime - (float)(int)key.overrideStartTime;
							}
							else
							{
								blendCopy.startTime = min(0.f, key.overrideStartTime);
							}
							CryLogAlways("Set times from GUI: %f %f", blendCopy.selectTime, blendCopy.startTime);
						}
						else if (key.prop == CTransitionPropertyKey::eTP_Select)
						{
							// This is a select time key, calculate new select time from the key position
							updated = true;
							if (key.tranFlags & SFragmentBlend::Cyclic)
							{
								//clipSelectTime = (fragKey.tranSelectTime - fragKey.tranLastClipEffectiveStart) / fragKey.tranLastClipDuration;
								blendCopy.selectTime = (key.m_time - key.prevClipStart) / key.prevClipDuration;
								CryLogAlways("Pre-clip select time: %f", blendCopy.selectTime);
								blendCopy.selectTime -= (float)(int)blendCopy.selectTime;
								CryLogAlways("Set select time from key %f (%f - %f) / %f", blendCopy.selectTime, key.m_time, key.prevClipStart, key.prevClipDuration);
							}
							else
							{
								blendCopy.selectTime = key.m_time - key.prevClipStart;
								CryLogAlways("Set select time from key %f (%f - %f)", blendCopy.selectTime, key.m_time, key.prevClipStart);
							}
						}
						else if (key.prop == CTransitionPropertyKey::eTP_Start)
						{
							// This is a start time key, calculate new start time from the key position
							updated = true;
							if (key.tranFlags & SFragmentBlend::Cyclic)
							{
								//clipStartTime  = (tranStartTime - fragKey.tranLastClipEffectiveStart) / fragKey.tranLastClipDuration;
								blendCopy.startTime = (key.m_time - key.refTime) / key.prevClipDuration;
								CryLogAlways("Pre-clip start time: %f", blendCopy.startTime);
								blendCopy.startTime -= (float)(int)blendCopy.startTime;
								CryLogAlways("Set start time from key %f (%f - %f) / %f", blendCopy.startTime, key.refTime, key.m_time, key.prevClipDuration);
							}
							else
							{
								blendCopy.startTime = min(0.0f, key.m_time - key.refTime);
								CryLogAlways("Set start time from key %f (%f - %f)", blendCopy.startTime, key.m_time, key.refTime);
							}
						}
					}

					CFragment fragmentNew;
					if (clipChangeCount && key.prop == CTransitionPropertyKey::eTP_Transition)
					{
						MannUtils::GetFragmentFromClipTracks(fragmentNew, scopeData.animNode[eMEM_TransitionEditor], key.toHistoryItem, key.m_time, key.toFragIndex);
						blendCopy.pFragment = &fragmentNew;
						updated = true;
					}

					if (updated)
					{
						pPropsTrack->UpdateBlendForKey(k, blendCopy);
					}
				}
				CRY_ASSERT(blend);
			}
		}

		//--- Clip has changed, rebuild any transition fragments
		if ((clipChangeCount || fragChangeCount) && pFragTrack)
		{
			const int numKeys = pFragTrack->GetNumKeys();
			float lastTime = 0.0f;
			for (uint32 k = 0; k < numKeys; k++)
			{
				CFragmentKey fragKey;
				pFragTrack->GetKey(k, &fragKey);

				if (fragKey.transition)
				{
					CRY_ASSERT(fragKey.context);

					bool alreadyDoneTransition = false;
					if (fragKey.sharedID != 0)
					{
						for (uint32 s = 0, mask = 1; s < i; s++, mask <<= 1)
						{
							if (fragKey.scopeMask & mask)
							{
								alreadyDoneTransition = true;
								break;
							}
						}
					}

					const float tranStartTime = fragKey.m_time;
					int prevKeyID = pFragTrack->GetPrecedingFragmentKey(k, true);
					const bool hasPrevClip = (prevKeyID >= 0);
					const SFragmentBlend* pFragmentBlend = fragKey.context->database->GetBlend(fragKey.tranFragFrom, fragKey.tranFragTo, fragKey.tranTagFrom, fragKey.tranTagTo, fragKey.tranBlendUid);
					if (pFragmentBlend)
					{
						float clipSelectTime, clipStartTime;

						if (hasPrevClip && !alreadyDoneTransition)
						{
							CFragmentKey prevFragKey;
							pFragTrack->GetKey(prevKeyID, &prevFragKey);

							if (fragKey.tranFlags & SFragmentBlend::Cyclic)
							{
								clipSelectTime = (fragKey.tranSelectTime - fragKey.tranLastClipEffectiveStart) / fragKey.tranLastClipDuration;
								clipStartTime = (tranStartTime - fragKey.tranLastClipEffectiveStart) / fragKey.tranLastClipDuration;
								clipSelectTime -= (float)(int)clipSelectTime;
								clipStartTime -= (float)(int)clipStartTime;
							}
							else
							{
								clipSelectTime = fragKey.tranSelectTime - prevFragKey.m_time;
								clipStartTime = min(tranStartTime - prevFragKey.clipDuration - max(prevFragKey.tranStartTime, prevFragKey.m_time), 0.0f);
							}
						}
						else
						{
							clipSelectTime = pFragmentBlend->selectTime;
							clipStartTime = pFragmentBlend->startTime;
						}
						clipStartTime -= fragKey.tranStartTimeRelative;

						if (clipChangeCount)
						{
							CFragment fragmentNew;
							MannUtils::GetFragmentFromClipTracks(fragmentNew, scopeData.animNode[eMEM_TransitionEditor], fragKey.historyItem, tranStartTime, fragKey.fragIndex);
							SFragmentBlend fragBlend;
							fragBlend.pFragment = &fragmentNew;
							fragBlend.enterTime = 0.0f;
							fragBlend.selectTime = clipSelectTime;
							fragBlend.startTime = clipStartTime;
							fragBlend.flags = fragKey.tranFlags;
							MannUtils::GetMannequinEditorManager().SetBlend(fragKey.context->database, fragKey.tranFragFrom, fragKey.tranFragTo, fragKey.tranTagFrom, fragKey.tranTagTo, fragKey.tranBlendUid, fragBlend);

							CMannequinDialog& mannDialog = *CMannequinDialog::GetCurrentInstance();
							mannDialog.TransitionBrowser()->Refresh();
						}
						else if (hasPrevClip)
						{
							SFragmentBlend fragBlendNew = *pFragmentBlend;
							fragBlendNew.selectTime = clipSelectTime;
							fragBlendNew.startTime = clipStartTime;
							fragBlendNew.flags = fragKey.tranFlags;
							MannUtils::GetMannequinEditorManager().SetBlend(fragKey.context->database, fragKey.tranFragFrom, fragKey.tranFragTo, fragKey.tranTagFrom, fragKey.tranTagTo, fragKey.tranBlendUid, fragBlendNew);

							CMannequinDialog& mannDialog = *CMannequinDialog::GetCurrentInstance();
							mannDialog.TransitionBrowser()->Refresh();
						}
					}
				}
			}
		}
	}

	if (anyChanges)
	{
		SetHistoryFromUI();

		PopulateAllClipTracks();
	}

	if (m_sequence.get())
		m_wndNodes.SetSequence(m_sequence.get());

	if (m_sequencePlayback)
	{
		m_sequencePlayback->SetTime(m_fTime, true);
	}

	//--- Reset the track based change counts
	const uint32 numNodes = m_sequence->GetNodeCount();
	for (uint32 i = 0; i < numNodes; i++)
	{
		CSequencerNode* seqNode = m_sequence->GetNode(i);
		const uint32 numTracks = seqNode->GetTrackCount();
		for (uint32 t = 0; t < numTracks; t++)
		{
			CSequencerTrack* seqTrack = seqNode->GetTrackByIndex(t);
			seqTrack->ResetChangeCount();
		}
	}
}

void CTransitionEditorPage::LoadSequence(const int scopeContextIdx, const FragmentID fromID, const FragmentID toID, const SFragTagState fromFragTag, const SFragTagState toFragTag, const SFragmentBlendUid blendUid)
{
	m_fromID = fromID;
	m_toID = toID;
	m_fromFragTag = fromFragTag;
	m_toFragTag = toFragTag;
	m_scopeMaskFrom = m_contexts->m_controllerDef->GetScopeMask(fromID, fromFragTag);
	m_scopeMaskTo = m_contexts->m_controllerDef->GetScopeMask(toID, toFragTag);

	m_forcedBlendUid = blendUid;

	// load the blend into the history
	m_fragmentHistory->m_history.LoadSequence(scopeContextIdx, fromID, toID, fromFragTag, toFragTag, blendUid, m_tagState);

	// this sets the keys onto each scope (track?)
	m_fragmentHistory->m_history.UpdateScopeMasks(m_contexts, m_tagState);

	m_sequencePlayback->SetTime(0.0f);

	// this goes away and populates everything like magic
	SetUIFromHistory();

	m_wndNodes.SetSequence(m_sequence.get());
	m_wndKeyProperties.SetSequence(m_sequence.get());

	PopulateTagList();

	OnUpdateTV(true);
}

//////////////////////////////////////////////////////////////////////////
void CTransitionEditorPage::ClearSequence()
{
	if (m_sequencePlayback != NULL)
	{
		m_sequencePlayback->SetTime(0.0f);
	}

	m_wndNodes.SetSequence(NULL);
	m_wndKeyProperties.SetSequence(NULL);
}

//////////////////////////////////////////////////////////////////////////
void CTransitionEditorPage::OnSequenceRestart(float newTime)
{
	m_modelViewport->OnSequenceRestart(newTime);
	m_bRestartingSequence = true;
}

//////////////////////////////////////////////////////////////////////////
void CTransitionEditorPage::PopulateTagList()
{
	// Make all tags that are used in the transition (from or to) non-zero
	SFragTagState transitionTags;
	transitionTags.globalTags = m_fromFragTag.globalTags | m_toFragTag.globalTags;
	transitionTags.fragmentTags = m_fromFragTag.fragmentTags | m_toFragTag.fragmentTags;

	// Store the current preview tag state
	const CTagDefinition& tagDefs = m_contexts->m_controllerDef->m_tags;
	const TagState globalTags = m_contexts->viewData[eMEM_TransitionEditor].m_pAnimContext->state.GetMask();

	SFragTagState usedTags(TAG_STATE_EMPTY, TAG_STATE_EMPTY);

	// Subtract scope tags from the transition tags
	const uint32 numScopes = m_contexts->m_controllerDef->m_scopeDef.size();
	SFragTagState queryGlobalTags = transitionTags;
	for (uint32 i = 0; i < numScopes; i++)
	{
		queryGlobalTags.globalTags = m_contexts->m_controllerDef->m_tags.GetDifference(queryGlobalTags.globalTags, m_contexts->m_controllerDef->m_scopeDef[i].additionalTags);
	}

	// Determine which tags are used in the history
	const CFragmentHistory& history = m_fragmentHistory->m_history;
	const uint32 numItems = history.m_items.size();
	for (uint32 i = 0; i < numItems; i++)
	{
		const CFragmentHistory::SHistoryItem& item = history.m_items[i];
		FragmentID fragID = item.fragment;

		if (fragID != FRAGMENT_ID_INVALID)
		{
			const uint32 numContexts = m_contexts->m_contextData.size();
			for (uint32 c = 0; c < numContexts; c++)
			{
				const SScopeContextData& context = m_contexts->m_contextData[c];
				if (context.database)
				{
					SFragTagState contextUsedTags;
					context.database->QueryUsedTags(fragID, queryGlobalTags, contextUsedTags);

					usedTags.globalTags = m_contexts->m_controllerDef->m_tags.GetUnion(usedTags.globalTags, contextUsedTags.globalTags);
				}
			}
		}
	}

	// Enable tags that are used in the history but not forced by the transition tags
	TagState enabledControls = m_contexts->m_controllerDef->m_tags.GetDifference(usedTags.globalTags, transitionTags.globalTags, true);

	// Update the properties pane with the relevant tags
	m_tagVars->DeleteAllVariables();
	m_tagControls.Init(m_tagVars, m_contexts->viewData[eMEM_TransitionEditor].m_pAnimContext->state.GetDef(), enabledControls);

	m_tagsPanel.DeleteVars();
	m_tagsPanel.SetVarBlock(m_tagVars.get(), functor(*this, &CTransitionEditorPage::OnInternalVariableChange), false);

	// set this up with the initial values we've already got
	SFragTagState fragTagState;
	fragTagState.globalTags = m_tagState;
	SetTagStateOnCtrl(fragTagState);
}

//////////////////////////////////////////////////////////////////////////
void CTransitionEditorPage::OnInternalVariableChange(IVariable* pVar)
{
	if (!m_bRefreshingTagCtrl)
	{
		SFragTagState tagState = GetTagStateFromCtrl();

		const uint32 numContexts = m_contexts->m_controllerDef->m_scopeContexts.GetNum();
		for (uint32 i = 0; i < numContexts; i++)
		{
			const SScopeContextData* contextData = m_contexts->GetContextDataForID(tagState.globalTags, i, eMEM_TransitionEditor);
			if (contextData)
			{
				const bool res = CMannequinDialog::GetCurrentInstance()->EnableContextData(eMEM_TransitionEditor, contextData->dataID);
				if (!res)
				{
					// Write out error msg via MessageBox
					MessageBox("Cannot set that value as it has no parent context.\nIs it a scope/barrel attachment that needs a gun tag setting first?", "Mannequin Error");

					// reset the displayed info?
					CMannequinDialog::GetCurrentInstance()->ClearContextData(eMEM_TransitionEditor, i);
					tagState.globalTags = m_tagState;
					SetTagStateOnCtrl(tagState);
				}
			}
			else
			{
				CMannequinDialog::GetCurrentInstance()->ClearContextData(eMEM_TransitionEditor, i);
			}
		}

		SetUIFromHistory();

		m_tagState = tagState.globalTags;
		m_contexts->viewData[eMEM_TransitionEditor].m_pAnimContext->state = tagState.globalTags;
		OnUpdateTV();
	}
}

//////////////////////////////////////////////////////////////////////////
void CTransitionEditorPage::ResetForcedBlend()
{
	m_forcedBlendUid = SFragmentBlendUid();
}

//////////////////////////////////////////////////////////////////////////
void CTransitionEditorPage::SelectFirstKey()
{
	if (!m_wndTrackPanel.SelectFirstKey(SEQUENCER_PARAM_TRANSITIONPROPS))
	{
		if (!m_wndTrackPanel.SelectFirstKey(SEQUENCER_PARAM_ANIMLAYER))
		{
			if (!m_wndTrackPanel.SelectFirstKey(SEQUENCER_PARAM_PROCLAYER))
			{
				m_wndTrackPanel.SelectFirstKey();
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CTransitionEditorPage::SetTagStateOnCtrl(const SFragTagState& tagState)
{
	m_bRefreshingTagCtrl = true;
	m_tagControls.Set(tagState.globalTags);
	m_bRefreshingTagCtrl = false;
}

//////////////////////////////////////////////////////////////////////////
SFragTagState CTransitionEditorPage::GetTagStateFromCtrl() const
{
	SFragTagState ret;
	ret.globalTags = m_tagControls.Get();

	return ret;
}

//////////////////////////////////////////////////////////////////////////
void CTransitionEditorPage::InitialiseToPreviewFile(const XmlNodeRef& xmlSequenceRoot)
{
	ResetForcedBlend();

	m_modelViewport->SetActionController(m_contexts->viewData[eMEM_TransitionEditor].m_pActionController);

	m_contextHistory.reset(new SFragmentHistoryContext(*m_contexts->viewData[eMEM_TransitionEditor].m_pActionController));
	m_contextHistory->m_history.LoadSequence(xmlSequenceRoot);
	m_contextHistory->m_history.UpdateScopeMasks(m_contexts, m_tagState);

	m_fragmentHistory.reset(new SFragmentHistoryContext(*m_contexts->viewData[eMEM_TransitionEditor].m_pActionController));

	m_tagState = m_contexts->viewData[eMEM_TransitionEditor].m_pAnimContext->state.GetMask();

	m_fMaxTime = m_fragmentHistory->m_history.m_endTime - m_fragmentHistory->m_history.m_firstTime;

	m_sequencePlayback = new CFragmentSequencePlayback(m_fragmentHistory->m_history, *m_contexts->viewData[eMEM_TransitionEditor].m_pActionController, eMEM_TransitionEditor);
	//m_sequencePlayback = new CActionSequencePlayback(ACTPreviewFileION_SCOPES_ALL, 0, 0, m_fragmentHistory->m_history, true);
	//m_sequencePlayback->SetMaxTime(m_fMaxTime);
	//m_contexts->viewData[eMEM_TransitionEditor].m_pActionController->Queue(m_sequencePlayback);

	SetUIFromHistory();
	PopulateTagList();
}

//////////////////////////////////////////////////////////////////////////
void CTransitionEditorPage::SetTagState(const TagState& tagState)
{
	const CTagDefinition& tagDef = m_contexts->m_controllerDef->m_tags;
	TagState tsNew = tagDef.GetUnion(m_tagState, tagState);
	m_contexts->viewData[eMEM_TransitionEditor].m_pAnimContext->state = tsNew;

	//--- Updated associated contexts
	const uint32 numContexts = m_contexts->m_controllerDef->m_scopeContexts.GetNum();
	for (uint32 i = 0; i < numContexts; i++)
	{
		const SScopeContextData* contextData = m_contexts->GetContextDataForID(tsNew, i, eMEM_TransitionEditor);
		if (contextData)
		{
			CMannequinDialog::GetCurrentInstance()->EnableContextData(eMEM_TransitionEditor, contextData->dataID);
		}
		else
		{
			CMannequinDialog::GetCurrentInstance()->ClearContextData(eMEM_TransitionEditor, i);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CTransitionEditorPage::SetTime(float fTime)
{
	if (m_sequencePlayback != NULL)
	{
		m_sequencePlayback->SetTime(fTime, true);
	}

	m_wndTrackPanel.SetCurrTime(fTime, true);
}

