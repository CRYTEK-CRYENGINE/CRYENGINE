// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

// #SchematycTODO : Find a less hacky way to set/get c_vars? Right now we're referencing c_vars exposed in Hunt game code by name when a more game agnostic approach would be better.

#include "StdAfx.h"

#include "MainFrameWnd.h"

#include <CrySystem/CryVersion.h>

#include <CryEntitySystem/IEntitySystem.h>
#include <CrySchematyc2/ILib.h>
#include <CrySchematyc2/IObject.h>
#include <CrySchematyc2/Script/IScriptRegistry.h>
#include <CrySchematyc2/Utils/CryLinkUtils.h>
#include <CrySchematyc2/GUID.h>

#include "Resource.h"
#include "BrowserIcons.h"
#include "LogWidget.h"
#include "PluginUtils.h"
#include "PreviewWidget.h"

#include <QtViewPane.h>

namespace Schematyc2
{
	namespace
	{
		static const UINT		IDW_SCHEMATYC2_BROWSER_PANE					= AFX_IDW_PANE_FIRST + 1;
		static const UINT		IDW_SCHEMATYC2_ENV_BROWSER_PANE			= AFX_IDW_PANE_FIRST + 2;
		static const UINT		IDW_SCHEMATYC2_DETAIL_PANE						= AFX_IDW_PANE_FIRST + 3;
		static const UINT		IDW_SCHEMATYC2_GRAPH_PANE						= AFX_IDW_PANE_FIRST + 4;
		static const UINT		IDW_SCHEMATYC2_LOG_OUTPUT_PANE_A			= AFX_IDW_PANE_FIRST + 5;
		static const UINT		IDW_SCHEMATYC2_LOG_OUTPUT_PANE_B			= AFX_IDW_PANE_FIRST + 6;
		static const UINT		IDW_SCHEMATYC2_LOG_OUTPUT_PANE_C			= AFX_IDW_PANE_FIRST + 7;
		static const UINT		IDW_SCHEMATYC2_LOG_OUTPUT_PANE_D			= AFX_IDW_PANE_FIRST + 8;
		static const UINT		IDW_SCHEMATYC2_COMPILER_OUTPUT_PANE	= AFX_IDW_PANE_FIRST + 9;
		static const UINT		IDW_SCHEMATYC2_PREVIEW_PANE					= AFX_IDW_PANE_FIRST + 10;

		static const UINT		IDC_SCHEMATYC_BROWSER								= 1;
		static const UINT		IDC_SCHEMATYC_ENV_BROWSER						= 2;
		static const UINT		IDC_SCHEMATYC_DETAIL								= 3;
		static const UINT		IDC_SCHEMATYC_GRAPH									= 4;
		static const UINT		IDC_SCHEMATYC_LOG_OUTPUT_A					= 5;
		static const UINT		IDC_SCHEMATYC_LOG_OUTPUT_B					= 6;
		static const UINT		IDC_SCHEMATYC_LOG_OUTPUT_C					= 7;
		static const UINT		IDC_SCHEMATYC_LOG_OUTPUT_D					= 8;
		static const UINT		IDC_SCHEMATYC_COMPILER_OUTPUT				= 9;
		static const UINT		IDC_SCHEMATYC_PREVIEW								= 10;

		static const UINT		IDC_SCHEMATYC_BROWSER_NEW						= 11;

		static const char*	MAIN_WND_CLASS_NAME									= "Schematyc2";
		static const char*	SETTINGS_FOLDER_NAME								= "schematyc";
		static const char*	SETTINGS_FILE_NAME									= "settings.xml";
		static const char*	LAYOUT_FILE_NAME										= "config.xml";
		static const char*	LAYOUT_SECTION											= "Layout";
	}

	//////////////////////////////////////////////////////////////////////////
	CMainFrameWnd::SGotoHelper::SGotoHelper()
		: m_currentGoto(0)
	{
		m_gotoHistory.reserve(32);
	}

	//////////////////////////////////////////////////////////////////////////
	void CMainFrameWnd::SGotoHelper::GoForward()
	{
		SGUID gotoGUID;
		if ((m_currentGoto + 1) >= m_gotoHistory.size())
		{
			if (!Schematyc2::GUID::IsEmpty(m_goto) && (m_goto != m_goBack))
			{
				m_gotoHistory.push_back(m_goBack);
				m_gotoHistory.push_back(m_goto);

				++m_currentGoto;
				gotoGUID = m_goto;

				m_goto = GUID::Empty();
				m_goBack = GUID::Empty();
			}
		}
		else
		{
			gotoGUID = m_gotoHistory[++m_currentGoto];
		}

		Schematyc2::CryLinkUtils::ExecuteCommand(Schematyc2::CryLinkUtils::ECommand::Show, gotoGUID);
	}

	//////////////////////////////////////////////////////////////////////////
	void CMainFrameWnd::SGotoHelper::GoBack()
	{
		if (m_currentGoto > 0)
		{
			--m_currentGoto;
		}

		Schematyc2::CryLinkUtils::ExecuteCommand(Schematyc2::CryLinkUtils::ECommand::Show, m_gotoHistory[m_currentGoto]);
	}

