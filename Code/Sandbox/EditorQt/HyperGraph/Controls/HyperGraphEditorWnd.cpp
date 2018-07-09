// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "HyperGraphEditorWnd.h"

#include <ILevelSystem.h>
#include <CryAISystem/IAgent.h>
#include <CryAISystem/IAISystem.h>
#include <CryAISystem/IAIAction.h>
#include <CryAction/ICustomActions.h>

#include <CryGame/IGameFramework.h>

#include <CryFlowGraph/IFlowGraphModuleManager.h>

#include "AI/AIManager.h"
#include "UI/UIManager.h"
#include "CustomActions/CustomActionsEditorManager.h"
#include "Material/MaterialFXGraphMan.h"

#include "GameEngine.h"

#include "HyperGraph/HyperGraphManager.h"
#include "HyperGraph/FlowGraphManager.h"
#include "HyperGraph/FlowGraph.h"
#include "HyperGraph/FlowGraphNode.h"
#include "HyperGraph/FlowGraphPreferences.h"
#include "HyperGraph/FlowGraphVariables.h"
#include "HyperGraph/FlowGraphModuleManager.h"

#include "HyperGraph/Nodes/CommentNode.h"
#include "HyperGraph/Nodes/CommentBoxNode.h"
#include "HyperGraph/Nodes/BlackBoxNode.h"

#include "HyperGraphEditorNodeList.h"
#include "FlowGraphProperties.h"
#include "FlowGraphSearchCtrl.h"
#include "FlowGraphDebuggerEditor.h"
#include "FlowGraphModuleDialogs.h"
#include "FlowGraphTokensDialogs.h"

#include "QtViewPane.h"
#include "EditorFramework/PersonalizationManager.h"
#include "Dialogs/GenericSelectItemDialog.h"
#include "Dialogs/StringInputDialog.h"
#include "Dialogs/QStringDialog.h"
#include "Controls/QuestionDialog.h"
#include "Controls/PropertyCtrl.h"
#include "Controls/SharedFonts.h"
#include "Util/MFCUtil.h"

#include "Objects/EntityObject.h"
#include "Objects/PrefabObject.h"


namespace
{
const int FlowGraphLayoutVersion = 0x0004;   // bump this up on every substantial pane layout change
}

#define GRAPH_FILE_FILTER                 "Graph XML Files (*.xml)|*.xml"

#define IDW_HYPERGRAPH_RIGHT_PANE         AFX_IDW_CONTROLBAR_FIRST + 10
#define IDW_HYPERGRAPH_TREE_PANE          AFX_IDW_CONTROLBAR_FIRST + 11
#define IDW_HYPERGRAPH_COMPONENTS_PANE    AFX_IDW_CONTROLBAR_FIRST + 12
#define IDW_HYPERGRAPH_SEARCH_PANE        AFX_IDW_CONTROLBAR_FIRST + 13
#define IDW_HYPERGRAPH_SEARCHRESULTS_PANE AFX_IDW_CONTROLBAR_FIRST + 14
#define IDW_HYPERGRAPH_BREAKPOINTS_PANE   AFX_IDW_CONTROLBAR_FIRST + 15
#define IDW_HYPERGRAPH_TITLEBAR           AFX_IDW_CONTROLBAR_FIRST + 16

#define HYPERGRAPH_DIALOGFRAME_CLASSNAME  "HyperGraphDialog"

#define IDC_HYPERGRAPH_COMPONENTS         1
#define IDC_HYPERGRAPH_GRAPHS             2
#define IDC_COMPONENT_SEARCH              3
#define IDC_BREAKPOINTS                   4

#define ID_NODE_INPUTS_GROUP              1
#define ID_NODE_INFO_GROUP                2
#define ID_GRAPH_INFO_GROUP               3
#define ID_GRAPH_TOKENS_GROUP             4

#define ID_COMPONENTS_GROUP               1

#define ID_NODE_INFO                      100
#define ID_GRAPH_INFO                     110


namespace {
const char* g_EntitiesFolderName = "Entities";
const char* g_AIActionsFolderName = "AI Actions";
const char* g_UIActionsFolderName = "UI Actions";
const char* g_CustomActionsFolderName = "Custom Actions";
const char* g_FGModulesFolderName = "FG Modules";
const char* g_MaterialFXFolderName = "Material FX";

}

//////////////////////////////////////////////////////////////////////////
// CHyperGraphDialog

class CHyperGraphViewClass : public IViewPaneClass
{
	//////////////////////////////////////////////////////////////////////////
	// IClassDesc
	//////////////////////////////////////////////////////////////////////////
	virtual ESystemClassID SystemClassID()   override { return ESYSTEM_CLASS_VIEWPANE; };
	virtual const char*    ClassName()       override { return "Flow Graph"; };
	virtual const char*    Category()        override { return "Game"; };
	virtual CRuntimeClass* GetRuntimeClass() override { return RUNTIME_CLASS(CHyperGraphDialog); };
	virtual const char*    GetPaneTitle()    override { return _T("Flow Graph"); };
	virtual bool           SinglePane()      override { return true; };
};

REGISTER_CLASS_DESC(CHyperGraphViewClass)

//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CHyperGraphDialog, CXTPFrameWnd)

BEGIN_MESSAGE_MAP(CHyperGraphDialog, CXTPFrameWnd)
ON_WM_DESTROY()
ON_WM_LBUTTONUP()
ON_WM_MOUSEMOVE()
ON_COMMAND(ID_FILE_NEW, OnFileNew)
ON_COMMAND(ID_FILE_SAVE, OnFileSaveAllExternal)
ON_COMMAND(ID_FILE_NEWACTION, OnFileNewAction)
ON_COMMAND(ID_FILE_NEWUIACTION, OnFileNewUIAction)
ON_COMMAND(ID_FILE_NEWCUSTOMACTION, OnFileNewCustomAction)
ON_COMMAND(ID_FILE_NEWMATERIALFXGRAPH, OnFileNewMatFXGraph)
ON_COMMAND(ID_FILE_NEWFGMODULE_GLOBAL, OnFileNewGlobalFGModule)
ON_COMMAND(ID_FILE_NEWFGMODULE_LEVEL, OnFileNewLevelFGModule)
ON_COMMAND(ID_FILE_IMPORT, OnFileImport)
ON_COMMAND(ID_FILE_EXPORT, OnFileExport)
ON_COMMAND(ID_EDIT_FG_FIND, OnFind)
ON_COMMAND(ID_FLOWGRAPH_PLAY, OnPlay)
ON_COMMAND(ID_FLOWGRAPH_STOP, OnStop)
ON_COMMAND(ID_FLOWGRAPH_DEBUG, OnToggleDebug)
ON_COMMAND(ID_FLOWGRAPH_DEBUG_ERASE, OnEraseDebug)
//ON_COMMAND( ID_FLOWGRAPH_PAUSE,OnPause )
ON_COMMAND(ID_FLOWGRAPH_STEP, OnStep)
ON_COMMAND(ID_TOOLS_EDITGRAPHTOKENS, OnEditGraphTokens)
ON_COMMAND(ID_TOOLS_EDITMODULE, OnEditModuleInputsOutputs)
ON_UPDATE_COMMAND_UI(ID_FLOWGRAPH_DEBUG, OnUpdateDebug)
ON_UPDATE_COMMAND_UI(ID_FLOWGRAPH_PLAY, OnUpdatePlay)
ON_COMMAND_EX_RANGE(ID_FLOWGRAPH_TYPE_AIACTION, ID_FLOWGRAPH_TYPE_MATERIALFX, OnDebugFlowgraphTypeToggle)
ON_UPDATE_COMMAND_UI_RANGE(ID_FLOWGRAPH_TYPE_AIACTION, ID_FLOWGRAPH_TYPE_MATERIALFX, OnDebugFlowgraphTypeUpdate)
//ON_UPDATE_COMMAND_UI(ID_FLOWGRAPH_PAUSE, OnUpdateStep)

ON_COMMAND_EX_RANGE(ID_COMPONENTS_APPROVED, ID_COMPONENTS_OBSOLETE, OnComponentsViewToggle)
ON_UPDATE_COMMAND_UI_RANGE(ID_COMPONENTS_APPROVED, ID_COMPONENTS_OBSOLETE, OnComponentsViewUpdate)
ON_COMMAND_EX(ID_VIEW_GRAPHS, OnToggleBar)
ON_COMMAND_EX(ID_VIEW_NODEINPUTS, OnToggleBar)
ON_COMMAND_EX(ID_VIEW_COMPONENTS, OnToggleBar)
ON_COMMAND_EX(ID_VIEW_BREAKPOINTS, OnToggleBar)
ON_COMMAND_EX(ID_VIEW_SEARCH, OnToggleBar)
ON_COMMAND_EX(ID_VIEW_SEARCHRESULTS, OnToggleBar)
ON_COMMAND_EX(ID_VIEW_TITLEBAR, OnToggleBar)
ON_UPDATE_COMMAND_UI(ID_VIEW_GRAPHS, OnUpdateControlBar)
ON_UPDATE_COMMAND_UI(ID_VIEW_NODEINPUTS, OnUpdateControlBar)
ON_UPDATE_COMMAND_UI(ID_VIEW_COMPONENTS, OnUpdateControlBar)
ON_UPDATE_COMMAND_UI(ID_VIEW_BREAKPOINTS, OnUpdateControlBar)
ON_UPDATE_COMMAND_UI(ID_VIEW_SEARCH, OnUpdateControlBar)
ON_UPDATE_COMMAND_UI(ID_VIEW_SEARCHRESULTS, OnUpdateControlBar)
ON_UPDATE_COMMAND_UI(ID_VIEW_TITLEBAR, OnUpdateControlBar)

ON_NOTIFY(TVN_SELCHANGED, IDC_HYPERGRAPH_GRAPHS, OnGraphsSelChanged)
ON_NOTIFY(NM_RCLICK, IDC_HYPERGRAPH_GRAPHS, OnGraphsRightClick)
ON_NOTIFY(NM_RCLICK, IDC_BREAKPOINTS, OnBreakpointsRightClick)
ON_NOTIFY(NM_CLICK, IDC_BREAKPOINTS, OnBreakpointsClick)
ON_NOTIFY(NM_DBLCLK, IDC_HYPERGRAPH_GRAPHS, OnGraphsDblClick)

ON_COMMAND(ID_UNDO, OnUndo)
ON_COMMAND(ID_REDO, OnRedo)
ON_COMMAND(ID_SAVE, OnSave)

ON_NOTIFY(XTP_LBN_SELCHANGE, ID_FG_BACK, OnNavListBoxControlSelChange)
ON_NOTIFY(XTP_LBN_SELCHANGE, ID_FG_FORWARD, OnNavListBoxControlSelChange)
ON_NOTIFY(XTP_LBN_POPUP, ID_FG_BACK, OnNavListBoxControlPopup)
ON_NOTIFY(XTP_LBN_POPUP, ID_FG_FORWARD, OnNavListBoxControlPopup)
ON_UPDATE_COMMAND_UI(ID_FG_BACK, OnUpdateNav)
ON_UPDATE_COMMAND_UI(ID_FG_FORWARD, OnUpdateNav)
ON_COMMAND(ID_FG_FORWARD, OnNavForward)
ON_COMMAND(ID_FG_BACK, OnNavBack)

ON_COMMAND(ID_TOOLS_CUSTOMIZEKEYBOARD, OnCustomize)
ON_COMMAND(ID_TOOLS_EXPORT_SHORTCUTS, OnExportShortcuts)
ON_COMMAND(ID_TOOLS_IMPORT_SHORTCUTS, OnImportShortcuts)
ON_EN_CHANGE(IDC_COMPONENT_SEARCH, OnComponentSearchEditChange)

// XT Commands.
ON_MESSAGE(XTPWM_DOCKINGPANE_NOTIFY, OnDockingPaneNotify)
ON_XTP_CREATECONTROL()
END_MESSAGE_MAP()
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
CHyperGraphDialog::CHyperGraphDialog()
	: m_isTitleBarVisible(true)
	, m_componentsViewMask(EFLN_APPROVED | EFLN_ADVANCED | EFLN_DEBUG)
  , m_pDragImage(nullptr)
  , m_dragNodeClass("")
	, m_pSearchOptionsView(nullptr)
  , m_pSearchResultsCtrl(nullptr)
	, m_pPropertiesFolder(nullptr)
	, m_pPropertiesItem(nullptr)
  , m_pSelNodeInfo(nullptr)
  , m_pGraphProperties(nullptr)
  , m_pGraphTokens(nullptr)
  , m_pNavGraph(nullptr)
	, m_bIgnoreObjectEvents(false)
{
	LOADING_TIME_PROFILE_SECTION;
	m_pWndProps.reset(new CPropertyCtrl());

	GetIEditorImpl()->RegisterNotifyListener(this);
	GetIEditorImpl()->GetObjectManager()->signalObjectChanged.Connect(this, &CHyperGraphDialog::OnObjectEvent);
	GetIEditorImpl()->GetObjectManager()->signalObjectsDetached.Connect(this, &CHyperGraphDialog::OnObjectsDetached);
	GetIEditorImpl()->GetObjectManager()->signalObjectsAttached.Connect(this, &CHyperGraphDialog::OnObjectsAttached);

	GetIEditorImpl()->GetFlowGraphManager()->AddListener(this);

	m_pFlowGraphDebugger = GetIFlowGraphDebuggerPtr();

	WNDCLASS wndcls;
	HINSTANCE hInst = AfxGetInstanceHandle();
	if (!(::GetClassInfo(hInst, HYPERGRAPH_DIALOGFRAME_CLASSNAME, &wndcls)))
	{
		// otherwise we need to register a new class
		wndcls.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
		wndcls.lpfnWndProc = ::DefWindowProc;
		wndcls.cbClsExtra = wndcls.cbWndExtra = 0;
		wndcls.hInstance = hInst;
		wndcls.hIcon = NULL;
		wndcls.hCursor = AfxGetApp()->LoadStandardCursor(IDC_ARROW);
		wndcls.hbrBackground = (HBRUSH) (COLOR_3DFACE + 1);
		wndcls.lpszMenuName = NULL;
		wndcls.lpszClassName = HYPERGRAPH_DIALOGFRAME_CLASSNAME;
		if (!AfxRegisterClass(&wndcls))
		{
			AfxThrowResourceException();
		}
	}
	CRect rc(0, 0, 0, 0);
	BOOL bRes = Create(WS_CHILD, rc, AfxGetMainWnd());
	ASSERT(bRes);
	if (!bRes)
		return;

	OnInitDialog();
}

