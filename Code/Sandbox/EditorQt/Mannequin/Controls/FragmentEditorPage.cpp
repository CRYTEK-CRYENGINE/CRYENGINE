// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FragmentEditorPage.h"

#include <ICryMannequinEditor.h>

#include "FragmentBrowser.h"
#include "PreviewerPage.h"
#include "../FragmentTrack.h"
#include "../FragmentEditor.h"
#include "../MannKeyPropertiesDlgFE.h"
#include "../MannequinDialog.h"
#include "../MannequinModelViewport.h"
#include "../MannequinNodes.h"
#include "Controls/PropertiesPanel.h"
#include "Dialogs/ToolbarDialog.h"
#include "../MannequinPlayback.h"
#include "../MannDebugOptionsDialog.h"
// For persistent debugging
#include <CryGame/IGameFramework.h>

IMPLEMENT_DYNAMIC(CFragmentEditorPage, CMannequinEditorPage)

BEGIN_MESSAGE_MAP(CFragmentEditorPage, CMannequinEditorPage)
ON_COMMAND(ID_TV_JUMPSTART, OnGoToStart)
ON_COMMAND(ID_TV_JUMPEND, OnGoToEnd)
ON_COMMAND(ID_TV_PLAY, OnPlay)
ON_COMMAND(ID_PLAY_LOOP, OnLoop)
ON_COMMAND(ID_BTN_TOGGLE_1P, OnToggle1P)
ON_COMMAND(ID_SHOW_DEBUG_OPTIONS, OnShowDebugOptions)
ON_COMMAND(ID_SET_TIME_TO_KEY, OnSetTimeToKey)
ON_COMMAND(ID_TOGGLE_SCRUB_UNITS, OnToggleTimelineUnits)
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
ON_UPDATE_COMMAND_UI(ID_SET_TIME_TO_KEY, OnUpdateSetTimeToKey)
ON_NOTIFY(TBN_DROPDOWN, AFX_IDW_TOOLBAR, OnToolbarDropDown)
ON_NOTIFY_EX_RANGE(TTN_NEEDTEXTA, 0, 0xFFFF, OnToolTipText)
ON_WM_SETFOCUS()
ON_WM_DESTROY()
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////

CFragmentEditorPage::CFragmentEditorPage(CWnd* pParent)
	: CMannequinEditorPage(CFragmentEditorPage::IDD, pParent),
	m_contexts(nullptr),
	m_hAccelerators(0),
	m_bTimeWasChanged(false),
	m_pViewportWidget(nullptr),
	m_modelViewport(nullptr)
{
}

CFragmentEditorPage::~CFragmentEditorPage()
{
	const CString strSection = _T("Settings\\Mannequin\\FragmentEditorLayout");
	CXTRegistryManager regMgr;
	regMgr.WriteProfileInt(strSection, "TimelineUnits", m_wndTrackPanel.GetTickDisplayMode());

	if (m_pViewportWidget)
	{
		QMfcViewportHost* temp = m_pViewportWidget;
		m_pViewportWidget = nullptr;
		delete temp;
	}
}

