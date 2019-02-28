// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PreviewerPage.h"

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
static const float MIN_TIME_RANGE = 1.0f;
static const int CHAR_BUFFER_SIZE = 512;
};

IMPLEMENT_DYNAMIC(CPreviewerPage, CMannequinEditorPage)

BEGIN_MESSAGE_MAP(CPreviewerPage, CMannequinEditorPage)
ON_COMMAND(ID_TV_JUMPSTART, OnGoToStart)
ON_COMMAND(ID_TV_JUMPEND, OnGoToEnd)
ON_COMMAND(ID_TV_PLAY, OnPlay)
ON_COMMAND(ID_PLAY_LOOP, OnLoop)
ON_COMMAND(ID_BTN_TOGGLE_1P, OnToggle1P)
ON_COMMAND(ID_SHOW_DEBUG_OPTIONS, OnShowDebugOptions)
ON_COMMAND(ID_PREVIEWER_NEWSEQUENCE, OnNewSequence)
ON_COMMAND(ID_PREVIEWER_LOADSEQUENCE, OnLoadSequence)
ON_COMMAND(ID_PREVIEWER_SAVESEQUENCE, OnSaveSequence)
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
ON_UPDATE_COMMAND_UI(ID_PREVIEWER_NEWSEQUENCE, OnUpdateNewSequence)
ON_UPDATE_COMMAND_UI(ID_PREVIEWER_LOADSEQUENCE, OnUpdateLoadSequence)
ON_UPDATE_COMMAND_UI(ID_PREVIEWER_SAVESEQUENCE, OnUpdateSaveSequence)
ON_NOTIFY(TBN_DROPDOWN, AFX_IDW_TOOLBAR, OnToolbarDropDown)
ON_NOTIFY_EX_RANGE(TTN_NEEDTEXTA, 0, 0xFFFF, OnToolTipText)
ON_WM_SETFOCUS()
END_MESSAGE_MAP()

// returns a game root relative path like "/C3/Animations/FragmentSequences/"
CString GetFragmentSequenceFolder()
{
	ICVar* pSequencePathCVar = gEnv->pConsole->GetCVar("mn_sequence_path");
	return CString(PathUtil::Make(PathUtil::GetGameFolder(), (pSequencePathCVar ? pSequencePathCVar->GetString() : "")).c_str());
}

#define SEQUENCE_FILE_FILTER "Sequence XML Files (*.xml)|*.xml"

//////////////////////////////////////////////////////////////////////////

CPreviewerPage::CPreviewerPage(CWnd* pParent)
	: CMannequinEditorPage(CPreviewerPage::IDD),
	m_contexts(NULL),
	m_modelViewport(NULL),
	m_sequence(NULL),
	m_seed(0),
	m_hAccelerators(0),
	m_pViewportWidget(NULL)
{
}

CPreviewerPage::~CPreviewerPage()
{
	const CString strSection = _T("Settings\\Mannequin\\PreviewerLayout");
	CXTRegistryManager regMgr;
	regMgr.WriteProfileInt(strSection, "TimelineUnits", m_wndTrackPanel.GetTickDisplayMode());
	delete m_pViewportWidget;
}

