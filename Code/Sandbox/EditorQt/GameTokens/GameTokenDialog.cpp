// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GameTokenDialog.h"

#include "Dialogs/QGroupDialog.h"
#include "Dialogs/ToolbarDialog.h"

#include "GameTokenManager.h"
#include "GameTokenLibrary.h"
#include "GameTokenItem.h"

#include "ViewManager.h"
#include "Util/Clipboard.h"
#include "Controls/SharedFonts.h"
#include "Util/MFCUtil.h"

enum
{
	IDC_GAMETOKENS_TREE     = AFX_IDW_PANE_FIRST,
	IDC_GAMETOKENS          = 1,
	IDC_TOKEN_TASKPANEL     = 2,

	TOKEN_COLUMN_NAME       = 0,
	TOKEN_COLUMN_GROUP      = 1,
	TOKEN_COLUMN_VALUE      = 2,
	TOKEN_COLUMN_TYPE       = 3,
	TOKEN_COLUMN_DESC       = 4,

	IDC_TOKEN_NAME          = 10,
	IDC_TOKEN_TYPE          = 11,
	IDC_TOKEN_VALUE         = 12,
	IDC_TOKEN_DESCRIPTION   = 13,
	IDC_TOKEN_LOCAL_ONLY    = 14,

	IDC_TOKEN_NAME_2        = 20,
	IDC_TOKEN_TYPE_2        = 21,
	IDC_TOKEN_VALUE_2       = 22,
	IDC_TOKEN_DESCRIPTION_2 = 23,
	IDC_TOKEN_LOCAL_ONLY_2  = 24
};

IMPLEMENT_DYNAMIC(CGameTokenDialog, CBaseLibraryDialog)

BOOL CGameTokenDialog::CXTPTaskPanelSpecific::OnCommand(WPARAM wParam, LPARAM lParam)
{
	if (LOWORD(wParam) == IDC_TOKEN_LOCAL_ONLY)
	{
		m_poMyOwner->OnTokenLocalOnlyChange();
		return TRUE;
	}
	else
	{
		return __super::OnCommand(wParam, lParam);
	}
}

/*
   //////////////////////////////////////////////////////////////////////////
   class CGameTokenUIDefinition
   {
   public:
   CVarBlockPtr m_vars;

   CSmartVariableEnum<int> tokenType;
   _smart_ptr<CVariableBase> pTokenValue;

   //////////////////////////////////////////////////////////////////////////
   CVarBlock* CreateVars()
   {
    m_vars = new CVarBlock;

    tokenType.AddEnumItem( "Bool",eFDT_Bool );
    tokenType.AddEnumItem( "Int",eFDT_Int );
    tokenType.AddEnumItem( "Float",eFDT_Float );
    tokenType.AddEnumItem( "String",eFDT_String );
    tokenType.AddEnumItem( "Vec3",eFDT_Vec3 );

    // Create UI vars, using GameTokenParams TypeInfo.
    CVariableArray* pMainTable = AddGroup("Game Token");

    pTokenValue = new CVariable<int>;
    AddVariable( *pMainTable,tokenType,"Type" );
    AddVariable( *pMainTable,*pTokenValue,"Value" );

    return m_vars;
   }

   //////////////////////////////////////////////////////////////////////////
   void WriteToUI( CGameTokenItem *pGameTokens )
   {
   }

   //////////////////////////////////////////////////////////////////////////
   void ReadFromUI( CGameTokenItem *pGameTokens )
   {
    int tp = tokenType;
    switch (tp) {
    case eFDT_Float:
      {
        m_vars->Clear();
        // Create UI vars, using GameTokenParams TypeInfo.
        CVariableArray* pMainTable = AddGroup("Game Token");

        pTokenValue = new CVariable<int>;
        AddVariable( *pMainTable,tokenType,"Type" );
        AddVariable( *pMainTable,*pTokenValue,"Value" );
      }
      break;
    case eFDT_Bool:
      {
        m_vars->Clear();
        // Create UI vars, using GameTokenParams TypeInfo.
        CVariableArray* pMainTable = AddGroup("Game Token");

        pTokenValue = new CVariable<bool>;
        AddVariable( *pMainTable,tokenType,"Type" );
        AddVariable( *pMainTable,*pTokenValue,"Value" );
      }
      break;
    }

    // Update GameTokens.
    pGameTokens->Update();
   }

   private:

   //////////////////////////////////////////////////////////////////////////
   CVariableArray* AddGroup( const char *sName )
   {
    CVariableArray* pArray = new CVariableArray;
    pArray->AddRef();
    pArray->SetFlags(IVariable::UI_BOLD);
    if (sName)
      pArray->SetName(sName);
    m_vars->AddVariable(pArray);
    return pArray;
   }

   //////////////////////////////////////////////////////////////////////////
   void AddVariable( CVariableBase &varArray,CVariableBase &var,const char *varName,unsigned char dataType=IVariable::DT_SIMPLE )
   {
    if (varName)
      var.SetName(varName);
    var.SetDataType(dataType);
    varArray.AddChildVar(&var);
   }
   //////////////////////////////////////////////////////////////////////////
   void AddVariable( CVarBlock *vars,CVariableBase &var,const char *varName,unsigned char dataType=IVariable::DT_SIMPLE )
   {
    if (varName)
      var.SetName(varName);
    var.SetDataType(dataType);
    vars->AddVariable(&var);
   }
   };

   static CGameTokenUIDefinition gGameTokenUI;
   //////////////////////////////////////////////////////////////////////////
 */

//////////////////////////////////////////////////////////////////////////
class CGameTokenTypeCombo : public CComboBox
{
public:
	CGameTokenDialog* m_pDlg;

	CGameTokenTypeCombo(CGameTokenDialog* pDlg) : m_pDlg(pDlg) {};
	BOOL Create(DWORD dwStyle, const CRect& rc, CWnd* pParentWnd, UINT nID)
	{
		BOOL bRes = __super::Create(dwStyle | CBS_HASSTRINGS | CBS_DROPDOWNLIST, rc, pParentWnd, nID);
		SetFont(CFont::FromHandle((HFONT)SMFCFonts::GetInstance().hSystemFont));
		AddString("Bool");
		AddString("Int");
		AddString("Float");
		AddString("String");
		AddString("Vec3");
		return bRes;
	}