	//////////////////////////////////////////////////////////////////////////
	void CMainFrameWnd::SGotoHelper::SetCurrentSelection(SGUID gotoNode, SGUID gobackNode)
	{
		m_goBack = gobackNode;
		m_goto = gotoNode;

		// Erase later history
		if (m_currentGoto < m_gotoHistory.size() && (m_goto != m_goBack))
		{
			m_gotoHistory.erase(m_gotoHistory.begin() + m_currentGoto, m_gotoHistory.end());
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CMainFrameWnd::SGotoHelper::ClearCurrentSelection()
	{
		m_goBack = GUID::Empty();
		m_goto = GUID::Empty();
	}

	//////////////////////////////////////////////////////////////////////////
	void CMainFrameWnd::SGotoHelper::GetCanGoForwardOrBack(bool& forward, bool& back) const
	{
		back = m_currentGoto > 0;
		forward = m_currentGoto < m_gotoHistory.size() || !GUID::IsEmpty(m_goto);
	}

	//////////////////////////////////////////////////////////////////////////
	// #SchematycTODO : Ideally this class would derive from TRefCounted base
	//                  rather than implementing it's own reference counting,
	//                  but that causes link errors.
	class CMainViewClass : public IViewPaneClass
	{
	public:

		inline CMainViewClass()
			: m_refCount(0)
		{}

		inline ULONG AddRef()
		{
			++ m_refCount;
			return m_refCount;
		};

		inline ULONG Release()
		{
			const int32	refCount = -- m_refCount;
			if(refCount <= 0)
			{
				delete this;
			}
			return refCount;
		}

		virtual ESystemClassID SystemClassID()
		{
			return ESYSTEM_CLASS_VIEWPANE;
		};

		virtual const ::GUID& ClassID()
		{
			static const ::GUID guid = { 0x530840ee, 0x6f1c, 0x4319, { 0x8a, 0xa2, 0xd9, 0xb5, 0x4e, 0xde, 0x43, 0x8a } };
			return guid;
		}

		virtual const char* ClassName()
		{
			return MAIN_WND_CLASS_NAME;
		};

		virtual const char* Category()
		{
			return "Schematyc Editor";
		};

		virtual CRuntimeClass* GetRuntimeClass()
		{
			return RUNTIME_CLASS(CMainFrameWnd);
		};

		virtual const char* GetPaneTitle()
		{
			return _T("Schematyc Editor");
		};

		virtual EDockingDirection GetDockingDirection()
		{
			return DOCK_FLOAT;
		};

		virtual CRect GetPaneRect()
		{
			return CRect(200, 200, 600, 500);
		};

		virtual bool SinglePane()
		{
			return true;
		};

		virtual bool WantIdleUpdate()
		{
			return true;
		};

	private:

		int32	m_refCount;
	};

	//////////////////////////////////////////////////////////////////////////
	IMPLEMENT_DYNCREATE(CMainFrameWnd, CXTPFrameWnd)

	//////////////////////////////////////////////////////////////////////////
	BEGIN_MESSAGE_MAP(CMainFrameWnd, CXTPFrameWnd)
		
		ON_COMMAND_EX(ID_SCHEMATYC_VIEW_BROWSER, OnViewMenuCommand)
		ON_COMMAND_EX(ID_SCHEMATYC_VIEW_ENV_BROWSER, OnViewMenuCommand)
		ON_COMMAND_EX(ID_SCHEMATYC_VIEW_DETAIL, OnViewMenuCommand)
		ON_COMMAND_EX(ID_SCHEMATYC_VIEW_GRAPH, OnViewMenuCommand)
		ON_COMMAND_EX(ID_SCHEMATYC_VIEW_LOG_OUTPUT_A, OnViewMenuCommand)
		ON_COMMAND_EX(ID_SCHEMATYC_VIEW_LOG_OUTPUT_B, OnViewMenuCommand)
		ON_COMMAND_EX(ID_SCHEMATYC_VIEW_LOG_OUTPUT_C, OnViewMenuCommand)
		ON_COMMAND_EX(ID_SCHEMATYC_VIEW_LOG_OUTPUT_D, OnViewMenuCommand)
		ON_COMMAND_EX(ID_SCHEMATYC_VIEW_COMPILER_OUTPUT, OnViewMenuCommand)
		ON_COMMAND_EX(ID_SCHEMATYC_VIEW_PREVIEW, OnViewMenuCommand)

		ON_UPDATE_COMMAND_UI(ID_SCHEMATYC_VIEW_BROWSER, OnUpdateViewMenuItem)
		ON_UPDATE_COMMAND_UI(ID_SCHEMATYC_VIEW_ENV_BROWSER, OnUpdateViewMenuItem)
		ON_UPDATE_COMMAND_UI(ID_SCHEMATYC_VIEW_DETAIL, OnUpdateViewMenuItem)
		ON_UPDATE_COMMAND_UI(ID_SCHEMATYC_VIEW_GRAPH, OnUpdateViewMenuItem)
		ON_UPDATE_COMMAND_UI(ID_SCHEMATYC_VIEW_LOG_OUTPUT_A, OnUpdateViewMenuItem)
		ON_UPDATE_COMMAND_UI(ID_SCHEMATYC_VIEW_LOG_OUTPUT_B, OnUpdateViewMenuItem)
		ON_UPDATE_COMMAND_UI(ID_SCHEMATYC_VIEW_LOG_OUTPUT_C, OnUpdateViewMenuItem)
		ON_UPDATE_COMMAND_UI(ID_SCHEMATYC_VIEW_LOG_OUTPUT_D, OnUpdateViewMenuItem)
		ON_UPDATE_COMMAND_UI(ID_SCHEMATYC_VIEW_COMPILER_OUTPUT, OnUpdateViewMenuItem)
		ON_UPDATE_COMMAND_UI(ID_SCHEMATYC_VIEW_PREVIEW, OnUpdateViewMenuItem)

		ON_COMMAND_EX(ID_SCHEMATYC_WORLD_DEBUG_SHOW_STATES, OnWorldDebugMenuCommand)
		ON_COMMAND_EX(ID_SCHEMATYC_WORLD_DEBUG_SHOW_VARIABLES, OnWorldDebugMenuCommand)
		ON_COMMAND_EX(ID_SCHEMATYC_WORLD_DEBUG_SHOW_CONTAINERS, OnWorldDebugMenuCommand)
		ON_COMMAND_EX(ID_SCHEMATYC_WORLD_DEBUG_SHOW_TIMERS, OnWorldDebugMenuCommand)
		ON_COMMAND_EX(ID_SCHEMATYC_WORLD_DEBUG_SHOW_ACTIONS, OnWorldDebugMenuCommand)
		ON_COMMAND_EX(ID_SCHEMATYC_WORLD_DEBUG_SHOW_SIGNAL_HISTORY, OnWorldDebugMenuCommand)
		
		ON_UPDATE_COMMAND_UI(ID_SCHEMATYC_WORLD_DEBUG_SHOW_STATES, OnUpdateWorldDebugMenuItem)
		ON_UPDATE_COMMAND_UI(ID_SCHEMATYC_WORLD_DEBUG_SHOW_VARIABLES, OnUpdateWorldDebugMenuItem)
		ON_UPDATE_COMMAND_UI(ID_SCHEMATYC_WORLD_DEBUG_SHOW_CONTAINERS, OnUpdateWorldDebugMenuItem)
		ON_UPDATE_COMMAND_UI(ID_SCHEMATYC_WORLD_DEBUG_SHOW_TIMERS, OnUpdateWorldDebugMenuItem)
		ON_UPDATE_COMMAND_UI(ID_SCHEMATYC_WORLD_DEBUG_SHOW_ACTIONS, OnUpdateWorldDebugMenuItem)
		ON_UPDATE_COMMAND_UI(ID_SCHEMATYC_WORLD_DEBUG_SHOW_SIGNAL_HISTORY, OnUpdateWorldDebugMenuItem)

		ON_COMMAND(ID_SCHEMATYC_DEBUG_SELECTED_ENTITY, OnDebugSelectedEntity)
		ON_UPDATE_COMMAND_UI(ID_SCHEMATYC_DEBUG_SELECTED_ENTITY, OnUpdateDebugSelectedEntity)

		ON_COMMAND_EX(ID_SCHEMATYC_ENTITY_DEBUG_SHOW_STATES, OnEntityDebugMenuCommand)
		ON_COMMAND_EX(ID_SCHEMATYC_ENTITY_DEBUG_SHOW_VARIABLES, OnEntityDebugMenuCommand)
		ON_COMMAND_EX(ID_SCHEMATYC_ENTITY_DEBUG_SHOW_CONTAINERS, OnEntityDebugMenuCommand)
		ON_COMMAND_EX(ID_SCHEMATYC_ENTITY_DEBUG_SHOW_TIMERS, OnEntityDebugMenuCommand)
		ON_COMMAND_EX(ID_SCHEMATYC_ENTITY_DEBUG_SHOW_ACTIONS, OnEntityDebugMenuCommand)
		ON_COMMAND_EX(ID_SCHEMATYC_ENTITY_DEBUG_SHOW_SIGNAL_HISTORY, OnEntityDebugMenuCommand)

		ON_UPDATE_COMMAND_UI(ID_SCHEMATYC_ENTITY_DEBUG_SHOW_STATES, OnUpdateEntityDebugMenuItem)
		ON_UPDATE_COMMAND_UI(ID_SCHEMATYC_ENTITY_DEBUG_SHOW_VARIABLES, OnUpdateEntityDebugMenuItem)
		ON_UPDATE_COMMAND_UI(ID_SCHEMATYC_ENTITY_DEBUG_SHOW_CONTAINERS, OnUpdateEntityDebugMenuItem)
		ON_UPDATE_COMMAND_UI(ID_SCHEMATYC_ENTITY_DEBUG_SHOW_TIMERS, OnUpdateEntityDebugMenuItem)
		ON_UPDATE_COMMAND_UI(ID_SCHEMATYC_ENTITY_DEBUG_SHOW_ACTIONS, OnUpdateEntityDebugMenuItem)
		ON_UPDATE_COMMAND_UI(ID_SCHEMATYC_ENTITY_DEBUG_SHOW_SIGNAL_HISTORY, OnUpdateEntityDebugMenuItem)

		ON_COMMAND(ID_SCHEMATYC_HELP_VIEW_WIKI, OnHelpViewWiki)
		ON_COMMAND(ID_SCHEMATYC_GO_TO_AND_FROM_TRANSITION_GRAPH, OnGotoOrFromTransitionGraph)
		ON_COMMAND(ID_SCHEMATYC_GO_TO, OnGoto)
		ON_COMMAND(ID_SCHEMATYC_GO_BACK, OnGoBack)
		ON_COMMAND(ID_SCHEMATYC_HELP_VIEW_SHORTCUT_KEYS, OnHelpViewShortcutKeys)
		ON_COMMAND(ID_SCHEMATYC_SAVE, OnSave)
		ON_COMMAND(ID_SCHEMATYC_SAVE_SETTINGS, OnSaveSettings)
		ON_COMMAND(ID_SCHEMATYC_REFRESH_ENV, OnRefreshEnv)
		
		ON_UPDATE_COMMAND_UI(ID_SCHEMATYC_REFRESH_ENV, OnUpdateRefreshEnv)

		ON_MESSAGE(XTPWM_DOCKINGPANE_NOTIFY, OnDockingPaneNotify)

		ON_WM_DESTROY()

	END_MESSAGE_MAP()

	//////////////////////////////////////////////////////////////////////////
	CMainFrameWnd::CMainFrameWnd()
		: m_pGraphWnd(nullptr)
		, m_pDetailWnd(nullptr)
		, m_pDetailScriptFile(nullptr)
		, m_pLogWndA(nullptr)
		, m_pLogWndB(nullptr)
		, m_pLogWndC(nullptr)
		, m_pLogWndD(nullptr)
		, m_pPreviewWnd(nullptr)
	{ 
		LOADING_TIME_PROFILE_SECTION_NAMED("CMainFrameWnd - Schematyc 2");
		const HINSTANCE	hInstance = AfxGetInstanceHandle();
		WNDCLASS				wndClass;
		if(!GetClassInfo(hInstance, MAIN_WND_CLASS_NAME, &wndClass))
		{
			wndClass.style					= CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
			wndClass.lpfnWndProc		= ::DefWindowProc;
			wndClass.cbClsExtra			= 0;
			wndClass.cbWndExtra			= 0;
			wndClass.hInstance			= hInstance;
			wndClass.hIcon					= NULL;
			wndClass.hCursor				= AfxGetApp()->LoadStandardCursor(IDC_ARROW);
			wndClass.hbrBackground	= (HBRUSH)(COLOR_3DFACE + 1);
			wndClass.lpszMenuName		= NULL;
			wndClass.lpszClassName	= MAIN_WND_CLASS_NAME;
			if(!AfxRegisterClass(&wndClass))
			{
				AfxThrowResourceException();
			}
		}
		GetIEditor()->RegisterNotifyListener(this);
		if(Create(WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), AfxGetMainWnd()))
		{
			OnInitDialog();
		}
		if (!LoadAccelTable(MAKEINTRESOURCE(IDR_SCHEMATYC_MENU)))
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Failed to load shortcut accelerator table for Schematyc2");
	}

	//////////////////////////////////////////////////////////////////////////
	CMainFrameWnd::~CMainFrameWnd()
	{
		bool bScriptFileModified = false;
		auto visitScriptFile = [&bScriptFileModified] (const TScriptFile& scriptFile) -> EVisitStatus
		{
			if((scriptFile.GetFlags() & EScriptFileFlags::Modified) != 0)
			{
				bScriptFileModified = true;
				return EVisitStatus::Stop;
			}
			return EVisitStatus::Continue;
		};
		GetSchematyc()->GetScriptRegistry().VisitFiles(TScriptFileConstVisitor::FromLambdaFunction(visitScriptFile));

		if(bScriptFileModified)
		{
			if (::MessageBoxA(nullptr, "Save changes before closing editor?", "Schematyc", 1) == IDOK)
			{
				GetSchematyc()->GetScriptRegistry().Save();
			}
		}

		GetIEditor()->UnregisterNotifyListener(this);

		SAFE_DELETE(m_pPreviewWnd);
		SAFE_DELETE(m_pLogWndD);
		SAFE_DELETE(m_pLogWndC);
		SAFE_DELETE(m_pLogWndB);
		SAFE_DELETE(m_pLogWndA);
		SAFE_DELETE(m_pDetailWnd);
		SAFE_DELETE(m_pGraphWnd);
	}

	//////////////////////////////////////////////////////////////////////////
	BOOL CMainFrameWnd::PreCreateWindow(CREATESTRUCT& cs)
	{
		LOADING_TIME_PROFILE_SECTION;
		if(__super::PreCreateWindow(cs))
		{
			cs.dwExStyle &= ~WS_EX_CLIENTEDGE;
			return TRUE;
		}
		return FALSE;
	}

	//////////////////////////////////////////////////////////////////////////
	BOOL CMainFrameWnd::Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd)
	{
		LOADING_TIME_PROFILE_SECTION;
		return __super::Create(MAIN_WND_CLASS_NAME, "", dwStyle, rect, pParentWnd);
	}