CHyperGraphDialog::~CHyperGraphDialog()
{
	SAFE_DELETE(m_pSearchResultsCtrl);
	SAFE_DELETE(m_pSearchOptionsView);
	SAFE_DELETE(m_pGraphProperties);
	SAFE_DELETE(m_pGraphTokens);

	if (GetIEditorImpl()->GetFlowGraphManager())
		GetIEditorImpl()->GetFlowGraphManager()->RemoveListener(this);

	GetIEditorImpl()->GetObjectManager()->signalObjectChanged.DisconnectObject(this);
	GetIEditorImpl()->GetObjectManager()->signalObjectsDetached.DisconnectObject(this);
	GetIEditorImpl()->GetObjectManager()->signalObjectsAttached.DisconnectObject(this);
	GetIEditorImpl()->UnregisterNotifyListener(this);
}


void CHyperGraphDialog::SavePersonalization()
{
	CPersonalizationManager* pPersonalizationManager = GetIEditorImpl()->GetPersonalizationManager();
	if (pPersonalizationManager)
	{
		pPersonalizationManager->SetProperty("FlowGraph", "Components Mask", m_componentsViewMask);
		pPersonalizationManager->SetProperty("FlowGraph", "Show Title Bar", m_isTitleBarVisible);
	}
}

void CHyperGraphDialog::LoadPersonalization()
{
	CPersonalizationManager* pPersonalizationManager = GetIEditorImpl()->GetPersonalizationManager();
	if (pPersonalizationManager)
	{
		QVariant qValue;
		qValue = pPersonalizationManager->GetProperty("FlowGraph", "Components Mask");
		m_componentsViewMask = qValue.isValid() ? qValue.toUInt() : (EFLN_APPROVED | EFLN_ADVANCED | EFLN_DEBUG);
		qValue = pPersonalizationManager->GetProperty("FlowGraph", "Show Title Bar");
		m_isTitleBarVisible = qValue.isValid() ? qValue.toBool() : true;
	}
}

//////////////////////////////////////////////////////////////////////////
BOOL CHyperGraphDialog::Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd)
{
	return __super::Create(HYPERGRAPH_DIALOGFRAME_CLASSNAME, NULL, dwStyle, rect, pParentWnd);
}

//////////////////////////////////////////////////////////////////////////
BOOL CHyperGraphDialog::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo)
{
	BOOL res = FALSE;
	if (m_view.m_hWnd)
	{
		res = m_view.OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
		if (TRUE == res)
			return res;
	}

	/*
	   CPushRoutingFrame push(this);
	   return CWnd::OnCmdMsg(nID,nCode,pExtra,pHandlerInfo);
	 */

	return __super::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
}