	DECLARE_MESSAGE_MAP()
	afx_msg void OnSelOk()
	{
		int nIndex = GetCurSel();
		if (nIndex != CB_ERR)
		{
			CString str;
			GetLBText(nIndex, str);
			if (m_pDlg->GetSelectedGameToken())
			{
				m_pDlg->GetSelectedGameToken()->SetTypeName(str);
				m_pDlg->GetSelectedGameToken()->Update();
			}
		}
		m_pDlg->UpdateSelectedItemInReport();
		DestroyWindow();
	}
	afx_msg void OnKillFocus()   { DestroyWindow(); }
	virtual void PostNcDestroy() { delete this; }
};
BEGIN_MESSAGE_MAP(CGameTokenTypeCombo, CComboBox)
ON_CONTROL_REFLECT(CBN_SELENDOK, OnSelOk)
ON_CONTROL_REFLECT(CBN_KILLFOCUS, OnKillFocus)
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
class CGameTokenValueEdit : public CEdit
{
public:
	CGameTokenDialog* m_pDlg;

	CGameTokenValueEdit(CGameTokenDialog* pDlg) : m_pDlg(pDlg) {};
	BOOL Create(DWORD dwStyle, const CRect& rc, CWnd* pParentWnd, UINT nID)
	{
		BOOL bRes = __super::Create(dwStyle, rc, pParentWnd, nID);
		SetFont(CFont::FromHandle((HFONT)SMFCFonts::GetInstance().hSystemFont));
		return bRes;
	}

	DECLARE_MESSAGE_MAP()
	afx_msg void OnChange()
	{
		CString str;
		GetWindowText(str);
		if (m_pDlg->GetSelectedGameToken())
		{
			if (GetDlgCtrlID() == IDC_TOKEN_DESCRIPTION_2)
				m_pDlg->GetSelectedGameToken()->SetDescription(str);
			else
				m_pDlg->GetSelectedGameToken()->SetValueString(str);
			m_pDlg->GetSelectedGameToken()->Update();
		}
		m_pDlg->UpdateSelectedItemInReport();
	}
	afx_msg void OnKillFocus()   { DestroyWindow(); }
	virtual void PostNcDestroy() { delete this; }
};
BEGIN_MESSAGE_MAP(CGameTokenValueEdit, CComboBox)
ON_CONTROL_REFLECT(EN_CHANGE, OnChange)
ON_CONTROL_REFLECT(EN_KILLFOCUS, OnKillFocus)
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
// CGameTokenDialog implementation.
//////////////////////////////////////////////////////////////////////////
CGameTokenDialog::CGameTokenDialog(CWnd* pParent) :
	m_bSkipUpdateItems(false),
	CBaseLibraryDialog(IDD_DB_ENTITY, pParent)
{
	m_pGameTokenManager = GetIEditorImpl()->GetGameTokenManager();
	m_pItemManager = m_pGameTokenManager;

	m_dragImage = 0;
	m_hDropItem = 0;

	// Immidiatly create dialog.
	Create(IDD_DB_ENTITY, pParent);
	m_wndTaskPanel.SetOwner(this);
}

CGameTokenDialog::~CGameTokenDialog()
{
}

void CGameTokenDialog::DoDataExchange(CDataExchange* pDX)
{
	CBaseLibraryDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CGameTokenTreeContainerDialog, CToolbarDialog)
ON_WM_SIZE()
END_MESSAGE_MAP()

BEGIN_MESSAGE_MAP(CGameTokenDialog, CBaseLibraryDialog)
ON_COMMAND(ID_DB_ADD, OnAddItem)
ON_NOTIFY(TVN_BEGINDRAG, IDC_GAMETOKENS_TREE, OnBeginDrag)
ON_NOTIFY(NM_RCLICK, IDC_GAMETOKENS_TREE, OnTreeRClick)
ON_WM_SIZE()
ON_WM_DESTROY()
ON_WM_MOUSEMOVE()
ON_WM_LBUTTONUP()

ON_CBN_SELENDOK(IDC_TOKEN_TYPE, OnTokenTypeChange)
ON_EN_CHANGE(IDC_TOKEN_VALUE, OnTokenValueChange)
ON_BN_CLICKED(IDC_TOKEN_LOCAL_ONLY, OnTokenLocalOnlyChange)
ON_EN_CHANGE(IDC_TOKEN_DESCRIPTION, OnTokenInfoChnage)

ON_MESSAGE(XTPWM_TASKPANEL_NOTIFY, OnTaskPanelNotify)
ON_NOTIFY(NM_DBLCLK, IDC_GAMETOKENS, OnReportDblClick)
ON_NOTIFY(XTP_NM_REPORT_SELCHANGED, IDC_GAMETOKENS, OnReportSelChange)
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
void CGameTokenDialog::OnDestroy()
{
	/*
	   int temp;
	   int HSplitter=0,VSplitter=0;
	   //m_wndHSplitter.GetRowInfo( 0,HSplitter,temp );
	   m_wndVSplitter.GetColumnInfo( 0,VSplitter,temp );
	   AfxGetApp()->WriteProfileInt("Dialogs\\GameTokensEditor","HSplitter",HSplitter );
	   AfxGetApp()->WriteProfileInt("Dialogs\\GameTokensEditor","VSplitter",VSplitter );
	 */

	//ReleaseGeometry();
	CBaseLibraryDialog::OnDestroy();
}