	//////////////////////////////////////////////////////////////////////////
	void CMainFrameWnd::OnEditorNotifyEvent(EEditorNotifyEvent event)
	{
		switch(event)
		{
		case eNotify_OnIdleUpdate:
			{
				m_pPreviewWnd->Update();
				break;
			}
		case eNotify_OnBeginGameMode:
			{
				CWnd::EnableWindow(false);
				break;
			}
		case eNotify_OnEndGameMode:
			{
				CWnd::EnableWindow(true);
				break;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	BOOL CMainFrameWnd::OnInitDialog()
	{
		LOADING_TIME_PROFILE_SECTION_NAMED("Schematyc Legacy OnInitDialog");
		SET_LOCAL_RESOURCE_SCOPE
		// Set window style.
		ModifyStyleEx(WS_EX_CLIENTEDGE, 0);
		ModifyStyle(0, WS_CLIPCHILDREN);
		// Create menu and toolbar.
		if(!CreateMenuAndToolbar())
		{
			return FALSE;
		}
		// Create child windows.
		m_browserCtrl.Create(WS_CHILD, CRect(0, 0, 0, 0), this, IDC_SCHEMATYC_BROWSER);
		m_envBrowserCtrl.Create(WS_CHILD, CRect(0, 0, 0, 0), this, IDC_SCHEMATYC_ENV_BROWSER);
		
		m_pGraphWnd = new CDocGraphWnd();
		m_pGraphWnd->Create(NULL, "", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this, IDC_SCHEMATYC_GRAPH);
		m_pGraphWnd->Init();

		m_pDetailWnd = new CDetailWnd();
		m_pDetailWnd->Create(NULL, "", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this, IDC_SCHEMATYC_DETAIL);
		m_pDetailWnd->Init();

		m_pLogWndA = new CLogWnd();
		m_pLogWndA->Create(nullptr, "", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this, IDC_SCHEMATYC_LOG_OUTPUT_A);
		m_pLogWndA->Init();

		m_pLogWndB = new CLogWnd();
		m_pLogWndB->Create(nullptr, "", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this, IDC_SCHEMATYC_LOG_OUTPUT_B);
		m_pLogWndB->Init();

		m_pLogWndC = new CLogWnd();
		m_pLogWndC->Create(nullptr, "", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this, IDC_SCHEMATYC_LOG_OUTPUT_C);
		m_pLogWndC->Init();

		m_pLogWndD = new CLogWnd();
		m_pLogWndD->Create(nullptr, "", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this, IDC_SCHEMATYC_LOG_OUTPUT_D);
		m_pLogWndD->Init();

		m_compilerOutputCtrl.Create(WS_CHILD | WS_VSCROLL | ES_MULTILINE, CRect(0, 0, 0, 0), this, IDC_SCHEMATYC_COMPILER_OUTPUT);
		m_pPreviewWnd = new CPreviewWnd();
		m_pPreviewWnd->Create(NULL, "", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this, IDC_SCHEMATYC_PREVIEW);
		m_pPreviewWnd->Init();

		// Create panes.
		CreatePanes();

		// Connect signal receivers.
		m_browserCtrl.Signals().selection.Connect(BrowserCtrlSelectionSignal::Delegate::FromMemberFunction<CMainFrameWnd, &CMainFrameWnd::OnBrowserSelection>(*this), m_connectionScope);
		m_browserCtrl.Signals().itemRemoved.Connect(BrowserCtrlItemRemovedSignal::Delegate::FromMemberFunction<CMainFrameWnd, &CMainFrameWnd::OnBrowserItemRemoved>(*this), m_connectionScope);
		m_browserCtrl.Signals().docModified.Connect(BrowserCtrlDocModifiedSignal::Delegate::FromMemberFunction<CMainFrameWnd, &CMainFrameWnd::OnBrowserDocModified>(*this), m_connectionScope);
		m_pDetailWnd->Signals().modification.Connect(DetailWidgetModificationSignal::Delegate::FromMemberFunction<CMainFrameWnd, &CMainFrameWnd::OnDetailModification>(*this), m_connectionScope);
		m_pGraphWnd->Signals().selection.Connect(DocGraphViewSelectionSignal::Delegate::FromMemberFunction<CMainFrameWnd, &CMainFrameWnd::OnGraphViewSelection>(*this), m_connectionScope);
		m_pGraphWnd->Signals().modification.Connect(DocGraphViewModificationSignal::Delegate::FromMemberFunction<CMainFrameWnd, &CMainFrameWnd::OnGraphViewModification>(*this), m_connectionScope);
		m_pGraphWnd->Signals().gotoSelection.Connect(DocGotoSelectionSignal::Delegate::FromMemberFunction<CMainFrameWnd, &CMainFrameWnd::OnGoto>(*this), m_connectionScope);
		m_pGraphWnd->Signals().goBackFromSelection.Connect(DocGotoSelectionSignal::Delegate::FromMemberFunction<CMainFrameWnd, &CMainFrameWnd::OnGoBack>(*this), m_connectionScope);
		m_pGraphWnd->Signals().gotoSelectionChanged.Connect(DocGotoSelectionChangedSignal::Delegate::FromMemberFunction<CMainFrameWnd, &CMainFrameWnd::OnGotoSelectionChanged>(*this), m_connectionScope);

		// Hide docking client to ensure there are no gaps between panes.
		m_dockingPaneManager.HideClient(TRUE);

		// Close log output panes B, C and D, and compiler output pane by default.
		m_dockingPaneManager.ClosePane(IDW_SCHEMATYC2_LOG_OUTPUT_PANE_B);
		m_dockingPaneManager.ClosePane(IDW_SCHEMATYC2_LOG_OUTPUT_PANE_C);
		m_dockingPaneManager.ClosePane(IDW_SCHEMATYC2_LOG_OUTPUT_PANE_D);
		m_dockingPaneManager.ClosePane(IDW_SCHEMATYC2_COMPILER_OUTPUT_PANE);
		// Load settings and initialize pane titles.
		LoadSettings();
		InitPaneTitles();
		return TRUE;
	}

	//////////////////////////////////////////////////////////////////////////
	void CMainFrameWnd::DoDataExchange(CDataExchange* pDataExchange)
	{
		__super::DoDataExchange(pDataExchange);
	}

	//////////////////////////////////////////////////////////////////////////
	void CMainFrameWnd::PostNcDestroy()
	{
		delete this;
	}

	//////////////////////////////////////////////////////////////////////////
	BOOL CMainFrameWnd::PreTranslateMessage(MSG* pMsg)
	{
		if(__super::PreTranslateMessage(pMsg))
		{
			return TRUE;
		}
		else if((pMsg->message >= WM_KEYFIRST) && (pMsg->message <= WM_KEYLAST))
		{
			::TranslateMessage(pMsg);
			::DispatchMessage(pMsg);
			return TRUE;
		}
		return FALSE;
	}

	//////////////////////////////////////////////////////////////////////////
	BOOL CMainFrameWnd::OnViewMenuCommand(UINT nID)
	{
		UINT	paneId = 0;
		switch(nID)
		{
		case ID_SCHEMATYC_VIEW_BROWSER:
			{
				paneId = IDW_SCHEMATYC2_BROWSER_PANE;
				break;
			}
		case ID_SCHEMATYC_VIEW_ENV_BROWSER:
			{
				paneId = IDW_SCHEMATYC2_ENV_BROWSER_PANE;
				break;
			}
		case ID_SCHEMATYC_VIEW_DETAIL:
			{
				paneId = IDW_SCHEMATYC2_DETAIL_PANE;
				break;
			}
		case ID_SCHEMATYC_VIEW_GRAPH:
			{
				paneId = IDW_SCHEMATYC2_GRAPH_PANE;
				break;
			}
		case ID_SCHEMATYC_VIEW_LOG_OUTPUT_A:
			{
				paneId = IDW_SCHEMATYC2_LOG_OUTPUT_PANE_A;
				break;
			}
		case ID_SCHEMATYC_VIEW_LOG_OUTPUT_B:
			{
				paneId = IDW_SCHEMATYC2_LOG_OUTPUT_PANE_B;
				break;
			}
		case ID_SCHEMATYC_VIEW_LOG_OUTPUT_C:
			{
				paneId = IDW_SCHEMATYC2_LOG_OUTPUT_PANE_C;
				break;
			}
		case ID_SCHEMATYC_VIEW_LOG_OUTPUT_D:
			{
				paneId = IDW_SCHEMATYC2_LOG_OUTPUT_PANE_D;
				break;
			}
		case ID_SCHEMATYC_VIEW_COMPILER_OUTPUT:
			{
				paneId = IDW_SCHEMATYC2_COMPILER_OUTPUT_PANE;
				break;
			}
		case ID_SCHEMATYC_VIEW_PREVIEW:
			{
				paneId = IDW_SCHEMATYC2_PREVIEW_PANE;
				break;
			}
		default:
			{
				return FALSE;
			}
		}
		CXTPDockingPane	*pPane = m_dockingPaneManager.FindPane(paneId);
		CRY_ASSERT(pPane != NULL);
		if(pPane != NULL)
		{
			if(pPane->IsClosed() == TRUE)
			{
				m_dockingPaneManager.ShowPane(pPane);
			}
			else
			{
				m_dockingPaneManager.ClosePane(pPane);
			}
			return TRUE;
		}
		return FALSE;
	}

	//////////////////////////////////////////////////////////////////////////
	void CMainFrameWnd::OnUpdateViewMenuItem(CCmdUI* pCmdUI)
	{
		UINT	paneId = 0;
		switch(pCmdUI->m_nID)
		{
		case ID_SCHEMATYC_VIEW_BROWSER:
			{
				paneId = IDW_SCHEMATYC2_BROWSER_PANE;
				break;
			}
		case ID_SCHEMATYC_VIEW_ENV_BROWSER:
			{
				paneId = IDW_SCHEMATYC2_ENV_BROWSER_PANE;
				break;
			}
		case ID_SCHEMATYC_VIEW_DETAIL:
			{
				paneId = IDW_SCHEMATYC2_DETAIL_PANE;
				break;
			}
		case ID_SCHEMATYC_VIEW_GRAPH:
			{
				paneId = IDW_SCHEMATYC2_GRAPH_PANE;
				break;
			}
		case ID_SCHEMATYC_VIEW_LOG_OUTPUT_A:
			{
				paneId = IDW_SCHEMATYC2_LOG_OUTPUT_PANE_A;
				break;
			}
		case ID_SCHEMATYC_VIEW_LOG_OUTPUT_B:
			{
				paneId = IDW_SCHEMATYC2_LOG_OUTPUT_PANE_B;
				break;
			}
		case ID_SCHEMATYC_VIEW_LOG_OUTPUT_C:
			{
				paneId = IDW_SCHEMATYC2_LOG_OUTPUT_PANE_C;
				break;
			}
		case ID_SCHEMATYC_VIEW_LOG_OUTPUT_D:
			{
				paneId = IDW_SCHEMATYC2_LOG_OUTPUT_PANE_D;
				break;
			}
		case ID_SCHEMATYC_VIEW_COMPILER_OUTPUT:
			{
				paneId = IDW_SCHEMATYC2_COMPILER_OUTPUT_PANE;
				break;
			}
		case ID_SCHEMATYC_VIEW_PREVIEW:
			{
				paneId = IDW_SCHEMATYC2_PREVIEW_PANE;
				break;
			}
		default:
			{
				pCmdUI->SetCheck(0);
				return;
			}
		}
		CXTPDockingPane	*pPane = m_dockingPaneManager.FindPane(paneId);
		CRY_ASSERT(pPane != NULL);
		if(pPane != NULL)
		{
			pCmdUI->SetCheck(pPane->IsClosed() == TRUE ? 0 : 1);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	BOOL CMainFrameWnd::OnWorldDebugMenuCommand(UINT nID)
	{
		ICVar*	pCVar = gEnv->pConsole->GetCVar("sc_WorldDebugConfig");
		CRY_ASSERT(pCVar != NULL);
		if(pCVar != NULL)
		{
			stack_string	worldDebugConfig = pCVar->GetString();
			char					debugOption[2] = "";
			switch(nID)
			{
			case ID_SCHEMATYC_WORLD_DEBUG_SHOW_STATES:
				{
					debugOption[0] = 's';
					break;
				}
			case ID_SCHEMATYC_WORLD_DEBUG_SHOW_VARIABLES:
				{
					debugOption[0] = 'v';
					break;
				}
			case ID_SCHEMATYC_WORLD_DEBUG_SHOW_CONTAINERS:
				{
					debugOption[0] = 'c';
					break;
				}
			case ID_SCHEMATYC_WORLD_DEBUG_SHOW_TIMERS:
				{
					debugOption[0] = 't';
					break;
				}
			case ID_SCHEMATYC_WORLD_DEBUG_SHOW_ACTIONS:
				{
					debugOption[0] = 'a';
					break;
				}
			case ID_SCHEMATYC_WORLD_DEBUG_SHOW_SIGNAL_HISTORY:
				{
					debugOption[0] = 'h';
					break;
				}
			}
			stack_string::size_type	pos = worldDebugConfig.find(debugOption);
			if(pos != stack_string::npos)
			{
				worldDebugConfig.erase(pos, 1);
			}
			else
			{
				worldDebugConfig.append(debugOption);
			}
			pCVar->Set(worldDebugConfig.c_str());
			return TRUE;
		}
		return FALSE;
	}

	//////////////////////////////////////////////////////////////////////////
	void CMainFrameWnd::OnUpdateWorldDebugMenuItem(CCmdUI* pCmdUI)
	{
		ICVar*	pCVar = gEnv->pConsole->GetCVar("sc_WorldDebugConfig");
		CRY_ASSERT(pCVar != NULL);
		if(pCVar != NULL)
		{
			const stack_string	worldDebugConfig = pCVar->GetString();
			char								debugOption[2] = "";
			switch(pCmdUI->m_nID)
			{
			case ID_SCHEMATYC_WORLD_DEBUG_SHOW_STATES:
				{
					debugOption[0] = 's';
					break;
				}
			case ID_SCHEMATYC_WORLD_DEBUG_SHOW_VARIABLES:
				{
					debugOption[0] = 'v';
					break;
				}
			case ID_SCHEMATYC_WORLD_DEBUG_SHOW_CONTAINERS:
				{
					debugOption[0] = 'c';
					break;
				}
			case ID_SCHEMATYC_WORLD_DEBUG_SHOW_TIMERS:
				{
					debugOption[0] = 't';
					break;
				}
			case ID_SCHEMATYC_WORLD_DEBUG_SHOW_ACTIONS:
				{
					debugOption[0] = 'a';
					break;
				}
			case ID_SCHEMATYC_WORLD_DEBUG_SHOW_SIGNAL_HISTORY:
				{
					debugOption[0] = 'h';
					break;
				}
			}
			pCmdUI->SetCheck(worldDebugConfig.find(debugOption) != stack_string::npos ? 1 : 0);
		}
		else
		{
			pCmdUI->SetCheck(0);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CMainFrameWnd::OnDebugSelectedEntity()
	{
		ICVar*	pCVar = gEnv->pConsole->GetCVar("sc_EntityDebug_Legacy");
		CRY_ASSERT(pCVar != NULL);
		if(pCVar != NULL)
		{
			IEntity*	pSelectedEntity = gEnv->pEntitySystem->GetEntity(PluginUtils::GetSelectedEntityId());
			pCVar->Set(pSelectedEntity != NULL ? pSelectedEntity->GetName() : "");
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CMainFrameWnd::OnUpdateDebugSelectedEntity(CCmdUI* pCmdUI)
	{
		stack_string	text = "Debug Selected Entity";
		ICVar*				pCVar = gEnv->pConsole->GetCVar("sc_EntityDebug_Legacy");
		const char*		selectedEntity = pCVar != NULL ? pCVar->GetString() : "";
		if(selectedEntity[0] != '\0')
		{
			text.append(" [");
			text.append(selectedEntity);
			text.append("]");
		}
		pCmdUI->SetText(text.c_str());
	}

	//////////////////////////////////////////////////////////////////////////
	BOOL CMainFrameWnd::OnEntityDebugMenuCommand(UINT nID)
	{
		ICVar*	pCVar = gEnv->pConsole->GetCVar("sc_EntityDebugConfig_Legacy");
		CRY_ASSERT(pCVar != NULL);
		if(pCVar != NULL)
		{
			stack_string	entityDebugConfig = pCVar->GetString();
			char					debugOption[2] = "";
			switch(nID)
			{
			case ID_SCHEMATYC_ENTITY_DEBUG_SHOW_STATES:
				{
					debugOption[0] = 's';
					break;
				}
			case ID_SCHEMATYC_ENTITY_DEBUG_SHOW_VARIABLES:
				{
					debugOption[0] = 'v';
					break;
				}
			case ID_SCHEMATYC_ENTITY_DEBUG_SHOW_CONTAINERS:
				{
					debugOption[0] = 'c';
					break;
				}
			case ID_SCHEMATYC_ENTITY_DEBUG_SHOW_TIMERS:
				{
					debugOption[0] = 't';
					break;
				}
			case ID_SCHEMATYC_ENTITY_DEBUG_SHOW_ACTIONS:
				{
					debugOption[0] = 'a';
					break;
				}
			case ID_SCHEMATYC_ENTITY_DEBUG_SHOW_SIGNAL_HISTORY:
				{
					debugOption[0] = 'h';
					break;
				}
			}
			stack_string::size_type	pos = entityDebugConfig.find(debugOption);
			if(pos != stack_string::npos)
			{
				entityDebugConfig.erase(pos, 1);
			}
			else
			{
				entityDebugConfig.append(debugOption);
			}
			pCVar->Set(entityDebugConfig.c_str());
			return TRUE;
		}
		return FALSE;
	}

	//////////////////////////////////////////////////////////////////////////
	void CMainFrameWnd::OnUpdateEntityDebugMenuItem(CCmdUI* pCmdUI)
	{
		ICVar*	pCVar = gEnv->pConsole->GetCVar("sc_EntityDebugConfig_Legacy");
		CRY_ASSERT(pCVar != NULL);
		if(pCVar != NULL)
		{
			const stack_string	entityDebugConfig = pCVar->GetString();
			char								debugOption[2] = "";
			switch(pCmdUI->m_nID)
			{
			case ID_SCHEMATYC_ENTITY_DEBUG_SHOW_STATES:
				{
					debugOption[0] = 's';
					break;
				}
			case ID_SCHEMATYC_ENTITY_DEBUG_SHOW_VARIABLES:
				{
					debugOption[0] = 'v';
					break;
				}
			case ID_SCHEMATYC_ENTITY_DEBUG_SHOW_CONTAINERS:
				{
					debugOption[0] = 'c';
					break;
				}
			case ID_SCHEMATYC_ENTITY_DEBUG_SHOW_TIMERS:
				{
					debugOption[0] = 't';
					break;
				}
			case ID_SCHEMATYC_ENTITY_DEBUG_SHOW_ACTIONS:
				{
					debugOption[0] = 'a';
					break;
				}
			case ID_SCHEMATYC_ENTITY_DEBUG_SHOW_SIGNAL_HISTORY:
				{
					debugOption[0] = 'h';
					break;
				}
			}
			pCmdUI->SetCheck(entityDebugConfig.find(debugOption) != stack_string::npos ? 1 : 0);
		}
		else
		{
			pCmdUI->SetCheck(0);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CMainFrameWnd::OnHelpViewWiki()
	{
		PluginUtils::ViewWiki("https://crywiki/display/GV/Schematyc");
	}

	//////////////////////////////////////////////////////////////////////////
	void CMainFrameWnd::OnGotoOrFromTransitionGraph()
	{
		CBrowserCtrlItemPtr selectedItem = m_browserCtrl.GetSelectedItem();
		if (selectedItem)
		{
			if (selectedItem->GetIcon() != BrowserIcon::STATE_MACHINE)
			{
				if (CCustomTreeCtrlItemPtr item = selectedItem->FindAncestor(BrowserIcon::STATE_MACHINE))
				{
					m_browserCtrl.SelectItem(item->GetGUID(), SGUID());
					m_lastSelectedTreeItem = selectedItem->GetGUID();
				}
			}
			else
			{
				m_browserCtrl.SelectItem(m_lastSelectedTreeItem, SGUID());
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CMainFrameWnd::OnGoto()
	{
		m_gotoHelper.GoForward();
		UpdateGotoToolbarButtons();
		SetFocus();
	}

	//////////////////////////////////////////////////////////////////////////
	void CMainFrameWnd::OnGoBack()
	{
		m_gotoHelper.GoBack();
		UpdateGotoToolbarButtons();
		SetFocus();
	}

	//////////////////////////////////////////////////////////////////////////
	void CMainFrameWnd::OnGotoSelectionChanged(const SGotoSelection& gotoSelection)
	{
		m_gotoHelper.SetCurrentSelection(gotoSelection.gotoNode, gotoSelection.gobackNode);
	}

	//////////////////////////////////////////////////////////////////////////
	void CMainFrameWnd::OnHelpViewShortcutKeys()
	{
		PluginUtils::ViewWiki("https://crywiki/display/GV/Schematyc+Editor+Shortcut+Keys");
	}

	//////////////////////////////////////////////////////////////////////////
	void CMainFrameWnd::OnSave()
	{
		// De-select current details to ensure properties are up to date.
		m_pDetailWnd->Revert();
		CXTPDockingPane*	pDetailPane = m_dockingPaneManager.FindPane(IDW_SCHEMATYC2_DETAIL_PANE);
		CRY_ASSERT(pDetailPane);
		if(pDetailPane)
		{
			pDetailPane->SetTitle(_T("Detail"));
			pDetailPane->SetTabCaption(_T("Detail"));
		}
		// Save documents.
		GetSchematyc()->GetScriptRegistry().Save();
		// Save environment settings.
		GetSchematyc()->GetEnvRegistry().SaveAllSettings();
		// Refresh browser control.
		m_browserCtrl.RefreshDocs();
	}

	//////////////////////////////////////////////////////////////////////////
	void CMainFrameWnd::OnSaveSettings()
	{
		SaveSettings();
	}

	//////////////////////////////////////////////////////////////////////////
	void CMainFrameWnd::OnRefreshEnv()
	{
		// Save.
		//OnSave();
		// Send signal to refresh environment.
		//GetSchematyc()->RefreshEnv();
		// Compile all script files.
		//GetSchematyc()->GetCompiler().CompileAll();
		// Refresh environment browser and update compiler output.
		//m_envBrowserCtrl.Refresh();
		//UpdateCompilerOutputCtrl(false);
	}

	//////////////////////////////////////////////////////////////////////////
	void CMainFrameWnd::OnUpdateRefreshEnv(CCmdUI* pCmdUI)
	{
		// For non-code builds disable button to refresh C++ environment.
		const SFileVersion currentBuildVersion = gEnv->pSystem->GetBuildVersion();
		const SFileVersion productionVersion = gEnv->pSystem->GetProductVersion();
		pCmdUI->Enable(currentBuildVersion == productionVersion);
	}

	//////////////////////////////////////////////////////////////////////////
	LRESULT CMainFrameWnd::OnDockingPaneNotify(WPARAM wParam, LPARAM lParam)
	{
		if(wParam == XTP_DPN_SHOWWINDOW)
		{
			if(CXTPDockingPane*	pDockingPane = reinterpret_cast<CXTPDockingPane *>(lParam))
			{
				if(!pDockingPane->IsValid())
				{
					switch(pDockingPane->GetID())
					{
					case IDW_SCHEMATYC2_BROWSER_PANE:
						{
							pDockingPane->Attach(&m_browserCtrl);
							m_browserCtrl.ShowWindow(SW_SHOW);
							break;
						}
					case IDW_SCHEMATYC2_ENV_BROWSER_PANE:
						{
							pDockingPane->Attach(&m_envBrowserCtrl);
							m_envBrowserCtrl.ShowWindow(SW_SHOW);
							break;
						}
					case IDW_SCHEMATYC2_DETAIL_PANE:
						{
							pDockingPane->Attach(m_pDetailWnd);
							m_pDetailWnd->ShowWindow(SW_SHOW);
							m_pDetailWnd->InitLayout();
							break;
						}
					case IDW_SCHEMATYC2_GRAPH_PANE:
						{
							pDockingPane->Attach(m_pGraphWnd);
							m_pGraphWnd->ShowWindow(SW_SHOW);
							m_pGraphWnd->InitLayout();
							break;
						}
					case IDW_SCHEMATYC2_LOG_OUTPUT_PANE_A:
						{
							pDockingPane->Attach(m_pLogWndA);
							m_pLogWndA->ShowWindow(SW_SHOW);
							m_pLogWndA->InitLayout();
							break;
						}
					case IDW_SCHEMATYC2_LOG_OUTPUT_PANE_B:
						{
							pDockingPane->Attach(m_pLogWndB);
							m_pLogWndB->ShowWindow(SW_SHOW);
							m_pLogWndB->InitLayout();
							break;
						}
					case IDW_SCHEMATYC2_LOG_OUTPUT_PANE_C:
						{
							pDockingPane->Attach(m_pLogWndC);
							m_pLogWndC->ShowWindow(SW_SHOW);
							m_pLogWndC->InitLayout();
							break;
						}
					case IDW_SCHEMATYC2_LOG_OUTPUT_PANE_D:
						{
							pDockingPane->Attach(m_pLogWndD);
							m_pLogWndD->ShowWindow(SW_SHOW);
							m_pLogWndD->InitLayout();
							break;
						}
					case IDW_SCHEMATYC2_COMPILER_OUTPUT_PANE:
						{
							pDockingPane->Attach(&m_compilerOutputCtrl);
							m_compilerOutputCtrl.ShowWindow(SW_SHOW);
							break;
						}
					case IDW_SCHEMATYC2_PREVIEW_PANE:
						{
							pDockingPane->Attach(m_pPreviewWnd);
							m_pPreviewWnd->ShowWindow(SW_SHOW);
							m_pPreviewWnd->InitLayout();
							break;
						}
					}
				}
			}
			return true;
		}
		else
		{
			return false;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CMainFrameWnd::OnDestroy()
	{
		SaveSettings();
	}

	//////////////////////////////////////////////////////////////////////////
	void CMainFrameWnd::RegisterViewClass()
	{
		GetIEditor()->GetClassFactory()->RegisterClass(new CMainViewClass());
	}

	//////////////////////////////////////////////////////////////////////////
	void CMainFrameWnd::SelectItem(SGUID& itemGuid, SGUID& childGuid)
	{
		m_browserCtrl.SelectItem(itemGuid, childGuid);
	}

	//////////////////////////////////////////////////////////////////////////
	void CMainFrameWnd::GotoItem(SGUID& itemGuid, SGUID& lastItemGuid)
	{
		m_gotoHelper.SetCurrentSelection(itemGuid, lastItemGuid);
		OnGoto();
	}

	void CMainFrameWnd::DiffSelectedItem()
	{
		m_browserCtrl.DiffSelectedItem();
	}

	//////////////////////////////////////////////////////////////////////////
	bool CMainFrameWnd::CreateMenuAndToolbar()
	{
		LOADING_TIME_PROFILE_SECTION;
		// Initialize command bars.
		if(!CXTPFrameWnd::InitCommandBars())
		{
			return false;
		}
		CXTPCommandBars*	pCommandBars = CXTPFrameWnd::GetCommandBars();
		CRY_ASSERT(pCommandBars);
		if(!pCommandBars)
		{
			return false;
		}
		// Load menu.
		pCommandBars->GetShortcutManager()->SetAccelerators(IDR_SCHEMATYC_MENU);
		pCommandBars->GetShortcutManager()->LoadShortcuts("Shortcuts\\Schematyc");
		CXTPCommandBar*	pMenu = pCommandBars->SetMenu( _T("Menu"), IDR_SCHEMATYC_MENU);
		CRY_ASSERT(pMenu != NULL);
		if(pMenu != NULL)
		{
			pMenu->SetFlags(xtpFlagStretched);
			pMenu->EnableCustomization(TRUE);
		}
		// Create toolbar.
		CXTPToolBar*	pToolBar = pCommandBars->Add(_T("ToolBar"), xtpBarTop);
		if(!pToolBar)
		{
			return false;
		}
		pToolBar->EnableCustomization(FALSE);
		pToolBar->SetCustomizeDialogPresent(FALSE);
		pToolBar->ShowExpandButton(FALSE);
		pToolBar->LoadToolBar(IDR_SCHEMATYC_TOOLBAR);

		if (CXTPControl* pGoTo = GetToolbarControl(pToolBar, ID_SCHEMATYC_GO_TO))
		{
			pGoTo->SetFlags(xtpFlagManualUpdate);
		}

		if(CXTPControl* pGoBack = GetToolbarControl(pToolBar, ID_SCHEMATYC_GO_BACK))
		{
			pGoBack->SetFlags(xtpFlagManualUpdate);
		}

		return true;
	}

	//////////////////////////////////////////////////////////////////////////
	void CMainFrameWnd::CreatePanes()
	{
		LOADING_TIME_PROFILE_SECTION;
		// Initialise docking pane manager
		m_dockingPaneManager.InstallDockingPanes(this);
		m_dockingPaneManager.SetTheme(xtpPaneThemeOffice2003);
		m_dockingPaneManager.SetThemedFloatingFrames(TRUE);
		m_dockingPaneManager.SetAlphaDockingContext(TRUE);
		m_dockingPaneManager.SetShowDockingContextStickers(TRUE);
		// Create panes.
		CXTPDockingPane* pBrowserPane = m_dockingPaneManager.CreatePane(IDW_SCHEMATYC2_BROWSER_PANE, CRect(0, 0, 200, 600), xtpPaneDockLeft);
		CXTPDockingPane* pGraphPane = m_dockingPaneManager.CreatePane(IDW_SCHEMATYC2_GRAPH_PANE, CRect(0, 0, 1000, 800), xtpPaneDockRight);
		CXTPDockingPane* pPreviewPane = m_dockingPaneManager.CreatePane(IDW_SCHEMATYC2_PREVIEW_PANE, CRect(0, 0, 1000, 800), xtpPaneDockRight, pBrowserPane);
		CXTPDockingPane* pLogOutputPaneA = m_dockingPaneManager.CreatePane(IDW_SCHEMATYC2_LOG_OUTPUT_PANE_A, CRect(0, 0, 1000, 200), xtpPaneDockBottom, pPreviewPane);
		CXTPDockingPane* pLogOutputPaneB = m_dockingPaneManager.CreatePane(IDW_SCHEMATYC2_LOG_OUTPUT_PANE_B, CRect(0, 0, 1000, 200), xtpPaneDockBottom, pPreviewPane);
		CXTPDockingPane* pLogOutputPaneC = m_dockingPaneManager.CreatePane(IDW_SCHEMATYC2_LOG_OUTPUT_PANE_C, CRect(0, 0, 1000, 200), xtpPaneDockBottom, pPreviewPane);
		CXTPDockingPane* pLogOutputPaneD = m_dockingPaneManager.CreatePane(IDW_SCHEMATYC2_LOG_OUTPUT_PANE_D, CRect(0, 0, 1000, 200), xtpPaneDockBottom, pPreviewPane);
		CXTPDockingPane* pCompilerOutputPane = m_dockingPaneManager.CreatePane(IDW_SCHEMATYC2_COMPILER_OUTPUT_PANE, CRect(0, 0, 500, 200), xtpPaneDockBottom, pPreviewPane);
		CXTPDockingPane* pDetailPane = m_dockingPaneManager.CreatePane(IDW_SCHEMATYC2_DETAIL_PANE, CRect(0, 0, 200, 600), xtpPaneDockRight, pPreviewPane);
		CXTPDockingPane* pEnvBrowserPane = m_dockingPaneManager.CreatePane(IDW_SCHEMATYC2_ENV_BROWSER_PANE, CRect(0, 0, 200, 400), xtpPaneDockBottom, pBrowserPane);
		// Arrange panes.
		m_dockingPaneManager.AttachPane(pGraphPane, pPreviewPane);
		m_dockingPaneManager.ShowPane(pPreviewPane);
		m_dockingPaneManager.AttachPane(pLogOutputPaneB, pLogOutputPaneA);
		m_dockingPaneManager.AttachPane(pLogOutputPaneC, pLogOutputPaneB);
		m_dockingPaneManager.AttachPane(pLogOutputPaneD, pLogOutputPaneC);
		m_dockingPaneManager.AttachPane(pCompilerOutputPane, pLogOutputPaneD);
		m_dockingPaneManager.ShowPane(pLogOutputPaneA);
	}

	//////////////////////////////////////////////////////////////////////////
	void CMainFrameWnd::LoadSettings()
	{
		LOADING_TIME_PROFILE_SECTION;
		stack_string settingsPath = GetIEditor()->GetUserFolder();
		settingsPath.append(SETTINGS_FOLDER_NAME);
		settingsPath.append("\\");
		settingsPath.append(SETTINGS_FILE_NAME);

		if(XmlNodeRef	xmlSettings = gEnv->pSystem->LoadXmlFromFile(settingsPath.c_str()))
		{
			if(XmlNodeRef	xmlGraphSettings = xmlSettings->findChild("Graph"))
			{
				m_pGraphWnd->LoadSettings(xmlGraphSettings);
			}
			if(XmlNodeRef	xmlLogASettings = xmlSettings->findChild("LogA"))
			{
				m_pLogWndA->LoadSettings(xmlLogASettings);
			}
			if(XmlNodeRef	xmlLogBSettings = xmlSettings->findChild("LogB"))
			{
				m_pLogWndB->LoadSettings(xmlLogBSettings);
			}
			if(XmlNodeRef	xmlLogCSettings = xmlSettings->findChild("LogC"))
			{
				m_pLogWndC->LoadSettings(xmlLogCSettings);
			}
			if(XmlNodeRef	xmlLogDSettings = xmlSettings->findChild("LogD"))
			{
				m_pLogWndD->LoadSettings(xmlLogDSettings);
			}
			if(XmlNodeRef	xmlPreviewSettings = xmlSettings->findChild("Preview"))
			{
				m_pPreviewWnd->LoadSettings(xmlPreviewSettings);
			}
		}

		stack_string	layoutPath = GetIEditor()->GetUserFolder();
		layoutPath.append(SETTINGS_FOLDER_NAME);
		layoutPath.append("\\");
		layoutPath.append(LAYOUT_FILE_NAME);

		CXTPDockingPaneLayout	dockingPaneLayout(&m_dockingPaneManager);
		if(dockingPaneLayout.LoadFromFile(layoutPath.c_str(), LAYOUT_SECTION))
		{
			m_dockingPaneManager.SetLayout(&dockingPaneLayout);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CMainFrameWnd::SaveSettings()
	{
		stack_string	settingsPath = GetIEditor()->GetUserFolder();
		settingsPath.append(SETTINGS_FOLDER_NAME);
		gEnv->pCryPak->MakeDir(settingsPath.c_str());
		settingsPath.append("\\");
		settingsPath.append(SETTINGS_FILE_NAME);

		XmlNodeRef	xmlSettings = gEnv->pSystem->CreateXmlNode("SchematycEditorSettings");
		if(XmlNodeRef xmlGraphSettings = m_pGraphWnd->SaveSettings("Graph"))
		{
			xmlSettings->addChild(xmlGraphSettings);
		}
		if(XmlNodeRef xmlLogASettings = m_pLogWndA->SaveSettings("LogA"))
		{
			xmlSettings->addChild(xmlLogASettings);
		}
		if(XmlNodeRef xmlLogBSettings = m_pLogWndB->SaveSettings("LogB"))
		{
			xmlSettings->addChild(xmlLogBSettings);
		}
		if(XmlNodeRef xmlLogCSettings = m_pLogWndC->SaveSettings("LogC"))
		{
			xmlSettings->addChild(xmlLogCSettings);
		}
		if(XmlNodeRef xmlLogDSettings = m_pLogWndD->SaveSettings("LogD"))
		{
			xmlSettings->addChild(xmlLogDSettings);
		}
		if(XmlNodeRef xmlPreviewSettings = m_pPreviewWnd->SaveSettings("Preview"))
		{
			xmlSettings->addChild(xmlPreviewSettings);
		}
		xmlSettings->saveToFile(settingsPath.c_str());

		stack_string	layoutPath = GetIEditor()->GetUserFolder();
		layoutPath.append(SETTINGS_FOLDER_NAME);
		layoutPath.append("\\");
		layoutPath.append(LAYOUT_FILE_NAME);

		CXTPDockingPaneLayout	dockingPaneLayout(&m_dockingPaneManager);
		m_dockingPaneManager.GetLayout(&dockingPaneLayout);
		dockingPaneLayout.SaveToFile(layoutPath.c_str(), LAYOUT_SECTION);
	}

	//////////////////////////////////////////////////////////////////////////
	void CMainFrameWnd::InitPaneTitles()
	{
		LOADING_TIME_PROFILE_SECTION;

		CXTPDockingPane* pBrowserPane = m_dockingPaneManager.FindPane(IDW_SCHEMATYC2_BROWSER_PANE);
		if(pBrowserPane)
		{
			pBrowserPane->SetTitle(_T("Browser"));
			pBrowserPane->SetTabCaption(_T("Browser"));
		}

		CXTPDockingPane* pEnvBrowserPane = m_dockingPaneManager.FindPane(IDW_SCHEMATYC2_ENV_BROWSER_PANE);
		if(pEnvBrowserPane)
		{
			pEnvBrowserPane->SetTitle(_T("Environment Browser"));
			pEnvBrowserPane->SetTabCaption(_T("Environment Browser"));
		}

		CXTPDockingPane* pDetailPane = m_dockingPaneManager.FindPane(IDW_SCHEMATYC2_DETAIL_PANE);
		if(pDetailPane)
		{
			pDetailPane->SetTitle(_T("Detail"));
			pDetailPane->SetTabCaption(_T("Detail"));
		}

		CXTPDockingPane* pGraphPane = m_dockingPaneManager.FindPane(IDW_SCHEMATYC2_GRAPH_PANE);
		if(pGraphPane)
		{
			pGraphPane->SetTitle(_T("Graph"));
			pGraphPane->SetTabCaption(_T("Graph"));
		}

		CXTPDockingPane* pLogOutputPaneA = m_dockingPaneManager.FindPane(IDW_SCHEMATYC2_LOG_OUTPUT_PANE_A);
		if(pLogOutputPaneA)
		{
			pLogOutputPaneA->SetTitle(_T("Log Output A"));
			pLogOutputPaneA->SetTabCaption(_T("Log Output A"));
		}

		CXTPDockingPane* pLogOutputPaneB = m_dockingPaneManager.FindPane(IDW_SCHEMATYC2_LOG_OUTPUT_PANE_B);
		if(pLogOutputPaneB)
		{
			pLogOutputPaneB->SetTitle(_T("Log Output B"));
			pLogOutputPaneB->SetTabCaption(_T("Log Output B"));
		}

		CXTPDockingPane* pLogOutputPaneC = m_dockingPaneManager.FindPane(IDW_SCHEMATYC2_LOG_OUTPUT_PANE_C);
		if(pLogOutputPaneC)
		{
			pLogOutputPaneC->SetTitle(_T("Log Output C"));
			pLogOutputPaneC->SetTabCaption(_T("Log Output C"));
		}

		CXTPDockingPane* pLogOutputPaneD = m_dockingPaneManager.FindPane(IDW_SCHEMATYC2_LOG_OUTPUT_PANE_D);
		if(pLogOutputPaneD)
		{
			pLogOutputPaneD->SetTitle(_T("Log Output D"));
			pLogOutputPaneD->SetTabCaption(_T("Log Output D"));
		}

		CXTPDockingPane* pCompilerOutputPane = m_dockingPaneManager.FindPane(IDW_SCHEMATYC2_COMPILER_OUTPUT_PANE);
		if(pCompilerOutputPane)
		{
			pCompilerOutputPane->SetTitle(_T("Compiler Output"));
			pCompilerOutputPane->SetTabCaption(_T("Compiler Output"));
		}

		CXTPDockingPane* pPreviewPane = m_dockingPaneManager.FindPane(IDW_SCHEMATYC2_PREVIEW_PANE);
		if(pPreviewPane)
		{
			pPreviewPane->SetTitle(_T("Preview"));
			pPreviewPane->SetTabCaption(_T("Preview"));
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CMainFrameWnd::SelectGraph(TScriptFile& scriptFile, IScriptGraph& scriptGraph)
	{
		stack_string           graphTitle(scriptGraph.GetElement_New().GetName());
		const EScriptGraphType scriptGraphType = scriptGraph.GetType();
		switch(scriptGraphType)
		{
		case EScriptGraphType::AbstractInterfaceFunction:
			{
				graphTitle.append(" - Abstract Interface Function Graph");
				break;
			}
		case EScriptGraphType::Function:
			{
				graphTitle.append(" - Function Graph");
				break;
			}
		case EScriptGraphType::Condition:
			{
				graphTitle.append(" - Condition Graph");
				break;
			}
		case EScriptGraphType::SignalReceiver:
			{
				graphTitle.append(" - Signal Receiver Graph");
				break;
			}
		case EScriptGraphType::Constructor:
			{
				graphTitle.append(" - Constructor Graph");
				break;
			}
		case EScriptGraphType::Transition:
			{
				graphTitle.append(" - Transition Graph");
				break;
			}
		case EScriptGraphType::Property:
			{
				graphTitle.append(" - Property Graph");
				break;
			}
		}
		m_pGraphWnd->LoadGraph(&scriptFile, &scriptGraph);
		CXTPDockingPane* pGraphPane = m_dockingPaneManager.FindPane(IDW_SCHEMATYC2_GRAPH_PANE);
		CRY_ASSERT(pGraphPane);
		if(pGraphPane)
		{
			pGraphPane->SetTitle(_T(graphTitle.c_str()));
		}
		// Update compiler output.
		UpdateCompilerOutputCtrl(false);
	}

	//////////////////////////////////////////////////////////////////////////
	void CMainFrameWnd::ClearGraph()
	{
		m_pGraphWnd->LoadGraph(nullptr, nullptr);
		CXTPDockingPane*	pGraphPane = m_dockingPaneManager.FindPane(IDW_SCHEMATYC2_GRAPH_PANE);
		CRY_ASSERT(pGraphPane);
		if(pGraphPane)
		{
			pGraphPane->SetTitle(_T("Graph"));
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CMainFrameWnd::UpdateCompilerOutputCtrl(bool bRecompileGraph)
	{
		struct SStringStreamBuffer
		{
			inline SStringStreamBuffer()
			{
				data.reserve(2048);
			}

			inline void Write(const char* szInput)
			{
				if(szInput)
				{
					data.append(szInput);
				}
			}

			string data;
		};

		SStringStreamBuffer stringStreamBuffer;
		TScriptFile*  pScriptFile = m_pGraphWnd->GetScriptFile();
		IScriptGraph* pScriptGraph = m_pGraphWnd->GetScriptGraph();
		if(pScriptFile && pScriptGraph)
		{
			// TODO : Should be able to retrieve compiled script files from registry by name/filename!
			ILibPtr	pLib = bRecompileGraph ? CompileScriptFile(*pScriptFile) : gEnv->pSchematyc2->GetLibRegistry().GetLib(pScriptFile->GetFileName());
			if(pLib)
			{
				pLib->PreviewGraphFunctions(pScriptGraph->GetElement_New().GetGUID(), StringStreamOutCallback::FromMemberFunction<SStringStreamBuffer, &SStringStreamBuffer::Write>(stringStreamBuffer));
			}
		}
		m_compilerOutputCtrl.SetText(stringStreamBuffer.data.c_str());
	}

	//////////////////////////////////////////////////////////////////////////
	void CMainFrameWnd::UpdatePreview()
	{
		m_pDetailWnd->UpdateView();
	}

	//////////////////////////////////////////////////////////////////////////
	ILibPtr CMainFrameWnd::CompileScriptFile(TScriptFile& scriptFile)
	{
		if(ILibPtr pLib = GetSchematyc()->GetCompiler().Compile(scriptFile))
		{
			GetSchematyc()->GetLibRegistry().RegisterLib(pLib);
			return pLib;
		}
		return ILibPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	void CMainFrameWnd::OnBrowserSelection(const SBrowserSelectionParams& params)
	{
		const CBrowserCtrlItemPtr& pBrowserItem = params.item;
		if(pBrowserItem)
		{
			// Reset detail property tree.
			m_pDetailWnd->Detach();
			m_pDetailScriptFile = m_browserCtrl.GetItemScriptFile(*pBrowserItem);
			// Select detail element and graph.
			IDetailItemPtr pDetailItem;
			stack_string   detailTitle = "Detail";
			IScriptGraph*  pScriptGraph = nullptr;
			switch(pBrowserItem->GetIcon())
			{
			case BrowserIcon::GROUP:
				{
					IScriptGroup* pScriptGroup = m_browserCtrl.GetItemScriptGroup(*pBrowserItem);
					CRY_ASSERT(pScriptGroup);
					if(pScriptGroup)
					{
						pDetailItem.reset(new CScriptElementDetailItem(m_pDetailScriptFile, pScriptGroup));
					}
					detailTitle.append(" - Group");
					break;
				}
			case BrowserIcon::ENUMERATION:
				{
					IScriptEnumeration* pScriptEnumeration = m_browserCtrl.GetItemScriptEnumeration(*pBrowserItem);
					CRY_ASSERT(pScriptEnumeration);
					if(pScriptEnumeration)
					{
						pDetailItem.reset(new CScriptElementDetailItem(m_pDetailScriptFile, pScriptEnumeration));
					}
					detailTitle.append(" - Enumeration");
					break;
				}
			case BrowserIcon::STRUCTURE:
				{
					IScriptStructure* pScriptStructure = m_browserCtrl.GetItemScriptStructure(*pBrowserItem);
					CRY_ASSERT(pScriptStructure);
					if(pScriptStructure)
					{
						pDetailItem.reset(new CScriptElementDetailItem(m_pDetailScriptFile, pScriptStructure));
					}
					detailTitle.append(" - Structure");
					break;
				}
			case BrowserIcon::SIGNAL:
				{
					IScriptSignal* pScriptSignal = m_browserCtrl.GetItemScriptSignal(*pBrowserItem);
					CRY_ASSERT(pScriptSignal);
					if(pScriptSignal)
					{
						pDetailItem.reset(new CScriptElementDetailItem(m_pDetailScriptFile, pScriptSignal));
					}
					detailTitle.append(" - Signal");
					break;
				}
			case BrowserIcon::ABSTRACT_INTERFACE:
				{
					IScriptAbstractInterface* pScriptAbstractInterface = m_browserCtrl.GetItemScriptAbstractInterface(*pBrowserItem);
					CRY_ASSERT(pScriptAbstractInterface);
					if(pScriptAbstractInterface)
					{
						pDetailItem.reset(new CScriptElementDetailItem(m_pDetailScriptFile, pScriptAbstractInterface));
					}
					detailTitle.append(" - Abstract Interface");
					break;
				}
			case BrowserIcon::ABSTRACT_INTERFACE_FUNCTION:
				{
					IScriptAbstractInterfaceFunction* pScriptAbstractInterfaceFunction = m_browserCtrl.GetItemScriptAbstractInterfaceFunction(*pBrowserItem);
					CRY_ASSERT(pScriptAbstractInterfaceFunction);
					if(pScriptAbstractInterfaceFunction)
					{
						pDetailItem.reset(new CScriptElementDetailItem(m_pDetailScriptFile, pScriptAbstractInterfaceFunction));
					}
					detailTitle.append(" - Abstract Interface Function");
					break;
				}
			case BrowserIcon::ABSTRACT_INTERFACE_TASK:
				{
					IScriptAbstractInterfaceTask* pScriptAbstractInterfaceTask = m_browserCtrl.GetItemScriptAbstractInterfaceTask(*pBrowserItem);
					CRY_ASSERT(pScriptAbstractInterfaceTask);
					if(pScriptAbstractInterfaceTask)
					{
						pDetailItem.reset(new CScriptElementDetailItem(m_pDetailScriptFile, pScriptAbstractInterfaceTask));
					}
					detailTitle.append(" - Abstract Interface Task");
					break;
				}
			case BrowserIcon::CLASS:
				{
					IScriptClass* pScriptClass = m_browserCtrl.GetItemScriptClass(*pBrowserItem);
					CRY_ASSERT(pScriptClass);
					if(pScriptClass)
					{
						pDetailItem.reset(new CScriptElementDetailItem(m_pDetailScriptFile, pScriptClass));
					}
					detailTitle.append(" - Class");
					break;
				}
			case BrowserIcon::CLASS_BASE:
				{
					IScriptClassBase* pScriptClassBase = m_browserCtrl.GetItemScriptClassBase(*pBrowserItem);
					CRY_ASSERT(pScriptClassBase);
					if(pScriptClassBase)
					{
						pDetailItem.reset(new CScriptElementDetailItem(m_pDetailScriptFile, pScriptClassBase));
					}
					detailTitle.append(" - Class Base");
					break;
				}
			case BrowserIcon::VARIABLE:
				{
					IScriptVariable* pScriptVariable = m_browserCtrl.GetItemScriptVariable(*pBrowserItem);
					CRY_ASSERT(pScriptVariable);
					if(pScriptVariable)
					{
						pDetailItem.reset(new CScriptElementDetailItem(m_pDetailScriptFile, pScriptVariable));
					}
					detailTitle.append(" - Variable");
					break;
				}
			case BrowserIcon::PROPERTY:
				{
					IScriptProperty* pScriptProperty = m_browserCtrl.GetItemScriptProperty(*pBrowserItem);
					CRY_ASSERT(pScriptProperty);
					if(pScriptProperty)
					{
						pDetailItem.reset(new CScriptElementDetailItem(m_pDetailScriptFile, pScriptProperty));
					}
					detailTitle.append(" - Property");
					break;
				}
			case BrowserIcon::TIMER:
				{
					IScriptTimer* pScriptTimer = m_browserCtrl.GetItemScriptTimer(*pBrowserItem);
					CRY_ASSERT(pScriptTimer);
					if(pScriptTimer)
					{
						pDetailItem.reset(new CScriptElementDetailItem(m_pDetailScriptFile, pScriptTimer));
					}
					detailTitle.append(" - Timer");
					break;
				}
			case BrowserIcon::ABSTRACT_INTERFACE_IMPLEMENTATION:
				{
					IScriptAbstractInterfaceImplementation* pScriptAbstractInterfaceImplementation = m_browserCtrl.GetItemScriptAbstractInterfaceImplementation(*pBrowserItem);
					CRY_ASSERT(pScriptAbstractInterfaceImplementation);
					if(pScriptAbstractInterfaceImplementation)
					{
						pDetailItem.reset(new CScriptElementDetailItem(m_pDetailScriptFile, pScriptAbstractInterfaceImplementation));
					}
					detailTitle.append(" - Abstract Interface Implementation");
					break;
				}
			case BrowserIcon::COMPONENT_INSTANCE:
				{
					IScriptComponentInstance* pScriptComponentInstance = m_browserCtrl.GetItemScriptComponentInstance(*pBrowserItem);
					CRY_ASSERT(pScriptComponentInstance);
					if(pScriptComponentInstance)
					{
						pDetailItem.reset(new CScriptElementDetailItem(m_pDetailScriptFile, pScriptComponentInstance));
						pScriptGraph = static_cast<IScriptGraphExtension*>(pScriptComponentInstance->GetExtensions().QueryExtension(EScriptExtensionId::Graph));
					}
					detailTitle.append(" - Component");
					break;
				}
			case BrowserIcon::ACTION_INSTANCE:
				{
					IScriptActionInstance* pScriptActionInstance = m_browserCtrl.GetItemScriptActionInstance(*pBrowserItem);
					CRY_ASSERT(pScriptActionInstance);
					if(pScriptActionInstance)
					{
						pDetailItem.reset(new CScriptElementDetailItem(m_pDetailScriptFile, pScriptActionInstance));
					}
					detailTitle.append(" - Action");
					break;
				}
			case BrowserIcon::STATE_MACHINE:
				{
					m_lastSelectedTreeItem = SGUID();
					IScriptStateMachine* pScriptStateMachine = m_browserCtrl.GetItemScriptStateMachine(*pBrowserItem);
					CRY_ASSERT(pScriptStateMachine);
					if(pScriptStateMachine)
					{
						pDetailItem.reset(new CScriptElementDetailItem(m_pDetailScriptFile, pScriptStateMachine));
						// Find state machine's transition graph.
						CRY_ASSERT(m_pDetailScriptFile);
						if(m_pDetailScriptFile)
						{
							TDocGraphVector docGraphs;
							DocUtils::CollectGraphs(*m_pDetailScriptFile, pScriptStateMachine->GetGUID(), false, docGraphs);
							CRY_ASSERT(docGraphs.size() == 1);
							if(docGraphs.size() == 1)
							{
								pScriptGraph = docGraphs[0];
							}
						}
					}
					detailTitle.append(" - State Machine");
					break;
				}
			case BrowserIcon::STATE:
				{
					IScriptState* pScriptState = m_browserCtrl.GetItemScriptState(*pBrowserItem);
					CRY_ASSERT(pScriptState);
					if(pScriptState)
					{
						pDetailItem.reset(new CScriptElementDetailItem(m_pDetailScriptFile, pScriptState));
					}
					detailTitle.append(" - State");
					break;
				}
			case BrowserIcon::SETTINGS_DOC:
				{
					IEnvSettingsPtr pEnvSettings = GetSchematyc()->GetEnvRegistry().GetSettings(pBrowserItem->GetText());
					CRY_ASSERT(pEnvSettings);
					if(pEnvSettings)
					{
						pDetailItem.reset(new CEnvSettingsDetailItem(pEnvSettings));
					}
					detailTitle.append(" - Settings");
					break;
				}
			case BrowserIcon::ABSTRACT_INTERFACE_FUNCTION_IMPLEMENTATION:
			case BrowserIcon::FUNCTION_GRAPH:
			case BrowserIcon::CONDITION_GRAPH:
			case BrowserIcon::SIGNAL_RECEIVER:
			case BrowserIcon::CONSTRUCTOR:
			case BrowserIcon::DESTRUCTOR:
				{
					IDocGraph* pDocGraph = m_browserCtrl.GetItemDocGraph(*pBrowserItem);
					CRY_ASSERT(pDocGraph);
					if(pDocGraph)
					{
						pDetailItem.reset(new CScriptElementDetailItem(m_pDetailScriptFile, pDocGraph));
						pScriptGraph = pDocGraph;
					}
					detailTitle.append(" - Graph");
					break;
				}
			}
			// Attach detail element to detail property tree.
			if(pDetailItem)
			{
				m_pDetailWnd->AttachItem(pDetailItem);
			}
			// Update detail pane.
			CXTPDockingPane* pDetailPane = m_dockingPaneManager.FindPane(IDW_SCHEMATYC2_DETAIL_PANE);
			CRY_ASSERT(pDetailPane);
			if(pDetailPane)
			{
				pDetailPane->SetTitle(detailTitle.c_str());
				pDetailPane->SetTabCaption(detailTitle.c_str());
			}
			// Select graph.
			if(pScriptGraph)
			{
				CRY_ASSERT(m_pDetailScriptFile);
				if(m_pDetailScriptFile)
				{
					SelectGraph(*m_pDetailScriptFile, *pScriptGraph);
					if(params.pChildGuid)
					{
						m_pGraphWnd->SelectAndFocusNode(*params.pChildGuid);
					}
				}
			}
			else
			{
				m_pGraphWnd->LoadGraph(nullptr, nullptr);
			}
			// Select preview item.
			const IScriptClass* pScriptClass = m_browserCtrl.GetItemScriptClass(*pBrowserItem);
			if(pScriptClass)
			{
				m_pPreviewWnd->SetClass(pScriptClass);
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CMainFrameWnd::OnBrowserItemRemoved(TScriptFile& scriptFile, IScriptElement& scriptElement)
	{
		m_pDetailWnd->Detach();
		ClearGraph();
		m_pPreviewWnd->SetClass(nullptr);
	}

	//////////////////////////////////////////////////////////////////////////
	void CMainFrameWnd::OnBrowserDocModified(TScriptFile& scriptFile)
	{
		scriptFile.Refresh(SScriptRefreshParams(EScriptRefreshReason::EditorDependencyModified));
		scriptFile.SetFlags(scriptFile.GetFlags() | EScriptFileFlags::Modified);
		// Revert details in case change affected currently selected object.
		m_pDetailWnd->Revert();
		// Re-compile script file.
		CompileScriptFile(scriptFile);
		// Update compiler output?
		if(&scriptFile == m_pGraphWnd->GetScriptFile())
		{
			UpdateCompilerOutputCtrl(false);
		}
		// Refresh graph view.
		m_pGraphWnd->Refresh();
		// Reset preview.
		m_pPreviewWnd->Reset();
	}

	//////////////////////////////////////////////////////////////////////////
	void CMainFrameWnd::OnDetailModification(IDetailItem& detailItem)
	{
		// Re-compile script file?
		if(m_pDetailScriptFile)
		{
			m_pDetailScriptFile->SetFlags(m_pDetailScriptFile->GetFlags() | EScriptFileFlags::Modified);
			CompileScriptFile(*m_pDetailScriptFile);
		}
		// Refresh browser control.
		m_browserCtrl.RefreshItem(detailItem.GetGUID());	// N.B. This temporary solution and ideally the browser control should interact directly with the detail panel.
		// Refresh graph view.
		m_pGraphWnd->Refresh();
		// Reset preview.
		m_pPreviewWnd->Reset();
	}

	//////////////////////////////////////////////////////////////////////////
	void CMainFrameWnd::OnGraphViewSelection(const SDocGraphViewSelection& selection)
	{
		m_pDetailWnd->Detach();
		if(selection.pNode)
		{
			EScriptGraphNodeType nodeType = selection.pNode->GetType();
			if (Schematyc2::NodeUtils::CanGoToNodeType(nodeType))
			{
				m_gotoHelper.SetCurrentSelection(selection.pNode->GetRefGUID(), selection.pNode->GetGraphGUID());
			}

			m_pDetailScriptFile = m_pGraphWnd->GetScriptFile();
			m_pDetailWnd->AttachItem(CDocGraphViewNodeDetailItemPtr(new CDocGraphViewNodeDetailItem(m_pDetailScriptFile, selection.pNode)));
			CXTPDockingPane*	pDetailPane = m_dockingPaneManager.FindPane(IDW_SCHEMATYC2_DETAIL_PANE);
			CRY_ASSERT(pDetailPane);
			if(pDetailPane)
			{
				pDetailPane->SetTitle(_T("Detail - Graph Node"));
				pDetailPane->SetTabCaption(_T("Detail - Graph Node"));
			}
		}
		else
		{
			m_gotoHelper.ClearCurrentSelection();

			m_pDetailScriptFile = nullptr;
			CXTPDockingPane*	pDetailPane = m_dockingPaneManager.FindPane(IDW_SCHEMATYC2_DETAIL_PANE);
			CRY_ASSERT(pDetailPane);
			if (pDetailPane)
			{
				pDetailPane->SetTitle(_T("Detail"));
				pDetailPane->SetTabCaption(_T("Detail"));
			}
		}

		UpdateGotoToolbarButtons();
	}

	//////////////////////////////////////////////////////////////////////////
	void CMainFrameWnd::OnGraphViewModification(TScriptFile& scriptFile)
	{
		scriptFile.SetFlags(scriptFile.GetFlags() | EScriptFileFlags::Modified);
		// Refresh browser control.
		m_browserCtrl.RefreshDocs();	// N.B. This temporary solution and ideally the browser control should interact directly with the detail panel.
		// Update compiler output?
		if(&scriptFile == m_pGraphWnd->GetScriptFile())
		{
			UpdateCompilerOutputCtrl(true);
		}
	}

	void CMainFrameWnd::UpdateGotoToolbarButtons()
	{
		bool back(false);
		bool forward(false);
		m_gotoHelper.GetCanGoForwardOrBack(forward, back);
		CXTPToolBar* pToolbar = GetCommandBars()->GetToolBar(IDR_SCHEMATYC_TOOLBAR);

		if (CXTPControl* pGoto = GetToolbarControl(pToolbar, ID_SCHEMATYC_GO_TO))
		{
			pGoto->SetEnabled(forward);
		}

		if (CXTPControl* pGoBack = GetToolbarControl(pToolbar, ID_SCHEMATYC_GO_BACK))
		{
			pGoBack->SetEnabled(back);
		}
	}

	CXTPControl* CMainFrameWnd::GetToolbarControl(CXTPToolBar* pToolbar, int id)
	{
		if (!pToolbar)
		{
			return nullptr;
		}

		for (int i = 0; i < pToolbar->GetControlCount(); ++i)
		{
			CXTPControl* pControl = pToolbar->GetControl(i);
			if (pControl->GetID() == id)
			{
				return pControl;
			}
		}

		return nullptr;
	}

}
