// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "DialogEditorDialog.h"
#include "DialogScriptRecord.h"
#include "DialogManager.h"

#include <CryAISystem/IAIAction.h>
#include <CryAISystem/IAISystem.h>
#include <CryAISystem/IAgent.h>

#include "QtViewPane.h"
#include "Dialogs/SourceControlDescDlg.h"
#include "Controls/SharedFonts.h"

#include "Dialogs/QGroupDialog.h"

#include <ISourceControl.h>
#include "Controls/QuestionDialog.h"
#include "Util/MFCUtil.h"
#include "ObjectEvent.h"
#include "IObjectManager.h"

#include "Resource.h"

using namespace DialogEditor;

#define DE_USE_SOURCE_CONTROL
// #undef DE_USE_SOURCE_CONTROL // don't use source control for the moment

#define SOED_DIALOGFRAME_CLASSNAME "DialogEditorDialog"

#define IDC_REPORTCONTROL          1234
#define IDC_SCRIPT_FOLDERS         2410
#define IDC_DESC                   2411
#define IDC_DIALOGHELP             2412

#define ID_DIALOGED_NEW_DIALOG     2410
#define ID_DIALOGED_DEL_DIALOG     2411
#define ID_DIALOGED_RENAME_DIALOG  2412

enum
{
	ITEM_IMAGE_FOLDER      = 2,

	ITEM_IMAGE_OVERLAY_CGF = 8,
	ITEM_IMAGE_OVERLAY_INPAK,
	ITEM_IMAGE_OVERLAY_READONLY,
	ITEM_IMAGE_OVERLAY_ONDISK,
	ITEM_IMAGE_OVERLAY_LOCKED,
	ITEM_IMAGE_OVERLAY_CHECKEDOUT,
	ITEM_IMAGE_OVERLAY_NO_CHECKOUT,
};

/*
   enum {
   ITEM_OVERLAY_ID_CGF          = 1,
   ITEM_OVERLAY_ID_INPAK        = 2,
   ITEM_OVERLAY_ID_READONLY     = 3,
   ITEM_OVERLAY_ID_ONDISK       = 4,
   ITEM_OVERLAY_ID_LOCKED       = 5,
   ITEM_OVERLAY_ID_CHECKEDOUT   = 6,
   ITEM_OVERLAY_ID_NO_CHECKOUT  = 7,
   };
 */

enum
{
	MENU_SCM_ADD = 24,
	MENU_SCM_CHECK_IN,
	MENU_SCM_CHECK_OUT,
	MENU_SCM_UNDO_CHECK_OUT,
	MENU_SCM_GET_LATEST,
	MENU_DLG_RENAME,
	MENU_DLG_DELETE,
	MENU_DLG_LOCALEDIT,
};

enum
{
	IDW_DIALOGED_TASK_PANE = AFX_IDW_CONTROLBAR_FIRST + 13,
	IDW_DIALOGED_PROPS_PANE,
	IDW_DIALOGED_TREE_PANE,
	IDW_DIALOGED_DESC_PANE,
	IDW_DIALOGED_HELP_PANE,
};

//////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CDialogFolderCtrl, CXTPReportControl)
ON_WM_LBUTTONDOWN()
ON_WM_LBUTTONUP()
ON_WM_MOUSEMOVE()
ON_WM_CAPTURECHANGED()
ON_WM_DESTROY()
ON_NOTIFY_REFLECT(XTP_NM_REPORT_HEADER_RCLICK, OnReportColumnRClick)
ON_NOTIFY_REFLECT(NM_DBLCLK, OnReportItemDblClick)
ON_NOTIFY_REFLECT(NM_RCLICK, OnReportItemRClick)
ON_NOTIFY_REFLECT(XTP_NM_REPORT_ROWEXPANDED, OnReportRowExpandChanged)
ON_WM_RBUTTONDOWN()
END_MESSAGE_MAP()
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
CDialogFolderCtrl::CDialogFolderCtrl()
{
	m_pDialog = 0;

	CMFCUtils::LoadTrueColorImageList(m_imageList, IDB_MATERIAL_TREE, 20, RGB(255, 0, 255));
	CMFCUtils::LoadTrueColorImageList(m_imageList, IDB_FILE_STATUS, 20, RGB(255, 0, 255));

	SetImageList(&m_imageList);

	CXTPReportColumn* pTreeCol = AddColumn(new CXTPReportColumn(0, _T("Dialog"), 150, TRUE, XTP_REPORT_NOICON, TRUE, TRUE));
	pTreeCol->SetTreeColumn(true);
	pTreeCol->SetSortable(FALSE);
	GetColumns()->SetSortColumn(pTreeCol, true);
	GetReportHeader()->AllowColumnRemove(FALSE);
	ShadeGroupHeadings(FALSE);
	SkipGroupsFocus(TRUE);
	SetMultipleSelection(FALSE);

	class CMyPaintManager : public CXTPReportPaintManager
	{
	public:
		virtual int GetRowHeight(CDC* pDC, CXTPReportRow* pRow)
		{
			return __super::GetRowHeight(pDC, pRow) - 1;
		}
		virtual int GetHeaderHeight()
		{
			return 0;
		}
	};

	CXTPReportPaintManager* pPMgr = new CMyPaintManager();
	pPMgr->m_nTreeIndent = 0x0a;
	pPMgr->m_bShadeSortColumn = false;
	pPMgr->m_strNoItems = _T("No Dialogs yet.\nUse [Add Dialog] to create new one.");
	pPMgr->SetGridStyle(FALSE, xtpGridNoLines);
	pPMgr->SetGridStyle(TRUE, xtpGridNoLines);
	SetPaintManager(pPMgr);
}

//////////////////////////////////////////////////////////////////////////
CDialogFolderCtrl::~CDialogFolderCtrl()
{
}

//////////////////////////////////////////////////////////////////////////
void CDialogFolderCtrl::OnDestroy()
{
	__super::OnDestroy();
}

//////////////////////////////////////////////////////////////////////////
void CDialogFolderCtrl::OnRButtonDown(UINT nFlags, CPoint point)
{
}

//////////////////////////////////////////////////////////////////////////
void CDialogFolderCtrl::OnReportColumnRClick(NMHDR* pNotifyStruct, LRESULT* result)
{
}