// CTVSelectKeyDialog message handlers
BOOL CGameTokenDialog::OnInitDialog()
{
	CBaseLibraryDialog::OnInitDialog();

	InitLibraryToolbar();
	//InitToolbar(IDR_DB_GAMETOKENS_BAR);

	CRect rc;
	GetClientRect(rc);

	//////////////////////////////////////////////////////////////////////////
	// Report control.
	//////////////////////////////////////////////////////////////////////////
	m_wndReport.Create(WS_CHILD | WS_TABSTOP | WS_VISIBLE | WM_VSCROLL, CRect(0, 0, 0, 0), this, IDC_GAMETOKENS);

	//  Add sample columns
	m_wndReport.AddColumn(new CXTPReportColumn(TOKEN_COLUMN_NAME, _T("Name"), 100, TRUE, 1));
	CXTPReportColumn* pGroupCol = m_wndReport.AddColumn(new CXTPReportColumn(TOKEN_COLUMN_GROUP, _T("Group"), 30, TRUE, XTP_REPORT_NOICON, TRUE, FALSE));
	m_wndReport.AddColumn(new CXTPReportColumn(TOKEN_COLUMN_VALUE, _T("Value"), 60, TRUE));
	m_wndReport.AddColumn(new CXTPReportColumn(TOKEN_COLUMN_TYPE, _T("Type"), 40, TRUE));
	m_wndReport.AddColumn(new CXTPReportColumn(TOKEN_COLUMN_DESC, _T("Description"), 100, TRUE));

	m_wndReport.GetColumns()->GetGroupsOrder()->Add(pGroupCol);
	m_wndReport.ShowGroupBy(FALSE);
	m_wndReport.SetMultipleSelection(TRUE);
	m_wndReport.SkipGroupsFocus(TRUE);
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Task panel.
	//////////////////////////////////////////////////////////////////////////
	m_wndTaskPanel.Create(WS_CHILD | WS_BORDER | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, rc, this, IDC_TOKEN_TASKPANEL);

	//m_taskImageList.Create( IDR_DB_GAMETOKENS_BAR,16,1,RGB(255,0,255) );
	//VERIFY( CMFCUtils::LoadTrueColorImageList( m_taskImageList,IDR_DB_GAMETOKENS_BAR,15,RGB(192,192,192) ) );

	//m_wndTaskPanel.SetImageList( &m_taskImageList );
	m_wndTaskPanel.SetBehaviour(xtpTaskPanelBehaviourExplorer);
	m_wndTaskPanel.SetTheme(xtpTaskPanelThemeNativeWinXP);
	m_wndTaskPanel.SetSelectItemOnFocus(TRUE);
	m_wndTaskPanel.AllowDrag(TRUE);

	m_wndTaskPanel.GetPaintManager()->m_rcGroupOuterMargins.SetRect(1, 3, 1, 3);
	m_wndTaskPanel.GetPaintManager()->m_rcGroupInnerMargins.SetRect(4, 4, 4, 4);
	m_wndTaskPanel.GetPaintManager()->m_rcItemOuterMargins.SetRect(1, 1, 1, 1);
	m_wndTaskPanel.GetPaintManager()->m_rcItemInnerMargins.SetRect(0, 0, 0, 0);
	m_wndTaskPanel.GetPaintManager()->m_rcControlMargins.SetRect(2, 0, 2, 0);
	m_wndTaskPanel.GetPaintManager()->m_nGroupSpacing = 3;

	//////////////////////////////////////////////////////////////////////////
	m_tokenValueEdit.Create(WS_CHILD | WS_VISIBLE | WS_BORDER, CRect(0, 0, 100, 18), this, IDC_TOKEN_VALUE);
	m_tokenValueEdit.SetFont(CFont::FromHandle((HFONT)SMFCFonts::GetInstance().hSystemFont));
	m_tokenValueEdit.SetParent(&m_wndTaskPanel);
	m_tokenValueEdit.SetOwner(this);

	//////////////////////////////////////////////////////////////////////////
	m_tokenTypeCombo.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | CBS_HASSTRINGS | CBS_DROPDOWNLIST, CRect(0, 0, 100, 100), this, IDC_TOKEN_TYPE);
	m_tokenTypeCombo.SetFont(CFont::FromHandle((HFONT)SMFCFonts::GetInstance().hSystemFont));
	m_tokenTypeCombo.SetParent(&m_wndTaskPanel);
	m_tokenTypeCombo.SetOwner(this);
	m_tokenTypeCombo.AddString("Bool");
	m_tokenTypeCombo.AddString("Int");
	m_tokenTypeCombo.AddString("Float");
	m_tokenTypeCombo.AddString("String");
	m_tokenTypeCombo.AddString("Vec3");

	m_tokenLocalOnlyCheck.Create("Local Only", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, CRect(0, 0, 60, 16), this, IDC_TOKEN_LOCAL_ONLY);
	m_tokenLocalOnlyCheck.SetFont(CFont::FromHandle(SMFCFonts::GetInstance().hSystemFont));
	m_tokenLocalOnlyCheck.SetParent(&m_wndTaskPanel);
	m_tokenLocalOnlyCheck.SetOwner(this);
	m_tokenLocalOnlyCheck.SetCheck(BST_UNCHECKED);

	//////////////////////////////////////////////////////////////////////////
	m_tokenInfoEdit.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE, CRect(0, 0, 100, 100), this, IDC_TOKEN_DESCRIPTION);
	m_tokenInfoEdit.SetFont(CFont::FromHandle((HFONT)SMFCFonts::GetInstance().hSystemFont));
	m_tokenInfoEdit.SetParent(&m_wndTaskPanel);
	m_tokenInfoEdit.SetOwner(this);

	// Add default tasks.
	CXTPTaskPanelGroupItem* pItem = NULL;
	CXTPTaskPanelGroup* pGroup = NULL;
	{
		pGroup = m_wndTaskPanel.AddGroup(1);
		pGroup->SetCaption("Library Tasks");

		pItem = pGroup->AddLinkItem(ID_DB_ADDLIB, 2);
		pItem->SetType(xtpTaskItemTypeLink);
		pItem = pGroup->AddLinkItem(ID_DB_DELLIB, 3);
		pItem->SetType(xtpTaskItemTypeLink);
		pItem = pGroup->AddLinkItem(ID_DB_LOADLIB, 1);
		pItem->SetType(xtpTaskItemTypeLink);
	}
	{
		pGroup = m_wndTaskPanel.AddGroup(2);
		pGroup->SetCaption("Game Token Tasks");

		pItem = pGroup->AddLinkItem(ID_DB_ADD, 6);
		pItem->SetType(xtpTaskItemTypeLink);
		pItem = pGroup->AddLinkItem(ID_DB_CLONE, 7);
		pItem->SetType(xtpTaskItemTypeLink);
		pItem = pGroup->AddLinkItem(ID_DB_REMOVE, 8);
		pItem->SetType(xtpTaskItemTypeLink);
		pItem = pGroup->AddLinkItem(ID_DB_RENAME, -1);
		pItem->SetType(xtpTaskItemTypeLink);
	}
	{
		pGroup = m_wndTaskPanel.AddGroup(3);
		pGroup->SetCaption("Selected Token");

		pItem = pGroup->AddLinkItem(IDC_TOKEN_NAME);
		pItem->SetType(xtpTaskItemTypeText);
		pItem = pGroup->AddTextItem("");
		pItem = pGroup->AddTextItem("Type:");
		pItem = pGroup->AddLinkItem(IDC_TOKEN_TYPE);
		pItem->SetType(xtpTaskItemTypeText);
		pItem = pGroup->AddControlItem(m_tokenTypeCombo);
		pItem = pGroup->AddTextItem("");
		pItem = pGroup->AddTextItem("Value:");
		pItem = pGroup->AddControlItem(m_tokenValueEdit);
		pItem = pGroup->AddTextItem("");
		pItem = pGroup->AddControlItem(m_tokenLocalOnlyCheck);
		pItem = pGroup->AddTextItem("");
		pItem = pGroup->AddTextItem("Description:");
		pItem = pGroup->AddControlItem(m_tokenInfoEdit);
	}
	//////////////////////////////////////////////////////////////////////////

	//m_listCtrl.EnableUserRowColor(true);

	/*
	   // Create left panel tree control.
	   m_treeCtrl.Create( WS_VISIBLE|WS_CHILD|WS_TABSTOP|WS_BORDER|TVS_HASBUTTONS|TVS_SHOWSELALWAYS|
	   TVS_LINESATROOT|TVS_HASLINES|TVS_FULLROWSELECT,rc,this,IDC_LIBRARY_ITEMS_TREE );

	   //int h2 = rc.Height()/2;
	   int h2 = 200;

	   int HSplitter = AfxGetApp()->GetProfileInt("Dialogs\\GameTokensEditor","HSplitter",200 );
	   int VSplitter = AfxGetApp()->GetProfileInt("Dialogs\\GameTokensEditor","VSplitter",200 );

	   m_wndVSplitter.CreateStatic( this,1,2,WS_CHILD|WS_VISIBLE );
	   //m_wndHSplitter.CreateStatic( &m_wndVSplitter,2,1,WS_CHILD|WS_VISIBLE );

	   //m_imageList.Create(IDC_GAMETOKENS_TREE, 16, 1, RGB (255, 0, 255));
	   CMFCUtils::LoadTrueColorImageList( m_imageList,IDB_TREE_VIEW,16,RGB(255,0,255) );

	   // TreeCtrl must be already created.
	   m_treeCtrl.SetParent( &m_wndVSplitter );
	   m_treeCtrl.SetImageList(&m_imageList,TVSIL_NORMAL);

	   //m_propsCtrl.Create( WS_VISIBLE|WS_CHILD|WS_BORDER,rc,&m_wndHSplitter,2 );
	   m_propsCtrl.Create( WS_VISIBLE|WS_CHILD|WS_BORDER,rc,&m_wndVSplitter,2 );


	   m_propsCtrl.AddVarBlock( gGameTokenUI.CreateVars() );
	   m_propsCtrl.SetUpdateCallback( functor(*this,OnUpdateProperties) );
	   m_propsCtrl.ExpandAllChilds( m_propsCtrl.GetRootItem(),false );
	   m_propsCtrl.EnableWindow( FALSE );

	   //m_wndHSplitter.SetPane( 0,0,&m_previewCtrl,CSize(100,HSplitter) );
	   //m_wndHSplitter.SetPane( 1,0,&m_propsCtrl,CSize(100,HSplitter) );

	   m_wndVSplitter.SetPane( 0,0,&m_treeCtrl,CSize(VSplitter,100) );
	   //m_wndVSplitter.SetPane( 0,1,&m_wndHSplitter,CSize(VSplitter,100) );
	   m_wndVSplitter.SetPane( 0,1,&m_propsCtrl,CSize(100,HSplitter) );
	 */
	RecalcLayout();

	ReloadLibs();

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

