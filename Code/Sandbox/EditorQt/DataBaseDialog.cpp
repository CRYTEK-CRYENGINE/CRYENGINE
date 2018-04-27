// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "Material\Material.h"

#include "DataBaseDialog.h"
#include "EntityProtLibDialog.h"
#include "Particles\ParticleDialog.h"
#include "Prefabs\PrefabDialog.h"
#include "GameTokens\GameTokenDialog.h"
#include "Controls/SharedFonts.h"
#include "QtViewPane.h"

#define IDC_TABCTRL 1

IMPLEMENT_DYNAMIC(CDataBaseDialogPage, CToolbarDialog)
IMPLEMENT_DYNCREATE(CDataBaseDialog, CToolbarDialog)

class CDataBaseViewPaneClass : public IViewPaneClass
{
	//////////////////////////////////////////////////////////////////////////
	// IClassDesc
	//////////////////////////////////////////////////////////////////////////
	virtual ESystemClassID SystemClassID()	 override { return ESYSTEM_CLASS_VIEWPANE; };
	virtual const char*    ClassName()       override { return DATABASE_VIEW_NAME; };
	virtual const char*    Category()        override { return "Editor"; };
	virtual CRuntimeClass* GetRuntimeClass() override { return RUNTIME_CLASS(CDataBaseDialog); };
	virtual const char*    GetPaneTitle()    override { return _T(DATABASE_VIEW_NAME); };
	virtual bool           SinglePane()      override { return true; };
};

REGISTER_CLASS_DESC(CDataBaseViewPaneClass)

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CDataBaseDialog::CDataBaseDialog(CWnd* pParent)
	: CToolbarDialog(IDD, pParent)
{
	LOADING_TIME_PROFILE_SECTION;
	m_selectedCtrl = -1;

	Create(IDD, pParent);
}

//////////////////////////////////////////////////////////////////////////
CDataBaseDialog::~CDataBaseDialog()
{
	for (int i = 0; i < m_windows.size(); i++)
	{
		delete m_windows[i];
	}
}

BEGIN_MESSAGE_MAP(CDataBaseDialog, CToolbarDialog)
//{{AFX_MSG_MAP(CWnd)
ON_WM_CREATE()
ON_NOTIFY(TCN_SELCHANGE, IDC_TABCTRL, OnTabSelect)
ON_WM_SIZE()
//}}AFX_MSG_MAP
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
void CDataBaseDialog::PostNcDestroy()
{
	delete this;
}

//////////////////////////////////////////////////////////////////////////
void CDataBaseDialog::DoDataExchange(CDataExchange* pDX)
{
	CToolbarDialog::DoDataExchange(pDX);
}

BOOL CDataBaseDialog::OnInitDialog()
{
	ModifyStyle(0, WS_CLIPCHILDREN);
	//	CRect rcBorders(1,1,1,1);
	//m_menubar.CreateEx( this,TBSTYLE_FLAT,WS_CHILD|WS_VISIBLE|CBRS_ALIGN_TOP|CBRS_TOOLTIPS|CBRS_FLYBY|CBRS_SIZE_DYNAMIC|CBRS_BORDER_BOTTOM|CBRS_BORDER_LEFT|CBRS_BORDER_RIGHT );
	//m_menubar.ModifyXTBarStyle( 0 ,CBRS_XT_CLIENT_OUTLINE );
	//m_menubar.LoadMenuBar( IDR_DB_ENTITY );

	CRect rc;
	m_tabCtrl.Create(TCS_HOTTRACK | TCS_TABS | TCS_FOCUSNEVER | TCS_SINGLELINE | WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, rc, this, IDC_TABCTRL);
	m_tabCtrl.ModifyStyle(WS_BORDER, 0, 0);
	m_tabCtrl.ModifyStyleEx(WS_EX_CLIENTEDGE | WS_EX_STATICEDGE | WS_EX_WINDOWEDGE, 0, 0);
	//m_tabCtrl.SetImageList( &m_tabImageList );
	m_tabCtrl.SetFont(CFont::FromHandle((HFONT)SMFCFonts::GetInstance().hSystemFont));

	AddTab(_T("Entity Library"), new CEntityProtLibDialog(&m_tabCtrl));
	AddTab(_T("Prefabs Library"), new CPrefabDialog(&m_tabCtrl));
	AddTab(_T("Particles"), new CParticleDialog(&m_tabCtrl));
	AddTab(_T("GameTokens"), new CGameTokenDialog(&m_tabCtrl));    // deactivated for now

	Select(0);

	return TRUE;
}

void CDataBaseDialog::Activate(CDataBaseDialogPage* dlg, bool bActive)
{
	if (bActive)
	{
		dlg->ShowWindow(SW_SHOW);
		/*
		   UINT idMenu = dlg->GetDialogMenuID();
		   if (idMenu)
		   {
		   if (m_menubar)
		   {
		    m_menubar.Detach();
		   }
		   m_menubar.LoadMenuBar( idMenu );
		   }
		   else
		   {
		   m_menubar.LoadMenuBar( idMenu );
		   }
		 */
	}
	else
	{
		dlg->ShowWindow(SW_HIDE);
	}
	dlg->SetActive(bActive);
}