BOOL CFragmentEditorPage::OnInitDialog()
{
	__super::OnInitDialog();

	m_contexts = CMannequinDialog::GetCurrentInstance()->Contexts();

	m_hAccelerators = LoadAccelerators(AfxGetApp()->m_hInstance, MAKEINTRESOURCE(IDR_MANN_ACCELERATOR));

	m_fTime = 0.0f;
	m_playSpeed = 1.0f;
	m_bPlay = false;
	m_bLoop = false;
	m_draggingTime = false;
	m_bRestartingSequence = false;
	m_bRefreshingTagCtrl = false;
	m_bReadingTagCtrl = false;

	m_fragmentPlayback = NULL;
	m_sequencePlayback = NULL;

	m_tagVars.reset(new CVarBlock());

	ModifyStyleEx(0, WS_EX_CLIENTEDGE);

	SetWindowText("Fragment Editor Page");

	// Main splitters
	m_wndSplitterVert.CreateStatic(this, 2, 1, WS_CHILD | WS_VISIBLE, AFX_IDW_PANE_FIRST);
	m_wndSplitterVert.ModifyStyle(0, WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
	m_wndSplitterVert.SetRowInfo(0, 600, CMannequinDialog::s_minPanelSize);
	m_wndSplitterVert.SetRowInfo(1, 200, CMannequinDialog::s_minPanelSize);

	m_wndSplitterHorz.CreateStatic(&m_wndSplitterVert, 1, 2, WS_CHILD | WS_VISIBLE, AFX_IDW_PANE_FIRST);
	m_wndSplitterHorz.ModifyStyle(0, WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
	m_wndSplitterHorz.SetColumnInfo(0, 400, CMannequinDialog::s_minPanelSize);
	m_wndSplitterHorz.SetColumnInfo(1, 100, CMannequinDialog::s_minPanelSize);
	m_wndSplitterHorz.SetDlgCtrlID(m_wndSplitterVert.IdFromRowCol(0, 0));

	// Track view
	m_wndTrackPanel.SetMannequinContexts(m_contexts);
	m_wndTrackPanel.Create(NULL, NULL, WS_VISIBLE | WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_HSCROLL, CRect(200, 0, 500, 300), this, NULL);
	m_wndTrackPanel.SetTimeRange(0, 2.0f);
	m_wndTrackPanel.SetTimeScale(100, 0);
	m_wndTrackPanel.SetCurrTime(0);
	m_wndTrackPanel.SetOwner(this);

	// Model viewport
	m_modelViewport = new CMannequinModelViewport(eMEM_FragmentEditor);
	m_modelViewport->SetType(ET_ViewportModel);
	m_modelViewport->SetTimelineUnits(m_wndTrackPanel.GetTickDisplayMode());
	m_viewportHost.Create(NULL, NULL, WS_VISIBLE | WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_HSCROLL, CRect(200, 0, 500, 300), this, NULL);
	m_viewportHost.ModifyStyle(WS_POPUP, WS_VISIBLE | WS_CHILD, 0);
	m_viewportHost.SetDlgCtrlID(m_wndSplitterHorz.IdFromRowCol(0, 0));
	m_viewportHost.SetParent(&m_wndSplitterHorz);
	m_viewportHost.SetOwner(this);

	CRect viewRect;
	m_viewportHost.GetClientRect(viewRect);
	m_pViewportWidget = new QMfcViewportHost(&m_viewportHost, m_modelViewport, viewRect);
	m_viewportHost.SetWidgetHost(m_pViewportWidget->GetHostWidget());
	m_viewportHost.ShowWindow(SW_SHOW);

	// Nodes tree view
	m_wndNodes.Create(WS_VISIBLE | WS_CHILD | WS_TABSTOP, CRect(0, 0, 200, 300), this, NULL);
	m_wndNodes.SetKeyListCtrl(&m_wndTrackPanel);
	m_wndNodes.SetSequence(m_wndTrackPanel.GetSequence());

	// Properties panel
	m_wndKeyProperties.Create(CMannKeyPropertiesDlgFE::IDD, this);
	m_wndKeyProperties.SetKeysCtrl(&m_wndTrackPanel);
	m_wndKeyProperties.SetSequence(m_wndTrackPanel.GetSequence());
	m_wndKeyProperties.ShowWindow(SW_SHOW);

	// 3-way sequencer splitter
	m_wndSplitterTracks.CreateStatic(&m_wndSplitterVert, 1, 3, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, 59749);
	m_wndSplitterTracks.SetColumnInfo(0, 100, CMannequinDialog::s_minPanelSize);
	m_wndSplitterTracks.SetColumnInfo(1, 100, CMannequinDialog::s_minPanelSize);
	m_wndSplitterTracks.SetColumnInfo(2, 100, CMannequinDialog::s_minPanelSize);
	m_wndSplitterTracks.SetPane(0, 0, &m_wndNodes, CSize(200, 200));
	m_wndSplitterTracks.SetPane(0, 1, &m_wndTrackPanel, CSize(400, 200));
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
	const CString strSection = _T("Settings\\Mannequin\\FragmentEditorLayout");
	CXTRegistryManager regMgr;

	int nTimelineUnits = regMgr.GetProfileInt(strSection, "TimelineUnits", m_wndTrackPanel.GetTickDisplayMode());
	if (nTimelineUnits != m_wndTrackPanel.GetTickDisplayMode())
	{
		OnToggleTimelineUnits();
	}

	return TRUE;
}

void CFragmentEditorPage::InitToolbar()
{
	m_cDlgToolBar.CreateEx(this, TBSTYLE_FLAT,
	                       WS_CHILD | WS_VISIBLE | CBRS_TOP | CBRS_TOOLTIPS | CBRS_FLYBY | CBRS_SIZE_DYNAMIC);
	m_cDlgToolBar.LoadToolBar24(IDR_MANN_FRAGMENTEDITOR_TOOLBAR);
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

void CFragmentEditorPage::ValidateToolbarButtonsState()
{
	m_cDlgToolBar.GetToolBarCtrl().CheckButton(ID_MANN_LOOKAT_TOGGLE, m_modelViewport->IsLookingAtCamera());
	m_cDlgToolBar.GetToolBarCtrl().CheckButton(ID_MANN_ROTATE_LOCATOR, !m_modelViewport->IsTranslateLocatorMode());
	m_cDlgToolBar.GetToolBarCtrl().CheckButton(ID_MANN_TRANSLATE_LOCATOR, m_modelViewport->IsTranslateLocatorMode());
	m_cDlgToolBar.GetToolBarCtrl().CheckButton(ID_MANN_ATTACH_TO_ENTITY, m_modelViewport->IsAttachedToEntity());
	m_cDlgToolBar.GetToolBarCtrl().CheckButton(ID_MANN_SHOW_SCENE_ROOTS, m_modelViewport->IsShowingSceneRoots());
	m_cDlgToolBar.GetToolBarCtrl().CheckButton(ID_BTN_TOGGLE_1P, m_modelViewport->IsCameraAttached());
}

void CFragmentEditorPage::OnDestroy()
{
}

BOOL CFragmentEditorPage::PreTranslateMessage(MSG* pMsg)
{
	bool bHandled = false;
	if (pMsg->message >= WM_KEYFIRST && pMsg->message <= WM_KEYLAST && m_hAccelerators)
		bHandled = ::TranslateAccelerator(m_hWnd, m_hAccelerators, pMsg);

	if (!bHandled)
		return CMannequinEditorPage::PreTranslateMessage(pMsg);

	return bHandled;
}

void CFragmentEditorPage::SaveLayout(const XmlNodeRef& xmlLayout)
{
	MannUtils::SaveSplitterToXml(xmlLayout, _T("SequencerSplitter"), m_wndSplitterTracks);
	MannUtils::SaveSplitterToXml(xmlLayout, _T("VerticalSplitter"), m_wndSplitterVert);
	MannUtils::SaveSplitterToXml(xmlLayout, _T("HorizontalSplitter"), m_wndSplitterHorz);
}

void CFragmentEditorPage::LoadLayout(const XmlNodeRef& xmlLayout)
{
	MannUtils::LoadSplitterFromXml(xmlLayout, _T("SequencerSplitter"), m_wndSplitterTracks, CMannequinDialog::s_minPanelSize);
	MannUtils::LoadSplitterFromXml(xmlLayout, _T("VerticalSplitter"), m_wndSplitterVert, CMannequinDialog::s_minPanelSize);
	MannUtils::LoadSplitterFromXml(xmlLayout, _T("HorizontalSplitter"), m_wndSplitterHorz, CMannequinDialog::s_minPanelSize);
}

void CFragmentEditorPage::OnSetFocus(CWnd* pOldWnd)
{
	__super::OnSetFocus(pOldWnd);

	if (auto pMannequinDialog = CMannequinDialog::GetCurrentInstance())
	{
		m_wndNodes.SyncKeyCtrl();

		if (auto pDockingPaneManager = pMannequinDialog->GetDockingPaneManager())
		{
			pDockingPaneManager->ShowPane(CMannequinDialog::IDW_FRAGMENTS_PANE, FALSE);
		}
	}
}

float CFragmentEditorPage::GetMarkerTimeStart() const
{
	return m_wndTrackPanel.GetMarkedTime().start;
}

float CFragmentEditorPage::GetMarkerTimeEnd() const
{
	return m_wndTrackPanel.GetMarkedTime().end;
}

void CFragmentEditorPage::InstallAction(float time)
{
	OnSequenceRestart(time);

	if (m_fragmentPlayback)
	{
		m_fragmentPlayback->Release();
	}
	ActionScopes fragmentScopeMask = m_wndTrackPanel.GetFragmentScopeMask();
	m_fragmentPlayback = new CFragmentPlayback(fragmentScopeMask, 0);
	SFragTagState tagState = m_wndTrackPanel.GetTagState();
	m_fragmentPlayback->SetFragment(m_wndTrackPanel.GetFragmentID(), tagState.fragmentTags, m_wndTrackPanel.GetFragmentOptionIdx(), m_wndTrackPanel.GetMaxTime());
	m_contexts->viewData[eMEM_FragmentEditor].m_pActionController->Queue(*m_fragmentPlayback);

	m_sequencePlayback->SetTime(time, true);
	m_fragmentPlayback->SetTime(time);
	CreateLocators();
}

void CFragmentEditorPage::Update()
{
	const float lastTime = m_fTime;
	bool isDraggingTime = m_wndTrackPanel.IsDraggingTime();
	float dsTime = m_wndTrackPanel.GetTime();
	if (m_bTimeWasChanged)
		dsTime = m_fTime;

	float effectiveSpeed = m_playSpeed;
	if (isDraggingTime || !m_bPlay)
		effectiveSpeed = 0.0f;

	// important to call before InstallAction (for sound muting through m_bPaused)
	m_modelViewport->SetPlaybackMultiplier(effectiveSpeed);
	if (isDraggingTime || m_bTimeWasChanged)
	{
		if (m_fTime != dsTime || m_bTimeWasChanged)
		{
			InstallAction(dsTime);
		}

		m_fTime = dsTime;
		m_bTimeWasChanged = false;
	}

	m_cDlgToolBar.SetTime(m_fTime, m_wndTrackPanel.GetSnapFps());

	m_sequencePlayback->Update(gEnv->pTimer->GetFrameTime() * effectiveSpeed, ModelViewport());

	m_draggingTime = isDraggingTime;

	if (m_fragmentPlayback)
	{
		if (m_fragmentPlayback->ReachedEnd())
		{
			if (m_bLoop)
			{
				const float startTime = GetMarkerTimeStart();

				InstallAction(startTime);

				// Set silent mode off.
				m_bRestartingSequence = false;
			}
			else
			{
				m_bPlay = false;
			}
		}

		m_fTime = m_fragmentPlayback->GetTimeSinceInstall();
	}
	m_wndTrackPanel.SetCurrTime(m_fTime);
	CheckForLocatorChanges(lastTime, m_fTime);

	if (MannUtils::IsSequenceDirty(m_contexts, m_wndTrackPanel.GetSequence(), m_lastChange, eMEM_FragmentEditor))
	{
		m_wndTrackPanel.OnAccept();

		InstallAction(m_fTime);

		m_wndTrackPanel.SetCurrTime(m_fTime, true);
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
void CFragmentEditorPage::OnGoToStart()
{
	SetTime(0.0f);
	m_bPlay = false;
	m_bTimeWasChanged = true;
}

//////////////////////////////////////////////////////////////////////////
void CFragmentEditorPage::OnGoToEnd()
{
	SetTime(m_fragmentPlayback->GetTimeMax());
	m_bPlay = false;
}

//////////////////////////////////////////////////////////////////////////
void CFragmentEditorPage::OnPlay()
{
	m_bPlay = !m_bPlay;

	if (m_bPlay)
	{
		if (m_fragmentPlayback && m_fragmentPlayback->ReachedEnd())
		{
			const float startTime = GetMarkerTimeStart();
			m_fTime = startTime;
		}
		// important to trigger InstallAction for audio to play
		m_bTimeWasChanged = true;
	}
}

//////////////////////////////////////////////////////////////////////////
void CFragmentEditorPage::OnLoop()
{
	m_bLoop = !m_bLoop;
}

//////////////////////////////////////////////////////////////////////////
void CFragmentEditorPage::OnToggle1P()
{
	m_modelViewport->ToggleCamera();
}

//////////////////////////////////////////////////////////////////////////
void CFragmentEditorPage::OnShowDebugOptions()
{
	CMannequinDebugOptionsDialog dialog(ModelViewport(), this);
	dialog.DoModal();
	m_cDlgToolBar.GetToolBarCtrl().CheckButton(ID_BTN_TOGGLE_1P, m_modelViewport->IsCameraAttached());
}

//////////////////////////////////////////////////////////////////////////
void CFragmentEditorPage::SetTimeToSelectedKey()
{
	CSequencerUtils::SelectedKeys selectedKeys;
	CSequencerUtils::GetSelectedKeys(m_wndTrackPanel.GetSequence(), selectedKeys);

	if (selectedKeys.keys.size() == 1)
	{
		CSequencerUtils::SelectedKey& key = selectedKeys.keys[0];
		CRY_ASSERT(key.pTrack != NULL);

		CSequencerKey* pKey = key.pTrack->GetKey(key.nKey);
		CRY_ASSERT(pKey != NULL);

		float fTime = pKey->m_time;
		SetTime(fTime);
		m_wndTrackPanel.SetCurrTime(fTime, true);
	}
}

void CFragmentEditorPage::OnSetTimeToKey()
{
	SetTimeToSelectedKey();
}

//////////////////////////////////////////////////////////////////////////
void CFragmentEditorPage::SetTime(float fTime)
{
	m_fTime = fTime;
	m_bTimeWasChanged = true;
}

//////////////////////////////////////////////////////////////////////////
void CFragmentEditorPage::OnToggleTimelineUnits()
{
	// Enum has 2 states, so (oldMode + 1) % 2
	m_wndTrackPanel.SetTickDisplayMode((ESequencerTickMode)((m_wndTrackPanel.GetTickDisplayMode() + 1) & 1));
	if (m_modelViewport)
		m_modelViewport->SetTimelineUnits(m_wndTrackPanel.GetTickDisplayMode());
}

//////////////////////////////////////////////////////////////////////////
void CFragmentEditorPage::OnReloadAnimations()
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
	//QT_TODO("Need other Animation Browser")
	/*
	   CAnimationBrowser *pAnimationBrowser = m_modelViewport->GetAnimBrowserPanel();
	   if( pAnimationBrowser != NULL )
	   {
	   pAnimationBrowser->ReloadAnimations();
	   }
	 */

	SetTime(0.0f);
}

//////////////////////////////////////////////////////////////////////////
void CFragmentEditorPage::OnToggleLookat()
{
	m_cDlgToolBar.GetToolBarCtrl().CheckButton(ID_MANN_LOOKAT_TOGGLE, m_modelViewport->ToggleLookingAtCamera());
}

//////////////////////////////////////////////////////////////////////////
void CFragmentEditorPage::OnClickRotateLocator()
{
	m_modelViewport->SetLocatorRotateMode();
	m_cDlgToolBar.GetToolBarCtrl().CheckButton(ID_MANN_ROTATE_LOCATOR, !m_modelViewport->IsTranslateLocatorMode());
	m_cDlgToolBar.GetToolBarCtrl().CheckButton(ID_MANN_TRANSLATE_LOCATOR, m_modelViewport->IsTranslateLocatorMode());
}

//////////////////////////////////////////////////////////////////////////
void CFragmentEditorPage::OnClickTranslateLocator()
{
	m_modelViewport->SetLocatorTranslateMode();
	m_cDlgToolBar.GetToolBarCtrl().CheckButton(ID_MANN_ROTATE_LOCATOR, !m_modelViewport->IsTranslateLocatorMode());
	m_cDlgToolBar.GetToolBarCtrl().CheckButton(ID_MANN_TRANSLATE_LOCATOR, m_modelViewport->IsTranslateLocatorMode());
}

//////////////////////////////////////////////////////////////////////////
void CFragmentEditorPage::OnToggleAttachToEntity()
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
void CFragmentEditorPage::OnToolbarDropDown(NMHDR* pnhdr, LRESULT* plr)
{
	CRect rc;
	CPoint pos;
	GetCursorPos(&pos);
	NMTOOLBAR* pnmtb = (NMTOOLBAR*)pnhdr;
	m_cDlgToolBar.GetToolBarCtrl().GetRect(pnmtb->iItem, rc);
	m_cDlgToolBar.ClientToScreen(rc);
	pos.x = rc.left;
	pos.y = rc.bottom;

	// Switch on button command id's.
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
void CFragmentEditorPage::OnPlayMenu(CPoint pos)
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
void CFragmentEditorPage::OnUpdateGoToStart(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(CMannequinDialog::GetCurrentInstance()->PreviewFileLoaded());
}

//////////////////////////////////////////////////////////////////////////
void CFragmentEditorPage::OnUpdateGoToEnd(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(CMannequinDialog::GetCurrentInstance()->PreviewFileLoaded());
}

//////////////////////////////////////////////////////////////////////////
void CFragmentEditorPage::OnUpdatePlay(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(m_bPlay ? TRUE : FALSE);
	pCmdUI->Enable(CMannequinDialog::GetCurrentInstance()->PreviewFileLoaded());
}

//////////////////////////////////////////////////////////////////////////
void CFragmentEditorPage::OnUpdateLoop(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(m_bLoop ? TRUE : FALSE);
	pCmdUI->Enable(CMannequinDialog::GetCurrentInstance()->PreviewFileLoaded());
}

void CFragmentEditorPage::OnUpdateSetTimeToKey(CCmdUI* pCmdUI)
{
	CSequencerUtils::SelectedKeys selectedKeys;
	CSequencerUtils::GetSelectedKeys(m_wndTrackPanel.GetSequence(), selectedKeys);
	pCmdUI->Enable(selectedKeys.keys.size() == 1);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CFragmentEditorPage::OnToggleShowSceneRoots()
{
	m_modelViewport->SetShowSceneRoots(!m_modelViewport->IsShowingSceneRoots());
	m_cDlgToolBar.GetToolBarCtrl().CheckButton(ID_MANN_SHOW_SCENE_ROOTS, m_modelViewport->IsShowingSceneRoots());
}

void CFragmentEditorPage::UpdateSelectedFragment()
{
	const SFragTagState tagState = m_wndTrackPanel.GetTagState();
	SetTagState(m_wndTrackPanel.GetFragmentID(), tagState);

	ActionScopes fragmentScopeMask = m_wndTrackPanel.GetFragmentScopeMask();
	ActionScopes sequenceScopeMask = (~fragmentScopeMask) & m_contexts->viewData[eMEM_FragmentEditor].m_pActionController->GetActiveScopeMask();

	m_sequencePlayback->SetScopeMask(sequenceScopeMask);
	SetTime(0.0f);

	CMannequinDialog::GetCurrentInstance()->PopulateTagList();
	CMannequinDialog::GetCurrentInstance()->SetTagStateOnCtrl();

	m_wndNodes.SetSequence(m_wndTrackPanel.GetSequence());

	CreateLocators();
}

void CFragmentEditorPage::CreateLocators()
{
	m_modelViewport->ClearLocators();

	if (m_wndTrackPanel.GetFragmentID() != FRAGMENT_ID_INVALID)
	{
		const CFragmentHistory& history = m_wndTrackPanel.GetFragmentHistory()->m_history;
		const uint32 numHistoryItems = history.m_items.size();
		for (uint32 h = 0; h < numHistoryItems; h++)
		{
			const CFragmentHistory::SHistoryItem& item = history.m_items[h];

			if ((item.type == CFragmentHistory::SHistoryItem::Param) && item.isLocation)
			{
				m_modelViewport->AddLocator(h, item.paramName, item.param.value);
			}
		}

		CSequencerUtils::SelectedKeys selectedKeys;
		CSequencerUtils::GetSelectedKeys(m_wndTrackPanel.GetSequence(), selectedKeys);

		for (int i = 0; i < selectedKeys.keys.size(); ++i)
		{
			CSequencerUtils::SelectedKey& selectedKey = selectedKeys.keys[i];

			const ESequencerParamType paramType = selectedKey.pTrack->GetParameterType();
			if (paramType == SEQUENCER_PARAM_PROCLAYER)
			{
				CProcClipKey key;
				selectedKey.pTrack->GetKey(selectedKey.nKey, &key);

				const float startTime = key.m_time;
				const float endTime = startTime + selectedKey.pTrack->GetKeyDuration(selectedKey.nKey);
				const bool currentlyActive = startTime <= m_fTime && m_fTime < endTime;

				if (currentlyActive && key.pParams)
				{
					QuatT transform;

					IEntity* pEntity = selectedKey.pNode->GetEntity();
					CRY_ASSERT(pEntity != NULL);

					IProceduralParamsEditor::SEditorCreateTransformGizmoResult result = key.pParams->OnEditorCreateTransformGizmo(*pEntity);
					if (result.createGizmo)
					{
						m_modelViewport->AddLocator(numHistoryItems + i, "ProcClip", result.gizmoLocation, pEntity, result.jointId, result.pAttachment, result.paramCRC, result.helperName);
					}
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CFragmentEditorPage::CheckForLocatorChanges(const float lastTime, const float newTime)
{
	if (lastTime == newTime)
		return;

	const CSequencerSequence* pSequence = m_wndTrackPanel.GetSequence();
	const int nNumNodes = pSequence->GetNodeCount();
	for (int nodeID = 0; nodeID < nNumNodes; ++nodeID)
	{
		const CSequencerNode* pNode = pSequence->GetNode(nodeID);
		const int nNumTracks = pNode->GetTrackCount();
		for (int trackID = 0; trackID < nNumTracks; ++trackID)
		{
			const CSequencerTrack* pTrack = pNode->GetTrackByIndex(trackID);
			const int nNumKeys = pTrack->GetNumKeys();
			for (int keyID = 0; keyID < nNumKeys; ++keyID)
			{
				const CSequencerKey* pKey = pTrack->GetKey(keyID);
				const float startTime = pKey->m_time;
				const float endTime = startTime + pTrack->GetKeyDuration(keyID);

				bool prevActive = startTime <= lastTime && lastTime < endTime;
				bool nowActive = startTime <= newTime && newTime < endTime;

				if (prevActive ^ nowActive)
				{
					// There is at least one change - create the locator(s)!
					CreateLocators();
					return;
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CFragmentEditorPage::SetTagState(const FragmentID fragID, const SFragTagState& tagState)
{
	// Remove scope tags from request so that separate scopes are still able to play distinct fragment options
	TagState tsRequest = tagState.globalTags;
	const uint32 numScopes = m_contexts->m_controllerDef->m_scopeDef.size();
	for (uint32 i = 0; i < numScopes; i++)
	{
		tsRequest = m_contexts->m_controllerDef->m_tags.GetDifference(tsRequest, m_contexts->m_controllerDef->m_scopeDef[i].additionalTags);
	}

	// Get the union of the requested TagState and the global override m_globalTags
	const CTagDefinition& tagDef = m_contexts->m_controllerDef->m_tags;
	const SFragTagState newTagState(tagDef.GetUnion(m_globalTags, tsRequest), tagState.fragmentTags);
	m_contexts->viewData[eMEM_FragmentEditor].m_pAnimContext->state = newTagState.globalTags;

	CMannequinDialog& mannDialog = *CMannequinDialog::GetCurrentInstance();

	//--- Updated associated contexts
	const uint32 numContexts = m_contexts->m_controllerDef->m_scopeContexts.GetNum();
	for (uint32 i = 0; i < numContexts; i++)
	{
		const SScopeContextData* contextData = m_contexts->GetContextDataForID(newTagState, fragID, i, eMEM_FragmentEditor);
		if (contextData)
		{
			mannDialog.EnableContextData(eMEM_FragmentEditor, contextData->dataID);
		}
		else
		{
			mannDialog.ClearContextData(eMEM_FragmentEditor, i);
		}
	}

	//--- Ensure that the currently selected context is always active for editing
	mannDialog.EnableContextData(eMEM_FragmentEditor, mannDialog.FragmentBrowser()->GetScopeContextId());

	PopulateTagList();
}

//////////////////////////////////////////////////////////////////////////
void CFragmentEditorPage::SetTagStateOnCtrl(const SFragTagState& tagState)
{
	m_bRefreshingTagCtrl = true;
	m_tagControls.Set(tagState.globalTags);
	m_bRefreshingTagCtrl = false;
}

//////////////////////////////////////////////////////////////////////////
SFragTagState CFragmentEditorPage::GetTagStateFromCtrl() const
{
	SFragTagState ret;
	ret.globalTags = m_tagControls.Get();

	return ret;
}

//////////////////////////////////////////////////////////////////////////
void CFragmentEditorPage::OnInternalVariableChange(IVariable* pVar)
{
	if (!m_bRefreshingTagCtrl)
	{
		m_bReadingTagCtrl = true;
		const SFragTagState tagState = GetTagStateFromCtrl();
		m_globalTags = tagState.globalTags;
		UpdateSelectedFragment();
		m_bReadingTagCtrl = false;
	}
}

//////////////////////////////////////////////////////////////////////////
void CFragmentEditorPage::OnSequenceRestart(float timePassed)
{
	m_modelViewport->OnSequenceRestart(timePassed);
	m_bRestartingSequence = true;
}

//////////////////////////////////////////////////////////////////////////
void CFragmentEditorPage::PopulateTagList()
{
	if (!m_bReadingTagCtrl && m_wndTrackPanel.GetFragmentHistory())
	{
		const SFragTagState tagsStateSelected = m_wndTrackPanel.GetTagState();
		const ActionScopes availableScopeMask = ~m_wndTrackPanel.GetFragmentScopeMask();
		SFragTagState usedTags(TAG_STATE_EMPTY, TAG_STATE_EMPTY);

		const uint32 numScopes = m_contexts->m_controllerDef->m_scopeDef.size();
		SFragTagState queryGlobalTags = tagsStateSelected;
		for (uint32 i = 0; i < numScopes; i++)
		{
			queryGlobalTags.globalTags = m_contexts->m_controllerDef->m_tags.GetDifference(queryGlobalTags.globalTags, m_contexts->m_controllerDef->m_scopeDef[i].additionalTags);
		}

		const CFragmentHistory& history = m_wndTrackPanel.GetFragmentHistory()->m_history;
		const uint32 numItems = history.m_items.size();
		for (uint32 i = 0; i < numItems; i++)
		{
			const CFragmentHistory::SHistoryItem& item = history.m_items[i];
			FragmentID fragID = item.fragment;

			if ((fragID != FRAGMENT_ID_INVALID) && ((availableScopeMask & item.installedScopeMask) == item.installedScopeMask))
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

		TagState enabledControls = m_contexts->m_controllerDef->m_tags.GetDifference(usedTags.globalTags, tagsStateSelected.globalTags, true);

		m_tagVars->DeleteAllVariables();
		m_tagControls.Init(m_tagVars, m_contexts->viewData[eMEM_FragmentEditor].m_pAnimContext->state.GetDef(), enabledControls);

		m_tagsPanel.DeleteVars();
		m_tagsPanel.SetVarBlock(m_tagVars.get(), functor(*this, &CFragmentEditorPage::OnInternalVariableChange), false);

		// set this up with the initial values we've already got
		SFragTagState fragTagState;
		fragTagState.globalTags = m_globalTags;
		SetTagStateOnCtrl(fragTagState);
	}
}

//////////////////////////////////////////////////////////////////////////
void CFragmentEditorPage::InitialiseToPreviewFile(const XmlNodeRef& xmlSequenceRoot)
{
	m_wndTrackPanel.InitialiseToPreviewFile(xmlSequenceRoot);
	m_modelViewport->SetActionController(m_contexts->viewData[eMEM_FragmentEditor].m_pActionController);
	m_contexts->viewData[eMEM_FragmentEditor].m_pActionController->SetFlag(AC_NoTransitions, true);
	for (uint32 i = 0; i < m_contexts->m_contextData.size(); i++)
	{
		SScopeContextData& contextData = m_contexts->m_contextData[i];
		if (contextData.viewData[eMEM_FragmentEditor].enabled && contextData.viewData[eMEM_FragmentEditor].m_pActionController)
		{
			contextData.viewData[eMEM_FragmentEditor].m_pActionController->SetFlag(AC_NoTransitions, true);
		}
	}
	SAFE_DELETE(m_sequencePlayback);
	m_fragmentPlayback = NULL;
	m_sequencePlayback = new CFragmentSequencePlayback(m_wndTrackPanel.GetFragmentHistory()->m_history, *m_contexts->viewData[eMEM_FragmentEditor].m_pActionController, eMEM_FragmentEditor, 0);

	m_globalTags = m_contexts->viewData[eMEM_FragmentEditor].m_pAnimContext->state.GetMask();
	PopulateTagList();
}

//////////////////////////////////////////////////////////////////////////
void CFragmentEditorPage::Reset()
{
	m_wndTrackPanel.SetFragment(FRAGMENT_ID_INVALID);
	m_wndKeyProperties.ForceUpdate();
}