//////////////////////////////////////////////////////////////////////////
UINT CGameTokenDialog::GetDialogMenuID()
{
	return IDR_DB_ENTITY;
};

//////////////////////////////////////////////////////////////////////////
// Create the toolbar
void CGameTokenDialog::InitToolbar(UINT nToolbarResID)
{
	/*
	   VERIFY( m_toolbar.CreateEx(this, TBSTYLE_FLAT|TBSTYLE_WRAPABLE,
	   WS_CHILD|WS_VISIBLE|CBRS_TOP|CBRS_TOOLTIPS|CBRS_FLYBY|CBRS_SIZE_DYNAMIC) );
	   VERIFY( m_toolbar.LoadToolBar24(IDR_DB_GAMETOKENS_BAR) );

	   // Resize the toolbar
	   CRect rc;
	   GetClientRect(rc);
	   m_toolbar.SetWindowPos(NULL, 0, 0, rc.right, 70, SWP_NOZORDER);
	   CSize sz = m_toolbar.CalcDynamicLayout(TRUE,TRUE);

	   CBaseLibraryDialog::InitToolbar(nToolbarResID);
	 */
}

//////////////////////////////////////////////////////////////////////////
class CGameTokenRecord : public CXTPReportRecord
{
	DECLARE_DYNAMIC(CGameTokenRecord)
public:
	_smart_ptr<CGameTokenItem> m_pItem;