//////////////////////////////////////////////////////////////////////////
void CDataBaseDialog::OnSize(UINT nType, int cx, int cy)
{
	////////////////////////////////////////////////////////////////////////
	// Resize
	////////////////////////////////////////////////////////////////////////

	RECT rcRollUp;

	CToolbarDialog::OnSize(nType, cx, cy);

	if (!m_tabCtrl.m_hWnd)
		return;

	// Get the size of the client window
	GetClientRect(&rcRollUp);

	m_tabCtrl.MoveWindow(rcRollUp.left, rcRollUp.top,
	                     rcRollUp.right, rcRollUp.bottom);

	RecalcLayout();

	CRect rc;
	for (int i = 0; i < m_windows.size(); i++)
	{
		CRect irc;
		m_tabCtrl.GetItemRect(0, irc);
		m_tabCtrl.GetClientRect(rc);
		if (m_windows[i])
		{
			rc.left += 4;
			rc.right -= 4;
			rc.top += irc.bottom - irc.top + 8;
			//rc.top += irc.bottom-irc.top + 2;
			rc.bottom -= 4;
			m_windows[i]->MoveWindow(rc);
		}
	}

	/*
	   // Set the position of the listbox
	   m_pwndRollUpCtrl->SetWindowPos(NULL, rcRollUp.left + 3, rcRollUp.top + 3 + h, rcRollUp.right - 6,
	   rcRollUp.bottom - 6 - m_infoSize.cy - infoOfs - h, SWP_NOZORDER);
	 */
}

//////////////////////////////////////////////////////////////////////////
void CDataBaseDialog::OnTabSelect(NMHDR* pNMHDR, LRESULT* pResult)
{
	int sel = m_tabCtrl.GetCurSel();
	Select(sel);
}

//////////////////////////////////////////////////////////////////////////
CDataBaseDialogPage* CDataBaseDialog::SelectDialog(EDataBaseItemType type, IDataBaseItem* pItem)
{
	switch (type)
	{
	case EDB_TYPE_MATERIAL:
		return 0;
		break;
	case EDB_TYPE_ENTITY_ARCHETYPE:
		Select(0);
		break;
	case EDB_TYPE_PREFAB:
		Select(1);
		break;
	case EDB_TYPE_PARTICLE:
		Select(2);
		break;
	case EDB_TYPE_GAMETOKEN:
		Select(3);
		break;
	default:
		return 0;
	}
	CDataBaseDialogPage* pPage = GetCurrent();
	if (pItem && pPage && pPage->IsKindOf(RUNTIME_CLASS(CBaseLibraryDialog)))
	{
		CBaseLibraryDialog* dlg = (CBaseLibraryDialog*)pPage;
		if (dlg->CanSelectItem((CBaseLibraryItem*)pItem))
		{
			dlg->SelectItem((CBaseLibraryItem*)pItem);
		}
	}
	return pPage;
}

//////////////////////////////////////////////////////////////////////////
void CDataBaseDialog::Select(int num)
{
	if (num == m_selectedCtrl)
		return;
	int prevSelected = m_selectedCtrl;
	if (prevSelected >= 0 && prevSelected < m_windows.size())
	{
		Activate(m_windows[prevSelected], false);
	}
	m_selectedCtrl = num;
	m_tabCtrl.SetCurSel(num);
	for (int i = 0; i < m_windows.size(); i++)
	{
		if (i == num)
		{
			Activate(m_windows[i], true);
		}
		else
		{
			m_windows[i]->ShowWindow(SW_HIDE);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CDataBaseDialogPage* CDataBaseDialog::GetPage(int num)
{
	assert(num >= 0 && num < m_windows.size());
	return m_windows[num];
}

//////////////////////////////////////////////////////////////////////////
void CDataBaseDialog::AddTab(const char* szTitle, CDataBaseDialogPage* wnd)
{
	wnd->ModifyStyle(0, WS_CLIPCHILDREN);
	m_tabCtrl.InsertItem(m_tabCtrl.GetItemCount(), szTitle, 0);
	m_windows.push_back(wnd);
	wnd->SetParent(&m_tabCtrl);
	if (m_windows.size() - 1 != m_tabCtrl.GetCurSel())
	{
		wnd->ShowWindow(SW_HIDE);
	}
}

//////////////////////////////////////////////////////////////////////////
CDataBaseDialogPage* CDataBaseDialog::GetCurrent()
{
	ASSERT(m_selectedCtrl < m_windows.size());
	return m_windows[m_selectedCtrl];
}

//////////////////////////////////////////////////////////////////////////
void CDataBaseDialog::Update()
{
	if (GetCurrent())
		GetCurrent()->Update();
}

BOOL CDataBaseDialog::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo)
{
	// Extend the framework's command route from this dialog to currently selected tab dialog.
	if (m_selectedCtrl >= 0 && m_selectedCtrl < m_windows.size())
	{
		if (m_windows[m_selectedCtrl]->OnCmdMsg(nID, nCode, pExtra, pHandlerInfo) == TRUE)
			return TRUE;
	}
	return CToolbarDialog::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
}