//////////////////////////////////////////////////////////////////////////
void CDialogFolderCtrl::OnLButtonDown(UINT nFlags, CPoint point)
{
	__super::OnLButtonDown(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CDialogFolderCtrl::OnLButtonUp(UINT nFlags, CPoint point)
{
	__super::OnLButtonUp(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CDialogFolderCtrl::OnMouseMove(UINT nFlags, CPoint point)
{
	__super::OnMouseMove(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CDialogFolderCtrl::OnCaptureChanged(CWnd* pWnd)
{
	__super::OnCaptureChanged(pWnd);
}

class CTreeScriptRecord : public CXTPReportRecord
{
	DECLARE_DYNAMIC(CTreeScriptRecord)

public:
	CTreeScriptRecord(CTreeScriptRecord* pParent, bool bIsGroup, const CString& groupName, const CString& name)
		: m_pParent(pParent), m_groupName(groupName), m_name(name), m_bIsGroup(bIsGroup)
	{
	}
	bool               IsGroup() const      { return m_bIsGroup; }
	const CString&     GetName() const      { return m_name; }
	const CString&     GetGroupName() const { return m_groupName; }
	CTreeScriptRecord* GetParent() const    { return m_pParent; }
protected:
	CTreeScriptRecord* m_pParent;
	CString            m_groupName;
	CString            m_name;
	bool               m_bIsGroup;
};
IMPLEMENT_DYNAMIC(CTreeScriptRecord, CXTPReportRecord);

//////////////////////////////////////////////////////////////////////////
void CDialogFolderCtrl::UpdateChildren(CXTPReportRow* pRow)
{
	BeginUpdate();
	if (pRow)
	{
		CXTPReportRows* pChildren = pRow->GetChilds();
		if (pChildren != 0)
		{
			int count = pChildren->GetCount();
			for (int i = 0; i < count; ++i)
			{
				CXTPReportRow* pChildRow = pChildren->GetAt(i);
				if (pChildRow)
				{
					CTreeScriptRecord* pRecord = static_cast<CTreeScriptRecord*>(pChildRow->GetRecord());
					if (pRecord && pRecord->IsGroup() == false)
					{
						UpdateSCStatus(pRecord, 0, true);
					}
				}
			}
		}
	}
	EndUpdate();
}

//////////////////////////////////////////////////////////////////////////
void CDialogFolderCtrl::OnReportItemDblClick(NMHDR* pNotifyStruct, LRESULT* result)
{
	XTP_NM_REPORTRECORDITEM* pItemNotify = (XTP_NM_REPORTRECORDITEM*) pNotifyStruct;
	ASSERT(pItemNotify != NULL);
	if (pItemNotify != 0)
	{
		CXTPReportRow* pRow = pItemNotify->pRow;
		if (pRow)
			pRow->SetExpanded(pRow->IsExpanded() ? FALSE : TRUE);
	}
}

void CDialogFolderCtrl::OnReportRowExpandChanged(NMHDR* pNotifyStruct, LRESULT* /*result*/)
{
	XTP_NM_REPORTRECORDITEM* pItemNotify = (XTP_NM_REPORTRECORDITEM*) pNotifyStruct;
	ASSERT(pItemNotify != NULL);

	if (pItemNotify != 0)
	{
		CXTPReportRow* pRow = pItemNotify->pRow;
		if (pRow && pRow->IsExpanded())
			UpdateChildren(pRow);
	}
}

//////////////////////////////////////////////////////////////////////////
void CDialogFolderCtrl::Reload()
{
	std::map<CString, CTreeScriptRecord*> groupMap;

	BeginUpdate();
	GetRecords()->RemoveAll();

	const TEditorDialogScriptMap& scripts = m_pDialog->GetManager()->GetAllScripts();
	for (TEditorDialogScriptMap::const_iterator iter = scripts.begin(); iter != scripts.end(); ++iter)
	{
		static const char* tokens = ".:/\\";

		CEditorDialogScript* pScript = const_cast<CEditorDialogScript*>(iter->second);
		CString fullClassName = pScript->GetID();

		CTreeScriptRecord* pItemGroupRec = 0;
		int midPos = 0;
		int pos = 0;
		int len = fullClassName.GetLength();
		bool bUsePrototypeName = false; // use real prototype name or stripped from fullClassName

		CString nodeShortName;
		CString groupName = fullClassName.Tokenize(tokens, pos);

		// check if a group name is given. if not, fake 'Misc:' group
		if (pos < 0 || pos >= len)
		{
			bUsePrototypeName = true; // re-fetch real prototype name
			// fullClassName.Insert(0, "Misc:");
			pos = 0;
			len = fullClassName.GetLength();
			groupName = fullClassName.Tokenize(tokens, pos);
		}

		CString pathName = "";
		while (pos >= 0 && pos < len)
		{
			pathName += groupName;
			pathName += ":";
			midPos = pos;

			CTreeScriptRecord* pGroupRec = stl::find_in_map(groupMap, pathName, 0);
			if (pGroupRec == 0)
			{
				pGroupRec = new CTreeScriptRecord(pItemGroupRec, true, groupName, "");
				CXTPReportRecordItem* pNewItem = new CXTPReportRecordItemText(groupName);
				pNewItem->SetIconIndex(ITEM_IMAGE_FOLDER);
				pGroupRec->AddItem(pNewItem);

				if (pItemGroupRec != 0)
				{
					pItemGroupRec->GetChilds()->Add(pGroupRec);
				}
				else
				{
					AddRecord(pGroupRec);
				}
				groupMap[pathName] = pGroupRec;
				pItemGroupRec = pGroupRec;
			}
			else
			{
				pItemGroupRec = pGroupRec;
			}

			// continue stripping
			groupName = fullClassName.Tokenize(tokens, pos);
		}
		;

		// short node name without ':'. used for display in last column
		nodeShortName = fullClassName.Mid(midPos);
		if (pathName.IsEmpty() == false) // Remove right-most character ":"
			pathName.Truncate(pathName.GetLength() - 1);
		CTreeScriptRecord* pRec = new CTreeScriptRecord(pItemGroupRec, false, pathName, pScript->GetID().c_str());
		CXTPReportRecordItemText* pNewItem = new CXTPReportRecordItemText(nodeShortName);
		pRec->AddItem(pNewItem);
		UpdateSCStatus(pRec, pScript);

		if (pItemGroupRec)
			pItemGroupRec->GetChilds()->Add(pRec);
		else
			AddRecord(pRec);
	}

	EndUpdate();
	Populate();
}

//////////////////////////////////////////////////////////////////////////
void CDialogFolderCtrl::UpdateSCStatus(CTreeScriptRecord* pRec, CEditorDialogScript* pScript, bool bUseCached)
{
	uint32 scAttr = SCC_FILE_ATTRIBUTE_NORMAL;

	int index = 0;

	if (pRec->IsGroup())
	{
		index = ITEM_IMAGE_FOLDER;
	}
	else
	{
		if (pScript == 0)
		{
			pScript = m_pDialog->GetManager()->GetScript(pRec->GetName(), false);
		}

		if (pScript)
		{
			uint32 scAttr = m_pDialog->GetManager()->GetFileAttrs(pScript, !bUseCached);
			{
				if (scAttr & SCC_FILE_ATTRIBUTE_INPAK)
					index = ITEM_IMAGE_OVERLAY_INPAK;
				else if (scAttr & SCC_FILE_ATTRIBUTE_CHECKEDOUT)
					index = ITEM_IMAGE_OVERLAY_CHECKEDOUT;
				else if ((scAttr & SCC_FILE_ATTRIBUTE_MANAGED) && (scAttr & SCC_FILE_ATTRIBUTE_NORMAL) && !(scAttr & SCC_FILE_ATTRIBUTE_READONLY))
					index = ITEM_IMAGE_OVERLAY_NO_CHECKOUT;
				else if (scAttr & SCC_FILE_ATTRIBUTE_MANAGED)
					index = ITEM_IMAGE_OVERLAY_LOCKED;
				else if (scAttr & SCC_FILE_ATTRIBUTE_READONLY)
					index = ITEM_IMAGE_OVERLAY_READONLY;
				else
					index = ITEM_IMAGE_OVERLAY_ONDISK;
			}
		}
	}

	CXTPReportRecordItem* pItem = pRec->GetItem(0);
	if (pItem && pItem->GetIconIndex() != index)
	{
		pItem->SetIconIndex(index);
	}
}

//////////////////////////////////////////////////////////////////////////
void CDialogFolderCtrl::OnSelectionChanged()
{
	CXTPReportRow* pRow = this->GetFocusedRow();
	if (pRow != 0)
	{
		CTreeScriptRecord* pRecord = DYNAMIC_DOWNCAST(CTreeScriptRecord, pRow->GetRecord());
		if (pRecord && pRecord->IsGroup() == false)
		{
			const CString& id = pRecord->GetName();
			m_pDialog->SetCurrentScript(id, false);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CString CDialogFolderCtrl::GetCurrentGroup()
{
	CXTPReportRow* pRow = this->GetFocusedRow();
	if (pRow != 0)
	{
		CTreeScriptRecord* pRecord = DYNAMIC_DOWNCAST(CTreeScriptRecord, pRow->GetRecord());
		if (pRecord)
			return pRecord->GetGroupName();
	}
	return "";
}

//////////////////////////////////////////////////////////////////////////
CTreeScriptRecord* FindRecord(CXTPReportRecords* pRecords, const CString& name)
{
	for (int i = 0; i < pRecords->GetCount(); ++i)
	{
		CTreeScriptRecord* pRecord = static_cast<CTreeScriptRecord*>(pRecords->GetAt(i));
		if (pRecord->GetName() == name)
			return pRecord;
	}

	for (int i = 0; i < pRecords->GetCount(); ++i)
	{
		CTreeScriptRecord* pRecord = static_cast<CTreeScriptRecord*>(pRecords->GetAt(i));
		CTreeScriptRecord* pFound = FindRecord(pRecord->GetChilds(), name);
		if (pFound)
			return pFound;
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CDialogFolderCtrl::UpdateEntry(const CString& entry)
{
	CTreeScriptRecord* pRecord = FindRecord(GetRecords(), entry);
	if (pRecord == 0)
		return;
	UpdateSCStatus(pRecord, 0);
	RedrawControl();
}

//////////////////////////////////////////////////////////////////////////
void CDialogFolderCtrl::GotoEntry(const CString& entry)
{
	CTreeScriptRecord* pRecord = FindRecord(GetRecords(), entry);
	if (pRecord == 0)
		return;

	CTreeScriptRecord* pParent = pRecord->GetParent();
	while (pParent != 0)
	{
		pParent->SetExpanded(TRUE);
		pParent = pParent->GetParent();
	}
	pRecord->SetExpanded(TRUE);

	Populate();

	CXTPReportRows* pRows = GetRows();
	for (int i = 0; i < pRows->GetCount(); ++i)
	{
		CXTPReportRow* pRow = pRows->GetAt(i);
		CTreeScriptRecord* pRecord = static_cast<CTreeScriptRecord*>(pRow->GetRecord());
		if (pRecord->GetName() == entry)
		{
			CXTPReportRow* pParentRow = pRow->GetParentRow();
			while (pParentRow != 0)
			{
				pParentRow->SetExpanded(TRUE);
				pParentRow = pParentRow->GetParentRow();
			}
			SetFocusedRow(pRow);
		}
	}
}

void CDialogFolderCtrl::OnReportItemRClick(NMHDR* pNotifyStruct, LRESULT* /*result*/)
{
	XTP_NM_REPORTRECORDITEM* pItemNotify = (XTP_NM_REPORTRECORDITEM*) pNotifyStruct;

	if (!pItemNotify->pRow)
		return;

	if (pItemNotify->pRow->IsGroupRow())
		return;

	CTreeScriptRecord* pRecord = static_cast<CTreeScriptRecord*>(pItemNotify->pRow->GetRecord());
	if (pRecord->IsGroup())
		return;

	const CString& scriptId = pRecord->GetName();
	CEditorDialogScript* pScript = m_pDialog->GetManager()->GetScript(scriptId, false);
	if (pScript == 0)
		return;

	CPoint ptClick = pItemNotify->pt;
	CMenu menu;
	VERIFY(menu.CreatePopupMenu());

	if (m_pDialog->GetManager()->CanModify(pScript))
	{
		menu.AppendMenu(MF_STRING, MENU_DLG_RENAME, "Rename");
		menu.AppendMenu(MF_STRING, MENU_DLG_DELETE, "Delete");
		menu.AppendMenu(MF_SEPARATOR, 0, "");
	}

	uint32 nFileAttr = m_pDialog->GetManager()->GetFileAttrs(pScript, false);
	if (nFileAttr & SCC_FILE_ATTRIBUTE_INPAK)
	{
		menu.AppendMenu(MF_STRING | MF_GRAYED, 0, " Dialog In Pak (Read Only)");
		if (nFileAttr & SCC_FILE_ATTRIBUTE_MANAGED)
			menu.AppendMenu(MF_STRING, MENU_SCM_GET_LATEST, "Get Latest Version");
		else
			menu.AppendMenu(MF_STRING, MENU_DLG_LOCALEDIT, "Local Edit");
	}
	else
	{
		menu.AppendMenu(MF_STRING | MF_GRAYED, 0, " Source Control");
		if (!(nFileAttr & SCC_FILE_ATTRIBUTE_MANAGED))
		{
			// If not managed.
			menu.AppendMenu(MF_STRING, MENU_SCM_ADD, "Add To Source Control");
		}
		else
		{
			// If managed.
			if (nFileAttr & SCC_FILE_ATTRIBUTE_READONLY)
				menu.AppendMenu(MF_STRING, MENU_SCM_CHECK_OUT, "Check Out");
			if (nFileAttr & SCC_FILE_ATTRIBUTE_CHECKEDOUT)
			{
				menu.AppendMenu(MF_STRING, MENU_SCM_CHECK_IN, "Check In");
				menu.AppendMenu(MF_STRING, MENU_SCM_UNDO_CHECK_OUT, "Undo Check Out");
			}
			if (nFileAttr & SCC_FILE_ATTRIBUTE_NORMAL && !(nFileAttr & SCC_FILE_ATTRIBUTE_CHECKEDOUT) && !(nFileAttr & SCC_FILE_ATTRIBUTE_READONLY))
			{
				menu.AppendMenu(MF_STRING, MENU_SCM_CHECK_IN, "Check In");
			}
			if (nFileAttr & SCC_FILE_ATTRIBUTE_MANAGED)
				menu.AppendMenu(MF_STRING, MENU_SCM_GET_LATEST, "Get Latest Version");
		}
	}

	// track menu
	// int nMenuResult = CXTPCommandBars::TrackPopupMenu(&menu, TPM_NONOTIFY | TPM_RETURNCMD | TPM_LEFTALIGN |TPM_RIGHTBUTTON, ptClick.x, ptClick.y, this, NULL);
	int nMenuResult = menu.TrackPopupMenu(TPM_NONOTIFY | TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON, ptClick.x, ptClick.y, this, NULL);

	bool bResult = true;
	switch (nMenuResult)
	{
	case MENU_DLG_RENAME:
		m_pDialog->OnRenameDialog();
		break;
	case MENU_DLG_DELETE:
		m_pDialog->OnDelDialog();
		break;
	case MENU_SCM_ADD:
		bResult = m_pDialog->DoSourceControlOp(pScript, CDialogEditorDialog::ESCM_IMPORT);
		break;
	case MENU_SCM_CHECK_OUT:
		bResult = m_pDialog->DoSourceControlOp(pScript, CDialogEditorDialog::ESCM_CHECKOUT);
		break;
	case MENU_SCM_CHECK_IN:
		bResult = m_pDialog->DoSourceControlOp(pScript, CDialogEditorDialog::ESCM_CHECKIN);
		break;
	case MENU_SCM_UNDO_CHECK_OUT:
		bResult = m_pDialog->DoSourceControlOp(pScript, CDialogEditorDialog::ESCM_UNDO_CHECKOUT);
		break;
	case MENU_SCM_GET_LATEST:
		bResult = m_pDialog->DoSourceControlOp(pScript, CDialogEditorDialog::ESCM_GETLATEST);
		break;
	case MENU_DLG_LOCALEDIT:
		m_pDialog->OnLocalEdit();
		break;
	}
	// other general items
}

//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CDialogEditorDialog, CXTPFrameWnd)

BEGIN_MESSAGE_MAP(CDialogEditorDialog, CXTPFrameWnd)
ON_WM_SETFOCUS()
ON_WM_DESTROY()
ON_WM_CLOSE()
//ON_WM_PAINT()
ON_WM_ERASEBKGND()
ON_WM_MOUSEMOVE()
ON_WM_LBUTTONUP()

// XT Commands.
ON_MESSAGE(XTPWM_DOCKINGPANE_NOTIFY, OnDockingPaneNotify)
ON_MESSAGE(XTPWM_TASKPANEL_NOTIFY, OnTaskPanelNotify)

// Modify pane commands
ON_COMMAND(ID_DIALOGED_NEW_DIALOG, OnAddDialog)
ON_COMMAND(ID_DIALOGED_DEL_DIALOG, OnDelDialog)
ON_COMMAND(ID_DIALOGED_RENAME_DIALOG, OnRenameDialog)

// description edit box
ON_EN_CHANGE(IDC_DESC, OnDescriptionEdit)

END_MESSAGE_MAP()
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
class CDialogEditorDialogViewClass : public IViewPaneClass
{
	//////////////////////////////////////////////////////////////////////////
	// IClassDesc
	//////////////////////////////////////////////////////////////////////////
	virtual ESystemClassID SystemClassID()	 override { return ESYSTEM_CLASS_VIEWPANE; };
	virtual const char*    ClassName()       override { return DIALOG_EDITOR_NAME; };
	virtual const char*    Category()        override { return "Game"; };
	virtual const char*    GetMenuPath()	 override { return "Deprecated"; }
	virtual CRuntimeClass* GetRuntimeClass() override { return RUNTIME_CLASS(CDialogEditorDialog); };
	virtual const char*    GetPaneTitle()    override { return _T(DIALOG_EDITOR_NAME); };
	virtual bool           SinglePane()      override { return true; };
};

REGISTER_CLASS_DESC(CDialogEditorDialogViewClass)

//////////////////////////////////////////////////////////////////////////
CDialogEditorDialog::CDialogEditorDialog()
{
	GetIEditor()->RegisterNotifyListener(this);

	//! This callback will be called on response to object event.
	typedef Functor2<CBaseObject*, int> EventCallback;

	WNDCLASS wndcls;
	HINSTANCE hInst = AfxGetInstanceHandle();
	if (!(::GetClassInfo(hInst, SOED_DIALOGFRAME_CLASSNAME, &wndcls)))
	{
		// otherwise we need to register a new class
		wndcls.style = 0 /*CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW*/;
		wndcls.lpfnWndProc = ::DefWindowProc;
		wndcls.cbClsExtra = wndcls.cbWndExtra = 0;
		wndcls.hInstance = hInst;
		wndcls.hIcon = NULL;
		wndcls.hCursor = AfxGetApp()->LoadStandardCursor(IDC_ARROW);
		wndcls.hbrBackground = NULL;    // (HBRUSH) (COLOR_3DFACE + 1);
		wndcls.lpszMenuName = NULL;
		wndcls.lpszClassName = SOED_DIALOGFRAME_CLASSNAME;
		if (!AfxRegisterClass(&wndcls))
		{
			AfxThrowResourceException();
		}
	}
	CRect rc(0, 0, 0, 0);
	BOOL bRes = Create(WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, rc, AfxGetMainWnd());
	if (!bRes)
		return;

	m_pDM = new CDialogManager();
	m_pDM->ReloadScripts();

	OnInitDialog();
}

//////////////////////////////////////////////////////////////////////////
CDialogEditorDialog::~CDialogEditorDialog()
{
	GetIEditor()->UnregisterNotifyListener(this);
	SaveCurrent();
	SAFE_DELETE(m_pDM);
}

//////////////////////////////////////////////////////////////////////////
BOOL CDialogEditorDialog::Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd)
{
	return __super::Create(SOED_DIALOGFRAME_CLASSNAME, "", dwStyle, rect, pParentWnd);
}

//////////////////////////////////////////////////////////////////////////
BOOL CDialogEditorDialog::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo)
{
	BOOL res = FALSE;

	res = m_View.OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
	if (res)
		return res;

	return __super::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
}

//////////////////////////////////////////////////////////////////////////
BOOL CDialogEditorDialog::OnInitDialog()
{
	LoadAccelTable(MAKEINTRESOURCE(IDR_HYPERGRAPH));

	try
	{
		// Initialize the command bars
		if (!InitCommandBars())
			return -1;
	}
	catch (CResourceException* e)
	{
		e->Delete();
		return -1;
	}

	ModifyStyleEx(WS_EX_CLIENTEDGE, 0);

	// Get a pointer to the command bars object.
	CXTPCommandBars* pCommandBars = GetCommandBars();
	if (pCommandBars == NULL)
	{
		TRACE0("Failed to create command bars object.\n");
		return -1;      // fail to create
	}

	// Add the menu bar
	/*
	   CXTPCommandBar* pMenuBar = pCommandBars->SetMenu( _T("Menu Bar"),IDR_HYPERGRAPH );
	   ASSERT(pMenuBar);
	   pMenuBar->SetFlags(xtpFlagStretched);
	   pMenuBar->EnableCustomization(FALSE);
	 */

	//////////////////////////////////////////////////////////////////////////
	// Create standart toolbar.
	// CXTPToolBar *pStdToolBar = pCommandBars->Add( _T("HyperGraph ToolBar"),xtpBarTop );
	// VERIFY(pStdToolBar->LoadToolBar(IDR_DIALOGEDITOR_TOOLBAR));
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	VERIFY(m_wndToolBar.CreateToolBar(WS_VISIBLE | WS_CHILD | CBRS_TOOLTIPS | CBRS_ALIGN_TOP, this, AFX_IDW_TOOLBAR));
	VERIFY(m_wndToolBar.LoadToolBar(IDR_DIALOGEDITOR_TOOLBAR));
	m_wndToolBar.SetFlags(xtpFlagAlignTop | xtpFlagStretched);
	m_wndToolBar.EnableCustomization(FALSE);
	{
		CXTPControl* pCtrl;
		pCtrl = m_wndToolBar.GetControls()->FindControl(xtpControlButton, ID_DE_ADDLINE, TRUE, FALSE);
		if (pCtrl)
		{
			pCtrl->SetStyle(xtpButtonIconAndCaption);
			pCtrl->SetCaption(_T("Add ScriptLine"));
		}
		pCtrl = m_wndToolBar.GetControls()->FindControl(xtpControlButton, ID_DE_DELLINE, TRUE, FALSE);
		if (pCtrl)
		{
			pCtrl->SetStyle(xtpButtonIconAndCaption);
			pCtrl->SetCaption(_T("Delete ScriptLine"));
		}
	}

	GetDockingPaneManager()->InstallDockingPanes(this);
	GetDockingPaneManager()->SetTheme(xtpPaneThemeOffice2003);
	GetDockingPaneManager()->SetThemedFloatingFrames(TRUE);

	// init panes
	CreatePanes();

	// init client pane
	CRect rc;
	GetClientRect(rc);
	m_View.Create(WS_CHILD | WS_CLIPCHILDREN | WS_VISIBLE, rc, this, IDC_REPORTCONTROL);
	m_View.ModifyStyleEx(0, WS_EX_STATICEDGE);
	m_View.SetDialogEditor(this);

	CXTPDockingPaneLayout layout(GetDockingPaneManager());
	if (layout.Load(_T("DialogEditor")) && layout.GetPaneList().GetCount() >= 4)
		GetDockingPaneManager()->SetLayout(&layout);

	// Load rules and populate ReportControl with actual values
	ReloadDialogBrowser();

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
void CDialogEditorDialog::CreatePanes()
{
	CRect rc;
	GetClientRect(&rc);

	/////////////////////////////////////////////////////////////////////////
	// Docking Pane for description
	CXTPDockingPane* pDockPane = GetDockingPaneManager()->CreatePane(IDW_DIALOGED_DESC_PANE, CRect(350, 0, 400, 60), xtpPaneDockTop);
	pDockPane->SetTitle(_T("Description"));
	pDockPane->SetOptions(xtpPaneNoCloseable | xtpPaneNoFloatable);

	/////////////////////////////////////////////////////////////////////////
	// Create Edit Control
	m_ctrlDescription.Create(WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_AUTOVSCROLL | ES_MULTILINE, rc, this, IDC_DESC);
	m_ctrlDescription.ModifyStyleEx(0, WS_EX_STATICEDGE);
	m_ctrlDescription.SetFont(CFont::FromHandle((HFONT)SMFCFonts::GetInstance().hSystemFont));
	m_ctrlDescription.SetMargins(2, 2);
	// m_ctrlDescription.SetBkColor( GetSysColor(COLOR_WINDOW) );
	// m_ctrlDescription.SetTextColor( GetSysColor(COLOR_WINDOWTEXT) );
	m_ctrlDescription.SetReadOnly(TRUE);

	/////////////////////////////////////////////////////////////////////////
	// Docking Pane for description
	pDockPane = GetDockingPaneManager()->CreatePane(IDW_DIALOGED_HELP_PANE, CRect(350, 0, 400, 90), xtpPaneDockBottom);
	pDockPane->SetTitle(_T("Dynamic Help"));
	pDockPane->SetOptions(xtpPaneNoCloseable | xtpPaneNoFloatable);

	/////////////////////////////////////////////////////////////////////////
	// Create Edit Control
	m_ctrlHelp.Create(WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_AUTOVSCROLL | ES_MULTILINE | ES_READONLY, rc, this, IDC_DIALOGHELP);
	m_ctrlHelp.ModifyStyleEx(0, WS_EX_STATICEDGE);
	m_ctrlHelp.SetFont(CFont::FromHandle((HFONT)SMFCFonts::GetInstance().hSystemFont));
	m_ctrlHelp.SetMargins(0, 0);
	m_ctrlHelp.SetBkColor(GetSysColor(COLOR_WINDOW));
	m_ctrlHelp.SetTextColor(GetSysColor(COLOR_WINDOWTEXT));

#if 0
	/////////////////////////////////////////////////////////////////////////
	// Docking Pane for Properties
	pDockPane = GetDockingPaneManager()->CreatePane(IDW_DIALOGED_PROPS_PANE, CRect(0, 0, 300, 240), xtpPaneDockRight);
	pDockPane->SetTitle(_T("Properties"));
	pDockPane->SetOptions(xtpPaneNoCloseable | xtpPaneNoFloatable);

	//////////////////////////////////////////////////////////////////////////
	// Create properties control.
	//////////////////////////////////////////////////////////////////////////
	m_Properties.Create(WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, rc, this, 1);
	m_Properties.ModifyStyleEx(0, WS_EX_STATICEDGE);

	//m_Properties.SetFlags( CPropertyCtrl::F_VS_DOT_NET_STYLE );
	m_Properties.SetItemHeight(16);
	m_Properties.ExpandAll();

	m_vars = new CVarBlock();
	m_Properties.AddVarBlock(m_vars);
	m_Properties.ExpandAll();
	m_Properties.EnableWindow(FALSE);

	m_Properties.SetUpdateCallback(functor(*this, &CDialogEditorDialog::OnUpdateProperties));
	m_Properties.EnableUpdateCallback(true);
#endif

	/////////////////////////////////////////////////////////////////////////
	// Docking Pane for TaskPanel
	CXTPDockingPane* pTaskPane = GetDockingPaneManager()->CreatePane(IDW_DIALOGED_TASK_PANE, CRect(0, 0, 176, 240), xtpPaneDockLeft);
	pTaskPane->SetTitle(_T("Tasks"));
	pTaskPane->SetOptions(xtpPaneNoCloseable | xtpPaneNoFloatable);

	/////////////////////////////////////////////////////////////////////////
	// Create empty Task Panel
	m_taskPanel.Create(WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, rc, this, 0);
	m_taskPanel.SetBehaviour(xtpTaskPanelBehaviourExplorer);
	m_taskPanel.SetTheme(xtpTaskPanelThemeNativeWinXP);
	m_taskPanel.SetSelectItemOnFocus(TRUE);
	m_taskPanel.AllowDrag(FALSE);
	//m_taskPanel.SetAnimation( xtpTaskPanelAnimationNo );
	m_taskPanel.GetPaintManager()->m_rcGroupOuterMargins.SetRect(4, 4, 4, 4);
	//m_taskPanel.GetPaintManager()->m_rcGroupInnerMargins.SetRect(0,2,0,2);
	m_taskPanel.GetPaintManager()->m_rcItemOuterMargins.SetRect(1, 1, 1, 1);
	m_taskPanel.GetPaintManager()->m_rcItemInnerMargins.SetRect(0, 2, 0, 2);
	m_taskPanel.GetPaintManager()->m_rcControlMargins.SetRect(2, 2, 2, 2);
	m_taskPanel.GetPaintManager()->m_nGroupSpacing = 0;

	CXTPTaskPanelGroupItem* pItem = NULL;
	CXTPTaskPanelGroup* pGroup = NULL;
	int groupId = 0;

	pGroup = m_taskPanel.AddGroup(++groupId);
	pGroup->SetCaption(_T("Dialog"));
	pItem = pGroup->AddLinkItem(ID_DIALOGED_NEW_DIALOG);
	pItem->SetType(xtpTaskItemTypeLink);
	pItem->SetCaption(_T("New..."));
	pItem->SetTooltip(_T("Adds a new Dialog Script"));
	pItem = pGroup->AddLinkItem(ID_DIALOGED_DEL_DIALOG);
	pItem->SetType(xtpTaskItemTypeLink);
	pItem->SetCaption(_T("Delete..."));
	pItem->SetTooltip(_T("Removes selected Dialog Script"));
	pItem = pGroup->AddLinkItem(ID_DIALOGED_RENAME_DIALOG);
	pItem->SetType(xtpTaskItemTypeLink);
	pItem->SetCaption(_T("Rename..."));
	pItem->SetTooltip(_T("Renames the Dialog Script"));

	/////////////////////////////////////////////////////////////////////////
	// Docking Pane for Tree Control
	pDockPane = GetDockingPaneManager()->CreatePane(IDW_DIALOGED_TREE_PANE, CRect(176, 0, 300, 240), xtpPaneDockTop, pTaskPane);
	pDockPane->SetTitle(_T("Dialogs"));
	pDockPane->SetOptions(xtpPaneNoCloseable | xtpPaneNoFloatable);

	/////////////////////////////////////////////////////////////////////////
	// Create Tree Control
	m_ctrlBrowser.Create(WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, rc, this, IDC_SCRIPT_FOLDERS);
	m_ctrlBrowser.ModifyStyleEx(0, WS_EX_STATICEDGE);
	m_ctrlBrowser.SetDialog(this);
}

//////////////////////////////////////////////////////////////////////////
void CDialogEditorDialog::ReloadDialogBrowser()
{
	m_ctrlBrowser.Reload();
	return;
}

//////////////////////////////////////////////////////////////////////////
void CDialogEditorDialog::OnUpdateProperties(IVariable* pVar)
{
}

//////////////////////////////////////////////////////////////////////////
// Called by the editor to notify the listener about the specified event.
void CDialogEditorDialog::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	if (event == eNotify_OnIdleUpdate)      // Sent every frame while editor is idle.
	{
		return;
	}

	switch (event)
	{
	case eNotify_OnBeginGameMode:           // Sent when editor goes to game mode.
	case eNotify_OnBeginSimulationMode:     // Sent when simulation mode is started.
	case eNotify_OnBeginSceneSave:          // Sent when document is about to be saved.
	case eNotify_OnQuit:                    // Sent before editor quits.
	case eNotify_OnBeginNewScene:           // Sent when the document is begin to be cleared.
	case eNotify_OnBeginSceneOpen:          // Sent when document is about to be opened.
	case eNotify_OnClearLevelContents:      // Send when the document is about to close.
		//		if ( m_bSaveNeeded && SaveLibrary() )
		//			m_bSaveNeeded = false;
		SaveCurrent();

		break;

	case eNotify_OnInit:                    // Sent after editor fully initialized.
	case eNotify_OnEndSceneOpen:            // Sent after document have been opened.
		//		ReloadEntries();
		break;

	case eNotify_OnEndSceneSave:            // Sent after document have been saved.
	case eNotify_OnEndNewScene:             // Sent after the document have been cleared.
	case eNotify_OnMissionChange:           // Send when the current mission changes.
		break;

	//////////////////////////////////////////////////////////////////////////
	// Editing events.
	//////////////////////////////////////////////////////////////////////////
	case eNotify_OnEditModeChange:          // Sent when editing mode change (move,rotate,scale,....)
	case eNotify_OnEditToolBeginChange:     // Sent when edit tool is about to be changed (ObjectMode,TerrainModify,....)
	case eNotify_OnEditToolEndChange:       // Sent when edit tool has been changed (ObjectMode,TerrainModify,....)
		break;

	// Game related events.
	case eNotify_OnEndGameMode:             // Send when editor goes out of game mode.
		break;

	// UI events.
	case eNotify_OnUpdateViewports:             // Sent when editor needs to update data in the viewports.
	case eNotify_OnInvalidateControls:          // Sent when editor needs to update some of the data that can be cached by controls like combo boxes.
	case eNotify_OnUpdateSequencer:             // Sent when editor needs to update the CryMannequin sequencer view.
	case eNotify_OnUpdateSequencerKeys:         // Sent when editor needs to update keys in the CryMannequin track view.
	case eNotify_OnUpdateSequencerKeySelection: // Sent when CryMannequin sequencer view changes selection of keys.
		break;

	// Object events.
	case eNotify_OnSelectionChange:         // Sent when object selection change.
		// Unfortunately I have never received this notification!!!
		// SinkSelection();
		break;
	case eNotify_OnPlaySequence:            // Sent when editor start playing animation sequence.
	case eNotify_OnStopSequence:            // Sent when editor stop playing animation sequence.
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
void CDialogEditorDialog::OnLocalEdit()
{
	CEditorDialogScript* pCurScript = m_View.GetScript();
	if (pCurScript == 0)
		return;
	if (m_pDM->CanModify(pCurScript))
		return;
	m_pDM->SaveScript(pCurScript, true);
	CString id = pCurScript->GetID();
	m_ctrlBrowser.UpdateEntry(id);
	SetCurrentScript(pCurScript, false, false, true);
	m_View.SetModified(false);
}

//////////////////////////////////////////////////////////////////////////
void CDialogEditorDialog::OnAddDialog()
{
	QGroupDialog dlg(QObject::tr("New DialogScript Name"));
	CString preGroup = m_ctrlBrowser.GetCurrentGroup();
	if (preGroup.IsEmpty() == false)
		dlg.SetGroup(preGroup);
	while (true)
	{
		if (dlg.exec() != QDialog::Accepted || dlg.GetString().IsEmpty())
			return;
		CString groupName = dlg.GetGroup();
		CString itemName = dlg.GetString();
		if (groupName.FindOneOf(":./") >= 0 || itemName.FindOneOf(":./") >= 0)
		{
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, _T("Group/Name may not contain characters :./"));
		}
		else
		{
			CString id;
			if (groupName.IsEmpty() == false)
			{
				id = groupName;
				id += ".";
			}
			id += itemName;

			CEditorDialogScript* pNewScript = m_pDM->GetScript(id, false);
			if (pNewScript != 0)
			{
				CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, _T("Dialog with ID '%s' already exists."), id.GetString());
			}
			else
			{
				pNewScript = m_pDM->GetScript(id, true);
				m_pDM->SaveScript(pNewScript, true);
				ReloadDialogBrowser();
				SetCurrentScript(pNewScript, true);
				return;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CDialogEditorDialog::OnDelDialog()
{
	CEditorDialogScript* pCurScript = m_View.GetScript();
	if (pCurScript == 0)
		return;
	if (!m_pDM->CanModify(pCurScript))
		return;

	CString ask;
	ask.Format(_T("Are you sure you want to remove the DialogScript '%s' ?\r\n(Note: DialogScript file also be deleted from disk/source control)"), pCurScript->GetID().c_str());

	if (QDialogButtonBox::StandardButton::Yes == CQuestionDialog::SQuestion(QObject::tr(""), QObject::tr(ask)))
	{
		if (GetIEditor()->IsSourceControlAvailable() && m_pDM->GetFileAttrs(pCurScript) & SCC_FILE_ATTRIBUTE_MANAGED)
		{
			CSourceControlDescDlg dlg;
			if (dlg.DoModal() == IDOK)
			{
				CString scriptId = pCurScript->GetID();
				CString gamePath = m_pDM->ScriptToFilename(scriptId);
				// CString fullPath = Path::GamePathToFullPath(gamePath);
				CString fullPath = gamePath;
				CryLogAlways("DialogEditor: Deleting file '%s' from SourceControl", fullPath);
#ifdef DE_USE_SOURCE_CONTROL
				if (!GetIEditor()->GetSourceControl()->Delete(fullPath, dlg.m_sDesc))
				{
					MessageBox("Could not delete file from source control!", "Error", MB_OK | MB_ICONERROR);
				}
#endif
			}
			else
				return;
		}

		m_pDM->DeleteScript(pCurScript, true); // delete from disk
		m_pDM->SyncCryActionScripts();
		ReloadDialogBrowser();
		SetCurrentScript(0, false, false);
	}
}

//////////////////////////////////////////////////////////////////////////
void CDialogEditorDialog::OnRenameDialog()
{
	CEditorDialogScript* pCurScript = m_View.GetScript();
	if (pCurScript == 0)
		return;
	if (!m_pDM->CanModify(pCurScript))
		return;

	// Here we save the script as it is before renaming it, so it will
	// have the latest changes preserved before renaming.
	if (!SaveCurrent())
		return;

	QGroupDialog dlg(QObject::tr("Change DialogScript Name"));
	CString curName = pCurScript->GetID();
	int n = curName.ReverseFind('.');
	if (n >= 0)
	{
		dlg.SetGroup(curName.Left(n));
		if (n + 1 < curName.GetLength())
			dlg.SetString(curName.Mid(n + 1));
	}
	else
	{
		dlg.SetString(curName);
	}

	while (true)
	{
		if (dlg.exec() != QDialog::Accepted || dlg.GetString().IsEmpty())
			return;
		CString groupName = dlg.GetGroup();
		CString itemName = dlg.GetString();
		if (groupName.FindOneOf(":./") >= 0 || itemName.FindOneOf(":./") >= 0)
		{
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, _T("Group/Name may not contain characters :./"));
		}
		else
		{
			CString id;
			if (groupName.IsEmpty() == false)
			{
				id = groupName;
				id += ".";
			}
			id += itemName;
			if (id == curName)
				return;

			// CString oldPath = Path::GamePathToFullPath(m_pDM->ScriptToFilename(curName));
			CString oldPath = m_pDM->ScriptToFilename(curName);
			uint32 scAttr = m_pDM->GetFileAttrs(pCurScript);
			string newName = id;
			// rename will save the script with into a new location
			// and rename it in the Manager. it will not delete it.
			// but: file querying for updated sc attributes would fail
			// as source control doesn't know anything about it
			bool ok = m_pDM->RenameScript(pCurScript, newName, false);
			if (ok == false)
			{
				CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, _T("Dialog with ID '%s' already exists."), id.GetString());
			}
			else
			{
				if (GetIEditor()->IsSourceControlAvailable() && (scAttr & SCC_FILE_ATTRIBUTE_MANAGED))
				{
					CString newId = newName;
					// CString newPath = Path::GamePathToFullPath(m_pDM->ScriptToFilename(newId));
					CString newPath = m_pDM->ScriptToFilename(newId);
					CryLogAlways("DialogEditor: Renaming '%s' to '%s' in SourceControl", oldPath, newPath);

#ifdef DE_USE_SOURCE_CONTROL
					if (!GetIEditor()->GetSourceControl()->Rename(oldPath, newPath, "Rename"))
					{
						MessageBox("Could not rename file in Source Control.", "Error", MB_OK | MB_ICONERROR);
					}
#endif
				}
				// in any case [managed/not managed], delete file
				::DeleteFile(oldPath);
				m_pDM->SyncCryActionScripts();
				ReloadDialogBrowser();
				SetCurrentScript(pCurScript, true, false, true);
				m_View.SetModified(false);
				return;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
LRESULT CDialogEditorDialog::OnDockingPaneNotify(WPARAM wParam, LPARAM lParam)
{
	if (wParam == XTP_DPN_SHOWWINDOW)
	{
		// get a pointer to the docking pane being shown.
		CXTPDockingPane* pwndDockWindow = (CXTPDockingPane*) lParam;
		if (!pwndDockWindow->IsValid())
		{
			switch (pwndDockWindow->GetID())
			{
			case IDW_DIALOGED_TASK_PANE:
				pwndDockWindow->Attach(&m_taskPanel);
				m_taskPanel.SetOwner(this);
				break;
			case IDW_DIALOGED_PROPS_PANE:
				// pwndDockWindow->Attach( &m_Properties );
				// m_Properties.SetOwner( this );
				break;
			case IDW_DIALOGED_TREE_PANE:
				pwndDockWindow->Attach(&m_ctrlBrowser);
				m_ctrlBrowser.SetOwner(this);
				break;
			case IDW_DIALOGED_DESC_PANE:
				pwndDockWindow->Attach(&m_ctrlDescription);
				m_ctrlDescription.SetOwner(this);
				break;
			case IDW_DIALOGED_HELP_PANE:
				pwndDockWindow->Attach(&m_ctrlHelp);
				m_ctrlHelp.SetOwner(this);
				break;
			default:
				return FALSE;
			}
		}
		return TRUE;
	}
	else if (wParam == XTP_DPN_CLOSEPANE)
	{
		// get a pointer to the docking pane being closed.
		CXTPDockingPane* pwndDockWindow = (CXTPDockingPane*)lParam;
		if (pwndDockWindow->IsValid())
		{
			switch (pwndDockWindow->GetID())
			{
			case IDW_DIALOGED_TASK_PANE:
				break;
			case IDW_DIALOGED_PROPS_PANE:
				break;
			case IDW_DIALOGED_TREE_PANE:
				break;
			case IDW_DIALOGED_DESC_PANE:
				break;
			}
		}
	}

	return FALSE;
}

//////////////////////////////////////////////////////////////////////////
BOOL CDialogEditorDialog::PreTranslateMessage(MSG* pMsg)
{
	bool bFramePreTranslate = true;
	if (pMsg->message >= WM_KEYFIRST && pMsg->message <= WM_KEYLAST)
	{
		CWnd* pWnd = CWnd::GetFocus();
		if (pWnd && pWnd->IsKindOf(RUNTIME_CLASS(CEdit)))
			bFramePreTranslate = false;
		if (pWnd && pWnd != &m_View)
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

//////////////////////////////////////////////////////////////////////////
LRESULT CDialogEditorDialog::OnTaskPanelNotify(WPARAM wParam, LPARAM lParam)
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
void CDialogEditorDialog::OnClose()
{
	// This will save any pending/unsaved changes of the current script.
	SetCurrentScript(0, false);
	__super::OnClose();
}

//////////////////////////////////////////////////////////////////////////
void CDialogEditorDialog::PostNcDestroy()
{
	delete this;
}

//////////////////////////////////////////////////////////////////////////
void CDialogEditorDialog::RecalcLayout(BOOL bNotify)
{
	__super::RecalcLayout(bNotify);
	if (IsWindow(m_View))
	{
		CRect rc = GetDockingPaneManager()->GetClientPane()->GetPaneWindowRect();
		ScreenToClient(rc);
		m_View.MoveWindow(rc);
		Invalidate();
	}
}

void CDialogEditorDialog::OnMouseMove(UINT flags, CPoint pt)
{
}

void CDialogEditorDialog::OnLButtonUp(UINT flags, CPoint pt)
{
}

//////////////////////////////////////////////////////////////////////////
BOOL CDialogEditorDialog::OnEraseBkgnd(CDC* pDC)
{
	return __super::OnEraseBkgnd(pDC);
}

//////////////////////////////////////////////////////////////////////////
void CDialogEditorDialog::OnSetFocus(CWnd* pOldWnd)
{
	__super::OnSetFocus(pOldWnd);
}

//////////////////////////////////////////////////////////////////////////
void CDialogEditorDialog::OnDestroy()
{
	// This will save any pending/unsaved changes of the current script.
	SetCurrentScript(0, false);

	CXTPDockingPaneLayout layout(GetDockingPaneManager());
	GetDockingPaneManager()->GetLayout(&layout);
	layout.Save(_T("DialogEditor"));
	__super::OnDestroy();
}

void CDialogEditorDialog::OnDescriptionEdit()
{
	CEditorDialogScript* pCurScript = m_View.GetScript();
	if (pCurScript)
	{
		m_View.SetModified(true);
	}
}

bool CDialogEditorDialog::SaveCurrent()
{
	CEditorDialogScript* pCurScript = m_View.GetScript();
	if (pCurScript)
	{
		CString desc;
		m_ctrlDescription.GetWindowText(desc);
		desc.Replace("\r", "");
		desc.Replace("\n", "\\n");
		pCurScript->SetDescription(desc);
	}
	if (pCurScript && m_View.IsModified())
	{
		m_View.SaveToScript();
		// also save a backup of previous file
		if (m_pDM->SaveScript(pCurScript, true, true) == false)
		{
			string id = pCurScript->GetID();
			pCurScript = m_pDM->LoadScript(id, true);
			m_View.SetScript(pCurScript);
		}
		else
		{
			m_View.SetModified(false);
		}
	}
	return true;
}

bool CDialogEditorDialog::SetCurrentScript(CEditorDialogScript* pScript, bool bSelectInTree, bool bSaveModified, bool bForceUpdate)
{
	if (bForceUpdate == false && pScript == m_View.GetScript())
		return true;

	if (bSaveModified)
	{
		SaveCurrent();
	}

	m_View.SetScript(pScript);
	UpdateWindowText(pScript, m_View.IsModified(), false);

	bool bEditable = pScript != 0 ? m_pDM->CanModify(pScript) : false;
	m_View.AllowEdit(bEditable);
	m_ctrlDescription.SetReadOnly(!bEditable);

	if (bSelectInTree)
	{
		CString entry = pScript != 0 ? pScript->GetID() : "";
		m_ctrlBrowser.GotoEntry(entry);
	}
	return true;
}

CString GetSCStatusText(uint32 scStatus)
{
	if (scStatus & SCC_FILE_ATTRIBUTE_INPAK)
		return _T("[In PAK]");
	else if (scStatus & SCC_FILE_ATTRIBUTE_CHECKEDOUT)
		return _T("[Checked Out]");
	else if ((scStatus & SCC_FILE_ATTRIBUTE_MANAGED) && (scStatus & SCC_FILE_ATTRIBUTE_NORMAL) && !(scStatus & SCC_FILE_ATTRIBUTE_READONLY))
		return _T("[Under SourceControl - Not Checked In Yet]");
	else if (scStatus & SCC_FILE_ATTRIBUTE_MANAGED)
		return _T("[Under SourceControl]");
	else if (scStatus & SCC_FILE_ATTRIBUTE_READONLY)
		return _T("[Local File - Read Only]");
	else return _T("[Local File]");
}

void CDialogEditorDialog::UpdateWindowText(CEditorDialogScript* pScript, bool bModified, bool bTitleOnly)
{
	CXTPDockingPane* pPane = GetDockingPaneManager()->FindPane(IDW_DIALOGED_DESC_PANE);
	if (pScript != 0)
	{
		bool bEditable = m_pDM->CanModify(pScript);

		if (bTitleOnly == false)
		{
			CString desc = pScript->GetDecription();
			desc.Replace("\\n", "\r\n");
			m_ctrlDescription.SetWindowText(desc);
			m_ctrlDescription.SetReadOnly(bEditable);
		}
		if (pPane != 0)
		{
			CString title;
			CString status;
			CString scStatus = GetSCStatusText(m_pDM->GetFileAttrs(pScript));
			if (!scStatus.IsEmpty())
			{
				status += "  -  ";
				status += scStatus;
			}
			title.Format(_T("Description: %s%s%s"), pScript->GetID().c_str(), bModified ? _T("  *Modified*") : "", status);
			pPane->SetTitle(title);
		}
	}
	else
	{
		if (bTitleOnly == false)
		{
			m_ctrlDescription.SetWindowText(_T(""));
			m_ctrlDescription.SetReadOnly(TRUE);
		}
		if (pPane != 0)
		{
			pPane->SetTitle(_T("Description"));
		}
	}
}

bool CDialogEditorDialog::SetCurrentScript(const CString& script, bool bSelectInTree, bool bSaveModified, bool bForceUpdate)
{
	CEditorDialogScript* pScript = 0;
	if (script.IsEmpty() == false)
		pScript = m_pDM->GetScript(script, false);
	return SetCurrentScript(pScript, bSelectInTree, bSaveModified, bForceUpdate);
}

void CDialogEditorDialog::NotifyChange(CEditorDialogScript* pScript, bool bChange)
{
	UpdateWindowText(pScript, bChange, true);
}

void CDialogEditorDialog::UpdateHelp(const CString& help)
{
	m_ctrlHelp.SetWindowText(help);
}

const char* SCToName(CDialogEditorDialog::ESourceControlOp op)
{
	switch (op)
	{
	case CDialogEditorDialog::ESCM_CHECKIN:
		return "Checkin";
	case CDialogEditorDialog::ESCM_CHECKOUT:
		return "Checkout";
	case CDialogEditorDialog::ESCM_GETLATEST:
		return "Get Latest";
	case CDialogEditorDialog::ESCM_UNDO_CHECKOUT:
		return "Undo checkout";
	case CDialogEditorDialog::ESCM_IMPORT:
		return "Import";
	default:
		return "Unknown";
	}
}

bool CDialogEditorDialog::DoSourceControlOp(CEditorDialogScript* pScript, ESourceControlOp scOp)
{
	if (pScript == 0)
		return false;

	if (!GetIEditor()->IsSourceControlAvailable())
		return false;

	CEditorDialogScript* pCurScript = m_View.GetScript();
	if (pCurScript != pScript)
	{
		assert(false);
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "CDialogEditorDialog::DoSourceControlOp: Wrong edited item. Ask AlexL to fix this.");
		return false;
	}

	SaveCurrent();

	bool bRes = true;

	CString scriptId = pScript->GetID();
	CString gamePath = m_pDM->ScriptToFilename(scriptId);
	// CString path = Path::GamePathToFullPath(gamePath);
	CString path = gamePath;

	CryLogAlways("DialogEditor: Doing SC-Op: %s for %s", SCToName(scOp), path);

#ifdef  DE_USE_SOURCE_CONTROL
	switch (scOp)
	{
	case ESCM_IMPORT:
		{
			CSourceControlDescDlg dlg;
			if (dlg.DoModal() == IDOK)
			{
				bRes = GetIEditor()->GetSourceControl()->Add(path, dlg.m_sDesc);
				if (!bRes)
				{
					// because it might have been added to source control, but not checked in
					m_ctrlBrowser.UpdateEntry(scriptId); // this will also update the SC status
					SetCurrentScript(pScript, false, false, true);
				}
			}
			else
				bRes = true;
		}
		break;
	case ESCM_CHECKIN:
		{
			CSourceControlDescDlg dlg;
			if (dlg.DoModal() == IDOK)
			{
				bRes = GetIEditor()->GetSourceControl()->CheckIn(path, dlg.m_sDesc);
			}
			else
				bRes = true;
		}
		break;
	case ESCM_CHECKOUT:
		bRes = GetIEditor()->GetSourceControl()->CheckOut(path);
		break;
	case ESCM_UNDO_CHECKOUT:
		bRes = GetIEditor()->GetSourceControl()->UndoCheckOut(path);
		if (bRes)
		{
			string id = scriptId;
			pScript = m_pDM->LoadScript(id, true);
		}
		break;
	case ESCM_GETLATEST:
		bRes = GetIEditor()->GetSourceControl()->GetLatestVersion(path);
		if (bRes)
		{
			string id = scriptId;
			pScript = m_pDM->LoadScript(id, true);
		}
		break;
	}
#endif

	if (bRes)
	{
		// ok
		m_ctrlBrowser.UpdateEntry(scriptId); // this will also update the SC status
		SetCurrentScript(pScript, false, false, true);
	}
	else
	{
		MessageBox("Source Control Operation Failed.\r\nCheck if Source Control Provider correctly setup and working directory is correct.", "Error", MB_OK | MB_ICONERROR);
	}
	return bRes;
}