	CGameTokenRecord(CGameTokenItem* pItem)
	{
		m_pItem = pItem;

		CXTPReportRecordItem* pName = new CXTPReportRecordItemText(m_pItem->GetName());
		pName->SetBold(TRUE);
		AddItem(pName);
		AddItem(new CXTPReportRecordItemText(m_pItem->GetGroupName()));
		AddItem(new CXTPReportRecordItemText(m_pItem->GetValueString()));
		AddItem(new CXTPReportRecordItemText(m_pItem->GetTypeName()));
		AddItem(new CXTPReportRecordItemText(m_pItem->GetDescription()));
	}
	void Update()
	{
		((CXTPReportRecordItemText*)GetItem(0))->SetValue(m_pItem->GetName());
		((CXTPReportRecordItemText*)GetItem(1))->SetValue(m_pItem->GetGroupName());
		((CXTPReportRecordItemText*)GetItem(2))->SetValue(m_pItem->GetValueString());
		((CXTPReportRecordItemText*)GetItem(3))->SetValue(m_pItem->GetTypeName());
		((CXTPReportRecordItemText*)GetItem(4))->SetValue(m_pItem->GetDescription());
	}

	virtual void GetItemMetrics(XTP_REPORTRECORDITEM_DRAWARGS* pDrawArgs, XTP_REPORTRECORDITEM_METRICS* pItemMetrics)
	{
	}
};
IMPLEMENT_DYNAMIC(CGameTokenRecord, CXTPReportRecord);

