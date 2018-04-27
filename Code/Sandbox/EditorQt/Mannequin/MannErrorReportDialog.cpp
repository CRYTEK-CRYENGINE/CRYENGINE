// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "MannErrorReportDialog.h"
#include "Util/Clipboard.h"
#include "Util/CryMemFile.h"          // CCryMemFile
#include "Util/MFCUtil.h"
#include "Util\Mailer.h"
#include "GameEngine.h"

#include "MannequinDialog.h"

#include <CryGame/IGameFramework.h>
#include "ICryMannequinEditor.h"

// CMannErrorReportDialog dialog

static int __stdcall CompareItems(LPARAM p1, LPARAM p2, LPARAM sort)
{
	CMannErrorRecord* err1 = (CMannErrorRecord*)p1;
	CMannErrorRecord* err2 = (CMannErrorRecord*)p2;
	if (err1->severity < err2->severity)
		return -1;
	else
		return (err1->severity > err2->severity);
}

//////////////////////////////////////////////////////////////////////////
CString CMannErrorRecord::GetErrorText()
{
	CString str = error;
	str.TrimRight();

	str.Format("[%2d]\t%s", count, (const char*)error);
	str.TrimRight();

	if (!file.IsEmpty())
	{
		str += CString("\t") + file;
	}
	else
	{
		str += CString("\t ");
	}

	return str;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
class CMannErrorMessageRecord : public CXTPReportRecord
{
	DECLARE_DYNAMIC(CMannErrorMessageRecord)
public:
	CMannErrorRecord*     m_pRecord;
	CXTPReportRecordItem* m_pIconItem;

	CMannErrorMessageRecord(CMannErrorRecord& err, SMannequinContexts* contexts)
	{
		m_pRecord = &err;

		const char* errorString[] = { "None", "Fragment", "Transition" };
		CString text, file, object, module, type;
		int count = err.count;

		text = err.error;
		file = err.file;
		type = errorString[err.type];

		CXTPReportRecordItem* checkBox = new CXTPReportRecordItem();
		checkBox->HasCheckbox(true, false);
		AddItem(checkBox);
		m_pIconItem = AddItem(new CXTPReportRecordItem());
		const char* fragName = (err.fragmentID == FRAGMENT_ID_INVALID) ? ((err.type == SMannequinErrorReport::Blend) ? "Any" : "None") : contexts->m_controllerDef->m_fragmentIDs.GetTagName(err.fragmentID);
		char tagList[512];
		MannUtils::FlagsToTagList(tagList, sizeof(tagList), err.tagState, err.fragmentID, *contexts->m_controllerDef, "");

		const char* fragName2 = "";
		char tagList2[512] = "";
		if (err.type == SMannequinErrorReport::Blend)
		{
			fragName2 = (err.fragmentIDTo == FRAGMENT_ID_INVALID) ? "Any" : contexts->m_controllerDef->m_fragmentIDs.GetTagName(err.fragmentIDTo);
			MannUtils::FlagsToTagList(tagList2, sizeof(tagList2), err.tagStateTo, err.fragmentIDTo, *contexts->m_controllerDef, "");
		}

		AddItem(new CXTPReportRecordItemText(fragName));
		AddItem(new CXTPReportRecordItemText(tagList));
		AddItem(new CXTPReportRecordItemText(fragName2));
		AddItem(new CXTPReportRecordItemText(tagList2));
		AddItem(new CXTPReportRecordItemText(type));
		AddItem(new CXTPReportRecordItemText(text));
		AddItem(new CXTPReportRecordItemText(file));

		//SetPreviewItem(new CXTPReportRecordItemPreview(strPreview));
		int nIcon = 0;
		if (err.severity == CMannErrorRecord::ESEVERITY_ERROR)
			nIcon = BITMAP_ERROR;
		else if (err.severity == CMannErrorRecord::ESEVERITY_WARNING)
			nIcon = BITMAP_WARNING;
		else if (err.severity == CMannErrorRecord::ESEVERITY_COMMENT)
			nIcon = BITMAP_COMMENT;
		m_pIconItem->SetIconIndex(nIcon);
		m_pIconItem->SetGroupPriority(nIcon);
		m_pIconItem->SetSortPriority(nIcon);
	}

	virtual void GetItemMetrics(XTP_REPORTRECORDITEM_DRAWARGS* pDrawArgs, XTP_REPORTRECORDITEM_METRICS* pItemMetrics)
	{
		if (m_pIconItem == pDrawArgs->pItem && pDrawArgs->pItem->GetIconIndex() == 0)
		{
			// Red error text.
			pItemMetrics->clrForeground = RGB(0xFF, 0, 0);
		}
	}
};

IMPLEMENT_DYNAMIC(CMannErrorMessageRecord, CXTPReportRecord)

//
//class CErrorReportViewPaneClass : public TRefCountBase<IViewPaneClass>
//{
//	//////////////////////////////////////////////////////////////////////////
//	// IClassDesc
//	//////////////////////////////////////////////////////////////////////////
//	virtual ESystemClassID SystemClassID() { return ESYSTEM_CLASS_VIEWPANE; };
//	virtual REFGUID ClassID()
//	{
//		static const GUID guid =
//			{ 0xea523b7e, 0x3f63, 0x821b, { 0x48, 0x23, 0xa1, 0x31, 0xfc, 0x5b, 0x46, 0xa3 } };
//		return guid;
//	}
//	virtual const char* ClassName() { return "Error Report"; };
//	virtual const char* Category() { return "Editor"; };
//	//////////////////////////////////////////////////////////////////////////
//	virtual CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CMannErrorReportDialog); };
//	virtual const char* GetPaneTitle() { return _T("Error Report"); };
//	virtual QRect GetPaneRect() { return QRect(0,0,600,200); };
//	virtual QSize GetMinSize() { return QSize(200,100); }
//	virtual bool SinglePane() { return true; };
//	virtual bool WantIdleUpdate() { return true; };
//};

//IMPLEMENT_DYNCREATE(CMannErrorReportDialog,CXTResizeDialog)

//////////////////////////////////////////////////////////////////////////
//void CMannErrorReportDialog::RegisterViewClass()
//{
//	GetIEditorImpl()->GetClassFactory()->RegisterClass( new CErrorReportViewPaneClass );
//}

//IMPLEMENT_DYNAMIC(CMannErrorReportDialog, CXTResizeDialog)

CMannErrorReportDialog::CMannErrorReportDialog(CWnd* pParent)
	:
	CXTResizeDialog(CMannErrorReportDialog::IDD, pParent),
	m_contexts(NULL)
{
}

CMannErrorReportDialog::~CMannErrorReportDialog()
{
}

void CMannErrorReportDialog::AddError(const CMannErrorRecord& error)
{
	m_errorRecords.push_back(error);
}

//////////////////////////////////////////////////////////////////////////
void CMannErrorReportDialog::Refresh()
{
	UpdateErrors();
}

//////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CMannErrorReportDialog, CXTResizeDialog)
ON_NOTIFY(NM_DBLCLK, IDC_ERRORS, OnNMDblclkErrors)
ON_BN_CLICKED(IDC_SELECTOBJECTS, OnSelectObjects)
ON_BN_CLICKED(IDC_SELECT_ALL, OnSelectAll)
ON_BN_CLICKED(IDC_CLEAR_SELECTION, OnClearSelection)
ON_BN_CLICKED(IDC_CHECK_FOR_ERRORS, OnCheckErrors)
ON_BN_CLICKED(IDC_DELETE_FRAGMENTS, OnDeleteFragments)
ON_WM_SIZE()

ON_NOTIFY(NM_CLICK, ID_REPORT_CONTROL, OnReportItemClick)
ON_NOTIFY(NM_RCLICK, ID_REPORT_CONTROL, OnReportItemRClick)
ON_NOTIFY(NM_DBLCLK, ID_REPORT_CONTROL, OnReportItemDblClick)
ON_NOTIFY(XTP_NM_REPORT_HEADER_RCLICK, ID_REPORT_CONTROL, OnReportColumnRClick)
ON_NOTIFY(XTP_NM_REPORT_HYPERLINK, ID_REPORT_CONTROL, OnReportHyperlink)
ON_NOTIFY(NM_KEYDOWN, ID_REPORT_CONTROL, OnReportKeyDown)
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
//void CMannErrorReportDialog::Close()
//{
//	DestroyWindow();
//
//	if (m_instance)
//	{
//		/*
//		CCryMemFile memFile( new BYTE[256], 256, 256 );
//		CArchive ar( &memFile, CArchive::store );
//		m_instance->m_wndReport.SerializeState( ar );
//		ar.Close();
//
//		UINT nSize = (UINT)memFile.GetLength();
//		LPBYTE pbtData = memFile.Detach();
//		CXTRegistryManager regManager;
//		regManager.WriteProfileBinary( "Dialogs\\ErrorReport", "Configuration", pbtData, nSize);
//		if ( pbtData )
//			delete [] pbtData;
//*/
//		m_instance->DestroyWindow();
//	}
//}

//////////////////////////////////////////////////////////////////////////
void CMannErrorReportDialog::Clear()
{
	m_errorRecords.clear();
	UpdateErrors();
}

void CMannErrorReportDialog::DoDataExchange(CDataExchange* pDX)
{
	__super::DoDataExchange(pDX);
}

// CMannErrorReportDialog message handlers

BOOL CMannErrorReportDialog::OnInitDialog()
{
	__super::OnInitDialog();

	m_contexts = CMannequinDialog::GetCurrentInstance()->Contexts();

	VERIFY(m_wndReport.Create(WS_CHILD | WS_TABSTOP | WS_VISIBLE | WM_VSCROLL, CRect(0, 0, 0, 0), this, ID_REPORT_CONTROL));

	//m_imageList.Create(IDB_ERROR_REPORT, 16, 1, RGB (255, 255, 255));
	CMFCUtils::LoadTrueColorImageList(m_imageList, IDB_ERROR_REPORT, 16, RGB(255, 255, 255));

	m_wndReport.SetImageList(&m_imageList);

	CXTPReportColumn* pFileCol = 0;

	//  Add sample columns
	m_wndReport.AddColumn(new CXTPReportColumn(COLUMN_CHECKBOX, _T("Sel"), 18, FALSE));
	m_wndReport.AddColumn(new CXTPReportColumn(COLUMN_SEVERITY, _T(""), 18, FALSE));
	m_wndReport.AddColumn(new CXTPReportColumn(COLUMN_FRAGMENT, _T("Frag"), 90, FALSE));
	m_wndReport.AddColumn(new CXTPReportColumn(COLUMN_TAGS, _T("Tags"), 40, TRUE));
	m_wndReport.AddColumn(new CXTPReportColumn(COLUMN_FRAGMENTTO, _T("FragTo"), 90, FALSE));
	m_wndReport.AddColumn(new CXTPReportColumn(COLUMN_TAGSTO, _T("TagsTo"), 40, TRUE));
	m_wndReport.AddColumn(new CXTPReportColumn(COLUMN_TYPE, _T("Type"), 80, FALSE));
	m_wndReport.AddColumn(new CXTPReportColumn(COLUMN_TEXT, _T("Text"), 300, TRUE));
	m_wndReport.AddColumn(pFileCol = new CXTPReportColumn(COLUMN_FILE, _T("File"), 100, TRUE));

	pFileCol->SetAlignment(DT_RIGHT);

	m_wndReport.GetPaintManager()->m_clrHyper = ::GetSysColor(COLOR_HIGHLIGHT);

	m_wndReport.GetColumns()->GetGroupsOrder()->Add(pFileCol);

	AutoLoadPlacement("Dialogs\\MannequinErrorReport");

	UINT nSize = 0;
	LPBYTE pbtData = NULL;
	CXTRegistryManager regManager;
	if (regManager.GetProfileBinary("Dialogs\\MannequinErrorReport", "Configuration", &pbtData, &nSize))
	{
		CCryMemFile memFile(pbtData, nSize);
		CArchive ar(&memFile, CArchive::load);
		m_wndReport.SerializeState(ar);
	}

	/*
	   CXTPReportColumns* pColumns = m_wndReport.GetColumns();
	   CXTPReportColumn* pColumn = pColumns->GetAt(0);
	   if (pColumn && pColumn->IsSortable())
	   {
	   pColumns->SetSortColumn(pColumn, true);
	   m_wndReport.Populate();
	   }
	 */

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

static volatile int TOP_CONTROL_OFFSET = 35;

void CMannErrorReportDialog::RepositionControls()
{
	if (m_wndReport)
	{
		CRect rc;
		GetClientRect(rc);
		rc.top += TOP_CONTROL_OFFSET;
		m_wndReport.MoveWindow(rc);
	}
}

void CMannErrorReportDialog::UpdateErrors()
{
	ReloadErrors();
	m_wndReport.RedrawControl();

	if (::IsWindow(GetSafeHwnd()))
	{
		RedrawWindow();

		RepositionControls();
	}
}

//////////////////////////////////////////////////////////////////////////
void CMannErrorReportDialog::OnSize(UINT nType, int cx, int cy)
{
	__super::OnSize(nType, cx, cy);

	RepositionControls();
}

//////////////////////////////////////////////////////////////////////////
void CMannErrorReportDialog::ReloadErrors()
{
	m_wndReport.ResetContent();

	//if(!m_pErrorReport)
	//	return;

	m_wndReport.BeginUpdate();

	for (int i = 0; i < m_errorRecords.size(); i++)
	{
		CMannErrorRecord& err = m_errorRecords[i];
		const char* str = err.error;

		m_wndReport.AddRecord(new CMannErrorMessageRecord(err, m_contexts));
	}

	m_wndReport.EndUpdate();
	m_wndReport.Populate();
}

void CMannErrorReportDialog::OnNMDblclkErrors(NMHDR* pNMHDR, LRESULT* pResult)
{
	/*
	   NMITEMACTIVATE *lpNM = (NMITEMACTIVATE*)pNMHDR;

	   CErrorRecord *err = (CErrorRecord*)m_errors.GetItemData( lpNM->iItem );
	   if (err)
	   {
	   if (err->pObject)
	   {
	    CUndo undo( "Select Object(s)" );
	    // Clear other selection.
	    GetIEditorImpl()->ClearSelection();
	    // Select this object.
	    GetIEditorImpl()->SelectObject( err->pObject );
	   }
	   else if (err->pMaterial)
	   {
	    GetIEditorImpl()->OpenDataBaseLibrary( EDB_MATERIAL_LIBRARY,err->pMaterial );
	   }
	   }
	 */

	// TODO: Add your control notification handler code here
	*pResult = 0;
}

//////////////////////////////////////////////////////////////////////////
void CMannErrorReportDialog::OnSelectObjects()
{
	/*
	   CUndo undo( "Select Object(s)" );
	   // Clear other selection.
	   GetIEditorImpl()->ClearSelection();
	   POSITION pos = m_errors.GetFirstSelectedItemPosition();
	   while (pos)
	   {
	   int nItem = m_errors.GetNextSelectedItem(pos);
	   CErrorRecord *pError = (CErrorRecord*)m_errors.GetItemData( nItem );
	   if (pError && pError->pObject)
	   {
	    // Select this object.
	    GetIEditorImpl()->SelectObject( pError->pObject );
	   }
	   }
	 */
}

//////////////////////////////////////////////////////////////////////////
void CMannErrorReportDialog::OnSelectAll()
{
	const int numRecords = m_wndReport.GetRecords()->GetCount();
	for (uint32 i = 0; i < numRecords; i++)
	{
		CXTPReportRecordItem* checkRecord = m_wndReport.GetRecords()->GetAt(i)->GetItem(COLUMN_CHECKBOX);
		checkRecord->SetChecked(true);
	}
	RedrawWindow();
	RepositionControls();
	m_wndReport.RedrawControl();
}

//////////////////////////////////////////////////////////////////////////
void CMannErrorReportDialog::OnClearSelection()
{
	const int numRecords = m_wndReport.GetRecords()->GetCount();
	for (uint32 i = 0; i < numRecords; i++)
	{
		CXTPReportRecordItem* checkRecord = m_wndReport.GetRecords()->GetAt(i)->GetItem(COLUMN_CHECKBOX);
		checkRecord->SetChecked(false);
	}
	RedrawWindow();
	RepositionControls();
	m_wndReport.RedrawControl();
}

//////////////////////////////////////////////////////////////////////////
void CMannErrorReportDialog::OnCheckErrors()
{
	CMannequinDialog::GetCurrentInstance()->Validate();
}

//////////////////////////////////////////////////////////////////////////
void CMannErrorReportDialog::OnDeleteFragments()
{
	const int numRecords = m_wndReport.GetRecords()->GetCount();
	uint32 numToDelete = 0;
	for (uint32 i = 0; i < numRecords; i++)
	{
		CXTPReportRecordItem* checkRecord = m_wndReport.GetRecords()->GetAt(i)->GetItem(COLUMN_CHECKBOX);
		if (checkRecord->IsChecked())
		{
			numToDelete++;
		}
	}

	if (numToDelete > 0)
	{
		static char msg[4096];
		cry_sprintf(msg, "Delete all %d selected fragments?", numToDelete);
		int nCode = MessageBox(
		  msg,
		  "Delete Fragments?",
		  MB_OKCANCEL | MB_ICONHAND | MB_SETFOREGROUND | MB_SYSTEMMODAL);

		if (nCode == IDOK)
		{
			std::vector<CMannErrorRecord*> recordsToDelete;

			for (uint32 i = 0; i < numRecords; i++)
			{
				CXTPReportRecordItem* checkRecord = m_wndReport.GetRecords()->GetAt(i)->GetItem(COLUMN_CHECKBOX);
				if (checkRecord->IsChecked())
				{
					recordsToDelete.push_back(&m_errorRecords[i]);
				}
			}

			// sort the records to delete in reverse order by optionindex to prevent these indices to
			// become invalid while deleting other options
			std::sort(recordsToDelete.begin(), recordsToDelete.end(), [](const CMannErrorRecord* p1, const CMannErrorRecord* p2)
			{
				return p1->fragOptionID > p2->fragOptionID;
			});

			int numRecordsToProcess = recordsToDelete.size();

			for (uint32 i = 0; i < numRecordsToProcess; i++)
			{
				CMannErrorRecord& errorRecord = *recordsToDelete[i];
				const uint32 numScopeDatas = m_contexts->m_contextData.size();
				for (uint32 s = 0; s < numScopeDatas; s++)
				{
					IAnimationDatabase* animDB = m_contexts->m_contextData[s].database;
					if (animDB && (errorRecord.file == animDB->GetFilename()))
					{
						IMannequin& mannequinSys = gEnv->pGameFramework->GetMannequinInterface();
						mannequinSys.GetMannequinEditorManager()->DeleteFragmentEntry(animDB, errorRecord.fragmentID, errorRecord.tagState, errorRecord.fragOptionID);
						break;
					}
				}
			}

			CMannequinDialog::GetCurrentInstance()->Validate();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
#define ID_REMOVE_ITEM            1
#define ID_SORT_ASC               2
#define ID_SORT_DESC              3
#define ID_GROUP_BYTHIS           4
#define ID_SHOW_GROUPBOX          5
#define ID_SHOW_FIELDCHOOSER      6
#define ID_COLUMN_BESTFIT         7
#define ID_COLUMN_ARRANGEBY       100
#define ID_COLUMN_ALIGMENT        200
#define ID_COLUMN_ALIGMENT_LEFT   ID_COLUMN_ALIGMENT + 1
#define ID_COLUMN_ALIGMENT_RIGHT  ID_COLUMN_ALIGMENT + 2
#define ID_COLUMN_ALIGMENT_CENTER ID_COLUMN_ALIGMENT + 3
#define ID_COLUMN_SHOW            500

void CMannErrorReportDialog::OnReportColumnRClick(NMHDR* pNotifyStruct, LRESULT* /*result*/)
{
	XTP_NM_REPORTRECORDITEM* pItemNotify = (XTP_NM_REPORTRECORDITEM*) pNotifyStruct;
	ASSERT(pItemNotify->pColumn);
	CPoint ptClick = pItemNotify->pt;

	CMenu menu;
	VERIFY(menu.CreatePopupMenu());

	// create main menu items
	menu.AppendMenu(MF_STRING, ID_SORT_ASC, _T("Sort &Ascending"));
	menu.AppendMenu(MF_STRING, ID_SORT_DESC, _T("Sort Des&cending"));
	menu.AppendMenu(MF_SEPARATOR, (UINT)-1, (LPCTSTR)NULL);
	menu.AppendMenu(MF_STRING, ID_GROUP_BYTHIS, _T("&Group by this field"));
	menu.AppendMenu(MF_STRING, ID_SHOW_GROUPBOX, _T("Group &by box"));
	menu.AppendMenu(MF_SEPARATOR, (UINT)-1, (LPCTSTR)NULL);
	menu.AppendMenu(MF_STRING, ID_REMOVE_ITEM, _T("&Remove column"));
	menu.AppendMenu(MF_STRING, ID_SHOW_FIELDCHOOSER, _T("Field &Chooser"));
	menu.AppendMenu(MF_SEPARATOR, (UINT)-1, (LPCTSTR)NULL);
	menu.AppendMenu(MF_STRING, ID_COLUMN_BESTFIT, _T("Best &Fit"));

	if (m_wndReport.IsGroupByVisible())
	{
		menu.CheckMenuItem(ID_SHOW_GROUPBOX, MF_BYCOMMAND | MF_CHECKED);
	}
	if (m_wndReport.GetReportHeader()->IsShowItemsInGroups())
	{
		menu.EnableMenuItem(ID_GROUP_BYTHIS, MF_BYCOMMAND | MF_DISABLED);
	}

	CXTPReportColumns* pColumns = m_wndReport.GetColumns();
	CXTPReportColumn* pColumn = pItemNotify->pColumn;

	// create arrange by items
	CMenu menuArrange;
	VERIFY(menuArrange.CreatePopupMenu());
	int nColumnCount = pColumns->GetCount();
	for (int nColumn = 0; nColumn < nColumnCount; nColumn++)
	{
		CXTPReportColumn* pCol = pColumns->GetAt(nColumn);
		if (pCol && pCol->IsVisible())
		{
			CString sCaption = pCol->GetCaption();
			if (!sCaption.IsEmpty())
				menuArrange.AppendMenu(MF_STRING, ID_COLUMN_ARRANGEBY + nColumn, sCaption);
		}
	}

	menuArrange.AppendMenu(MF_SEPARATOR, 60, (LPCTSTR)NULL);

	menuArrange.AppendMenu(MF_STRING, ID_COLUMN_ARRANGEBY + nColumnCount, _T("Show in groups"));
	menuArrange.CheckMenuItem(ID_COLUMN_ARRANGEBY + nColumnCount,
	                          MF_BYCOMMAND | ((m_wndReport.GetReportHeader()->IsShowItemsInGroups()) ? MF_CHECKED : MF_UNCHECKED));
	menu.InsertMenu(0, MF_BYPOSITION | MF_POPUP, (UINT_PTR) menuArrange.m_hMenu,
	                _T("Arrange By"));

	// create columns items
	CMenu menuColumns;
	VERIFY(menuColumns.CreatePopupMenu());
	for (int nColumn = 0; nColumn < nColumnCount; nColumn++)
	{
		CXTPReportColumn* pCol = pColumns->GetAt(nColumn);
		CString sCaption = pCol->GetCaption();
		//if (!sCaption.IsEmpty())
		menuColumns.AppendMenu(MF_STRING, ID_COLUMN_SHOW + nColumn, sCaption);
		menuColumns.CheckMenuItem(ID_COLUMN_SHOW + nColumn,
		                          MF_BYCOMMAND | (pCol->IsVisible() ? MF_CHECKED : MF_UNCHECKED));
	}

	menu.InsertMenu(0, MF_BYPOSITION | MF_POPUP, (UINT_PTR) menuColumns.m_hMenu, _T("Columns"));

	//create Text alignment submenu
	CMenu menuAlign;
	VERIFY(menuAlign.CreatePopupMenu());

	menuAlign.AppendMenu(MF_STRING, ID_COLUMN_ALIGMENT_LEFT, _T("Align Left"));
	menuAlign.AppendMenu(MF_STRING, ID_COLUMN_ALIGMENT_RIGHT, _T("Align Right"));
	menuAlign.AppendMenu(MF_STRING, ID_COLUMN_ALIGMENT_CENTER, _T("Align Center"));

	int nAlignOption = 0;
	switch (pColumn->GetAlignment())
	{
	case DT_LEFT:
		nAlignOption = ID_COLUMN_ALIGMENT_LEFT;
		break;
	case DT_RIGHT:
		nAlignOption = ID_COLUMN_ALIGMENT_RIGHT;
		break;
	case DT_CENTER:
		nAlignOption = ID_COLUMN_ALIGMENT_CENTER;
		break;
	}

	menuAlign.CheckMenuItem(nAlignOption, MF_BYCOMMAND | MF_CHECKED);
	menu.InsertMenu(11, MF_BYPOSITION | MF_POPUP, (UINT_PTR) menuAlign.m_hMenu, _T("&Alignment"));

	// track menu
	int nMenuResult = CXTPCommandBars::TrackPopupMenu(&menu, TPM_NONOTIFY | TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON, ptClick.x, ptClick.y, this, NULL);

	// arrange by items
	if (nMenuResult >= ID_COLUMN_ARRANGEBY && nMenuResult < ID_COLUMN_ALIGMENT)
	{
		for (int nColumn = 0; nColumn < nColumnCount; nColumn++)
		{
			CXTPReportColumn* pCol = pColumns->GetAt(nColumn);
			if (pCol && pCol->IsVisible())
			{
				if (nMenuResult == ID_COLUMN_ARRANGEBY + nColumn)
				{
					nMenuResult = ID_SORT_ASC;
					pColumn = pCol;
					break;
				}
			}
		}
		// group by item
		if (ID_COLUMN_ARRANGEBY + nColumnCount == nMenuResult)
		{
			m_wndReport.GetReportHeader()->ShowItemsInGroups(
			  !m_wndReport.GetReportHeader()->IsShowItemsInGroups());
		}
	}

	// process Alignment options
	if (nMenuResult > ID_COLUMN_ALIGMENT && nMenuResult < ID_COLUMN_SHOW)
	{
		switch (nMenuResult)
		{
		case ID_COLUMN_ALIGMENT_LEFT:
			pColumn->SetAlignment(DT_LEFT);
			break;
		case ID_COLUMN_ALIGMENT_RIGHT:
			pColumn->SetAlignment(DT_RIGHT);
			break;
		case ID_COLUMN_ALIGMENT_CENTER:
			pColumn->SetAlignment(DT_CENTER);
			break;
		}
	}

	// process column selection item
	if (nMenuResult >= ID_COLUMN_SHOW)
	{
		CXTPReportColumn* pCol = pColumns->GetAt(nMenuResult - ID_COLUMN_SHOW);
		if (pCol)
		{
			pCol->SetVisible(!pCol->IsVisible());
		}
	}

	// other general items
	switch (nMenuResult)
	{
	case ID_REMOVE_ITEM:
		pColumn->SetVisible(FALSE);
		m_wndReport.Populate();
		break;
	case ID_SORT_ASC:
	case ID_SORT_DESC:
		if (pColumn && pColumn->IsSortable())
		{
			pColumns->SetSortColumn(pColumn, nMenuResult == ID_SORT_ASC);
			m_wndReport.Populate();
		}
		break;

	case ID_GROUP_BYTHIS:
		if (pColumns->GetGroupsOrder()->IndexOf(pColumn) < 0)
		{
			pColumns->GetGroupsOrder()->Add(pColumn);
		}
		m_wndReport.ShowGroupBy(TRUE);
		m_wndReport.Populate();
		break;
	case ID_SHOW_GROUPBOX:
		m_wndReport.ShowGroupBy(!m_wndReport.IsGroupByVisible());
		break;
	case ID_SHOW_FIELDCHOOSER:
		//OnShowFieldChooser();
		break;
	case ID_COLUMN_BESTFIT:
		m_wndReport.GetColumns()->GetReportHeader()->BestFit(pColumn);
		break;
	}
}

#define ID_POPUP_COLLAPSEALLGROUPS 1
#define ID_POPUP_EXPANDALLGROUPS   2

//////////////////////////////////////////////////////////////////////////
void CMannErrorReportDialog::CopyToClipboard()
{
	CClipboard clipboard;
	CString str;
	CXTPReportSelectedRows* selRows = m_wndReport.GetSelectedRows();
	for (int i = 0; i < selRows->GetCount(); i++)
	{
		CMannErrorMessageRecord* pRecord = DYNAMIC_DOWNCAST(CMannErrorMessageRecord, selRows->GetAt(i)->GetRecord());
		if (pRecord)
		{
			str += pRecord->m_pRecord->GetErrorText();
			//if(pRecord->m_pRecord->pObject)
			//	str+=" [Object: " + pRecord->m_pRecord->pObject->GetName() + "]";
			//if(pRecord->m_pRecord->pItem)
			//	str+=" [Material: " + pRecord->m_pRecord->pItem->GetName() + "]";
			str += "\r\n";
		}
	}
	if (!str.IsEmpty())
		clipboard.PutString(str.GetString(), "Errors");

}

//////////////////////////////////////////////////////////////////////////
void CMannErrorReportDialog::OnReportItemRClick(NMHDR* pNotifyStruct, LRESULT* /*result*/)
{
	XTP_NM_REPORTRECORDITEM* pItemNotify = (XTP_NM_REPORTRECORDITEM*) pNotifyStruct;

	if (!pItemNotify->pRow)
		return;

	if (pItemNotify->pRow->IsGroupRow())
	{
		CMenu menu;
		menu.CreatePopupMenu();
		menu.AppendMenu(MF_STRING, ID_POPUP_COLLAPSEALLGROUPS, _T("Collapse &All Groups"));
		menu.AppendMenu(MF_STRING, ID_POPUP_EXPANDALLGROUPS, _T("E&xpand All Groups"));

		// track menu
		int nMenuResult = CXTPCommandBars::TrackPopupMenu(&menu, TPM_NONOTIFY | TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON, pItemNotify->pt.x, pItemNotify->pt.y, this, NULL);

		// general items processing
		switch (nMenuResult)
		{
		case ID_POPUP_COLLAPSEALLGROUPS:
			pItemNotify->pRow->GetControl()->CollapseAll();
			break;
		case ID_POPUP_EXPANDALLGROUPS:
			pItemNotify->pRow->GetControl()->ExpandAll();
			break;
		}
	}
	else if (pItemNotify->pItem)
	{
		CMenu menu;
		menu.CreatePopupMenu();
		menu.AppendMenu(MF_STRING, 0, _T("Select Object(s)"));
		menu.AppendMenu(MF_STRING, 1, _T("Copy Warning(s) To Clipboard"));
		menu.AppendMenu(MF_STRING, 2, _T("E-mail Error Report"));
		menu.AppendMenu(MF_STRING, 3, _T("Open in Excel"));

		// track menu
		int nMenuResult = CXTPCommandBars::TrackPopupMenu(&menu, TPM_NONOTIFY | TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON, pItemNotify->pt.x, pItemNotify->pt.y, this, NULL);
		switch (nMenuResult)
		{
		case 1:
			CopyToClipboard();
			break;
		case 2:
			SendInMail();
			break;
		case 3:
			OpenInExcel();
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMannErrorReportDialog::SendInMail()
{
	//if (!m_pErrorReport)
	//	return;

	// Send E-Mail.
	CString textMsg;
	for (int i = 0; i < m_errorRecords.size(); i++)
	{
		CMannErrorRecord& err = m_errorRecords[i];
		textMsg += err.GetErrorText() + "\n";
	}

	std::vector<const char*> who;
	CString subject;
	subject.Format("Level %s Error Report", (const char*)GetIEditorImpl()->GetGameEngine()->GetLevelName());
	CMailer::SendMail(subject, textMsg, who, who, true);
}

//////////////////////////////////////////////////////////////////////////
void CMannErrorReportDialog::OpenInExcel()
{
	//if (!m_pErrorReport)
	//	return;

	CString levelName = PathUtil::GetFileName(GetIEditorImpl()->GetGameEngine()->GetLevelName());

	CString filename;
	filename.Format("ErrorList_%s.csv", (const char*)levelName);

	// Save to temp file.
	FILE* f = fopen(filename, "wt");
	if (f)
	{
		for (int i = 0; i < m_errorRecords.size(); i++)
		{
			CMannErrorRecord& err = m_errorRecords[i];
			CString text = (const char*)err.GetErrorText();
			text.Replace(',', '.');
			text.Replace('\t', ',');
			fprintf(f, "%s\n", (LPCTSTR)text);
		}
		fclose(f);
		::ShellExecute(NULL, "open", filename, NULL, NULL, SW_SHOW);
	}
	else
	{
		Warning("Failed to save %s", (const char*)filename);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMannErrorReportDialog::OnReportItemClick(NMHDR* pNotifyStruct, LRESULT* /*result*/)
{
	XTP_NM_REPORTRECORDITEM* pItemNotify = (XTP_NM_REPORTRECORDITEM*) pNotifyStruct;

	if (!pItemNotify->pRow || !pItemNotify->pColumn)
		return;

	TRACE(_T("Click on row %d, col %d\n"),
	      pItemNotify->pRow->GetIndex(), pItemNotify->pColumn->GetItemIndex());

	/*
	   if (pItemNotify->pColumn->GetItemIndex() == COLUMN_CHECK)
	   {
	   m_wndReport.Populate();
	   }
	 */
}

//////////////////////////////////////////////////////////////////////////
void CMannErrorReportDialog::OnReportKeyDown(NMHDR* pNotifyStruct, LRESULT* /*result*/)
{
	LPNMKEY lpNMKey = (LPNMKEY)pNotifyStruct;

	if (!m_wndReport.GetFocusedRow())
		return;

	if (lpNMKey->nVKey == VK_SPACE)
	{
		CXTPReportSelectedRows* pSelRows = m_wndReport.GetSelectedRows();

		if (pSelRows)
		{
			bool setState = false;
			bool queriedState = false;
			for (int i = 0; i < pSelRows->GetCount(); i++)
			{
				CMannErrorMessageRecord* pRecord = DYNAMIC_DOWNCAST(CMannErrorMessageRecord, pSelRows->GetAt(i)->GetRecord());
				if (pRecord)
				{
					CXTPReportRecordItem* pCheckBoxItem = pRecord->GetItem(COLUMN_CHECKBOX);

					if (pCheckBoxItem)
					{
						if (!queriedState)
						{
							setState = !pCheckBoxItem->IsChecked();
						}

						pCheckBoxItem->SetChecked(setState);
					}
				}
			}

			RedrawWindow();
			RepositionControls();
			m_wndReport.RedrawControl();
		}
	}

	if (lpNMKey->nVKey == VK_RETURN)
	{
		/*
		   CMannErrorMessageRecord* pRecord = DYNAMIC_DOWNCAST(CMannErrorMessageRecord,m_wndReport.GetFocusedRow()->GetRecord());
		   if (pRecord)
		   {
		   if (pRecord->SetRead())
		   {
		    m_wndReport.Populate();
		   }
		   }
		 */
	}

	if (lpNMKey->nVKey == 'C' && GetAsyncKeyState(VK_CONTROL))
	{
		CopyToClipboard();
	}
}

//////////////////////////////////////////////////////////////////////////
void CMannErrorReportDialog::OnReportItemDblClick(NMHDR* pNotifyStruct, LRESULT* result)
{
	XTP_NM_REPORTRECORDITEM* pItemNotify = (XTP_NM_REPORTRECORDITEM*) pNotifyStruct;
	if (!pItemNotify->pRow)
		return;
	CMannErrorMessageRecord* pRecord = DYNAMIC_DOWNCAST(CMannErrorMessageRecord, pItemNotify->pRow->GetRecord());
	if (pRecord)
	{
		bool bDone = false;
		CMannErrorRecord* pError = pRecord->m_pRecord;
		if (pError)
		{
			//if (pError->pObject != NULL)
			//{
			//	CUndo undo( "Select Object(s)" );
			//	// Clear other selection.
			//	GetIEditorImpl()->ClearSelection();
			//	// Select this object.
			//	GetIEditorImpl()->SelectObject( pError->pObject );

			//	CViewport *vp = GetIEditorImpl()->GetActiveView();
			//	if (vp)
			//		vp->CenterOnSelection();
			//	bDone = true;
			//}
			//if (pError->pItem != NULL)
			//{
			//	GetIEditorImpl()->OpenDataBaseLibrary( pError->pItem->GetType(),pError->pItem );
			//	bDone = true;
			//}

			if (pError->fragmentID != FRAGMENT_ID_INVALID)
			{
				CMannequinDialog::GetCurrentInstance()->EditFragment(pError->fragmentID, pError->tagState, pError->contextDef->contextID);
			}

			//if(!bDone && GetIEditorImpl()->GetActiveView())
			//{
			//	float x, y, z;
			//	if(GetPositionFromString(pError->error, &x, &y, &z))
			//	{
			//		CViewport *vp = GetIEditorImpl()->GetActiveView();
			//		Matrix34 tm = vp->GetViewTM();
			//		tm.SetTranslation(Vec3(x, y, z));
			//		vp->SetViewTM(tm);
			//	}
			//}
		}
	}

	/*
	   XTP_NM_REPORTRECORDITEM* pItemNotify = (XTP_NM_REPORTRECORDITEM*) pNotifyStruct;

	   if (pItemNotify->pRow)
	   {
	   TRACE(_T("Double Click on row %d\n"),
	    pItemNotify->pRow->GetIndex());

	   CMannErrorMessageRecord* pRecord = DYNAMIC_DOWNCAST(CMannErrorMessageRecord, pItemNotify->pRow->GetRecord());
	   if (pRecord)
	   {
	    /*
	    {
	      m_wndReport.Populate();
	    }
	 * /
	   }
	   }
	 */
}

//////////////////////////////////////////////////////////////////////////
void CMannErrorReportDialog::OnReportHyperlink(NMHDR* pNotifyStruct, LRESULT* result)
{
	XTP_NM_REPORTRECORDITEM* pItemNotify = (XTP_NM_REPORTRECORDITEM*) pNotifyStruct;
	if (pItemNotify->nHyperlink >= 0)
	{
		CMannErrorMessageRecord* pRecord = DYNAMIC_DOWNCAST(CMannErrorMessageRecord, pItemNotify->pRow->GetRecord());
		if (pRecord)
		{
			bool bDone = false;
			CMannErrorRecord* pError = pRecord->m_pRecord;
			//if (pError && pError->pObject != NULL)
			//{
			//	CUndo undo( "Select Object(s)" );
			//	// Clear other selection.
			//	GetIEditorImpl()->ClearSelection();
			//	// Select this object.
			//	GetIEditorImpl()->SelectObject( pError->pObject );
			//	bDone = true;
			//}
			//if (pError && pError->pItem != NULL)
			//{
			//	GetIEditorImpl()->OpenDataBaseLibrary( pError->pItem->GetType(),pError->pItem );
			//	bDone = true;
			//}

			//if(!bDone && pError && GetIEditorImpl()->GetActiveView())
			//{
			//	float x, y, z;
			//	if(GetPositionFromString(pError->error, &x, &y, &z))
			//	{
			//		CViewport *vp = GetIEditorImpl()->GetActiveView();
			//		Matrix34 tm = vp->GetViewTM();
			//		tm.SetTranslation(Vec3(x, y, z));
			//		vp->SetViewTM(tm);
			//	}
			//}

		}
		TRACE(_T("Hyperlink Click : \n row %d \n col %d \n Hyperlink %d\n"),
		      pItemNotify->pRow->GetIndex(), pItemNotify->pColumn->GetItemIndex(), pItemNotify->nHyperlink);
	}
}

/*
   //////////////////////////////////////////////////////////////////////////
   void CMannErrorReportDialog::OnShowFieldChooser()
   {
   CMainFrm* pMainFrm = (CMainFrame*)AfxGetMainWnd();
   if (pMainFrm)
   {
    BOOL bShow = !pMainFrm->m_wndFieldChooser.IsVisible();
    pMainFrm->ShowControlBar(&pMainFrm->m_wndFieldChooser, bShow, FALSE);
   }
   }
 */