BOOL CPreviewerPage::OnInitDialog()
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

	SetWindowText("Previewer Page");

	const char* SEQUENCE_NAME = "Preview";
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
	m_viewportHost.Create(NULL, NULL, WS_VISIBLE | WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_HSCROLL, CRect(200, 0, 500, 300), this, NULL);
	m_viewportHost.ModifyStyle(WS_POPUP, WS_VISIBLE | WS_CHILD, 0);
	m_viewportHost.SetDlgCtrlID(m_wndSplitterHorz.IdFromRowCol(0, 0));
	m_viewportHost.SetParent(&m_wndSplitterHorz);
	m_viewportHost.SetOwner(this);
	m_viewportHost.ShowWindow(SW_SHOW);

	CRect viewRect;
	m_viewportHost.GetClientRect(viewRect);
	m_modelViewport = new CMannequinModelViewport(eMEM_Previewer);
	m_modelViewport->SetType(ET_ViewportModel);

	m_pViewportWidget = new QMfcViewportHost(&m_viewportHost, m_modelViewport, viewRect);
	m_viewportHost.SetWidgetHost(m_pViewportWidget->GetHostWidget());

	// Track view
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

	// Set up timeline units
	const CString strSection = _T("Settings\\Mannequin\\PreviewerLayout");
	CXTRegistryManager regMgr;

	int nTimelineUnits = regMgr.GetProfileInt(strSection, "TimelineUnits", m_wndTrackPanel.GetTickDisplayMode());
	if (nTimelineUnits != m_wndTrackPanel.GetTickDisplayMode())
	{
		OnToggleTimelineUnits();
	}

	return TRUE;
}