//////////////////////////////////////////////////////////////////////////
void CGameTokenDialog::UpdateSelectedItemInReport()
{
	if (!GetSelectedGameToken())
		return;
	CGameTokenItem* pToken = GetSelectedGameToken();

	// find row.
	for (int i = 0; i < m_wndReport.GetRows()->GetCount(); i++)
	{
		CGameTokenRecord* pRecord = DYNAMIC_DOWNCAST(CGameTokenRecord, m_wndReport.GetRows()->GetAt(i)->GetRecord());
		if (pRecord && (pRecord->m_pItem == pToken))
		{
			pRecord->Update();
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CGameTokenDialog::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	__super::OnEditorNotifyEvent(event);
	switch (event)
	{
	case eNotify_OnBeginNewScene:
		m_wndReport.SetRedraw(FALSE);
		m_wndReport.GetRecords()->RemoveAll();
		m_wndReport.GetSelectedRows()->Clear();
		m_wndReport.Populate();
		break;

	case eNotify_OnClearLevelContents:
		m_wndReport.SetRedraw(FALSE);
		m_wndReport.GetRecords()->RemoveAll();
		m_wndReport.GetSelectedRows()->Clear();
		m_wndReport.Populate();
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
void CGameTokenDialog::ReloadItems()
{
	m_wndReport.SetRedraw(FALSE);
	m_wndReport.GetRecords()->RemoveAll();
	m_wndReport.GetSelectedRows()->Clear();

	if (!m_pLibrary)
	{
		m_wndReport.SetRedraw(TRUE);
		return;
	}

	CXTPReportRecord* pSelRec = NULL;
	m_bIgnoreSelectionChange = true;
	for (int i = 0; i < m_pLibrary->GetItemCount(); i++)
	{
		CGameTokenItem* pItem = (CGameTokenItem*)m_pLibrary->GetItem(i);
		CXTPReportRecord* pRec = m_wndReport.AddRecord(new CGameTokenRecord(pItem));
		if (pItem == m_pCurrentItem)
			pSelRec = pRec;
	}

	m_wndReport.Populate();

	// find row.
	for (int i = 0; i < m_wndReport.GetRows()->GetCount(); i++)
	{
		if (pSelRec == m_wndReport.GetRows()->GetAt(i)->GetRecord())
		{
			m_wndReport.GetRows()->GetAt(i)->SetSelected(TRUE);
			m_wndReport.EnsureVisible(m_wndReport.GetRows()->GetAt(i));
			break;
		}
	}

	m_wndReport.SetRedraw(TRUE);
	m_wndReport.RedrawControl();

	m_bIgnoreSelectionChange = false;
}

//////////////////////////////////////////////////////////////////////////
void CGameTokenDialog::OnSize(UINT nType, int cx, int cy)
{
	__super::OnSize(nType, cx, cy);
	// resize splitter window.
	if (m_wndReport.m_hWnd)
	{
		CRect rc;
		GetClientRect(rc);
		CRect rc1(rc);
		rc1.right = 200;
		m_wndTaskPanel.MoveWindow(rc1, FALSE);
		CRect rc2(rc);
		rc2.left = 200;
		m_wndReport.MoveWindow(rc2, FALSE);
	}

	RecalcLayout();
}

//////////////////////////////////////////////////////////////////////////
void CGameTokenDialog::OnAddItem()
{
	if (!m_pLibrary)
		return;

	QGroupDialog dlg(QObject::tr("New GameToken Name"));
	dlg.SetGroup(m_selectedGroup);
	//dlg.SetString( entityClass );
	if (dlg.exec() != QDialog::Accepted || dlg.GetString().IsEmpty())
	{
		return;
	}

	CString fullName = m_pItemManager->MakeFullItemName(m_pLibrary, dlg.GetGroup().GetString(), dlg.GetString().GetString());
	if (m_pItemManager->FindItemByName(fullName))
	{
		Warning("Item with name %s already exist", (const char*)fullName);
		return;
	}

	CGameTokenItem* pItem = (CGameTokenItem*)m_pItemManager->CreateItem(m_pLibrary);

	// Make prototype name.
	SetItemName(pItem, dlg.GetGroup().GetString(), dlg.GetString().GetString());
	pItem->Update();

	ReloadItems();
	SelectItem(pItem);
}

//////////////////////////////////////////////////////////////////////////
void CGameTokenDialog::DeleteItem(CBaseLibraryItem* pItem)
{
	__super::DeleteItem(pItem);
	ReloadItems();
}

//////////////////////////////////////////////////////////////////////////
void CGameTokenDialog::SelectItem(CBaseLibraryItem* item, bool bForceReload)
{
	bool bChanged = item != m_pCurrentItem || bForceReload;
	CBaseLibraryDialog::SelectItem(item, bForceReload);

	if (!bChanged)
		return;

	m_pGameTokenManager->SetSelectedItem(item); // alexl: tell manager which item is selected

	if (!item)
	{
		//m_propsCtrl.EnableWindow(FALSE);
		return;
	}

	CGameTokenItem* pGameToken = GetSelectedGameToken();
	if (!pGameToken)
		return;

	m_bSkipUpdateItems = true;
	m_wndTaskPanel.FindGroup(3)->FindItem(IDC_TOKEN_NAME)->SetCaption(pGameToken->GetFullName());
	m_tokenValueEdit.SetWindowText(pGameToken->GetValueString());
	m_tokenTypeCombo.SelectString(-1, pGameToken->GetTypeName());
	m_tokenLocalOnlyCheck.SetCheck(pGameToken->GetLocalOnly() ? BST_CHECKED : BST_UNCHECKED);
	CString text = pGameToken->GetDescription();
	text.Replace("\\n", "\r\n");
	m_tokenInfoEdit.SetWindowText(text);
	m_bSkipUpdateItems = false;

	/*


	   m_propsCtrl.EnableWindow(TRUE);
	   m_propsCtrl.EnableUpdateCallback(false);

	   CGameTokenItem *pGameToken = GetSelectedGameToken();
	   if (pGameToken)
	   {
	   // Update UI with new item content.
	   gGameTokenUI.WriteToUI( pGameToken );
	   }
	   m_propsCtrl.EnableUpdateCallback(true);
	 */
}

//////////////////////////////////////////////////////////////////////////
void CGameTokenDialog::Update()
{
}

//////////////////////////////////////////////////////////////////////////
CGameTokenItem* CGameTokenDialog::GetSelectedGameToken()
{
	CBaseLibraryItem* pItem = m_pCurrentItem;
	return (CGameTokenItem*)pItem;
}

void CGameTokenDialog::SetSelectedGameToken(CGameTokenItem *pItem)
{
	SelectItem(static_cast<CBaseLibraryItem *>(pItem), true);
}

//////////////////////////////////////////////////////////////////////////
void CGameTokenDialog::OnBeginDrag(NMHDR* pNMHDR, LRESULT* pResult)
{
	NM_TREEVIEW* pNMTreeView = (NM_TREEVIEW*)pNMHDR;

	HTREEITEM hItem = pNMTreeView->itemNew.hItem;

	CGameTokenItem* pGameToken = (CGameTokenItem*)m_treeCtrl.GetItemData(hItem);
	if (!pGameToken)
		return;

	m_pDraggedItem = pGameToken;

	m_treeCtrl.Select(hItem, TVGN_CARET);

	m_hDropItem = 0;
	m_dragImage = m_treeCtrl.CreateDragImage(hItem);
	if (m_dragImage)
	{
		m_hDraggedItem = hItem;
		m_hDropItem = hItem;
		m_dragImage->BeginDrag(0, CPoint(-10, -10));

		CRect rc;
		AfxGetMainWnd()->GetWindowRect(rc);

		CPoint p = pNMTreeView->ptDrag;
		ClientToScreen(&p);
		p.x -= rc.left;
		p.y -= rc.top;

		m_dragImage->DragEnter(AfxGetMainWnd(), p);
		SetCapture();
		GetIEditorImpl()->EnableUpdate(false);
	}

	*pResult = 0;
}

//////////////////////////////////////////////////////////////////////////
void CGameTokenDialog::OnMouseMove(UINT nFlags, CPoint point)
{
	if (m_dragImage)
	{
		CPoint p;

		p = point;
		ClientToScreen(&p);
		m_treeCtrl.ScreenToClient(&p);

		TVHITTESTINFO hit;
		ZeroStruct(hit);
		hit.pt = p;
		HTREEITEM hHitItem = m_treeCtrl.HitTest(&hit);
		if (hHitItem)
		{
			if (m_hDropItem != hHitItem)
			{
				if (m_hDropItem)
					m_treeCtrl.SetItem(m_hDropItem, TVIF_STATE, 0, 0, 0, 0, TVIS_DROPHILITED, 0);
				// Set state of this item to drop target.
				m_treeCtrl.SetItem(hHitItem, TVIF_STATE, 0, 0, 0, TVIS_DROPHILITED, TVIS_DROPHILITED, 0);
				m_hDropItem = hHitItem;
				m_treeCtrl.Invalidate();
			}
		}

		CRect rc;
		AfxGetMainWnd()->GetWindowRect(rc);
		p = point;
		ClientToScreen(&p);
		p.x -= rc.left;
		p.y -= rc.top;
		m_dragImage->DragMove(p);

		SetCursor(m_hCursorNoDrop);
		// Check if can drop here.
		{
			CPoint p;
			GetCursorPos(&p);
			CViewport* viewport = GetIEditorImpl()->GetViewManager()->GetViewportAtPoint(p);
			if (viewport)
			{
				SetCursor(m_hCursorCreate);
				CPoint vp = p;
				viewport->ScreenToClient(&vp);
				/*
				   HitContext hit;
				   if (viewport->HitTest( vp,hit ))
				   {
				   if (hit.object && hit.object->IsKindOf(RUNTIME_CLASS(CGameTokenObject)))
				   {
				    SetCursor( m_hCursorReplace );
				   }
				   }
				 */
			}
		}
	}

	CBaseLibraryDialog::OnMouseMove(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CGameTokenDialog::OnLButtonUp(UINT nFlags, CPoint point)
{
	//CXTResizeDialog::OnLButtonUp(nFlags, point);

	if (m_hDropItem)
	{
		m_treeCtrl.SetItem(m_hDropItem, TVIF_STATE, 0, 0, 0, 0, TVIS_DROPHILITED, 0);
		m_hDropItem = 0;
	}

	if (m_dragImage)
	{
		CPoint p;
		GetCursorPos(&p);

		GetIEditorImpl()->EnableUpdate(true);

		m_dragImage->DragLeave(AfxGetMainWnd());
		m_dragImage->EndDrag();
		delete m_dragImage;
		m_dragImage = 0;
		ReleaseCapture();
		SetCursor(m_hCursorDefault);

		CPoint treepoint = p;
		m_treeCtrl.ScreenToClient(&treepoint);

		CRect treeRc;
		m_treeCtrl.GetClientRect(treeRc);

		if (treeRc.PtInRect(treepoint))
		{
			// Droped inside tree.
			TVHITTESTINFO hit;
			ZeroStruct(hit);
			hit.pt = treepoint;
			HTREEITEM hHitItem = m_treeCtrl.HitTest(&hit);
			if (hHitItem)
			{
				//DropToItem( hHitItem,m_hDraggedItem,m_pDraggedItem );
				m_hDraggedItem = 0;
				m_pDraggedItem = 0;
				return;
			}
			//DropToItem( 0,m_hDraggedItem,m_pDraggedItem );
		}
		else
		{
			// Not droped inside tree.

			CWnd* wnd = WindowFromPoint(p);

			CViewport* viewport = GetIEditorImpl()->GetViewManager()->GetViewportAtPoint(p);
			if (viewport)
			{
				bool bHit = false;
				CPoint vp = p;
				viewport->ScreenToClient(&vp);

				/*
				   // Drag and drop into one of views.
				   // Start object creation.
				   HitContext  hit;
				   if (viewport->HitTest( vp,hit ))
				   {
				   if (hit.object && hit.object->IsKindOf(RUNTIME_CLASS(CGameTokenObject)))
				   {
				    bHit = true;
				    CUndo undo( "Assign GameToken" );
				    ((CGameTokenObject*)hit.object)->SetGameToken( (CGameTokenItem*)m_pDraggedItem,false );
				   }
				   }
				   if (!bHit)
				   {
				   CUndo undo( "Create GameToken" );
				   CString guid = GuidUtil::ToString( m_pDraggedItem->GetGUID() );
				   GetIEditorImpl()->StartObjectCreation( GameToken_OBJECT_CLASS_NAME,guid );
				   }
				 */
			}
		}
		m_pDraggedItem = 0;
	}
	m_pDraggedItem = 0;
	m_hDraggedItem = 0;

	CBaseLibraryDialog::OnLButtonUp(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CGameTokenDialog::OnTreeRClick(NMHDR* pNMHDR, LRESULT* pResult)
{
	// Show helper menu.
	CPoint point;

	CGameTokenItem* pGameTokens = 0;

	// Find node under mouse.
	GetCursorPos(&point);
	m_treeCtrl.ScreenToClient(&point);
	// Select the item that is at the point myPoint.
	UINT uFlags;
	HTREEITEM hItem = m_treeCtrl.HitTest(point, &uFlags);
	if ((hItem != NULL) && (TVHT_ONITEM & uFlags))
	{
		pGameTokens = (CGameTokenItem*)m_treeCtrl.GetItemData(hItem);
	}

	if (!pGameTokens)
		return;

	SelectItem(pGameTokens);

	// Create pop up menu.
	CMenu menu;
	menu.CreatePopupMenu();

	if (pGameTokens)
	{
		CClipboard clipboard;
		bool bNoPaste = clipboard.IsEmpty();
		int pasteFlags = 0;
		if (bNoPaste)
			pasteFlags |= MF_GRAYED;

		menu.AppendMenu(MF_STRING, ID_DB_CUT, "Cut");
		menu.AppendMenu(MF_STRING, ID_DB_COPY, "Copy");
		menu.AppendMenu(MF_STRING | pasteFlags, ID_DB_PASTE, "Paste");
		menu.AppendMenu(MF_STRING, ID_DB_CLONE, "Clone");
		menu.AppendMenu(MF_SEPARATOR, 0, "");
		menu.AppendMenu(MF_STRING, ID_DB_RENAME, "Rename");
		menu.AppendMenu(MF_STRING, ID_DB_REMOVE, "Delete");
	}

	GetCursorPos(&point);
	menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_LEFTBUTTON, point.x, point.y, this);
}

//////////////////////////////////////////////////////////////////////////
void CGameTokenDialog::OnCopy()
{
	CGameTokenItem* pItem = GetSelectedGameToken();
	if (pItem)
	{
		XmlNodeRef node = XmlHelpers::CreateXmlNode("GameToken");
		CBaseLibraryItem::SerializeContext ctx(node, false);
		ctx.bCopyPaste = true;

		CClipboard clipboard;
		pItem->Serialize(ctx);
		clipboard.Put(node);
	}
}

//////////////////////////////////////////////////////////////////////////
void CGameTokenDialog::OnPaste()
{
	if (!m_pLibrary)
		return;

	CGameTokenItem* pItem = GetSelectedGameToken();
	if (!pItem)
		return;

	CClipboard clipboard;
	if (clipboard.IsEmpty())
		return;
	XmlNodeRef node = clipboard.Get();
	if (!node)
		return;

	if (strcmp(node->getTag(), "GameToken") == 0)
	{
		CBaseLibraryItem* pItem = (CBaseLibraryItem*)m_pGameTokenManager->CreateItem(m_pLibrary);
		if (pItem)
		{
			CBaseLibraryItem::SerializeContext serCtx(node, true);
			serCtx.bCopyPaste = true;
			serCtx.bUniqName = true; // this ensures that a new GameToken is created because otherwise the old one would be renamed
			pItem->Serialize(serCtx);
			SelectItem(pItem);
			ReloadItems();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CGameTokenDialog::DropToItem(HTREEITEM hItem, HTREEITEM hSrcItem, CGameTokenItem* pGameTokens)
{
	return;
}

//////////////////////////////////////////////////////////////////////////
LRESULT CGameTokenDialog::OnTaskPanelNotify(WPARAM wParam, LPARAM lParam)
{
	switch (wParam)
	{
	case XTP_TPN_CLICK:
		{
			CXTPTaskPanelGroupItem* pItem = (CXTPTaskPanelGroupItem*)lParam;
			UINT nCmdID = pItem->GetID();
			SendMessage(WM_COMMAND, MAKEWPARAM(nCmdID, 0), 0);
		}
		break;

	case XTP_TPN_RCLICK:
		break;
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CGameTokenDialog::OnReportDblClick(NMHDR* pNotifyStruct, LRESULT* result)
{
	XTP_NM_REPORTRECORDITEM* pItemNotify = (XTP_NM_REPORTRECORDITEM*) pNotifyStruct;
	if (pItemNotify->pColumn)
	{
		// Clicked on visibility index
		CGameTokenRecord* pRecord = DYNAMIC_DOWNCAST(CGameTokenRecord, pItemNotify->pRow->GetRecord());
		if (pRecord)
		{
			CRect rc1 = pItemNotify->pColumn->GetRect();
			CRect rc2 = pItemNotify->pRow->GetRect();
			CRect rc = CRect(rc1.left, rc2.top, rc1.right, rc2.bottom);
			if (pItemNotify->pColumn->GetIndex() == TOKEN_COLUMN_TYPE)
			{
				CGameTokenTypeCombo* pCombo = new CGameTokenTypeCombo(this);
				pCombo->Create(WS_CHILD | WS_VISIBLE | WS_BORDER | CBS_HASSTRINGS | CBS_DROPDOWNLIST, CRect(0, 0, 100, 100), &m_wndReport, IDC_TOKEN_TYPE_2);
				pCombo->MoveWindow(rc);
				pCombo->SelectString(-1, pRecord->m_pItem->GetTypeName());
				pCombo->SetFocus();
			}
			if (pItemNotify->pColumn->GetIndex() == TOKEN_COLUMN_VALUE)
			{
				CGameTokenValueEdit* pEdit = new CGameTokenValueEdit(this);
				pEdit->Create(WS_CHILD | WS_VISIBLE | WS_BORDER, CRect(0, 0, 100, 100), &m_wndReport, IDC_TOKEN_VALUE_2);
				pEdit->MoveWindow(rc);
				pEdit->SetWindowText(pRecord->m_pItem->GetValueString());
				pEdit->SetFocus();
			}
			if (pItemNotify->pColumn->GetIndex() == TOKEN_COLUMN_DESC)
			{
				CGameTokenValueEdit* pEdit = new CGameTokenValueEdit(this);
				pEdit->Create(WS_CHILD | WS_VISIBLE | WS_BORDER, CRect(0, 0, 100, 100), &m_wndReport, IDC_TOKEN_DESCRIPTION_2);
				pEdit->MoveWindow(rc);
				pEdit->SetWindowText(pRecord->m_pItem->GetDescription());
				pEdit->SetFocus();
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CGameTokenDialog::OnReportSelChange(NMHDR* pNotifyStruct, LRESULT* result)
{
	CXTPReportSelectedRows* poSelectedRows(NULL);
	CXTPReportRow* poCurrentRow(NULL);
	POSITION pstPosition(NULL);
	CGameTokenRecord* poCurrentTokenRecord(NULL);

	m_cpoSelectedLibraryItems.clear();

	// Firs we get the selected rows...
	poSelectedRows = m_wndReport.GetSelectedRows();

	// The mean to traverse all the selected rows is, for some reason yet to be discovered,
	// we must first get the first selected row position and then we can get the next
	// selected row based on the position.
	// In other words: as we need the position to get the actually selected rows, we must
	// first call GetFirstSelectedRowPosition() and than, with the first position we
	// will have access to the actual rows.
	// Another important issue is that GetNextSelectedRow() changes the value of the passed
	// parameter, in this case the position, and it will be null and there are no more selected
	// elements, so this should be used as the control parameter for the traversing loop.
	pstPosition = poSelectedRows->GetFirstSelectedRowPosition();
	while (pstPosition)
	{
		poCurrentRow = poSelectedRows->GetNextSelectedRow(pstPosition);
		poCurrentTokenRecord = DYNAMIC_DOWNCAST(CGameTokenRecord, poCurrentRow->GetRecord());
		if (!poCurrentTokenRecord)
		{
			continue;
		}
		m_cpoSelectedLibraryItems.insert(poCurrentTokenRecord->m_pItem);
		SelectItem(poCurrentTokenRecord->m_pItem);
	}
	*result = 0;
}

void CGameTokenDialog::UpdateField(int nControlId, const CString& strValue, bool bBoolValue)
{
	if (GetSelectedGameToken())
	{
		CXTPReportSelectedRows* pSelRows = m_wndReport.GetSelectedRows();
		for (int i = 0; i < pSelRows->GetCount(); ++i)
		{
			CGameTokenRecord* pRecord = DYNAMIC_DOWNCAST(CGameTokenRecord, pSelRows->GetAt(i)->GetRecord());
			if (pRecord && (pRecord->m_pItem))
			{
				bool bRecreateToken = false;
				CGameTokenItem* pToken = pRecord->m_pItem;
				switch (nControlId)
				{
				case IDC_TOKEN_TYPE:
					pToken->SetTypeName(strValue);
					bRecreateToken = true;
					break;
				case IDC_TOKEN_VALUE:
					pToken->SetValueString(strValue);
					break;
				case IDC_TOKEN_DESCRIPTION:
					pToken->SetDescription(strValue);
					break;
				case IDC_TOKEN_LOCAL_ONLY:
					pToken->SetLocalOnly(bBoolValue);
					break;
				}
				pToken->Update(bRecreateToken);
				pRecord->Update();
			}
		}
		m_wndReport.RedrawControl();
	}
}

//////////////////////////////////////////////////////////////////////////
void CGameTokenDialog::OnTokenTypeChange()
{
	// TODO: Add your control notification handler code here
	if (m_bSkipUpdateItems)
		return;
	int nIndex = m_tokenTypeCombo.GetCurSel();
	if (nIndex == CB_ERR)
		return;

	CString str;
	m_tokenTypeCombo.GetLBText(nIndex, str);
	UpdateField(IDC_TOKEN_TYPE, str);
}

//////////////////////////////////////////////////////////////////////////
void CGameTokenDialog::OnTokenValueChange()
{
	if (m_bSkipUpdateItems)
		return;

	CString str;
	m_tokenValueEdit.GetWindowText(str);
	UpdateField(IDC_TOKEN_VALUE, str);
}

//////////////////////////////////////////////////////////////////////////
void CGameTokenDialog::OnTokenInfoChnage()
{
	if (m_bSkipUpdateItems)
		return;

	CString str;
	m_tokenInfoEdit.GetWindowText(str);
	str.Replace("\r", "");
	str.Replace("\n", "\\n");
	UpdateField(IDC_TOKEN_DESCRIPTION, str);
}

//////////////////////////////////////////////////////////////////////////
void CGameTokenDialog::OnTokenLocalOnlyChange()
{
	if (m_bSkipUpdateItems)
		return;

	UpdateField(IDC_TOKEN_LOCAL_ONLY, "", m_tokenLocalOnlyCheck.GetCheck() == BST_CHECKED);
}