//////////////////////////////////////////////////////////////////////////
BOOL CHyperGraphDialog::OnInitDialog()
{
	ModifyStyleEx(WS_EX_CLIENTEDGE, 0);

	CRect rc;
	GetClientRect(&rc);

	// Command bars (menu bar)
	try
	{
		if (!InitCommandBars())
			return -1;

	}
	catch (CResourceException* e)
	{
		e->Delete();
		return -1;
	}
	CXTPCommandBars* pCommandBars = GetCommandBars();
	if (pCommandBars == NULL)
	{
		TRACE0("Failed to create command bars object.\n");
		return -1;      // fail to create
	}

	LoadPersonalization();
	CMFCUtils::LoadShortcuts(pCommandBars, IDR_HYPERGRAPH, "FlowGraph");

	// Add the menu bar
	CXTPCommandBar* pMenuBar = pCommandBars->SetMenu(_T("Menu Bar"), IDR_HYPERGRAPH);
	ASSERT(pMenuBar);
	pMenuBar->SetFlags(xtpFlagStretched);
	pMenuBar->EnableCustomization(TRUE);

	// Toolbar
	CXTPToolBar* pStdToolBar = pCommandBars->Add(_T("HyperGraph ToolBar"), xtpBarTop);
	VERIFY(pStdToolBar->LoadToolBar(IDR_FLOWGRAPH_BAR));

	// theme
	GetDockingPaneManager()->InstallDockingPanes(this);
	GetDockingPaneManager()->SetTheme(xtpPaneThemeOffice2003);
	GetDockingPaneManager()->SetThemedFloatingFrames(TRUE);

	// set tool status
	CFlowGraphSearchOptions::GetSearchOptions()->Serialize(true);
	m_componentsReportCtrl.SetComponentFilterMask(m_componentsViewMask);
	m_view.SetComponentViewMask(m_componentsViewMask);

	// Right pane needs to be created before left pane since left pane will receive window events right after creation
	// and some of the event handler ( OnGraphsSelChanged ) access objects that are created in CreateRightPane
	CreateRightPane();
	CreateLeftPane();

	// Create View.
	m_view.Create(WS_CHILD | WS_CLIPCHILDREN | WS_VISIBLE, rc, this, AFX_IDW_PANE_FIRST);

	// Set layout
	CXTRegistryManager regMgr;
	int paneLayoutVersion = regMgr.GetProfileInt(_T("FlowGraph"), _T("FlowGraphLayoutVersion"), 0);
	if (paneLayoutVersion == FlowGraphLayoutVersion)
	{
		CXTPDockingPaneLayout layout(GetDockingPaneManager());
		if (layout.Load(_T("FlowGraphLayout")))
		{
			if (layout.GetPaneList().GetCount() > 0)
				GetDockingPaneManager()->SetLayout(&layout);
		}
	}
	else
	{
		regMgr.WriteProfileInt(_T("FlowGraph"), _T("FlowGraphLayoutVersion"), FlowGraphLayoutVersion);
	}

	// Title bar with the current graph name
	m_titleBar.Create(this);
	static UINT indicators[] = { 1 };
	m_titleBar.SetIndicators(indicators, 1);
	m_titleBar.SetPaneInfo(0, 1, SBPS_STRETCH, 100);
	UpdateTitle(nullptr);
	if (!m_isTitleBarVisible)
		m_titleBar.ShowWindow(SW_HIDE);

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::PostNcDestroy()
{
	delete this;
}

//////////////////////////////////////////////////////////////////////////
BOOL CHyperGraphDialog::PreTranslateMessage(MSG* pMsg)
{
	bool bFramePreTranslate = true;
	if (pMsg->message >= WM_KEYFIRST && pMsg->message <= WM_KEYLAST)
	{
		CWnd* pWnd = CWnd::GetFocus();
		if (pWnd && pWnd->IsKindOf(RUNTIME_CLASS(CEdit)))
			bFramePreTranslate = false;
	}

	if (bFramePreTranslate)
	{
		// allow tooltip messages to be filtered
		if (__super::PreTranslateMessage(pMsg))
			return TRUE;
	}

	if (pMsg->message >= WM_KEYFIRST && pMsg->message <= WM_KEYLAST)
	{
		// All keypresses are translated by this frame window

		::TranslateMessage(pMsg);
		::DispatchMessage(pMsg);

		return TRUE;
	}

	return FALSE;
}

void CHyperGraphDialog::OnDestroy()
{
	SavePersonalization();

	CXTPDockingPaneLayout layout(GetDockingPaneManager());
	GetDockingPaneManager()->GetLayout(&layout);
	layout.Save(_T("FlowGraphLayout"));
	CFlowGraphSearchOptions::GetSearchOptions()->Serialize(false);

	__super::OnDestroy();
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::CreateRightPane()
{
	const uint panelWidth = 220;
	CRect rc(0, 0, panelWidth, 550);

	//////////////////////////////////////////////////////////////////////////
	// Create properties control.
	CXTPDockingPane* pDockPane = GetDockingPaneManager()->CreatePane(IDW_HYPERGRAPH_RIGHT_PANE, rc, xtpPaneDockRight);
	pDockPane->SetTitle(_T("Properties"));

	m_wndTaskPanel.Create(WS_CHILD, rc, this, 0);

	m_wndTaskPanel.SetBehaviour(xtpTaskPanelBehaviourExplorer);
	m_wndTaskPanel.SetTheme(xtpTaskPanelThemeOffice2000);
	m_wndTaskPanel.SetAnimation(xtpTaskPanelAnimationNo);
	m_wndTaskPanel.SetHotTrackStyle(xtpTaskPanelHighlightItem);
	m_wndTaskPanel.SetSelectItemOnFocus(TRUE);
	m_wndTaskPanel.AllowDrag(FALSE);

	m_wndTaskPanel.GetPaintManager()->m_rcGroupOuterMargins.SetRect(0, 0, 0, 0);
	m_wndTaskPanel.GetPaintManager()->m_rcGroupInnerMargins.SetRect(0, 0, 0, 0);
	m_wndTaskPanel.GetPaintManager()->m_rcItemOuterMargins.SetRect(0, 0, 0, 0);
	m_wndTaskPanel.GetPaintManager()->m_rcItemInnerMargins.SetRect(1, 1, 1, 1);
	m_wndTaskPanel.GetPaintManager()->m_rcControlMargins.SetRect(2, 0, 2, 0);
	m_wndTaskPanel.GetPaintManager()->m_nGroupSpacing = 0;

	m_pWndProps->Create(WS_CHILD, rc, &m_wndTaskPanel);
	m_pWndProps->SetItemHeight(16);
	m_view.SetPropertyCtrl(m_pWndProps.get());

	// sel. node inputs
	m_pPropertiesFolder = m_wndTaskPanel.AddGroup(ID_NODE_INPUTS_GROUP);
	m_pPropertiesFolder->SetCaption("Selected Node Inputs");
	m_pPropertiesFolder->SetItemLayout(xtpTaskItemLayoutDefault);
	m_pPropertiesItem = m_pPropertiesFolder->AddControlItem(m_pWndProps->GetSafeHwnd());
	m_wndTaskPanel.ExpandGroup(m_pPropertiesFolder, FALSE);

	// sel. node info
	m_pSelNodeInfo = new CHGSelNodeInfoPanel(this, &m_wndTaskPanel);
	m_pSelNodeInfo->Create(WS_CHILD | WS_BORDER, CRect(0, 0, 0, 0), this, ID_NODE_INFO_GROUP);

	// graph properties
	m_pGraphProperties = new CHGGraphPropsPanel(this, &m_wndTaskPanel);
	m_pGraphProperties->Create(WS_CHILD | WS_BORDER, CRect(0, 0, 0, 0), this, ID_GRAPH_INFO_GROUP);

	// graph tokens
	m_pGraphTokens = new CFlowGraphTokensCtrl(this, &m_wndTaskPanel);
	m_pGraphTokens->Create(WS_CHILD | WS_BORDER, CRect(0, 0, 0, 0), this, ID_GRAPH_TOKENS_GROUP);


	//////////////////////////////////////////////////////////////////////////
	// Search 
	CXTPDockingPane* pSearchPane = GetDockingPaneManager()->CreatePane(IDW_HYPERGRAPH_SEARCH_PANE, CRect(0, 0, panelWidth, 285), xtpPaneDockBottom, pDockPane);
	pSearchPane->SetTitle(_T("Search"));
	m_pSearchOptionsView = new CFlowGraphSearchCtrl(this);

	CXTPDockingPane* pSearchResultsPane = GetDockingPaneManager()->CreatePane(IDW_HYPERGRAPH_SEARCHRESULTS_PANE, rc, xtpPaneDockBottom, pSearchPane);
	pSearchResultsPane->SetTitle(_T("SearchResults"));
	//pSearchResultsPane->SetMinTrackSize(CSize(100,50));

	m_pSearchResultsCtrl = new CFlowGraphSearchResultsCtrl(this);
	m_pSearchResultsCtrl->Create(WS_CHILD | WS_BORDER, CRect(0, 0, 0, 0), this, 0);
	m_pSearchResultsCtrl->SetOwner(this);

	// set the results control for the search control
	m_pSearchOptionsView->SetResultsCtrl(m_pSearchResultsCtrl);
	m_pSearchOptionsView->SetOwner(this);

	//////////////////////////////////////////////////////////////////////////
	// Breakpoints
	CXTPDockingPane* pBreakpoints = GetDockingPaneManager()->CreatePane(IDW_HYPERGRAPH_BREAKPOINTS_PANE, CRect(0, 0, panelWidth, 200), xtpPaneDockBottom, pSearchResultsPane);
	pBreakpoints->SetTitle(_T("Breakpoints"));
	m_breakpointsTreeCtrl.Create(WS_CHILD | WS_TABSTOP, rc, this, IDC_BREAKPOINTS);
}


void CHyperGraphDialog::CreateLeftPane()
{
	CRect rc(0, 0, 220, 500);

	//////////////////////////////////////////////////////////////////////////
	// Nodes (previously called components)
	CXTPDockingPane* pComponentsPane = GetDockingPaneManager()->CreatePane(IDW_HYPERGRAPH_COMPONENTS_PANE, rc, xtpPaneDockLeft);
	pComponentsPane->SetTitle(_T("Nodes"));

	m_componentsTaskPanel.Create(WS_CHILD, rc, this, 0);
	m_componentsTaskPanel.SetBehaviour(xtpTaskPanelBehaviourExplorer);
	m_componentsTaskPanel.SetTheme(xtpTaskPanelThemeNativeWinXP);
	m_componentsTaskPanel.SetHotTrackStyle(xtpTaskPanelHighlightItem);
	m_componentsTaskPanel.SetSelectItemOnFocus(TRUE);
	m_componentsTaskPanel.AllowDrag(FALSE);
	m_componentsTaskPanel.SetAnimation(xtpTaskPanelAnimationNo);
	m_componentsTaskPanel.GetPaintManager()->m_rcGroupOuterMargins.SetRect(0, 0, 0, 0);
	m_componentsTaskPanel.GetPaintManager()->m_rcGroupInnerMargins.SetRect(0, 0, 0, 0);
	m_componentsTaskPanel.GetPaintManager()->m_rcItemOuterMargins.SetRect(0, 0, 0, 0);
	m_componentsTaskPanel.GetPaintManager()->m_rcItemInnerMargins.SetRect(1, 1, 1, 1);
	m_componentsTaskPanel.GetPaintManager()->m_rcControlMargins.SetRect(2, 0, 2, 0);
	m_componentsTaskPanel.GetPaintManager()->m_nGroupSpacing = 0;

	CXTPTaskPanelGroup* pNodeGroup = m_componentsTaskPanel.AddGroup(ID_COMPONENTS_GROUP);
	pNodeGroup->ShowCaption(FALSE);
	pNodeGroup->AddTextItem(_T("Search keyword:"));
	m_componentSearchEdit.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE, CRect(0, 0, 90, 20), this, IDC_COMPONENT_SEARCH);
	m_componentSearchEdit.SetFont(CFont::FromHandle((HFONT)SMFCFonts::GetInstance().hSystemFont));
	m_componentSearchEdit.SetParent(&m_componentsTaskPanel);
	pNodeGroup->AddControlItem(m_componentSearchEdit.GetSafeHwnd());

	pNodeGroup->AddTextItem(_T("Reference:"));
	m_componentsReportCtrl.Create(WS_CHILD | WS_TABSTOP, CRect(0, 0, 220, 1100), this, IDC_HYPERGRAPH_COMPONENTS);
	m_componentsReportCtrl.SetDialog(this);
	m_componentsReportCtrl.SetParent(&m_componentsTaskPanel);
	m_componentsReportCtrl.Reload();
	pNodeGroup->AddControlItem(m_componentsReportCtrl.GetSafeHwnd());

	//////////////////////////////////////////////////////////////////////////
	// Graphs
	CXTPDockingPane* pGraphsPane = GetDockingPaneManager()->CreatePane(IDW_HYPERGRAPH_TREE_PANE, rc, xtpPaneDockBottom, pComponentsPane);
	pGraphsPane->SetTitle(_T("Graphs"));

	m_graphsTreeCtrl.Create(WS_CHILD | WS_TABSTOP, rc, this, IDC_HYPERGRAPH_GRAPHS);
	m_graphsTreeCtrl.EnableMultiSelect(TRUE); // some operations support multiple selected graphs, such as Delete or Disable All
	m_graphsTreeCtrl.FullReload();
}


/////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnViewSelectionChange()
{
	if (!m_pPropertiesItem)
		return;

	m_wndTaskPanel.ExpandGroup(m_pPropertiesFolder, FALSE);

	CRect rc;
	m_wndTaskPanel.GetClientRect(rc);
	// Resize to fit properties.
	int h = m_pWndProps->GetVisibleHeight();
	m_pWndProps->SetWindowPos(NULL, 0, 0, rc.right, h + 1, SWP_NOMOVE);

	m_pWndProps->GetClientRect(rc);

	::GetClientRect(m_pWndProps->GetSafeHwnd(), rc);

	m_pPropertiesItem->SetControlHandle(m_pWndProps->GetSafeHwnd());
	m_wndTaskPanel.Reposition(FALSE);

	std::vector<CHyperNode*> nodes;
	m_view.GetSelectedNodes(nodes);
	m_pSelNodeInfo->SetNodeInfo(nodes);

	CXTPTaskPanelGroup* pGraphGroup = m_wndTaskPanel.FindGroup(ID_NODE_INPUTS_GROUP);
	if (pGraphGroup)
		pGraphGroup->SetExpanded(TRUE);

}

//////////////////////////////////////////////////////////////////////////
LRESULT CHyperGraphDialog::OnDockingPaneNotify(WPARAM wParam, LPARAM lParam)
{
	if (wParam == XTP_DPN_SHOWWINDOW)
	{
		// get a pointer to the docking pane being shown.
		CXTPDockingPane* pwndDockWindow = (CXTPDockingPane*)lParam;
		if (!pwndDockWindow->IsValid())
		{
			switch (pwndDockWindow->GetID())
			{
			case IDW_HYPERGRAPH_RIGHT_PANE:
				pwndDockWindow->Attach(&m_wndTaskPanel);
				m_wndTaskPanel.ShowWindow(SW_SHOW);
				break;
			case IDW_HYPERGRAPH_TREE_PANE:
				pwndDockWindow->Attach(&m_graphsTreeCtrl);
				m_graphsTreeCtrl.ShowWindow(SW_SHOW);
				break;
			case IDW_HYPERGRAPH_COMPONENTS_PANE:
				pwndDockWindow->Attach(&m_componentsTaskPanel);
				m_componentsTaskPanel.ShowWindow(SW_SHOW);
				break;
			case IDW_HYPERGRAPH_SEARCH_PANE:
				pwndDockWindow->Attach(m_pSearchOptionsView);
				m_pSearchOptionsView->ShowWindow(SW_SHOW);
				break;
			case IDW_HYPERGRAPH_SEARCHRESULTS_PANE:
				pwndDockWindow->Attach(m_pSearchResultsCtrl);
				m_pSearchResultsCtrl->ShowWindow(SW_SHOW);
				break;
			case IDW_HYPERGRAPH_BREAKPOINTS_PANE:
				pwndDockWindow->Attach(&m_breakpointsTreeCtrl);
				m_breakpointsTreeCtrl.ShowWindow(SW_SHOW);
				break;
			default:
				return FALSE;
			}
		}
		return TRUE;
	}
	else if (wParam == XTP_DPN_CLOSEPANE)
	{
		// get a pointer to the docking pane being shown.
		CXTPDockingPane* pwndDockWindow = (CXTPDockingPane*)lParam;
		if (pwndDockWindow->IsValid())
		{
			switch (pwndDockWindow->GetID())
			{
			case IDW_HYPERGRAPH_RIGHT_PANE:
				break;
			}
		}
	}

	return FALSE;
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::PrepareLevelUnload()
{
	SetGraph(nullptr);
	m_graphsTreeCtrl.PrepareLevelUnload();
	m_graphList.clear();
}

void CHyperGraphDialog::SetGraph(CHyperGraph* pGraph)
{
	if (m_view.GetGraph() == pGraph)
		return;

	m_view.SetGraph(pGraph);
	m_graphsTreeCtrl.SetCurrentGraph(pGraph);
	m_pGraphProperties->SetGraph(pGraph);
	m_pGraphTokens->SetGraph(pGraph);
	UpdateTitle(pGraph);

	// graph navigation history
	if (pGraph != 0)
	{
		std::list<CHyperGraph*>::iterator iter = std::find(m_graphList.begin(), m_graphList.end(), pGraph);
		if (iter != m_graphList.end())
		{
			m_graphIterCur = iter;
		}
		else
		{
			// maximum entry 20
			while (m_graphList.size() > 20)
			{
				m_graphList.pop_front();
			}
			m_graphIterCur = m_graphList.insert(m_graphList.end(), pGraph);
		}
	}
	else
		m_graphIterCur = m_graphList.end();
}

CHyperGraph* CHyperGraphDialog::GetGraph() const
{
	return m_view.GetGraph();
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::NewGraph()
{
	CHyperGraph* pGraph = static_cast<CHyperGraph*>(GetIEditorImpl()->GetFlowGraphManager()->CreateGraph());

	if (pGraph)
		pGraph->AddRef();

	SetGraph(pGraph);
}


void CHyperGraphDialog::OnFileNew()
{
	NewGraph();
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnFileSaveAllExternal()
{
	for (std::list<CHyperGraph*>::iterator iter = m_graphList.begin(); iter != m_graphList.end(); ++iter)
	{
		CHyperGraph* pGraph = *iter;

		if (pGraph->IsFlowGraph())
		{
			CHyperFlowGraph* pFlowGraph = static_cast<CHyperFlowGraph*>(pGraph);
			IFlowGraph::EFlowGraphType type = pFlowGraph->GetIFlowGraph()->GetType();
			const bool bIsEntityGraph = (type == IFlowGraph::eFGT_Default) && (pFlowGraph->GetEntity() != nullptr);

			if (!bIsEntityGraph && pFlowGraph->IsModified())
			{
				SaveGraph(pGraph);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::SaveGraph(CHyperGraph* pGraph /*, bool bCopyToFile*/)
{
	if (!pGraph)
		return;

	CString filename = pGraph->GetFilename();
	filename = PathUtil::ReplaceExtension(filename.GetString(), ".xml");
	if (filename.IsEmpty())
	{
		ExportGraph(pGraph);
	}
	else
	{
		if (pGraph->IsFlowGraph())
		{
			bool bSuccess = false;
			CHyperFlowGraph* pFlowGraph = static_cast<CHyperFlowGraph*>(pGraph);
			if (pFlowGraph->GetIFlowGraph()->GetType() == IFlowGraph::eFGT_Module)
			{
				bSuccess = GetIEditorImpl()->GetFlowGraphModuleManager()->SaveModuleGraph(pFlowGraph);
			}
			else
			{
				bSuccess = pGraph->Save(filename);
			}

			if (bSuccess)
			{
				string logMessage;
				logMessage.Format("Saved graph '%s' to '%s'", pGraph->GetName(), filename);
				CLogFile::WriteLine(logMessage.c_str());
			}
			else
			{
				CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "Failed to save graph '%s' to '%s'", pGraph->GetName(), filename);
			}
		}

		// After saving custom action graph, flag it so in-memory copy in CCustomActionManager will be reloaded from xml
		ICustomAction* pCustomAction = pGraph->GetCustomAction();
		if (pCustomAction)
		{
			pCustomAction->Invalidate();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::ExportGraph(CHyperGraph* pGraph)
{
	if (!pGraph)
	{
		Warning("Please first select a FlowGraph to export");
		return;
	}

	CString filename = CString(pGraph->GetName()) + ".xml";
	CString dir = PathUtil::GetPathWithoutFilename(filename);
	if (CFileUtil::SelectSaveFile(GRAPH_FILE_FILTER, "xml", dir, filename))
	{
		if (pGraph->Save(filename))
		{
			string logMessage;
			logMessage.Format("Successfully exported graph '%s' to '%s'", pGraph->GetName(), filename);
			CLogFile::WriteLine(logMessage.c_str());
		}
		else
		{
			Warning("problem exporting graph '%s'", pGraph->GetName());
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::ExportGraphSelection()
{
	CHyperGraph* pGraph = m_view.GetGraph();
	if (!pGraph)
		return;

	std::vector<CHyperNode*> nodes;
	if (!m_view.GetSelectedNodes(nodes))
	{
		Warning("You must have selected graph nodes to export");
		return;
	}

	CString filename = CString(pGraph->GetName()) + ".xml";
	CString dir = PathUtil::GetPathWithoutFilename(filename);
	if (CFileUtil::SelectSaveFile(GRAPH_FILE_FILTER, "xml", dir, filename))
	{
		CWaitCursor wait;
		CHyperGraphSerializer serializer(pGraph, 0);

		for (int i = 0; i < nodes.size(); i++)
		{
			CHyperNode* pNode = nodes[i];
			serializer.SaveNode(pNode, true);
		}

		if (nodes.size() > 0)
		{
			XmlNodeRef node = XmlHelpers::CreateXmlNode("Graph");
			serializer.Serialize(node, false);

			if (!XmlHelpers::SaveXmlNode(node, filename))
			{
				Warning("FlowGraph not saved to file '%s'", filename.GetString());
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnFileNewGlobalFGModule()
{
	NewFGModule();
}

void CHyperGraphDialog::OnFileNewLevelFGModule()
{
	if (GetIEditorImpl()->GetGameEngine()->IsLevelLoaded())
	{
		NewFGModule(false);
	}
}

void CHyperGraphDialog::NewFGModule(bool bGlobal /*=true*/)
{
	string filename;
	CEditorFlowGraphModuleManager* pModuleManager = GetIEditorImpl()->GetFlowGraphModuleManager();

	if (!pModuleManager)
		return;

	if (pModuleManager->NewModule(filename, bGlobal))
	{
		IFlowGraphPtr pModuleGraph = pModuleManager->GetModuleFlowGraph(PathUtil::GetFileName(filename));
		if (pModuleGraph)
		{
			CFlowGraphManager* pManager = GetIEditorImpl()->GetFlowGraphManager();
			CHyperFlowGraph* pFlowGraph = pManager->FindGraph(pModuleGraph);
			assert(pFlowGraph);
			if (pFlowGraph)
			{
				pFlowGraph->GetIFlowGraph()->SetType(IFlowGraph::eFGT_Module);
				pManager->OpenView(pFlowGraph);
			}
		}
		else
		{
			SetGraph(nullptr);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnFileNewAction()
{
	string filename;
	if (GetIEditorImpl()->GetAI()->NewAction(filename))
	{
		IAIAction* pAction = gEnv->pAISystem->GetAIActionManager()->GetAIAction(PathUtil::GetFileName(filename.GetString()));
		if (pAction)
		{
			CFlowGraphManager* pManager = GetIEditorImpl()->GetFlowGraphManager();
			CHyperFlowGraph* pFlowGraph = pManager->FindGraphForAction(pAction);
			assert(pFlowGraph);
			if (pFlowGraph)
			{
				pFlowGraph->GetIFlowGraph()->SetType(IFlowGraph::eFGT_AIAction);
				pManager->OpenView(pFlowGraph);
			}
		}
		else
		{
			SetGraph(nullptr);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnEditGraphTokens()
{
	if (CHyperGraph* pGraph = m_view.GetGraph())
	{
		if (pGraph->IsFlowGraph())
		{
			CHyperFlowGraph* pFlowGraph = (CHyperFlowGraph*)pGraph;
			IFlowGraph* pIFlowGraph = pFlowGraph->GetIFlowGraph();
			if (pIFlowGraph)
			{
				// get latest tokens from graph
				size_t tokenCount = pIFlowGraph->GetGraphTokenCount();
				TTokens newTokens;
				newTokens.reserve(tokenCount);
				for (size_t i = 0; i < tokenCount; ++i)
				{
					const IFlowGraph::SGraphToken* graphToken = pIFlowGraph->GetGraphToken(i);
					STokenData newToken;
					newToken.name = graphToken->name.c_str();
					newToken.type = graphToken->type;
					newTokens.push_back(newToken);
				}

				CFlowGraphTokensDlg dlg(this);
				dlg.SetTokenData(newTokens);

				dlg.DoModal();

				// get token list from dialog, set on graph
				TTokens tokens = dlg.GetTokenData();

				pIFlowGraph->RemoveGraphTokens();

				tokenCount = tokens.size();
				for (size_t i = 0; i < tokenCount; ++i)
				{
					IFlowGraph::SGraphToken graphToken;
					graphToken.name = tokens[i].name.GetBuffer();
					graphToken.type = tokens[i].type;

					pIFlowGraph->AddGraphToken(graphToken);
				}

				m_pGraphTokens->UpdateGraphProperties(pGraph);
				pGraph->SetModified(true);

				pFlowGraph->SendNotifyEvent(nullptr, EHG_GRAPH_TOKENS_UPDATED);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnEditModuleInputsOutputs()
{
	if (CHyperGraph* pGraph = m_view.GetGraph())
	{
		if (pGraph->IsFlowGraph())
		{
			CHyperFlowGraph* pFlowGraph = static_cast<CHyperFlowGraph*>(pGraph);

			if (pFlowGraph->GetIFlowGraph()->GetType() == IFlowGraph::eFGT_Module)
			{
				IFlowGraphModule* pModule = gEnv->pFlowSystem->GetIModuleManager()->GetModule(pGraph->GetName());

				if (pModule)
				{
					CFlowGraphEditModuleDlg dlg(pModule, this);
					dlg.DoModal();
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnFileNewUIAction()
{
	CString filename;
	if (gEnv->pFlashUI && GetIEditorImpl()->GetUIManager()->NewUIAction(filename))
	{
		IUIAction* pUIAction = gEnv->pFlashUI->GetUIAction(PathUtil::GetFileName((const char*)filename));
		if (pUIAction)
		{
			CFlowGraphManager* pManager = GetIEditorImpl()->GetFlowGraphManager();
			CHyperFlowGraph* pFlowGraph = pManager->FindGraphForUIAction(pUIAction);
			assert(pFlowGraph);
			if (pFlowGraph)
				pManager->OpenView(pFlowGraph);
		}
		else
			SetGraph(NULL);
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnFileNewCustomAction()
{
	CString filename;
	if (GetIEditorImpl()->GetCustomActionManager()->NewCustomAction(filename))
	{
		ICustomActionManager* pCustomActionManager = gEnv->pGameFramework->GetICustomActionManager();
		CRY_ASSERT(pCustomActionManager != NULL);
		if (!pCustomActionManager)
			return;

		ICustomAction* pCustomAction = pCustomActionManager->GetCustomActionFromLibrary(PathUtil::GetFileName((const char*)filename));
		if (pCustomAction)
		{
			CFlowGraphManager* pFlowGraphManager = GetIEditorImpl()->GetFlowGraphManager();
			CHyperFlowGraph* pFlowGraph = pFlowGraphManager->FindGraphForCustomAction(pCustomAction);
			assert(pFlowGraph);
			if (pFlowGraph)
				pFlowGraphManager->OpenView(pFlowGraph);
		}
		else
			SetGraph(NULL);
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnFileNewMatFXGraph()
{
	CString filename;
	CHyperGraph* pGraph = NULL;
	if (GetIEditorImpl()->GetMatFxGraphManager()->NewMaterialFx(filename, &pGraph))
	{
		GetIEditorImpl()->GetFlowGraphManager()->OpenView(pGraph);
	}
	else
	{
		SetGraph(NULL);
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnFileImport()
{
	CHyperGraph* pGraph = m_view.GetGraph();
	if (!pGraph)
	{
		Warning("Please first create a FlowGraph to which to import");
		return;
	}

	CString filename;
	// if (CFileUtil::SelectSingleFile( EFILE_TYPE_ANY,filename,GRAPH_FILE_FILTER )) // game dialog
	if (CFileUtil::SelectFile(GRAPH_FILE_FILTER, "", filename))   // native dialog
	{
		CWaitCursor wait;
		bool ok = m_view.GetGraph()->Import(filename);
		if (!ok)
		{
			Warning("Cannot import file '%s'.\nMake sure the file is actually a FlowGraph XML File.", filename.GetString());
		}
		m_view.InvalidateView(true);
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnFileExport()
{
	ExportGraph(m_view.GetGraph());
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnFind()
{
	CXTPDockingPane* pSearchPane = GetDockingPaneManager()->FindPane(IDW_HYPERGRAPH_SEARCH_PANE);
	assert(pSearchPane != 0);

	// show the pane
	GetDockingPaneManager()->ShowPane(pSearchPane);

	// goto combo box
	CWnd* pWnd = m_pSearchOptionsView->GetDlgItem(IDC_COMBO_FIND);
	assert(pWnd != 0);
	m_pSearchOptionsView->GotoDlgCtrl(pWnd);
}

//////////////////////////////////////////////////////////////////////////
BOOL CHyperGraphDialog::OnComponentsViewToggle(UINT nID)
{
	switch (nID)
	{
	case ID_COMPONENTS_APPROVED:
		m_componentsViewMask ^= EFLN_APPROVED;
		break;
	case ID_COMPONENTS_ADVANCED:
		m_componentsViewMask ^= EFLN_ADVANCED;
		break;
	case ID_COMPONENTS_DEBUG:
		m_componentsViewMask ^= EFLN_DEBUG;
		break;
	case ID_COMPONENTS_OBSOLETE:
		m_componentsViewMask ^= EFLN_OBSOLETE;
		break;
	default:
		return FALSE;
	}
	SavePersonalization();
	m_componentsReportCtrl.SetComponentFilterMask(m_componentsViewMask);
	m_componentsReportCtrl.Reload();
	m_view.SetComponentViewMask(m_componentsViewMask);
	return TRUE;
}

void CHyperGraphDialog::OnComponentsViewUpdate(CCmdUI* pCmdUI)
{
	uint32 flag;
	switch (pCmdUI->m_nID)
	{
	case ID_COMPONENTS_APPROVED:
		flag = EFLN_APPROVED;
		break;
	case ID_COMPONENTS_ADVANCED:
		flag = EFLN_ADVANCED;
		break;
	case ID_COMPONENTS_DEBUG:
		flag = EFLN_DEBUG;
		break;
	case ID_COMPONENTS_OBSOLETE:
		flag = EFLN_OBSOLETE;
		break;
	default:
		return;
	}
	pCmdUI->SetCheck((m_componentsViewMask & flag) ? 1 : 0);
}

BOOL CHyperGraphDialog::OnDebugFlowgraphTypeToggle(UINT nID)
{
	if (!m_pFlowGraphDebugger)
		return FALSE;

	IFlowGraph::EFlowGraphType type;

	switch (nID)
	{
	case ID_FLOWGRAPH_TYPE_AIACTION:
		type = IFlowGraph::eFGT_AIAction;
		break;
	case ID_FLOWGRAPH_TYPE_UIACTION:
		type = IFlowGraph::eFGT_UIAction;
		break;
	case ID_FLOWGRAPH_TYPE_MODULE:
		type = IFlowGraph::eFGT_Module;
		break;
	case ID_FLOWGRAPH_TYPE_CUSTOMACTION:
		type = IFlowGraph::eFGT_CustomAction;
		break;
	case ID_FLOWGRAPH_TYPE_MATERIALFX:
		type = IFlowGraph::eFGT_MaterialFx;
		break;
	default:
		return FALSE;
	}

	const bool ignored = m_pFlowGraphDebugger->IsFlowgraphTypeIgnored(type);
	m_pFlowGraphDebugger->IgnoreFlowgraphType(type, !ignored);

	return TRUE;
}

void CHyperGraphDialog::OnDebugFlowgraphTypeUpdate(CCmdUI* pCmdUI)
{
	if (!m_pFlowGraphDebugger)
		return;

	IFlowGraph::EFlowGraphType type;

	switch (pCmdUI->m_nID)
	{
	case ID_FLOWGRAPH_TYPE_AIACTION:
		type = IFlowGraph::eFGT_AIAction;
		break;
	case ID_FLOWGRAPH_TYPE_UIACTION:
		type = IFlowGraph::eFGT_UIAction;
		break;
	case ID_FLOWGRAPH_TYPE_MODULE:
		type = IFlowGraph::eFGT_Module;
		break;
	case ID_FLOWGRAPH_TYPE_CUSTOMACTION:
		type = IFlowGraph::eFGT_CustomAction;
		break;
	case ID_FLOWGRAPH_TYPE_MATERIALFX:
		type = IFlowGraph::eFGT_MaterialFx;
		break;
	default:
		return;
	}

	const bool ignored = m_pFlowGraphDebugger->IsFlowgraphTypeIgnored(type);
	pCmdUI->SetCheck(ignored ? 1 : 0);
}

void CHyperGraphDialog::OnComponentBeginDrag(CXTPReportRow* pRow, CPoint pt)
{
	if (!pRow)
		return;
	CComponentRecord* pRec = static_cast<CComponentRecord*>(pRow->GetRecord());
	if (!pRec)
		return;
	if (pRec->IsGroup())
		return;

	m_pDragImage = m_componentsReportCtrl.CreateDragImage(pRow);
	if (m_pDragImage)
	{
		m_pDragImage->BeginDrag(0, CPoint(-10, -10));

		m_dragNodeClass = pRec->GetName();

		ScreenToClient(&pt);

		CRect rc;
		GetWindowRect(rc);

		CPoint p = pt;
		ClientToScreen(&p);
		p.x -= rc.left;
		p.y -= rc.top;

		m_componentsReportCtrl.RedrawControl();
		m_pDragImage->DragEnter(this, p);
		SetCapture();
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnMouseMove(UINT nFlags, CPoint point)
{
	if (m_pDragImage)
	{
		CRect rc;
		GetWindowRect(rc);
		CPoint p = point;
		ClientToScreen(&p);
		p.x -= rc.left;
		p.y -= rc.top;
		m_pDragImage->DragMove(p);
		return;
	}

	__super::OnMouseMove(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnLButtonUp(UINT nFlags, CPoint point)
{
	if (m_pDragImage)
	{
		CPoint p;
		GetCursorPos(&p);

		::ReleaseCapture();
		m_pDragImage->DragLeave(this);
		m_pDragImage->EndDrag();
		m_pDragImage->DeleteImageList();
		// delete m_pDragImage; // FIXME: crashes
		m_pDragImage = 0;

		CWnd* wnd = CWnd::WindowFromPoint(p);
		if (wnd == &m_view)
		{
			m_view.ScreenToClient(&p);
			m_view.CreateNode(m_dragNodeClass, p);
		}

		return;
	}
	__super::OnLButtonUp(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnPlay()
{
	if (m_pFlowGraphDebugger)
		m_pFlowGraphDebugger->EnableStepMode(false);

	GetIEditorImpl()->GetFlowGraphDebuggerEditor()->ResumeGame();
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnStop()
{
	GetIEditorImpl()->GetGameEngine()->EnableFlowSystemUpdate(false);
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnToggleDebug()
{
	ICVar* pToggle = gEnv->pConsole->GetCVar("fg_iEnableFlowgraphNodeDebugging");
	if (pToggle)
	{
		pToggle->Set((pToggle->GetIVal() > 0) ? 0 : 1);
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnEraseDebug()
{
	std::list<CHyperGraph*>::iterator iter = m_graphList.begin();
	for (; iter != m_graphList.end(); ++iter)
	{
		CHyperGraph* pGraph = *iter;
		if (pGraph && pGraph->IsNodeActivationModified())
		{
			pGraph->ClearDebugPortActivation();

			IHyperGraphEnumerator* pEnum = pGraph->GetNodesEnumerator();
			for (IHyperNode* pINode = pEnum->GetFirst(); pINode; pINode = pEnum->GetNext())
			{
				CHyperNode* pNode = (CHyperNode*)pINode;
				pNode->Invalidate(true, false);
			}
			pEnum->Release();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnPause()
{
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnStep()
{
	if (m_pFlowGraphDebugger)
	{
		if (m_pFlowGraphDebugger->BreakpointHit())
		{
			m_pFlowGraphDebugger->EnableStepMode(true);
			GetIEditorImpl()->GetFlowGraphDebuggerEditor()->ResumeGame();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnUpdatePlay(CCmdUI* pCmdUI)
{
	if (GetIEditorImpl()->GetGameEngine()->IsFlowSystemUpdateEnabled())
		pCmdUI->SetCheck(1);
	else
		pCmdUI->SetCheck(0);
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnUpdateStep(CCmdUI* pCmdUI)
{
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnUpdateDebug(CCmdUI* pCmdUI)
{
	ICVar* pToggle = gEnv->pConsole->GetCVar("fg_iEnableFlowgraphNodeDebugging");
	if (pToggle && pToggle->GetIVal() > 0)
		pCmdUI->SetCheck(1);
	else
		pCmdUI->SetCheck(0);
}

void CHyperGraphDialog::DisableGraph(CHyperFlowGraph* pFlowGraph, bool bEnable)
{
	if (pFlowGraph)
	{
		pFlowGraph->SetEnabled(bEnable);

		UpdateGraphProperties(pFlowGraph);

		m_graphsTreeCtrl.UpdateGraphLabel(pFlowGraph);
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnGraphsRightClick(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = TRUE;

	TDTreeItems chTreeItems;

	CPoint screenPoint;
	GetCursorPos(&screenPoint);

	m_graphsTreeCtrl.GetSelectedItems(chTreeItems);

	// If no item is selected, we will try to select the item under the cursor.
	if (chTreeItems.empty())
	{
		UINT uFlags;
		CPoint point(screenPoint);
		ScreenToClient(&point);
		HTREEITEM hCurrentItem = m_graphsTreeCtrl.HitTest(point, &uFlags);
		if (TVHT_ONITEM & uFlags)
		{
			m_graphsTreeCtrl.Select(hCurrentItem, TVGN_CARET);
			chTreeItems.push_back(hCurrentItem);
		}
		else
		{
			// If no item was under the cursos, there is nothing to be done here.
			return;
		}
	}

	CMenu menu;

	if (CreateContextMenu(menu, chTreeItems))
	{
		int nCmd = menu.TrackPopupMenu(TPM_RETURNCMD | TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_NONOTIFY, screenPoint.x, screenPoint.y, this);
		ProcessMenu(nCmd, chTreeItems);
	}
}

void CHyperGraphDialog::OnBreakpointsRightClick(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = 0;

	CPoint screenPoint;
	if (GetCursorPos(&screenPoint))
	{
		UINT uFlags;
		CPoint point(screenPoint);
		m_breakpointsTreeCtrl.ScreenToClient(&point);
		const HTREEITEM hCurrentItem = m_breakpointsTreeCtrl.HitTest(point, &uFlags);
		if (hCurrentItem && (TVHT_ONITEM & uFlags))
		{
			CMenu menu;
			if (CreateBreakpointContextMenu(menu, hCurrentItem))
			{
				const int nCmd = menu.TrackPopupMenu(TPM_RETURNCMD | TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_NONOTIFY, screenPoint.x, screenPoint.y, this);
				ProcessBreakpointMenu(nCmd, hCurrentItem);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::EnableItems(HTREEITEM hItem, bool bEnable, bool bRecurse)
{
	HTREEITEM hChildItem = m_graphsTreeCtrl.GetChildItem(hItem);
	while (hChildItem != NULL)
	{
		if (m_graphsTreeCtrl.ItemHasChildren(hChildItem))
			EnableItems(hChildItem, bEnable, bRecurse);

		CHyperGraph* pGraph = (CHyperGraph*)m_graphsTreeCtrl.GetItemData(hChildItem);
		if (pGraph)
		{
			CHyperFlowGraph* pFlowGraph = pGraph->IsFlowGraph() ? static_cast<CHyperFlowGraph*>(pGraph) : 0;
			assert(pFlowGraph);
			if (pFlowGraph)
			{
				DisableGraph(pFlowGraph, bEnable);
			}
		}
		hChildItem = m_graphsTreeCtrl.GetNextItem(hChildItem, TVGN_NEXT);
	}
}

void CHyperGraphDialog::RenameFlowGraph(CHyperFlowGraph* pFlowGraph, CString newName)
{
	if (pFlowGraph)
	{
		m_graphsTreeCtrl.RenameGraphItem(pFlowGraph->GetHTreeItem(), newName);
		pFlowGraph->SetName(newName);
		if (m_view.GetGraph() == pFlowGraph)
		{
			UpdateTitle(pFlowGraph);
		}
	}
}

void CHyperGraphDialog::RemoveGraph(CHyperGraph* pGraph)
{
	if (!pGraph)
		return;

	// remove graph from the tree list and navigation history.
	m_graphsTreeCtrl.DeleteGraphItem(pGraph);
	stl::find_and_erase(m_graphList, pGraph);
	// clear the view and properties if this was the active graph.
	if (m_view.GetGraph() == pGraph)
	{
		SetGraph(nullptr);
	}
}


//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnGraphsDblClick(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = TRUE;

	CPoint point;
	GetCursorPos(&point);
	m_graphsTreeCtrl.ScreenToClient(&point);

	CEntityObject* pOwnerEntity = 0;
	CHyperGraph* pGraph = 0;

	UINT uFlags;
	HTREEITEM hItem = m_graphsTreeCtrl.HitTest(point, &uFlags);
	if ((hItem != NULL) && (TVHT_ONITEM & uFlags))
	{
		pGraph = (CHyperGraph*)m_graphsTreeCtrl.GetItemData(hItem);
		if (pGraph)
		{
			pOwnerEntity = ((CHyperFlowGraph*)pGraph)->GetEntity();
		}
		if (pOwnerEntity)
		{
			CUndo undo("Select Object(s)");
			GetIEditorImpl()->ClearSelection();
			GetIEditorImpl()->SelectObject(pOwnerEntity);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnGraphsSelChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = TRUE;
	NM_TREEVIEW* pNMTreeView = (NM_TREEVIEW*)pNMHDR;
	CHyperGraph* pGraph = (CHyperGraph*)pNMTreeView->itemNew.lParam;
	if (pGraph)
	{
		SetGraph(pGraph);
	}
}

//////////////////////////////////////////////////////////////////////////
BOOL CHyperGraphDialog::OnToggleBar(UINT nID)
{
	switch (nID)
	{
	case ID_VIEW_TITLEBAR:
		{
			ShowControlBar(&m_titleBar, !m_isTitleBarVisible, false);
			m_isTitleBarVisible = !m_isTitleBarVisible;
			SavePersonalization();
			return TRUE;
		}
	case ID_VIEW_COMPONENTS:
		nID = IDW_HYPERGRAPH_COMPONENTS_PANE;
		break;
	case ID_VIEW_NODEINPUTS:
		nID = IDW_HYPERGRAPH_RIGHT_PANE;
		break;
	case ID_VIEW_GRAPHS:
		nID = IDW_HYPERGRAPH_TREE_PANE;
		break;
	case ID_VIEW_SEARCH:
		nID = IDW_HYPERGRAPH_SEARCH_PANE;
		break;
	case ID_VIEW_SEARCHRESULTS:
		nID = IDW_HYPERGRAPH_SEARCHRESULTS_PANE;
		break;
	case ID_VIEW_BREAKPOINTS:
		nID = IDW_HYPERGRAPH_BREAKPOINTS_PANE;
		break;
	default:
		return FALSE;
	}
	CXTPDockingPane* pBar = GetDockingPaneManager()->FindPane(nID);
	if (pBar)
	{
		if (pBar->IsClosed())
			GetDockingPaneManager()->ShowPane(pBar);
		else
			GetDockingPaneManager()->ClosePane(pBar);
		return TRUE;
	}
	return FALSE;
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnUpdateControlBar(CCmdUI* pCmdUI)
{
	UINT nID = 0;
	switch (pCmdUI->m_nID)
	{
	case ID_VIEW_TITLEBAR:
		{
			pCmdUI->SetCheck(m_isTitleBarVisible);
			return;
		}
	case ID_VIEW_COMPONENTS:
		nID = IDW_HYPERGRAPH_COMPONENTS_PANE;
		break;
	case ID_VIEW_NODEINPUTS:
		nID = IDW_HYPERGRAPH_RIGHT_PANE;
		break;
	case ID_VIEW_GRAPHS:
		nID = IDW_HYPERGRAPH_TREE_PANE;
		break;
	case ID_VIEW_SEARCH:
		nID = IDW_HYPERGRAPH_SEARCH_PANE;
		break;
	case ID_VIEW_SEARCHRESULTS:
		nID = IDW_HYPERGRAPH_SEARCHRESULTS_PANE;
		break;
	case ID_VIEW_BREAKPOINTS:
		nID = IDW_HYPERGRAPH_BREAKPOINTS_PANE;
		break;
	default:
		return;
	}
	CXTPCommandBars* pCommandBars = GetCommandBars();
	if (pCommandBars != NULL)
	{
		CXTPToolBar* pBar = pCommandBars->GetToolBar(nID);
		if (pBar)
		{
			pCmdUI->SetCheck((pBar->IsVisible()) ? 1 : 0);
			return;
		}
	}
	CXTPDockingPane* pBar = GetDockingPaneManager()->FindPane(nID);
	if (pBar)
	{
		pCmdUI->SetCheck((!pBar->IsClosed()) ? 1 : 0);
		return;
	}
	pCmdUI->SetCheck(0);
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::ShowResultsPane(bool bShow, bool bFocus)
{
	if (bShow)
	{
		CXTPDockingPane* pPane = GetDockingPaneManager()->FindPane(IDW_HYPERGRAPH_SEARCHRESULTS_PANE);
		assert(pPane != 0);
		GetDockingPaneManager()->ShowPane(pPane);
		if (bFocus)
			pPane->SetFocus();
	}
	else
		GetDockingPaneManager()->HidePane(IDW_HYPERGRAPH_SEARCHRESULTS_PANE);
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	switch (event)
	{
	case eNotify_OnClearLevelContents:
		m_bIgnoreObjectEvents = true;
		PrepareLevelUnload();
		break;
	case eNotify_OnBeginSceneOpen:
	case eNotify_OnBeginUndoRedo:
		m_bIgnoreObjectEvents = true;
		m_graphsTreeCtrl.SetIgnoreReloads(true);
		break;

	case eNotify_OnEndSceneOpen:
	case eNotify_OnEndUndoRedo:
		m_bIgnoreObjectEvents = false;
		m_graphsTreeCtrl.SetIgnoreReloads(false);
		m_graphsTreeCtrl.FullReload();
		break;
	case eNotify_OnEnableFlowSystemUpdate:
	case eNotify_OnDisableFlowSystemUpdate:
		{
			m_view.UpdateFrozen();
		}
		break;
	}
}

void CHyperGraphDialog::OnObjectEvent(CObjectEvent& eventObj)
{
	if (m_bIgnoreObjectEvents)
		return;

	CBaseObject* pObject = eventObj.m_pObj;
	if (!pObject)
	{
		return;
	}

	switch (eventObj.m_type)
	{
	case OBJECT_ON_GROUP_OPEN:
		{
			// add all graphs from the prefab entities that have containers
			if (pObject->IsKindOf(RUNTIME_CLASS(CPrefabObject)))
			{
				m_graphsTreeCtrl.SetRedraw(FALSE);
				CPrefabObject* pPrefabObject = static_cast<CPrefabObject*>(pObject);
				std::vector<CBaseObject*> prefabChildren;
				pPrefabObject->GetAllPrefabFlagedChildren(prefabChildren);
				for (size_t i = 0, numChildren = prefabChildren.size(); i < numChildren; ++i)
				{
					if (prefabChildren[i]->IsKindOf(RUNTIME_CLASS(CEntityObject)))
					{
						CBaseObject* pEntityObject = prefabChildren[i];
						CHyperFlowGraph* pFlowGraph = static_cast<CEntityObject*>(pEntityObject)->GetFlowGraph();
						if (pFlowGraph)
						{
							m_graphsTreeCtrl.AddGraphToTree(pFlowGraph, false);
							m_graphsTreeCtrl.EnsureVisible(pFlowGraph->GetHTreeItem());
						}
					}
				}
				m_graphsTreeCtrl.SetRedraw(TRUE);
				m_graphsTreeCtrl.Invalidate(TRUE);
			}
		}
		break;
	case OBJECT_ON_GROUP_CLOSE:
		{
			// remove all graphs from the prefab entities that have containers
			if (pObject->IsKindOf(RUNTIME_CLASS(CPrefabObject)))
			{
				std::vector<CBaseObject*> prefabChildren;
				pObject->GetAllPrefabFlagedChildren(prefabChildren);
				for (size_t i = 0, numChildren = prefabChildren.size(); i < numChildren; ++i)
				{
					if (prefabChildren[i]->IsKindOf(RUNTIME_CLASS(CEntityObject)))
					{
						CBaseObject* pEntityObject = prefabChildren[i];
						CHyperFlowGraph* pFlowGraph = static_cast<CEntityObject*>(pEntityObject)->GetFlowGraph();
						if (pFlowGraph)
						{
							RemoveGraph(pFlowGraph);
						}
					}
				}
			}
		}
		break;
	case OBJECT_ON_RENAME:
		{
			if (pObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
			{
				CHyperFlowGraph* pFlowGraph = static_cast<CEntityObject*>(pObject)->GetFlowGraph();
				CString newName = pObject->GetName().c_str();
				RenameFlowGraph(pFlowGraph, newName);
			}
			else if (pObject->IsKindOf(RUNTIME_CLASS(CPrefabObject)))
			{
				CPrefabObject* pPrefabObject = static_cast<CPrefabObject*>(pObject);
				if (pPrefabObject->IsOpen())
				{
					std::vector<CBaseObject*> prefabChildren;
					pPrefabObject->GetAllPrefabFlagedChildren(prefabChildren);
					for (size_t i = 0, numChildren = prefabChildren.size(); i < numChildren; ++i)
					{
						if (prefabChildren[i]->IsKindOf(RUNTIME_CLASS(CEntityObject)))
						{
							CBaseObject* pEntityObject = prefabChildren[i];
							CHyperFlowGraph* pFlowGraph = static_cast<CEntityObject*>(pEntityObject)->GetFlowGraph();
							if (pFlowGraph)
							{
								CString newName = pObject->GetName().c_str();
								m_graphsTreeCtrl.RenameParentFolder(pFlowGraph->GetHTreeItem(), newName);
								if (m_view.GetGraph() == pFlowGraph)
								{
									UpdateTitle(pFlowGraph);
								}
								return;
							}
						}
					}
				}
			}
			else if (pObject->IsKindOf(RUNTIME_CLASS(CGroup)))
			{
				TBaseObjects children;
				pObject->GetAllChildren(children);
				for (size_t i = 0, numChildren = children.size(); i < numChildren; ++i)
				{
					if (children[i]->IsKindOf(RUNTIME_CLASS(CEntityObject)))
					{
						CBaseObject* pEntityObject = children[i];
						CHyperFlowGraph* pFlowGraph = static_cast<CEntityObject*>(pEntityObject)->GetFlowGraph();

						if (!pFlowGraph)
							continue;

						CString newName;
						newName.Format("[%s] %s", pObject->GetName(), pEntityObject->GetName());
						m_graphsTreeCtrl.RenameGraphItem(pFlowGraph->GetHTreeItem(), newName);
					}
				}
			}
		}
		break;
	}
}

void CHyperGraphDialog::OnObjectsAttached(CBaseObject* pParent, const std::vector<CBaseObject*>& objects)
{
	if (m_bIgnoreObjectEvents)
		return;

	for (CBaseObject* pObject : objects)
	{
		if (pObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
		{
			if (CHyperFlowGraph* pFlowGraph = static_cast<CEntityObject*>(pObject)->GetFlowGraph())
			{
				if (CPrefabObject* pPrefab = (CPrefabObject*)pObject->GetPrefab())
				{
					GetIEditorImpl()->GetFlowGraphManager()->SendNotifyEvent(EHG_GRAPH_OWNER_CHANGE, pFlowGraph);
				}
				else // group
				{
					CString newName;
					newName.Format("[%s] %s", pObject->GetGroup()->GetName(), pObject->GetName());
					RenameFlowGraph(pFlowGraph, newName);
				}
			}
		}
	}
}

void CHyperGraphDialog::OnObjectsDetached(CBaseObject* pParent, const std::vector<CBaseObject*>& objects)
{
	if (m_bIgnoreObjectEvents)
		return;

	for (CBaseObject* pObject : objects)
	{
		if (pObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
		{
			if (CHyperFlowGraph* pFlowGraph = static_cast<CEntityObject*>(pObject)->GetFlowGraph())
			{
				if (CPrefabObject* pPrefab = (CPrefabObject*)pObject->GetPrefab())
				{
					GetIEditorImpl()->GetFlowGraphManager()->SendNotifyEvent(EHG_GRAPH_OWNER_CHANGE, pFlowGraph);
				}
				else // group
				{
					CString newName = pObject->GetName();
					RenameFlowGraph(pFlowGraph, newName);
				}
			}
		}
	}
}

void CHyperGraphDialog::OnHyperGraphManagerEvent(EHyperGraphEvent event, IHyperGraph* pGraph, IHyperNode* pNode)
{
	if (m_bIgnoreObjectEvents)
		return;

	switch (event)
	{
	case EHG_GRAPH_REMOVED:
		RemoveGraph(static_cast<CHyperGraph*>(pGraph));
		break;
	case EHG_GRAPH_UPDATE_FROZEN:
		m_view.UpdateFrozen();
		break;
	case EHG_GRAPH_CLEAR_SELECTION:
		m_view.ClearSelection();
		m_view.InvalidateView(true);
		break;
	case EHG_GRAPH_INVALIDATE:
		m_view.InvalidateView(true);
		break;
	}
}


void CHyperGraphDialog::SetIgnoreEvents(bool bIgnore, bool bRefresh)
{
	LOADING_TIME_PROFILE_SECTION;

	if (m_bIgnoreObjectEvents != bIgnore)
	{
		m_bIgnoreObjectEvents = bIgnore;
		m_graphsTreeCtrl.SetIgnoreReloads(bIgnore);
		m_view.SetIgnoreHyperGraphEvents(bIgnore);
	}

	if (bRefresh)
	{
		m_graphsTreeCtrl.FullReload();
		m_componentsReportCtrl.Reload();
		m_breakpointsTreeCtrl.FillBreakpoints();
		m_pGraphProperties->UpdateGraphProperties(m_view.GetGraph());
		m_pGraphTokens->UpdateGraphProperties(m_view.GetGraph());
	}
}

void CHyperGraphDialog::UpdateGraphProperties(CHyperGraph* pGraph)
{
	m_pGraphProperties->UpdateGraphProperties(pGraph);
	m_pGraphTokens->UpdateGraphProperties(pGraph);

	pGraph->SendNotifyEvent(nullptr, EHG_GRAPH_PROPERTIES_CHANGED);
}

void CHyperGraphDialog::UpdateTitle(CHyperGraph* pGraph)
{
		CString title("");
		GetGraphPrettyName(pGraph, title, false);
		m_titleBar.SetPaneText(0, title);
}


int CHyperGraphDialog::OnCreateControl(LPCREATECONTROLSTRUCT lpCreateControl)
{
	CXTPToolBar* pToolBar = lpCreateControl->bToolBar ? DYNAMIC_DOWNCAST(CXTPToolBar, lpCreateControl->pCommandBar) : NULL;

	if ((lpCreateControl->nID == ID_FG_BACK || lpCreateControl->nID == ID_FG_FORWARD) && pToolBar)
	{
		CXTPControlPopup* pButtonNav = CXTPControlPopup::CreateControlPopup(xtpControlSplitButtonPopup);

		CXTPPopupToolBar* pNavBar = CXTPPopupToolBar::CreatePopupToolBar(GetCommandBars());
		pNavBar->EnableCustomization(FALSE);
		pNavBar->SetBorders(CRect(2, 2, 2, 2));
		pNavBar->DisableShadow();

		CXTPControlListBox* pControlListBox = (CXTPControlListBox*)pNavBar->GetControls()->Add(new CXTPControlListBox(), lpCreateControl->nID);
		pControlListBox->SetWidth(10);
		//pControlListBox->SetLinesMinMax(1, 6);
		pControlListBox->SetMultiplSel(FALSE);

		// CXTPControlStatic* pControlListBoxInfo = (CXTPControlStatic*)pUndoBar->GetControls()->Add(new CXTPControlStatic(), lpCreateControl->nID);
		// pControlListBoxInfo->SetWidth(140);

		pButtonNav->SetCommandBar(pNavBar);
		pNavBar->InternalRelease();

		lpCreateControl->pControl = pButtonNav;
		return TRUE;
	}

	return FALSE;
}

CXTPControlStatic* FindInfoControl(CXTPControl* pControl)
{
	CXTPCommandBar* pCommandBar = pControl->GetParent();

	for (int i = 0; i < pCommandBar->GetControls()->GetCount(); i++)
	{
		CXTPControlStatic* pControlStatic = DYNAMIC_DOWNCAST(CXTPControlStatic, pCommandBar->GetControl(i));
		if (pControlStatic && pControlStatic->GetID() == pControl->GetID())
		{
			return pControlStatic;
		}

	}
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnNavListBoxControlSelChange(NMHDR* pNMHDR, LRESULT* pRes)
{
	ASSERT(pNMHDR != NULL);
	ASSERT(pRes != NULL);

	CXTPControlListBox* pControlListBox = DYNAMIC_DOWNCAST(CXTPControlListBox, ((NMXTPCONTROL*)pNMHDR)->pControl);
	if (pControlListBox)
	{
		CListBox* pListBox = pControlListBox->GetListCtrl();
		int index = pListBox->GetCurSel();
		m_pNavGraph = index == LB_ERR ? 0 : reinterpret_cast<CHyperGraph*>(pListBox->GetItemDataPtr(index));

		/*
		   CXTPControlStatic* pInfo = FindInfoControl(pControlListBox);
		   if (pInfo)
		   {
		   CString str;
		   str.Format(_T("Undo %i Actions"), pControlListBox->GetListCtrl()->GetSelCount());
		   pInfo->SetCaption(str);
		   pInfo->DelayRedrawParent();
		   }
		 */

		*pRes = 1;
	}
}

int CalcWidth(CListBox* pListBox)
{
	int nMaxTextWidth = 0;
	int nScrollWidth = 0;

#if 0
	SCROLLINFO scrollInfo;
	memset(&scrollInfo, 0, sizeof(SCROLLINFO));
	scrollInfo.cbSize = sizeof(SCROLLINFO);
	scrollInfo.fMask = SIF_ALL;
	pListBox->GetScrollInfo(SB_VERT, &scrollInfo, SIF_ALL);

	if (pListBox->GetCount() > 1 && ((int)scrollInfo.nMax >= (int)scrollInfo.nPage))
	{
		nScrollWidth = GetSystemMetrics(SM_CXVSCROLL);
	}
#endif

	CDC* pDC = pListBox->GetDC();
	if (pDC)
	{
		CFont* pOldFont = pDC->SelectObject(pListBox->GetFont());
		CString Text;
		const int nItems = pListBox->GetCount();
		for (int i = 0; i < nItems; ++i)
		{
			pListBox->GetText(i, Text);
			const int nTextWidth = pDC->GetTextExtent(Text).cx;
			if (nTextWidth > nMaxTextWidth)
				nMaxTextWidth = nTextWidth;
		}
		pDC->SelectObject(pOldFont);
		VERIFY(pListBox->ReleaseDC(pDC) != 0);
	}
	else
	{
		ASSERT(FALSE);
	}
	return nMaxTextWidth + nScrollWidth;
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnNavListBoxControlPopup(NMHDR* pNMHDR, LRESULT* pRes)
{
	ASSERT(pNMHDR != NULL);
	ASSERT(pRes != NULL);

	UpdateNavHistory();

	CXTPControlListBox* pControlListBox = DYNAMIC_DOWNCAST(CXTPControlListBox, ((NMXTPCONTROL*)pNMHDR)->pControl);
	if (pControlListBox)
	{
		m_pNavGraph = 0;
		CListBox* pListBox = pControlListBox->GetListCtrl();
		pListBox->ResetContent();

		bool forward = pControlListBox->GetID() == ID_FG_FORWARD;
		if (forward)
		{
			std::list<CHyperGraph*>::iterator iterStart;
			std::list<CHyperGraph*>::iterator iterEnd;
			iterStart = m_graphIterCur;
			if (iterStart != m_graphList.end())
				++iterStart;
			iterEnd = m_graphList.end();
			while (iterStart != iterEnd)
			{
				CString prettyName;
				GetGraphPrettyName((*iterStart), prettyName, true);
				int index = pListBox->AddString(prettyName);
				pListBox->SetItemDataPtr(index, (*iterStart));
				++iterStart;
			}
		}
		else
		{
			std::list<CHyperGraph*>::iterator iterStart;
			std::list<CHyperGraph*>::iterator iterEnd;
			iterStart = m_graphIterCur;
			if (iterStart != m_graphList.begin())
			{
				iterEnd = m_graphList.begin();
				do
				{
					--iterStart;
					CString prettyName;
					GetGraphPrettyName((*iterStart), prettyName, true);
					int index = pListBox->AddString(prettyName);
					pListBox->SetItemDataPtr(index, (*iterStart));
				}
				while (iterStart != iterEnd);
			}
		}

		int width = CalcWidth(pListBox) + 4;
		pListBox->SetHorizontalExtent(width);
		pControlListBox->SetWidth(width);

		CXTPControlStatic* pInfo = FindInfoControl(pControlListBox);
		if (pInfo)
		{
			CString str;
			pInfo->SetCaption(_T("Undo 0 Actions"));
			pInfo->DelayRedrawParent();
		}

		*pRes = 1;
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnUpdateNav(CCmdUI* pCmdUI)
{
	BOOL enable = FALSE;

	switch (pCmdUI->m_nID)
	{
	case ID_FG_BACK:
		enable = m_graphList.size() > 0 && m_graphIterCur != m_graphList.begin();
		break;
	case ID_FG_FORWARD:
		enable = m_graphList.size() > 0 && m_graphIterCur != --m_graphList.end() && m_graphIterCur != m_graphList.end();
		break;
	default:
		// don't do anything
		return;
	}
	pCmdUI->Enable(enable);
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnNavForward()
{
	UpdateNavHistory();

	if (m_pNavGraph != 0)
	{
		SetGraph(m_pNavGraph);
		m_pNavGraph = 0;
	}
	else if (m_graphList.size() > 0 && m_graphIterCur != --m_graphList.end() && m_graphIterCur != m_graphList.end())
	{
		std::list<CHyperGraph*>::iterator iter = m_graphIterCur;
		++iter;
		CHyperGraph* pGraph = *(iter);
		SetGraph(pGraph);
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnNavBack()
{
	UpdateNavHistory();

	if (m_pNavGraph != 0)
	{
		SetGraph(m_pNavGraph);
		m_pNavGraph = 0;
	}
	else if (m_graphList.size() > 0 && m_graphIterCur != m_graphList.begin())
	{
		std::list<CHyperGraph*>::iterator iter = m_graphIterCur;
		--iter;
		CHyperGraph* pGraph = *(iter);
		SetGraph(pGraph);
	}
}

void CHyperGraphDialog::OnUndo()
{
	GetIEditorImpl()->GetIUndoManager()->Undo();
}

void CHyperGraphDialog::OnRedo()
{
	GetIEditorImpl()->GetIUndoManager()->Redo();
}

void CHyperGraphDialog::OnSave()
{
	GetIEditorImpl()->GetCommandManager()->Execute("general.save_level");
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnComponentSearchEditChange()
{
	CString keyword;
	m_componentSearchEdit.GetWindowText(keyword);
	m_componentsReportCtrl.SetSearchKeyword(keyword);
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::UpdateNavHistory()
{
	// early return, when history is empty anyway [loading...]
	if (m_graphList.empty())
		return;

	CFlowGraphManager* pMgr = GetIEditorImpl()->GetFlowGraphManager();
	const size_t fgCount = pMgr->GetFlowGraphCount();
	std::set<CHyperGraph*> graphs;
	for (size_t i = 0; i < fgCount; ++i)
	{
		graphs.insert(pMgr->GetFlowGraph(i));
	}

	// validate history
	bool adjustCur = false;
	std::list<CHyperGraph*>::iterator iter = m_graphList.begin();
	while (iter != m_graphList.end())
	{
		CHyperGraph* pGraph = *iter;
		if (graphs.find(pGraph) == graphs.end())
		{
			// remove graph from history
			std::list<CHyperGraph*>::iterator toDelete = iter;
			++iter;
			if (m_graphIterCur == toDelete)
				adjustCur = true;

			m_graphList.erase(toDelete);
		}
		else
		{
			++iter;
		}
	}

	// adjust current iterator in case the current graph got deleted
	if (adjustCur)
	{
		m_graphIterCur = m_graphList.end();
	}
}

//////////////////////////////////////////////////////////////////////////
bool CHyperGraphDialog::CreateContextMenu(CMenu& roMenu, TDTreeItems& rchSelectedItems)
{
	enum EMenuFlags
	{
		eGraphValid = 0,
		eGraphEnabled,
		eIsOwnedByEntity,
		eIsAssociatedToFlowGraph,
		eIsEntitySubFolder,
		eIsEntitySubFolderOrFolder,
		eIsModule,
		eIsModuleFolder,
		eIsDebuggingEnabled,
		eTotalEMenuFlags,
	};

	bool aboMenuFlags[eTotalEMenuFlags];

	memset(aboMenuFlags, 1, sizeof(bool) * eTotalEMenuFlags);

	bool boIsEnabled(false);
	int nTotalTreeItems(0);
	int nCurrentTreeItem(0);

	nTotalTreeItems = rchSelectedItems.size();
	for (nCurrentTreeItem = 0; nCurrentTreeItem < nTotalTreeItems; ++nCurrentTreeItem)
	{
		HTREEITEM& rhCurrentTreeItem = rchSelectedItems[nCurrentTreeItem];
		if (!IsGraphEnabled(rhCurrentTreeItem, boIsEnabled))
		{
			aboMenuFlags[eGraphValid] = false;
		}
		else if (nCurrentTreeItem == 0)
		{
			aboMenuFlags[eGraphEnabled] = boIsEnabled;
		}
		else
		{
			if (aboMenuFlags[eGraphEnabled] != boIsEnabled)
			{
				aboMenuFlags[eGraphValid] = false;
			}
		}

		if (!IsOwnedByAnEntity(rhCurrentTreeItem))
		{
			aboMenuFlags[eIsOwnedByEntity] = false;
		}

		if (!IsGraphType(rhCurrentTreeItem, IFlowGraph::eFGT_Module))
		{
			aboMenuFlags[eIsModule] = false;
		}

		if (!IsAssociatedToFlowGraph(rhCurrentTreeItem))
		{
			aboMenuFlags[eIsAssociatedToFlowGraph] = false;
		}

		if (!IsAnEntitySubFolder(rhCurrentTreeItem))
		{
			aboMenuFlags[eIsEntitySubFolder] = false;
		}

		if (!IsAnEntityFolderOrSubFolder(rhCurrentTreeItem))
		{
			aboMenuFlags[eIsEntitySubFolderOrFolder] = false;
		}

		if (!IsEnabledForDebugging(rhCurrentTreeItem))
		{
			aboMenuFlags[eIsDebuggingEnabled] = false;
		}

		CString itemName = m_graphsTreeCtrl.GetItemText(rhCurrentTreeItem);
		if (itemName.Compare(g_FGModulesFolderName))
		{
			aboMenuFlags[eIsModuleFolder] = false;
		}
	}

	roMenu.CreatePopupMenu();

	if (aboMenuFlags[eIsModuleFolder])
	{
		roMenu.AppendMenu(MF_STRING, ID_GRAPHS_NEW_GLOBAL_MODULE, "New Global FG Module");
		roMenu.AppendMenu(MF_STRING, ID_GRAPHS_NEW_LEVEL_MODULE, "New Level FG Module");
	}

	if (aboMenuFlags[eGraphValid])
	{
		if (!aboMenuFlags[eIsModule])
		{
			roMenu.AppendMenu(MF_STRING, ID_GRAPHS_ENABLE, aboMenuFlags[eGraphEnabled] ? "Enable" : "Disable");
			roMenu.AppendMenu(MF_SEPARATOR);
		}

		// we can only rename the custom graphs found in Files node
		if (!aboMenuFlags[eIsOwnedByEntity])
		{
			// Save Graph
			roMenu.AppendMenu(MF_STRING | (m_view.GetGraph()->IsModified() ? MF_ENABLED : MF_DISABLED), ID_GRAPHS_SAVE_GRAPH, "Save Graph");

			// Rename Graph (non-module only)
			if (!aboMenuFlags[eIsModule])
			{
				roMenu.AppendMenu(MF_STRING, ID_GRAPHS_RENAME_GRAPH, "Rename Graph");
			}

			// Discard Graph
			roMenu.AppendMenu(MF_SEPARATOR);
			roMenu.AppendMenu(MF_STRING | (m_view.GetGraph()->IsModified() ? MF_ENABLED : MF_DISABLED), ID_GRAPHS_DISCARD_GRAPH, "Discard Changes");

			// Edit Module I/O ports
			if (aboMenuFlags[eIsModule])
			{
				roMenu.AppendMenu(MF_STRING, ID_TOOLS_EDITMODULE, "Edit Module Ports");
			}

			if (!aboMenuFlags[eIsModule])
				roMenu.AppendMenu(MF_SEPARATOR);
		}
	}

	if (aboMenuFlags[eIsOwnedByEntity])
	{
		roMenu.AppendMenu(MF_STRING, ID_GRAPHS_SELECT_ENTITY, "Select Entity");
		roMenu.AppendMenu(MF_SEPARATOR);
	}

	// Delete Graph
	if (aboMenuFlags[eGraphValid])
	{
		roMenu.AppendMenu(MF_STRING, ID_GRAPHS_REMOVE_GRAPH, "Delete Graph");
		roMenu.AppendMenu(MF_SEPARATOR);
	}

	if (aboMenuFlags[eIsAssociatedToFlowGraph] && !aboMenuFlags[eIsModule])
	{
		roMenu.AppendMenu(MF_STRING, ID_GRAPHS_CHANGE_FOLDER, "Change Folder");
	}

	if (aboMenuFlags[eIsEntitySubFolder])
	{
		// Renaming does not make sense in batch considering the current usage of the interface.
		if (rchSelectedItems.size() == 1)
		{
			roMenu.AppendMenu(MF_STRING, ID_GRAPHS_RENAME_FOLDER, "Rename Folder");
		}
	}

	if (aboMenuFlags[eIsEntitySubFolderOrFolder])
	{
		roMenu.AppendMenu(MF_STRING, ID_GRAPHS_ENABLE_ALL, "Enable All");
		roMenu.AppendMenu(MF_STRING, ID_GRAPHS_DISABLE_ALL, "Disable All");
	}

	if (aboMenuFlags[eGraphValid])
	{
		roMenu.AppendMenu(MF_SEPARATOR);
		roMenu.AppendMenu(MF_STRING, ID_GRAPHS_EXPORT, "Export Graph");
		roMenu.AppendMenu(MF_STRING | m_view.GetGraph()->IsSelectedAny() ? MF_ENABLED : MF_DISABLED, ID_GRAPHS_EXPORTSELECTION, "Export Selection");
		roMenu.AppendMenu(MF_SEPARATOR);
		roMenu.AppendMenu(MF_STRING | aboMenuFlags[eIsDebuggingEnabled] ? MF_ENABLED : MF_DISABLED, ID_GRAPHS_ENABLE_DEBUGGING, "Enable Debugging");
		roMenu.AppendMenu(MF_STRING | aboMenuFlags[eIsDebuggingEnabled] ? MF_DISABLED : MF_ENABLED, ID_GRAPHS_DISABLE_DEBUGGING, "Disable Debugging");
	}

	roMenu.AppendMenu(MF_STRING, ID_GRAPHS_RELOAD, "Reload Graph List");

	return true;
}
//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::ProcessMenu(int nMenuOption, TDTreeItems& rchSelectedItem)
{
	CString currentGroupName;

	CEntityObject* pOwnerEntity = nullptr;
	CHyperGraph* pGraph = nullptr;
	CHyperFlowGraph* pFlowGraph = nullptr;

	std::vector<CEntityObject*> cSelectedEntities;
	std::vector<CHyperGraph*> cSelectedGraphs;

	int nTotalSelectedItems(0);
	int nCurrentSelectedItem(0);

	nTotalSelectedItems = rchSelectedItem.size();
	for (nCurrentSelectedItem = 0; nCurrentSelectedItem < nTotalSelectedItems; ++nCurrentSelectedItem)
	{
		HTREEITEM& rhCurrentSelectedItem = rchSelectedItem[nCurrentSelectedItem];

		pGraph = (CHyperGraph*)m_graphsTreeCtrl.GetItemData(rhCurrentSelectedItem);
		if (pGraph)
			pFlowGraph = pGraph->IsFlowGraph() ? static_cast<CHyperFlowGraph*>(pGraph) : nullptr;

		if (pFlowGraph)
		{
			pOwnerEntity = pFlowGraph->GetEntity();
		}

		currentGroupName = m_graphsTreeCtrl.GetItemText(rhCurrentSelectedItem);

		switch (nMenuOption)
		{
		case ID_GRAPHS_RELOAD:
			{
				m_graphsTreeCtrl.FullReload();
			}
			break;
		case ID_GRAPHS_ENABLE_ALL:
		case ID_GRAPHS_DISABLE_ALL:
			{
				bool enabled(nMenuOption == ID_GRAPHS_ENABLE_ALL);
				m_graphsTreeCtrl.SetRedraw(FALSE);
				EnableItems(rhCurrentSelectedItem, enabled, true);
				m_graphsTreeCtrl.SetRedraw(TRUE);
			}
			break;
		case ID_GRAPHS_RENAME_FOLDER:
			{
				CFlowGraphManager* const pFlowGraphManager = GetIEditorImpl()->GetFlowGraphManager();

				bool bIsAction = false;
				std::set<CString> groupsSet;
				pFlowGraphManager->GetAvailableGroups(groupsSet, bIsAction);

				CString groupName;
				bool bDoNew = true;
				bool bChange = false;

				if (groupsSet.size() > 0)
				{
					std::vector<CString> groups(groupsSet.begin(), groupsSet.end());

					CGenericSelectItemDialog gtDlg(this);
					if (bIsAction == false)
						gtDlg.SetTitle(_T("Choose New Group for the Flow Graph"));
					else
						gtDlg.SetTitle(_T("Choose Group for the AIAction Graph"));

					gtDlg.PreSelectItem(currentGroupName);
					gtDlg.SetItems(groups);
					gtDlg.AllowNew(true);
					switch (gtDlg.DoModal())
					{
					case IDOK:
						groupName = gtDlg.GetSelectedItem();
						bChange = true;
						bDoNew = false;
						break;
					case IDNEW:
						// bDoNew  = true; // is true anyway
						break;
					default:
						bDoNew = false;
						break;
					}
				}

				if (bDoNew)
				{
					QStringDialog dlg(bIsAction == false ?
						_T("Enter Group Name for the Flow Graph") :
						_T("Enter Group Name for the AIAction Graph"));
					dlg.SetString(currentGroupName);
					if (dlg.exec() == QDialog::Accepted)
					{
						bChange = true;
						groupName = dlg.GetString();
					}
				}

				if (bChange && groupName != currentGroupName)
				{
					m_graphsTreeCtrl.RenameFolder(rhCurrentSelectedItem, groupName);
					if (m_view.GetGraph() == pFlowGraph)
					{
						UpdateTitle(pFlowGraph);
					}
				}
			}
			break;

		case ID_GRAPHS_SELECT_ENTITY:
			{
				if (pOwnerEntity)
				{
					CUndo undo("Select Object(s)");
					if (nCurrentSelectedItem == 0)
					{
						GetIEditorImpl()->ClearSelection();
					}
					GetIEditorImpl()->SelectObject(pOwnerEntity);
				}
			}
			break;
		case ID_GRAPHS_NEW_GLOBAL_MODULE:
			{
				OnFileNewGlobalFGModule();
			}
			break;
		case ID_GRAPHS_NEW_LEVEL_MODULE:
			{
				OnFileNewLevelFGModule();
			}
			break;
		case ID_TOOLS_EDITMODULE:
			{
				OnEditModuleInputsOutputs();
			}
			break;
		case ID_GRAPHS_SAVE_GRAPH:
			{
				SaveGraph(pGraph);
			}
			break;

		case ID_GRAPHS_DISCARD_GRAPH:
			{
				string logMessage;
				logMessage.Format("Do you want to discard all changes on graph '%s'?", pGraph->GetName());
				if (pGraph->IsModified() && (IDYES == MessageBox(logMessage.c_str(), "Flowgraph", MB_ICONQUESTION | MB_YESNO | MB_APPLMODAL)))
				{
					if (!pGraph->Reload(true))
					{
						Warning("$4Problem discarding changes in graph '%s'. Does the file '%s' exist?", pGraph->GetName(), pGraph->GetFilename());
					}
				}
			}
			break;
		case ID_GRAPHS_REMOVE_GRAPH:
			{
				// add all graphs to be processed in bulk with the last item
				if (pGraph)
				{
					cSelectedGraphs.push_back(pGraph);
				}
				// last item - present dialog to confirm and apply to all items in cSelectedGraphs
				if (nCurrentSelectedItem == nTotalSelectedItems - 1 && !cSelectedGraphs.empty())
				{
					CString str = "Delete Graph(s)?\n";
					size_t displayGraphCount = MIN(cSelectedGraphs.size(), 17);
					for (size_t i = 0; i < displayGraphCount; ++i)
					{
						str += (const char*)cSelectedGraphs[i]->GetName();
						str += "\n";
					}
					if (displayGraphCount < cSelectedEntities.size())
					{
						str += "...";
					}

					if (MessageBox(str, "Confirm", MB_OKCANCEL) == IDOK)
					{
						CUndo undo("Delete Entity Graph(s)");
						for (size_t i = 0, graphCount = cSelectedGraphs.size(); i < graphCount; ++i)
						{
							CHyperFlowGraph* pFlowGraph = static_cast<CHyperFlowGraph*>(cSelectedGraphs[i]);
							if (pFlowGraph)
							{
								// update the tree list and navigation history. clear the view and properties if this was the active graph.
								RemoveGraph(pFlowGraph);

								bool bIsModule = (pFlowGraph->GetIFlowGraph()->GetType() == IFlowGraph::eFGT_Module);
								if (bIsModule)
								{
									if (IFlowGraphModuleManager* pModuleManager = gEnv->pFlowSystem->GetIModuleManager())
										pModuleManager->DeleteModuleXML(pFlowGraph->GetName());
								}

								CEntityObject* pGraphEntity = pFlowGraph->GetEntity();
								if (pGraphEntity)
								{
									pGraphEntity->RemoveFlowGraph();
								}

								if (!bIsModule && !pGraphEntity)
									SAFE_RELEASE(pFlowGraph);
							}
						}
					}
				}
			}
			break;

		case ID_GRAPHS_EXPORT:
			{
				ExportGraph(pGraph);
			}
			break;

		case ID_GRAPHS_EXPORTSELECTION:
			{
				ExportGraphSelection();
			}
			break;

		case ID_GRAPHS_ENABLE:
			{
				if (pFlowGraph)
				{
					// Toggle enabled status of graph.
					DisableGraph(pFlowGraph, !pFlowGraph->IsEnabled());
				}
			}
			break;
		case ID_GRAPHS_RENAME_GRAPH:
			{
				if (pFlowGraph)
				{
					string renameMessage = "Edit Flow Graph name";
					// show the dialog to input new name
					CStringInputDialog dlg(pFlowGraph->GetName(), renameMessage.c_str());

					// if all OK, rename it
					if (IDOK == dlg.DoModal())
					{
						if (dlg.GetResultingText().IsEmpty())
						{
							CQuestionDialog::SWarning(QObject::tr(""), QObject::tr("You supplied an empty name, no change was made"));
							break;
						}

						// rename flowgraph
						pFlowGraph->SetName(dlg.GetResultingText());
						m_graphsTreeCtrl.UpdateGraphLabel(pFlowGraph);
						UpdateTitle(pFlowGraph);
					}
				}
			}
			break;
		case ID_GRAPHS_CHANGE_FOLDER:
			{
				// currently nested folders are not supported, so only graph items can change folder
				// add all graphs to be processed in bulk with the last item.
				if (pGraph)
				{
					cSelectedGraphs.push_back(pGraph);
				}

				// last item - present dialog to choose the new folder to apply to all items in cSelectedGraphs
				if (nCurrentSelectedItem == nTotalSelectedItems - 1)
				{
					if (pFlowGraph) // GetAvailableGroups
					{
						CFlowGraphManager* const pFlowGraphManager = (CFlowGraphManager*)pFlowGraph->GetManager();
						bool bIsAction = pFlowGraph->GetAIAction();
						std::set<CString> groupsSet;
						pFlowGraphManager->GetAvailableGroups(groupsSet, bIsAction);

						CString groupName;
						bool bDoNew = true;
						bool bChange = false;

						if (groupsSet.size() > 0)
						{
							std::vector<CString> groups(groupsSet.begin(), groupsSet.end());

							CGenericSelectItemDialog gtDlg(this);
							if (bIsAction == false)
								gtDlg.SetTitle(_T("Choose Group for the Flow Graph(s)"));
							else
								gtDlg.SetTitle(_T("Choose Group for the AIAction Graph(s)"));

							gtDlg.PreSelectItem(pFlowGraph->GetGroupName());
							gtDlg.SetItems(groups);
							gtDlg.AllowNew(true);
							switch (gtDlg.DoModal())
							{
							case IDOK:
								groupName = gtDlg.GetSelectedItem();
								bChange = true;
								bDoNew = false;
								break;
							case IDNEW:
								// bDoNew  = true; // is true anyway
								break;
							default:
								bDoNew = false;
								break;
							}
						}

						if (bDoNew)
						{
							QStringDialog dlg(bIsAction == false ?
								_T("Enter Group Name for the Flow Graph(s)") :
								_T("Enter Group Name for the AIAction Graph(s)"));
							dlg.SetString(pFlowGraph->GetGroupName());
							if (dlg.exec() == QDialog::Accepted)
							{
								bChange = true;
								groupName = dlg.GetString();
							}
						}

						// actually moving all selected graphs to pre-existing or new 'groupname'
						if (bChange)
						{
							size_t nTotalSelectedGraphs = cSelectedGraphs.size();
							for (size_t i = 0; i < nTotalSelectedGraphs; ++i)
							{
								// graph needs to be of the same type than the last selected graph
								if (pFlowGraph && cSelectedGraphs[i]->IsFlowGraph())
								{
									CHyperFlowGraph* pSelectedGraph = static_cast<CHyperFlowGraph*>(cSelectedGraphs[i]);
									if (pSelectedGraph->GetIFlowGraph()->GetType() == pFlowGraph->GetIFlowGraph()->GetType())
									{
										if (groupName != pSelectedGraph->GetGroupName())
										{
											pSelectedGraph->SetGroupName(groupName);
										}
									}
								}
							}
						}
					}
				}
			}
			break;

		case ID_GRAPHS_ENABLE_DEBUGGING:
			{
				if (pFlowGraph && m_pFlowGraphDebugger)
				{
					m_pFlowGraphDebugger->RemoveIgnoredFlowgraph(pFlowGraph->GetIFlowGraph());
				}
			}
			break;
		case ID_GRAPHS_DISABLE_DEBUGGING:
			{
				if (pFlowGraph && m_pFlowGraphDebugger)
				{
					m_pFlowGraphDebugger->AddIgnoredFlowgraph(pFlowGraph->GetIFlowGraph());
				}
			}
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CHyperGraphDialog::IsAnEntityFolderOrSubFolder(HTREEITEM hSelectedItem)
{
	HTREEITEM hParentItem(NULL);
	CString strGroupName;

	if (IsAssociatedToFlowGraph(hSelectedItem))
	{
		return false;
	}

	hParentItem = m_graphsTreeCtrl.GetParentItem(hSelectedItem);
	strGroupName = m_graphsTreeCtrl.GetItemText(hSelectedItem);

	if (
	  (strGroupName == g_EntitiesFolderName)
	  ||
	  (hParentItem != 0 && m_graphsTreeCtrl.GetItemText(hParentItem) == g_EntitiesFolderName)
	  )
	{
		return true;
	}

	return false;
}
//////////////////////////////////////////////////////////////////////////
bool CHyperGraphDialog::IsAnEntitySubFolder(HTREEITEM hSelectedItem)
{
	HTREEITEM hParentItem(NULL);
	CString strGroupName;

	if (IsAssociatedToFlowGraph(hSelectedItem))
	{
		return false;
	}

	hParentItem = m_graphsTreeCtrl.GetParentItem(hSelectedItem);
	strGroupName = m_graphsTreeCtrl.GetItemText(hParentItem);

	if (hParentItem != 0 &&
	    (strGroupName != g_AIActionsFolderName) && // I think this would never happen togheter with the other condition...
	    (strGroupName != g_FGModulesFolderName)
	    )
	{
		return true;
	}

	return false;
}
//////////////////////////////////////////////////////////////////////////
bool CHyperGraphDialog::IsGraphEnabled(HTREEITEM hSelectedItem, bool& boIsEnabled)
{
	CHyperGraph* pGraph = 0;
	CHyperFlowGraph* pFlowGraph = 0;

	pGraph = (CHyperGraph*)m_graphsTreeCtrl.GetItemData(hSelectedItem);
	if (pGraph)
	{
		pFlowGraph = pGraph->IsFlowGraph() ? static_cast<CHyperFlowGraph*>(pGraph) : 0;
	}

	if (pFlowGraph)
	{
		if (pFlowGraph->GetAIAction() == 0 && pFlowGraph->GetUIAction() == 0)
		{
			if (!pGraph->IsEnabled())
			{
				boIsEnabled = true;
			}
			else
			{
				boIsEnabled = false;
			}
			return true;
		}
	}

	return false;
}

bool CHyperGraphDialog::IsGraphType(HTREEITEM hSelectedItem, IFlowGraph::EFlowGraphType type)
{
	CHyperGraph* pGraph = 0;
	CHyperFlowGraph* pFlowGraph = 0;
	CEntityObject* pOwnerEntity = 0;

	pGraph = (CHyperGraph*)m_graphsTreeCtrl.GetItemData(hSelectedItem);
	if (pGraph)
	{
		pFlowGraph = pGraph->IsFlowGraph() ? static_cast<CHyperFlowGraph*>(pGraph) : 0;
	}

	if (pFlowGraph)
	{
		return pFlowGraph->GetIFlowGraph()->GetType() == type;
	}
	return false;
}

bool CHyperGraphDialog::IsEnabledForDebugging(HTREEITEM hSelectedItem)
{
	if (!m_pFlowGraphDebugger)
		return false;

	if (CHyperGraph* pHypergraph = (CHyperGraph*)m_graphsTreeCtrl.GetItemData(hSelectedItem))
	{
		if (pHypergraph->IsFlowGraph())
		{
			CHyperFlowGraph* pFlowgraph = static_cast<CHyperFlowGraph*>(pHypergraph);

			return m_pFlowGraphDebugger->IsFlowgraphIgnored(pFlowgraph->GetIFlowGraph()) || m_pFlowGraphDebugger->IsFlowgraphTypeIgnored(pFlowgraph->GetIFlowGraph()->GetType());
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CHyperGraphDialog::IsOwnedByAnEntity(HTREEITEM hSelectedItem)
{
	CHyperGraph* pGraph = 0;
	CHyperFlowGraph* pFlowGraph = 0;
	CEntityObject* pOwnerEntity = 0;

	pGraph = (CHyperGraph*)m_graphsTreeCtrl.GetItemData(hSelectedItem);
	if (pGraph)
	{
		pFlowGraph = pGraph->IsFlowGraph() ? static_cast<CHyperFlowGraph*>(pGraph) : 0;
	}

	if (pFlowGraph)
	{
		pOwnerEntity = pFlowGraph->GetEntity();
		if (pOwnerEntity)
		{
			return true;
		}
	}
	return false;
}
//////////////////////////////////////////////////////////////////////////
bool CHyperGraphDialog::IsAssociatedToFlowGraph(HTREEITEM hSelectedItem)
{
	CHyperGraph* pGraph = (CHyperGraph*)m_graphsTreeCtrl.GetItemData(hSelectedItem);

	if (pGraph)
	{
		return pGraph->IsFlowGraph();
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnCustomize()
{
	CMFCUtils::ShowShortcutsCustomizeDlg(GetCommandBars(), IDR_HYPERGRAPH, "FlowGraph");
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnExportShortcuts()
{
	CMFCUtils::ExportShortcuts(GetCommandBars()->GetShortcutManager());
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnImportShortcuts()
{
	CMFCUtils::ImportShortcuts(GetCommandBars()->GetShortcutManager(), "FlowGraph");
}

bool CHyperGraphDialog::CreateBreakpointContextMenu(CMenu& roMenu, HTREEITEM hItem)
{
	SBreakpointItem* breakpointItem = reinterpret_cast<SBreakpointItem*>(m_breakpointsTreeCtrl.GetItemData(hItem));

	if (breakpointItem == NULL)
		return false;

	roMenu.CreatePopupMenu();

	switch (breakpointItem->type)
	{
	case SBreakpointItem::eIT_Graph:
		{
			roMenu.AppendMenu(MF_STRING, ID_BREAKPOINT_REMOVE_GRAPH, "Remove Breakpoints for Graph");
		}
		break;
	case SBreakpointItem::eIT_Node:
		{
			roMenu.AppendMenu(MF_STRING, ID_BREAKPOINT_REMOVE_NODE, "Remove Breakpoints for Node");
		}
		break;
	case SBreakpointItem::eIT_Port:
		{
			if (m_pFlowGraphDebugger)
			{
				roMenu.AppendMenu(MF_STRING, ID_BREAKPOINT_REMOVE_PORT, "Remove Breakpoint");
				roMenu.AppendMenu(MF_SEPARATOR);

				const bool bIsEnabled = m_pFlowGraphDebugger->IsBreakpointEnabled(breakpointItem->flowgraph, breakpointItem->addr);
				roMenu.AppendMenu(MF_STRING | (bIsEnabled ? MF_CHECKED : MF_UNCHECKED), ID_BREAKPOINT_ENABLE, "Enabled");
				const bool bIsTracepoint = m_pFlowGraphDebugger->IsTracepoint(breakpointItem->flowgraph, breakpointItem->addr);
				roMenu.AppendMenu(MF_STRING | (bIsTracepoint ? MF_CHECKED : MF_UNCHECKED), ID_TRACEPOINT_ENABLE, "Tracepoint");
			}
		}

		break;
	}

	return true;
}

void CHyperGraphDialog::ProcessBreakpointMenu(int nMenuOption, HTREEITEM hItem)
{
	switch (nMenuOption)
	{
	case ID_BREAKPOINT_REMOVE_GRAPH:
		{
			m_breakpointsTreeCtrl.RemoveBreakpointsForGraph(hItem);
		}
		break;
	case ID_BREAKPOINT_REMOVE_NODE:
		{
			m_breakpointsTreeCtrl.RemoveBreakpointsForNode(hItem);
		}
		break;
	case ID_BREAKPOINT_REMOVE_PORT:
		{
			m_breakpointsTreeCtrl.RemoveBreakpointForPort(hItem);
		}
		break;
	case ID_BREAKPOINT_ENABLE:
	case ID_TRACEPOINT_ENABLE:
		{
			if (m_pFlowGraphDebugger)
			{
				SBreakpointItem* breakpointItem = reinterpret_cast<SBreakpointItem*>(m_breakpointsTreeCtrl.GetItemData(hItem));

				if (breakpointItem == NULL)
					return;

				if (nMenuOption == ID_BREAKPOINT_ENABLE)
				{
					const bool bIsEnabled = m_pFlowGraphDebugger->IsBreakpointEnabled(breakpointItem->flowgraph, breakpointItem->addr);
					m_breakpointsTreeCtrl.EnableBreakpoint(hItem, !bIsEnabled);
				}
				else
				{
					const bool bIsTracepoint = m_pFlowGraphDebugger->IsTracepoint(breakpointItem->flowgraph, breakpointItem->addr);
					m_breakpointsTreeCtrl.EnableTracepoint(hItem, !bIsTracepoint);
				}
			}
		}
		break;
	}
	;
}

void CHyperGraphDialog::OnBreakpointsClick(NMHDR* pNMHDR, LRESULT* pResult)
{
	DWORD dw = GetMessagePos();
	CPoint p(GET_X_LPARAM(dw), GET_Y_LPARAM(dw));
	m_breakpointsTreeCtrl.ScreenToClient(&p);

	UINT htFlags = 0;
	HTREEITEM hHitItem = m_breakpointsTreeCtrl.HitTest(p, &htFlags);

	if ((hHitItem != NULL) && (htFlags & TVHT_ONITEM))
	{
		SBreakpointItem* breakpointItem = reinterpret_cast<SBreakpointItem*>(m_breakpointsTreeCtrl.GetItemData(hHitItem));

		if (breakpointItem)
		{
			CHyperFlowGraph* pFlowgraph = GetIEditorImpl()->GetFlowGraphManager()->FindGraph(breakpointItem->flowgraph);

			if (pFlowgraph == NULL)
				return;

			switch (breakpointItem->type)
			{
			case SBreakpointItem::eIT_Graph:
				{
					SetGraph(pFlowgraph);
					m_view.FitFlowGraphToView();
				}
				break;
			case SBreakpointItem::eIT_Node:
			case SBreakpointItem::eIT_Port:
				{
					SetGraph(pFlowgraph);

					CFlowNode* pNode = pFlowgraph->FindFlowNode(breakpointItem->addr.node);

					if (pNode)
					{
						m_view.CenterViewAroundNode(pNode);
						pFlowgraph->UnselectAll();
						pNode->SetSelected(true);
					}
				}
				break;
			}
		}
	}
}


void CHyperGraphDialog::GetGraphPrettyName(CHyperGraph* pGraph, CString& computedName, bool shortVersion)
{
	if (pGraph)
	{
		if (!pGraph->IsFlowGraph())
			computedName = pGraph->GetName();
		else
		{
			CHyperFlowGraph* pFlowGraph = static_cast<CHyperFlowGraph*>(pGraph);
			if (pFlowGraph->GetIFlowGraph()->GetType() == IFlowGraph::eFGT_Default)
			{
				CEntityObject* pEntity = pFlowGraph->GetEntity();
				if (pEntity)
				{
					if (pEntity->IsPartOfPrefab())
					{
						if (CPrefabObject* pPrefab = (CPrefabObject*)pEntity->GetPrefab())
						{
							computedName.Format("[Prefab] %s : %s : ", pPrefab->GetPrefabItem()->GetFullName(), pPrefab->GetName().c_str());
						}
					}
					else
					{
						computedName.Format("[Entity] ");
						if (!shortVersion && !pFlowGraph->GetGroupName().IsEmpty())
						{
							computedName.Format("%s%s : ", computedName, pFlowGraph->GetGroupName());
						}
					}
				}
				else
				{
					computedName.Format("[Files] ");
				}
				computedName.Format("%s%s", computedName, pFlowGraph->GetName());
			}
			else
			{
				computedName = pFlowGraph->GetIFlowGraph()->GetDebugName();

			}
		}

		if (!shortVersion && !pGraph->GetFilename().IsEmpty())
		{
			computedName.Format("%s   (%s)", computedName, pGraph->GetFilename());
		}
	}
}