void CPreviewerPage::InitToolbar()
{
	m_cDlgToolBar.CreateEx(this, TBSTYLE_FLAT,
	                       WS_CHILD | WS_VISIBLE | CBRS_TOP | CBRS_TOOLTIPS | CBRS_FLYBY | CBRS_SIZE_DYNAMIC);
	m_cDlgToolBar.LoadToolBar24(IDR_MANN_PREVIEWER_TOOLBAR);
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

void CPreviewerPage::ValidateToolbarButtonsState()
{
	m_cDlgToolBar.GetToolBarCtrl().CheckButton(ID_MANN_LOOKAT_TOGGLE, m_modelViewport->IsLookingAtCamera());
	m_cDlgToolBar.GetToolBarCtrl().CheckButton(ID_MANN_ROTATE_LOCATOR, !m_modelViewport->IsTranslateLocatorMode());
	m_cDlgToolBar.GetToolBarCtrl().CheckButton(ID_MANN_TRANSLATE_LOCATOR, m_modelViewport->IsTranslateLocatorMode());
	m_cDlgToolBar.GetToolBarCtrl().CheckButton(ID_MANN_ATTACH_TO_ENTITY, m_modelViewport->IsAttachedToEntity());
	m_cDlgToolBar.GetToolBarCtrl().CheckButton(ID_MANN_SHOW_SCENE_ROOTS, m_modelViewport->IsShowingSceneRoots());
	m_cDlgToolBar.GetToolBarCtrl().CheckButton(ID_BTN_TOGGLE_1P, m_modelViewport->IsCameraAttached());
}

BOOL CPreviewerPage::PreTranslateMessage(MSG* pMsg)
{
	bool bHandled = false;
	if (pMsg->message >= WM_KEYFIRST && pMsg->message <= WM_KEYLAST && m_hAccelerators)
		bHandled = ::TranslateAccelerator(m_hWnd, m_hAccelerators, pMsg);

	if (!bHandled)
		return CMannequinEditorPage::PreTranslateMessage(pMsg);

	return bHandled;
}

void CPreviewerPage::SaveLayout(const XmlNodeRef& xmlLayout)
{
	MannUtils::SaveSplitterToXml(xmlLayout, _T("SequencerSplitter"), m_wndSplitterTracks);
	MannUtils::SaveSplitterToXml(xmlLayout, _T("VerticalSplitter"), m_wndSplitterVert);
	MannUtils::SaveSplitterToXml(xmlLayout, _T("HorizontalSplitter"), m_wndSplitterHorz);
}

void CPreviewerPage::LoadLayout(const XmlNodeRef& xmlLayout)
{
	MannUtils::LoadSplitterFromXml(xmlLayout, _T("SequencerSplitter"), m_wndSplitterTracks, CMannequinDialog::s_minPanelSize);
	MannUtils::LoadSplitterFromXml(xmlLayout, _T("VerticalSplitter"), m_wndSplitterVert, CMannequinDialog::s_minPanelSize);
	MannUtils::LoadSplitterFromXml(xmlLayout, _T("HorizontalSplitter"), m_wndSplitterHorz, CMannequinDialog::s_minPanelSize);
}

void CPreviewerPage::OnSetFocus(CWnd* pOldWnd)
{
	__super::OnSetFocus(pOldWnd);
	m_wndNodes.SyncKeyCtrl();
}

float CPreviewerPage::GetMarkerTimeStart() const
{
	return m_wndTrackPanel.GetMarkedTime().start;
}

float CPreviewerPage::GetMarkerTimeEnd() const
{
	return m_wndTrackPanel.GetMarkedTime().end;
}

void CPreviewerPage::Update()
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

	if (MannUtils::IsSequenceDirty(m_contexts, m_sequence.get(), m_lastChange, eMEM_Previewer))
	{
		OnUpdateTV();
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
void CPreviewerPage::OnGoToStart()
{
	m_fTime = 0.0f;
	m_bPlay = false;

	m_sequencePlayback->SetTime(0.0f);
}

//////////////////////////////////////////////////////////////////////////
void CPreviewerPage::OnGoToEnd()
{
	m_fTime = m_fMaxTime;
	m_bPlay = false;

	m_sequencePlayback->SetTime(m_sequence->GetTimeRange().end);
}

//////////////////////////////////////////////////////////////////////////
void CPreviewerPage::OnPlay()
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
void CPreviewerPage::OnLoop()
{
	m_bLoop = !m_bLoop;
}

//////////////////////////////////////////////////////////////////////////
void CPreviewerPage::OnToolbarDropDown(NMHDR* pnhdr, LRESULT* plr)
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
void CPreviewerPage::OnPlayMenu(CPoint pos)
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
void CPreviewerPage::OnUpdateGoToStart(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(CMannequinDialog::GetCurrentInstance()->PreviewFileLoaded());
}

//////////////////////////////////////////////////////////////////////////
void CPreviewerPage::OnUpdateGoToEnd(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(CMannequinDialog::GetCurrentInstance()->PreviewFileLoaded());
}

//////////////////////////////////////////////////////////////////////////
void CPreviewerPage::OnUpdatePlay(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(m_bPlay ? TRUE : FALSE);
	pCmdUI->Enable(CMannequinDialog::GetCurrentInstance()->PreviewFileLoaded());
}

//////////////////////////////////////////////////////////////////////////
void CPreviewerPage::OnUpdateLoop(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(m_bLoop ? TRUE : FALSE);
	pCmdUI->Enable(CMannequinDialog::GetCurrentInstance()->PreviewFileLoaded());
}

//////////////////////////////////////////////////////////////////////////
void CPreviewerPage::OnUpdateNewSequence(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(CMannequinDialog::GetCurrentInstance()->PreviewFileLoaded());
}

//////////////////////////////////////////////////////////////////////////
void CPreviewerPage::OnUpdateLoadSequence(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(CMannequinDialog::GetCurrentInstance()->PreviewFileLoaded());
}

//////////////////////////////////////////////////////////////////////////
void CPreviewerPage::OnUpdateSaveSequence(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(CMannequinDialog::GetCurrentInstance()->PreviewFileLoaded());
}

//////////////////////////////////////////////////////////////////////////
void CPreviewerPage::OnToggle1P()
{
	m_modelViewport->ToggleCamera();
}

//////////////////////////////////////////////////////////////////////////
void CPreviewerPage::OnShowDebugOptions()
{
	CMannequinDebugOptionsDialog dialog(m_modelViewport, this);
	dialog.DoModal();
	m_cDlgToolBar.GetToolBarCtrl().CheckButton(ID_BTN_TOGGLE_1P, m_modelViewport->IsCameraAttached());
}

//////////////////////////////////////////////////////////////////////////
void CPreviewerPage::OnToggleTimelineUnits()
{
	// Enum has 2 states, so (oldMode + 1) % 2
	m_wndTrackPanel.SetTickDisplayMode((ESequencerTickMode)((m_wndTrackPanel.GetTickDisplayMode() + 1) & 1));
	if (m_modelViewport)
		m_modelViewport->SetTimelineUnits(m_wndTrackPanel.GetTickDisplayMode());
}

//////////////////////////////////////////////////////////////////////////
void CPreviewerPage::OnReloadAnimations()
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
void CPreviewerPage::OnToggleLookat()
{
	m_cDlgToolBar.GetToolBarCtrl().CheckButton(ID_MANN_LOOKAT_TOGGLE, m_modelViewport->ToggleLookingAtCamera());
}

//////////////////////////////////////////////////////////////////////////
void CPreviewerPage::OnClickRotateLocator()
{
	m_modelViewport->SetLocatorRotateMode();
	m_cDlgToolBar.GetToolBarCtrl().CheckButton(ID_MANN_ROTATE_LOCATOR, !m_modelViewport->IsTranslateLocatorMode());
	m_cDlgToolBar.GetToolBarCtrl().CheckButton(ID_MANN_TRANSLATE_LOCATOR, m_modelViewport->IsTranslateLocatorMode());
}

//////////////////////////////////////////////////////////////////////////
void CPreviewerPage::OnClickTranslateLocator()
{
	m_modelViewport->SetLocatorTranslateMode();
	m_cDlgToolBar.GetToolBarCtrl().CheckButton(ID_MANN_ROTATE_LOCATOR, !m_modelViewport->IsTranslateLocatorMode());
	m_cDlgToolBar.GetToolBarCtrl().CheckButton(ID_MANN_TRANSLATE_LOCATOR, m_modelViewport->IsTranslateLocatorMode());
}

//////////////////////////////////////////////////////////////////////////
void CPreviewerPage::OnToggleShowSceneRoots()
{
	m_modelViewport->SetShowSceneRoots(!m_modelViewport->IsShowingSceneRoots());
	m_cDlgToolBar.GetToolBarCtrl().CheckButton(ID_MANN_SHOW_SCENE_ROOTS, m_modelViewport->IsShowingSceneRoots());
}

//////////////////////////////////////////////////////////////////////////
void CPreviewerPage::OnToggleAttachToEntity()
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
float CPreviewerPage::PopulateClipTracks(CSequencerNode* node, const int scopeID)
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

	IActionController* pActionController = m_contexts->viewData[eMEM_Previewer].m_pActionController;
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

	const ActionScopes scopeFlag = BIT64(scopeID);

	float maxTime = MIN_TIME_RANGE;
	SClipTrackContext trackContext;
	trackContext.tagState.globalTags = m_tagState;
	trackContext.context = m_contexts->m_scopeData[scopeID].context[eMEM_Previewer];

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

			trackContext.context = m_contexts->GetContextDataForID(trackContext.tagState.globalTags, contextID, eMEM_Previewer);
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
						if ((BIT64(i) & scopeMask) != 0)
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
							item.fragOptionIdxSel = m_contexts->viewData[eMEM_Previewer].m_pAnimContext->randGenerator.GenerateUint32();
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

					blendQuery.forceBlendUid = m_forcedBlendUid;

					SFragmentSelection fragmentSelection;
					SFragmentData fragData;
					const bool hasFragment = trackContext.context->database->Query(fragData, blendQuery, item.fragOptionIdxSel, trackContext.context->animSet, &fragmentSelection);
					MannUtils::AdjustFragDataForVEGs(fragData, *trackContext.context, eMEM_Previewer, motionParams);
					SBlendQueryResult blend1, blend2;
					trackContext.context->database->FindBestBlends(blendQuery, blend1, blend2);
					const SFragmentBlend* pFirstBlend = blend1.pFragmentBlend;
					const bool hasTransition = (pFirstBlend != NULL);

					float blendStartTime = 0.0f;  // in 'track time'
					float blendSelectTime = 0.0f; // in 'track time'
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

										float cycleIncrement = 0.0f;
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
						const int id = fragTrack->CreateKey(transitionTime);
						CFragmentKey fragKey;
						fragTrack->GetKey(id, &fragKey);
						fragKey.SetFromQuery(blend1);
						fragKey.context = trackContext.context;
						fragKey.clipDuration = duration;
						if (!blend2.pFragmentBlend)
						{
							fragKey.clipDuration += firstBlendDuration;
						}
						fragKey.tranSelectTime = blendSelectTime;
						fragKey.tranLastClipEffectiveStart = transitionClipEffectiveStartTime;
						fragKey.tranLastClipDuration = transitionLastClipDuration;
						fragKey.historyItem = trackContext.historyItem;
						fragKey.fragIndex = partID++;
						fragTrack->SetKey(id, &fragKey);
						transitionTime += duration;
					}
					if (blend2.pFragmentBlend)
					{
						const float duration = fragData.duration[partID];
						const int id = fragTrack->CreateKey(transitionTime);
						CFragmentKey fragKey;
						fragTrack->GetKey(id, &fragKey);
						fragKey.SetFromQuery(blend2);
						fragKey.context = trackContext.context;
						fragKey.clipDuration = duration + firstBlendDuration;
						fragKey.tranSelectTime = blendSelectTime;
						fragKey.tranLastClipEffectiveStart = transitionClipEffectiveStartTime;
						fragKey.tranLastClipDuration = transitionLastClipDuration;
						fragKey.historyItem = trackContext.historyItem;
						fragKey.fragIndex = partID++;
						fragTrack->SetKey(id, &fragKey);
						transitionTime += duration;
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
void CPreviewerPage::PopulateAllClipTracks()
{
	if (!m_fragmentHistory.get())
		return;

	m_contexts->viewData[eMEM_Previewer].m_pAnimContext->randGenerator.seed(m_seed);

	uint32 numScopes = m_contexts->m_scopeData.size();
	float maxTime = MIN_TIME_RANGE;
	for (uint32 i = 0; i < numScopes; i++)
	{
		CSequencerNode* animNode = m_contexts->m_scopeData[i].animNode[eMEM_Previewer];
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
void CPreviewerPage::SetUIFromHistory()
{
	if (!m_fragmentHistory.get())
		return;

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
		bool hasPossibleContext = (m_contexts->potentiallyActiveScopes & BIT64(scopeData.scopeID)) != 0;
		if (hasPossibleContext)
		{
			CScopeNode* animNode = new CScopeNode(m_sequence.get(), &scopeData, eMEM_Previewer);
			m_sequence->AddNode(animNode);

			if (m_contexts->m_scopeData[i].context[eMEM_Previewer])
			{
				animNode->SetName(m_contexts->m_scopeData[i].name + " (" + m_contexts->m_scopeData[i].context[eMEM_Previewer]->name + ")");
			}
			else
			{
				animNode->SetName(m_contexts->m_scopeData[i].name + " (none)");
			}

			float maxTime = MIN_TIME_RANGE;
			MannUtils::InsertFragmentTrackFromHistory(animNode, *m_fragmentHistory, 0.0f, 0.0f, maxTime, m_contexts->m_scopeData[i]);
			m_contexts->m_scopeData[i].fragTrack[eMEM_Previewer] = (CFragmentTrack*)animNode->GetTrackForParameter(SEQUENCER_PARAM_FRAGMENTID);// fragTrack;

			m_contexts->m_scopeData[i].animNode[eMEM_Previewer] = animNode;
		}
	}

	PopulateAllClipTracks();

	m_wndNodes.SetSequence(m_sequence.get());

	if (m_sequencePlayback)
	{
		m_sequencePlayback->SetTime(m_fTime);
	}
}

//////////////////////////////////////////////////////////////////////////
void CPreviewerPage::SetHistoryFromUI()
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
void CPreviewerPage::OnUpdateTV()
{
	SetHistoryFromUI();

	PopulateAllClipTracks();

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

//////////////////////////////////////////////////////////////////////////
void CPreviewerPage::NewSequence()
{
	ResetForcedBlend();
	m_fragmentHistory->m_history.m_items.clear();
	SetUIFromHistory();
}

//////////////////////////////////////////////////////////////////////////
void CPreviewerPage::OnNewSequence()
{
	NewSequence();
}

//////////////////////////////////////////////////////////////////////////
void CPreviewerPage::LoadSequence(const char* szFilename)
{
	ResetForcedBlend();

	CString folder = ::GetFragmentSequenceFolder();
	CString filename(folder + "test.xml");

	if (szFilename || CFileUtil::SelectSingleFile(EFILE_TYPE_ANY, filename, SEQUENCE_FILE_FILTER, folder))
	{
		if (szFilename)
			filename = szFilename;

		m_fragmentHistory->m_history.LoadSequence(filename);
		m_fragmentHistory->m_history.UpdateScopeMasks(m_contexts, m_tagState);

		m_sequencePlayback->SetTime(0.0f);

		SetUIFromHistory();

		m_wndNodes.SetSequence(m_sequence.get());
		m_wndKeyProperties.SetSequence(m_sequence.get());
	}
}

void CPreviewerPage::LoadSequence(const int scopeContextIdx, const FragmentID fromID, const FragmentID toID, const SFragTagState fromFragTag, const SFragTagState toFragTag, const SFragmentBlendUid blendUid)
{
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
}

//////////////////////////////////////////////////////////////////////////
void CPreviewerPage::OnLoadSequence()
{
	LoadSequence();
}

//////////////////////////////////////////////////////////////////////////
void CPreviewerPage::SaveSequence(const char* szFilename)
{
	OnUpdateTV();

	char currDir[MAX_PATH + 1];
	::GetCurrentDirectory(MAX_PATH, currDir);
	CString fullFolder(currDir);
	fullFolder += "\\" + ::GetFragmentSequenceFolder();
	PathUtil::ToDosPath(fullFolder.GetBuffer());
	CFileUtil::CreateDirectory(fullFolder);

	CString filename("test.xml");
	if (szFilename || CFileUtil::SelectSaveFile(SEQUENCE_FILE_FILTER, "xml", fullFolder, filename))
	{
		if (szFilename)
			filename = szFilename;

		m_fragmentHistory->m_history.SaveSequence(filename);
	}
}

//////////////////////////////////////////////////////////////////////////
void CPreviewerPage::OnSaveSequence()
{
	SaveSequence();
}

//////////////////////////////////////////////////////////////////////////
void CPreviewerPage::OnSequenceRestart(float newTime)
{
	m_modelViewport->OnSequenceRestart(newTime);
	m_bRestartingSequence = true;
}

//////////////////////////////////////////////////////////////////////////
void CPreviewerPage::PopulateTagList()
{
	m_tagVars->DeleteAllVariables();
	m_tagControls.Init(m_tagVars, m_contexts->viewData[eMEM_Previewer].m_pAnimContext->state.GetDef());

	m_tagsPanel.DeleteVars();
	m_tagsPanel.SetVarBlock(m_tagVars.get(), functor(*this, &CPreviewerPage::OnInternalVariableChange));

	// set this up with the initial values we've already got
	SFragTagState fragTagState;
	fragTagState.globalTags = m_tagState;
	SetTagStateOnCtrl(fragTagState);
}

//////////////////////////////////////////////////////////////////////////
void CPreviewerPage::OnInternalVariableChange(IVariable* pVar)
{
	if (!m_bRefreshingTagCtrl)
	{
		SFragTagState tagState = GetTagStateFromCtrl();

		const uint32 numContexts = m_contexts->m_controllerDef->m_scopeContexts.GetNum();
		for (uint32 i = 0; i < numContexts; i++)
		{
			const SScopeContextData* contextData = m_contexts->GetContextDataForID(tagState.globalTags, i, eMEM_Previewer);
			if (contextData)
			{
				const bool res = CMannequinDialog::GetCurrentInstance()->EnableContextData(eMEM_Previewer, contextData->dataID);
				if (!res)
				{
					// Write out error msg via MessageBox
					MessageBox("Cannot set that value as it has no parent context.\nIs it a scope/barrel attachment that needs a gun tag setting first?", "Mannequin Error");

					// reset the displayed info?
					CMannequinDialog::GetCurrentInstance()->ClearContextData(eMEM_Previewer, i);
					tagState.globalTags = m_tagState;
					SetTagStateOnCtrl(tagState);
				}
			}
			else
			{
				CMannequinDialog::GetCurrentInstance()->ClearContextData(eMEM_Previewer, i);
			}
		}

		SetUIFromHistory();

		m_tagState = tagState.globalTags;
		m_contexts->viewData[eMEM_Previewer].m_pAnimContext->state = tagState.globalTags;
		OnUpdateTV();
	}
}

//////////////////////////////////////////////////////////////////////////
void CPreviewerPage::ResetForcedBlend()
{
	m_forcedBlendUid = SFragmentBlendUid();
}

//////////////////////////////////////////////////////////////////////////
void CPreviewerPage::SetTagStateOnCtrl(const SFragTagState& tagState)
{
	m_bRefreshingTagCtrl = true;
	m_tagControls.Set(tagState.globalTags);
	m_bRefreshingTagCtrl = false;
}

//////////////////////////////////////////////////////////////////////////
SFragTagState CPreviewerPage::GetTagStateFromCtrl() const
{
	SFragTagState ret;
	ret.globalTags = m_tagControls.Get();

	return ret;
}

//////////////////////////////////////////////////////////////////////////
void CPreviewerPage::InitialiseToPreviewFile()
{
	ResetForcedBlend();

	m_modelViewport->SetActionController(m_contexts->viewData[eMEM_Previewer].m_pActionController);

	m_fragmentHistory.reset(new SFragmentHistoryContext(*m_contexts->viewData[eMEM_Previewer].m_pActionController));

	m_tagState = m_contexts->viewData[eMEM_Previewer].m_pAnimContext->state.GetMask();

	m_fMaxTime = m_fragmentHistory->m_history.m_endTime - m_fragmentHistory->m_history.m_firstTime;

	m_sequencePlayback = new CFragmentSequencePlayback(m_fragmentHistory->m_history, *m_contexts->viewData[eMEM_Previewer].m_pActionController, eMEM_Previewer);
	//m_sequencePlayback = new CActionSequencePlayback(ACTION_SCOPES_ALL, 0, 0, m_fragmentHistory->m_history, true);
	//m_sequencePlayback->SetMaxTime(m_fMaxTime);
	//m_contexts->viewData[eMEM_Previewer].m_pActionController->Queue(m_sequencePlayback);

	SetUIFromHistory();
	PopulateTagList();
}

//////////////////////////////////////////////////////////////////////////
void CPreviewerPage::SetTagState(const TagState& tagState)
{
	const CTagDefinition& tagDef = m_contexts->m_controllerDef->m_tags;
	TagState tsNew = tagDef.GetUnion(m_tagState, tagState);
	m_contexts->viewData[eMEM_Previewer].m_pAnimContext->state = tsNew;

	//--- Updated associated contexts
	const uint32 numContexts = m_contexts->m_controllerDef->m_scopeContexts.GetNum();
	for (uint32 i = 0; i < numContexts; i++)
	{
		const SScopeContextData* contextData = m_contexts->GetContextDataForID(tsNew, i, eMEM_Previewer);
		if (contextData)
		{
			CMannequinDialog::GetCurrentInstance()->EnableContextData(eMEM_Previewer, contextData->dataID);
		}
		else
		{
			CMannequinDialog::GetCurrentInstance()->ClearContextData(eMEM_Previewer, i);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CPreviewerPage::SetTime(float fTime)
{
	if (m_sequencePlayback != NULL)
	{
		m_sequencePlayback->SetTime(fTime, true);
	}

	m_wndTrackPanel.SetCurrTime(fTime, true);
}

