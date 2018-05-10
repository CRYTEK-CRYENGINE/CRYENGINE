// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "MannFileManager.h"
#include "Util/Clipboard.h"
#include "Util/CryMemFile.h"          // CCryMemFile
#include "Util/MFCUtil.h"
#include "Util/Mailer.h"
#include "GameEngine.h"

#include "MannequinDialog.h"

#include <CryGame/IGameFramework.h>
#include "Helper/MannequinFileChangeWriter.h"

#include "Controls/InPlaceButton.h"

#include <ISourceControl.h>
#include <FilePathUtil.h>

//////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CMannFileManager, CXTResizeDialog)
ON_BN_CLICKED(IDC_ADD, OnAddToSourceControl)
ON_BN_CLICKED(IDC_CHECK_OUT, OnCheckOutSelection)
ON_BN_CLICKED(IDC_UNDO, OnUndoSelection)
ON_BN_CLICKED(IDC_REFRESH, OnRefresh)
ON_BN_CLICKED(IDC_SAVE, OnSaveFiles)
ON_BN_CLICKED(IDC_RELOAD_ALL, OnReloadAllFiles)
ON_WM_SIZE()

ON_NOTIFY(NM_CLICK, ID_REPORT_CONTROL, OnReportItemClick)
ON_NOTIFY(NM_RCLICK, ID_REPORT_CONTROL, OnReportItemRClick)
ON_NOTIFY(NM_DBLCLK, ID_REPORT_CONTROL, OnReportItemDblClick)
ON_NOTIFY(XTP_NM_REPORT_HEADER_RCLICK, ID_REPORT_CONTROL, OnReportColumnRClick)
ON_NOTIFY(XTP_NM_REPORT_HYPERLINK, ID_REPORT_CONTROL, OnReportHyperlink)
ON_NOTIFY(XTP_NM_REPORT_SELCHANGED, ID_REPORT_CONTROL, OnReportSelChanged)
ON_NOTIFY(NM_KEYDOWN, ID_REPORT_CONTROL, OnReportKeyDown)
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
enum EFileReportColumn
{
	FILECOLUMN_STATUS,
	FILECOLUMN_FILE,
	FILECOLUMN_TYPE,
};

enum EFileStatusIcon
{
	STATUSICON_OK,
	STATUSICON_READONLY,
	STATUSICON_CHECKEDOUT,
	STATUSICON_PAK
};

class CXTPMannFileRecord : public CXTPReportRecord
{
	DECLARE_DYNAMIC(CXTPMannFileRecord)
public:
	CXTPMannFileRecord(const SFileEntry& fileEntry)
		: m_fileEntry(fileEntry)
		, m_bCheckedOut(false)
		, m_bInPak(false)
		, m_bReadOnly(false)
	{
		m_fullFileName = PathUtil::GamePathToCryPakPath(m_fileEntry.filename.GetString(), true);

		m_pIconItem = AddItem(new CXTPReportRecordItem());
		AddItem(new CXTPReportRecordItemText(m_fileEntry.filename));
		AddItem(new CXTPReportRecordItemText(m_fileEntry.typeDesc));

		UpdateState();
	}

	bool              CheckedOut() const { return m_bCheckedOut; }
	bool              InPak() const      { return m_bInPak; }
	bool              ReadOnly() const   { return m_bReadOnly; }
	const SFileEntry& FileEntry() const  { return m_fileEntry; }

	void              UpdateState()
	{
		int nIcon = STATUSICON_OK;
		uint32 attr = CFileUtil::GetAttributes(m_fullFileName);

		m_bCheckedOut = false;
		m_bInPak = false;
		m_bReadOnly = false;

		// Checked out?
		if ((attr & SCC_FILE_ATTRIBUTE_MANAGED) && (attr & SCC_FILE_ATTRIBUTE_CHECKEDOUT))
		{
			m_bCheckedOut = true;
			nIcon = STATUSICON_CHECKEDOUT;
		}
		// In a pak?
		else if (attr & SCC_FILE_ATTRIBUTE_INPAK)
		{
			m_bInPak = true;
			nIcon = STATUSICON_PAK;
		}
		// Read-only?
		else if (attr & SCC_FILE_ATTRIBUTE_READONLY)
		{
			m_bReadOnly = true;
			nIcon = STATUSICON_READONLY;
		}

		UpdateIcon(nIcon);
	}

	void CheckOut()
	{
		if (m_bCheckedOut || m_bInPak)
			return;

		if (CFileUtil::CheckoutFile(m_fullFileName))
		{
			m_bCheckedOut = true;
			m_bReadOnly = false;
			UpdateIcon(STATUSICON_CHECKEDOUT);
		}
		else
		{
			CString message;
			message.Format("Failed to checkout file \"%s\"", m_fileEntry.filename);
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, message);
		}
	}

	void AddToSourceControl()
	{
		if (m_bInPak)
			return;

		if (!CFileUtil::AddFileToSourceControl(m_fullFileName))
		{
			CString message;
			message.Format("Failed to add the file \"%s\" to Source Control", m_fileEntry.filename);
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, message);
		}
	}

	const CString& GetFullFileName() const
	{
		return m_fullFileName;
	}

private:
	void UpdateIcon(int nIcon)
	{
		m_pIconItem->SetIconIndex(nIcon);
		m_pIconItem->SetGroupPriority(nIcon);
		m_pIconItem->SetSortPriority(nIcon);

		const char* text = "";
		switch (nIcon)
		{
		case STATUSICON_OK:
			text = "Writeable";
			break;
		case STATUSICON_READONLY:
			text = "Read-Only";
			break;
		case STATUSICON_CHECKEDOUT:
			text = "Checked Out";
			break;
		case STATUSICON_PAK:
			text = "In PAK File";
			break;
		}

		m_pIconItem->SetTooltip(text);
	}

	CXTPReportRecordItem* m_pIconItem;
	const SFileEntry&     m_fileEntry;
	CString               m_fullFileName;
	bool                  m_bCheckedOut;
	bool                  m_bInPak;
	bool                  m_bReadOnly;
};

//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNAMIC(CXTPMannFileRecord, CXTPReportRecord)

//////////////////////////////////////////////////////////////////////////
CMannFileManager::CMannFileManager(CMannequinFileChangeWriter& fileChangeWriter, bool bChangedFilesMode, CWnd* pParent)
	: CXTResizeDialog(bChangedFilesMode ? CMannFileManager::IDD_MONITOR : CMannFileManager::IDD, pParent)
	, m_fileChangeWriter(fileChangeWriter)
	, m_bInChangedFileMode(bChangedFilesMode)
{
	m_bSourceControlAvailable = GetIEditorImpl()->IsSourceControlAvailable();
}

//////////////////////////////////////////////////////////////////////////
BOOL CMannFileManager::OnInitDialog()
{
	__super::OnInitDialog();

	CRect rc;
	GetClientRect(&rc);

	//m_pShowCurrentPreviewFilesOnlyCheckbox.reset( new CInPlaceButton( functor( *this, &CMannFileManager::OnDisplayOnlyCurrentPreviewClicked ) ) );

	//CRect rcCheckbox = rc;
	//rcCheckbox.left = 5;
	//rcCheckbox.top = rcCheckbox.bottom - 20;
	//rcCheckbox.bottom -= 5;
	//rcCheckbox.right = 300;
	//m_pShowCurrentPreviewFilesOnlyCheckbox->Create( "Save only current preview files", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, rcCheckbox, this, ID_DISPLAY_ONLY_PREVIEW_FILES );
	//m_pShowCurrentPreviewFilesOnlyCheckbox->SetChecked( m_fileChangeWriter.GetFilterFilesByControllerDef() );

	CRect rcReport = rc;
	rcReport.top += 35;
	rcReport.bottom -= 35;
	VERIFY(m_wndReport.Create(WS_CHILD | WS_TABSTOP | WS_VISIBLE | WM_VSCROLL, rcReport, this, ID_REPORT_CONTROL));

	CMFCUtils::LoadTrueColorImageList(m_imageList, IDB_MANNEQUIN_FILEMANAGER_IMAGELIST, 16, RGB(255, 0, 255));
	m_wndReport.SetImageList(&m_imageList);

	CXTPReportColumn* pFileCol = 0;

	m_wndReport.GetColumns()->Clear();
	m_wndReport.AddColumn(new CXTPReportColumn(FILECOLUMN_STATUS, _T(""), 18, FALSE));
	m_wndReport.AddColumn(new CXTPReportColumn(FILECOLUMN_FILE, _T("File"), 30, TRUE));
	m_wndReport.AddColumn(pFileCol = new CXTPReportColumn(FILECOLUMN_TYPE, _T("Type"), 30, TRUE));

	m_wndReport.GetPaintManager()->m_clrHyper = ::GetSysColor(COLOR_HIGHLIGHT);

	m_wndReport.GetColumns()->GetGroupsOrder()->Add(pFileCol);

	SetResize(IDC_ADD, SZ_HORREPOS(1));
	SetResize(IDC_CHECK_OUT, SZ_HORREPOS(1));
	SetResize(IDC_SAVE, SZ_REPOS(1));
	if (m_bInChangedFileMode)
	{
		SetResize(IDC_RELOAD_ALL, SZ_REPOS(1));
	}
	else
	{
		SetResize(IDC_UNDO, SZ_HORREPOS(1));
		SetResize(IDCANCEL, SZ_REPOS(1));
	}
	SetResize(ID_REPORT_CONTROL, SZ_RESIZE(1));

	AutoLoadPlacement("Dialogs\\MannequinFileManager");

	UINT nSize = 0;
	LPBYTE pbtData = NULL;
	CXTRegistryManager regManager;
	if (regManager.GetProfileBinary("Dialogs\\MannequinFileManager", "Configuration", &pbtData, &nSize))
	{
		CCryMemFile memFile(pbtData, nSize);
		CArchive ar(&memFile, CArchive::load);
		m_wndReport.SerializeState(ar);
	}

	UpdateFileRecords();

	if (!m_bSourceControlAvailable)
	{
		GetDlgItem(IDC_ADD)->ShowWindow(SW_HIDE);
		GetDlgItem(IDC_CHECK_OUT)->ShowWindow(SW_HIDE);
	}

	CenterWindow();

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

//////////////////////////////////////////////////////////////////////////
void CMannFileManager::SetButtonStates()
{
	bool bHasSelection = false;
	bool bPartlyReadOnly = false;
	bool bSelectionPartlyInPak = false;
	CXTPReportRows* rows = m_wndReport.GetRows();
	for (int i = 0; i < rows->GetCount(); i++)
	{
		CXTPReportRow* row = rows->GetAt(i);

		if (row->IsGroupRow())
			continue;

		CXTPMannFileRecord* pRecord = DYNAMIC_DOWNCAST(CXTPMannFileRecord, row->GetRecord());
		if (pRecord)
		{
			if (row->IsSelected() == TRUE)
				bHasSelection = true;

			if (pRecord->ReadOnly())
				bPartlyReadOnly = true;

			if (row->IsSelected() && pRecord->InPak())
				bSelectionPartlyInPak = true;
		}
	}

	if (!m_bInChangedFileMode)
	{
		GetDlgItem(IDC_UNDO)->EnableWindow(bHasSelection);
	}

	GetDlgItem(IDC_ADD)->EnableWindow(bHasSelection && !bSelectionPartlyInPak && m_bSourceControlAvailable);
	GetDlgItem(IDC_CHECK_OUT)->EnableWindow(bHasSelection && !bSelectionPartlyInPak && m_bSourceControlAvailable);
	GetDlgItem(IDC_SAVE)->EnableWindow(!bPartlyReadOnly);
}

//////////////////////////////////////////////////////////////////////////
void CMannFileManager::UpdateFileRecords()
{
	ReloadFileRecords();
	SetButtonStates();
	m_wndReport.RedrawControl();
}

//////////////////////////////////////////////////////////////////////////
void CMannFileManager::ReloadFileRecords()
{
	m_wndReport.BeginUpdate();
	{
		m_wndReport.ResetContent();

		const size_t modifiedFilesCount = m_fileChangeWriter.GetModifiedFilesCount();
		for (size_t i = 0; i < modifiedFilesCount; ++i)
		{
			const SFileEntry& fileEntry = m_fileChangeWriter.GetModifiedFileEntry(i);

			CXTPMannFileRecord* pFileRecord = new CXTPMannFileRecord(fileEntry);
			m_wndReport.AddRecord(pFileRecord);
		}

		m_wndReport.Populate();
	}
	m_wndReport.EndUpdate();
}

//////////////////////////////////////////////////////////////////////////
void CMannFileManager::OnAddToSourceControl()
{
	if (!m_bSourceControlAvailable)
		return;

	CXTPReportSelectedRows* selRows = m_wndReport.GetSelectedRows();
	for (int i = 0; i < selRows->GetCount(); i++)
	{
		CXTPMannFileRecord* pRecord = DYNAMIC_DOWNCAST(CXTPMannFileRecord, selRows->GetAt(i)->GetRecord());
		if (pRecord && !pRecord->InPak())
		{
			if (!CFileUtil::FileExists(pRecord->GetFullFileName()))
				m_fileChangeWriter.WriteNewFile(pRecord->FileEntry().filename);

			pRecord->AddToSourceControl();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMannFileManager::OnCheckOutSelection()
{
	if (!m_bSourceControlAvailable)
		return;

	CXTPReportSelectedRows* selRows = m_wndReport.GetSelectedRows();
	for (int i = 0; i < selRows->GetCount(); i++)
	{
		CXTPMannFileRecord* pRecord = DYNAMIC_DOWNCAST(CXTPMannFileRecord, selRows->GetAt(i)->GetRecord());
		if (pRecord && !pRecord->CheckedOut() && !pRecord->InPak())
			pRecord->CheckOut();
	}

	for (int i = 0; i < selRows->GetCount(); i++)
	{
		CXTPMannFileRecord* pRecord = DYNAMIC_DOWNCAST(CXTPMannFileRecord, selRows->GetAt(i)->GetRecord());
		pRecord->UpdateState();
	}

	SetButtonStates();
	m_wndReport.RedrawControl();
}

//////////////////////////////////////////////////////////////////////////
void CMannFileManager::OnUndoSelection()
{
	std::vector<const char*> undoSelection;
	CXTPReportSelectedRows* selRows = m_wndReport.GetSelectedRows();
	for (int i = 0; i < selRows->GetCount(); i++)
	{
		CXTPMannFileRecord* pRecord = DYNAMIC_DOWNCAST(CXTPMannFileRecord, selRows->GetAt(i)->GetRecord());
		if (pRecord)
		{
			undoSelection.push_back(pRecord->FileEntry().filename);
		}
	}

	const uint32 numDels = undoSelection.size();
	for (int i = 0; i < numDels; i++)
	{
		m_fileChangeWriter.UndoModifiedFile(undoSelection[i]);
	}

	UpdateFileRecords();

	if (m_fileChangeWriter.GetModifiedFilesCount() == 0)
		EndDialog(IDOK);
}

//////////////////////////////////////////////////////////////////////////
void CMannFileManager::OnReloadAllFiles()
{
	IAnimationDatabaseManager& manager = gEnv->pGameFramework->GetMannequinInterface().GetAnimationDatabaseManager();
	manager.ReloadAll();

	if (CMannequinDialog* pMannequinDialog = CMannequinDialog::GetCurrentInstance())
	{
		pMannequinDialog->LoadNewPreviewFile(NULL);
		pMannequinDialog->StopEditingFragment();
	}

	EndDialog(IDOK);
}

//////////////////////////////////////////////////////////////////////////
void CMannFileManager::OnRefresh()
{
	UpdateFileRecords();

	if (m_fileChangeWriter.GetModifiedFilesCount() == 0)
		EndDialog(IDOK);
}

//////////////////////////////////////////////////////////////////////////
void CMannFileManager::OnSaveFiles()
{
	CString filesInPaks;
	CXTPReportRows* rows = m_wndReport.GetRows();
	for (int i = 0; i < rows->GetCount(); i++)
	{
		CXTPMannFileRecord* pRecord = DYNAMIC_DOWNCAST(CXTPMannFileRecord, rows->GetAt(i)->GetRecord());
		if (pRecord && pRecord->InPak())
			filesInPaks += pRecord->FileEntry().filename + "\n";
	}

	if (filesInPaks.GetLength() > 0)
	{
		CString message;
		message.Format("The following files are being edited directly from the PAK files\n"
		               "and will need to be manually merged in Perforce:\n\n%s\n"
		               "Would you like to continue?", filesInPaks);
		if (CQuestionDialog::SQuestion("CryMannequin", message.GetBuffer()) != QDialogButtonBox::Yes)
			return;
	}

	m_fileChangeWriter.WriteModifiedFiles();

	EndDialog(IDOK);
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

void CMannFileManager::OnReportColumnRClick(NMHDR* pNotifyStruct, LRESULT* /*result*/)
{
	XTP_NM_REPORTRECORDITEM* pItemNotify = (XTP_NM_REPORTRECORDITEM*) pNotifyStruct;
	CXTPReportColumn* pColumn = pItemNotify->pColumn;
	ASSERT(pColumn);

	CXTPReportColumns* pColumns = m_wndReport.GetColumns();

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
	if (1 < pColumns->GetVisibleColumnsCount())
	{
		menu.AppendMenu(MF_STRING, ID_REMOVE_ITEM, _T("&Remove column"));
	}
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
void CMannFileManager::OnReportItemRClick(NMHDR* pNotifyStruct, LRESULT* /*result*/)
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
	/*	else if (pItemNotify->pItem)
	   {
	    CMenu menu;
	    menu.CreatePopupMenu();
	    menu.AppendMenu(MF_STRING, 0,_T("Select Object(s)") );
	    menu.AppendMenu(MF_STRING, 1,_T("Copy Warning(s) To Clipboard") );
	    menu.AppendMenu(MF_STRING, 2,_T("E-mail Error Report") );
	    menu.AppendMenu(MF_STRING, 3,_T("Open in Excel") );

	    // track menu
	    int nMenuResult = CXTPCommandBars::TrackPopupMenu( &menu, TPM_NONOTIFY|TPM_RETURNCMD|TPM_LEFTALIGN|TPM_RIGHTBUTTON, pItemNotify->pt.x, pItemNotify->pt.y, this, NULL);
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
	   }*/
}

//////////////////////////////////////////////////////////////////////////
void CMannFileManager::OnReportItemClick(NMHDR* pNotifyStruct, LRESULT* /*result*/)
{
	SetButtonStates();

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
void CMannFileManager::OnReportKeyDown(NMHDR* pNotifyStruct, LRESULT* /*result*/)
{
	LPNMKEY lpNMKey = (LPNMKEY)pNotifyStruct;

	if (!m_wndReport.GetFocusedRow())
		return;

	if (lpNMKey->nVKey == VK_RETURN)
	{
		CXTPMannFileRecord* pRecord = DYNAMIC_DOWNCAST(CXTPMannFileRecord, m_wndReport.GetFocusedRow()->GetRecord());
		/*
		   if (pRecord)
		   {
		   if (pRecord->SetRead())
		   {
		    m_wndReport.Populate();
		   }
		   }
		 */
	}

}

//////////////////////////////////////////////////////////////////////////
void CMannFileManager::OnReportItemDblClick(NMHDR* pNotifyStruct, LRESULT* result)
{
	XTP_NM_REPORTRECORDITEM* pItemNotify = (XTP_NM_REPORTRECORDITEM*) pNotifyStruct;
	if (!pItemNotify->pRow)
		return;
	CXTPMannFileRecord* pRecord = DYNAMIC_DOWNCAST(CXTPMannFileRecord, pItemNotify->pRow->GetRecord());
	if (pRecord)
	{
		bool bDone = false;
		//CMannFileRecord *pFileRecord = pRecord->m_pRecord;
		//if (pFileRecord)
		//{
		//if (pFileRecord->pObject != NULL)
		//{
		//	CUndo undo( "Select Object(s)" );
		//	// Clear other selection.
		//	GetIEditorImpl()->ClearSelection();
		//	// Select this object.
		//	GetIEditorImpl()->SelectObject( pFileRecord->pObject );

		//	CViewport *vp = GetIEditorImpl()->GetActiveView();
		//	if (vp)
		//		vp->CenterOnSelection();
		//	bDone = true;
		//}
		//if (pFileRecord->pItem != NULL)
		//{
		//	GetIEditorImpl()->OpenDataBaseLibrary( pFileRecord->pItem->GetType(),pFileRecord->pItem );
		//	bDone = true;
		//}

		//if (pFileRecord->fragmentID != FRAGMENT_ID_INVALID)
		//{
		//	m_contexts.m_pAnimContext->mannequinDialog->EditFragment(pFileRecord->fragmentID, pFileRecord->tagState, pFileRecord->contextDef->contextID);
		//}

		//if(!bDone && GetIEditorImpl()->GetActiveView())
		//{
		//	float x, y, z;
		//	if(GetPositionFromString(pFileRecord->error, &x, &y, &z))
		//	{
		//		CViewport *vp = GetIEditorImpl()->GetActiveView();
		//		Matrix34 tm = vp->GetViewTM();
		//		tm.SetTranslation(Vec3(x, y, z));
		//		vp->SetViewTM(tm);
		//	}
		//}
		//}
	}

	/*
	   XTP_NM_REPORTRECORDITEM* pItemNotify = (XTP_NM_REPORTRECORDITEM*) pNotifyStruct;

	   if (pItemNotify->pRow)
	   {
	   TRACE(_T("Double Click on row %d\n"),
	    pItemNotify->pRow->GetIndex());

	   CXTPMannFileRecord* pRecord = DYNAMIC_DOWNCAST(CXTPMannFileRecord, pItemNotify->pRow->GetRecord());
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
void CMannFileManager::OnReportSelChanged(NMHDR* pNotifyStruct, LRESULT* result)
{
	SetButtonStates();
}

//////////////////////////////////////////////////////////////////////////
void CMannFileManager::OnReportHyperlink(NMHDR* pNotifyStruct, LRESULT* result)
{
	XTP_NM_REPORTRECORDITEM* pItemNotify = (XTP_NM_REPORTRECORDITEM*) pNotifyStruct;
	if (pItemNotify->nHyperlink >= 0)
	{
		CXTPMannFileRecord* pRecord = DYNAMIC_DOWNCAST(CXTPMannFileRecord, pItemNotify->pRow->GetRecord());
		if (pRecord)
		{
			bool bDone = false;
			//CMannFileRecord *pFileRecord = pRecord->m_pRecord;
			//if (pFileRecord && pFileRecord->pObject != NULL)
			//{
			//	CUndo undo( "Select Object(s)" );
			//	// Clear other selection.
			//	GetIEditorImpl()->ClearSelection();
			//	// Select this object.
			//	GetIEditorImpl()->SelectObject( pFileRecord->pObject );
			//	bDone = true;
			//}
			//if (pFileRecord && pFileRecord->pItem != NULL)
			//{
			//	GetIEditorImpl()->OpenDataBaseLibrary( pFileRecord->pItem->GetType(),pFileRecord->pItem );
			//	bDone = true;
			//}

			//if(!bDone && pFileRecord && GetIEditorImpl()->GetActiveView())
			//{
			//	float x, y, z;
			//	if(GetPositionFromString(pFileRecord->error, &x, &y, &z))
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

//////////////////////////////////////////////////////////////////////////
void CMannFileManager::OnDisplayOnlyCurrentPreviewClicked()
{
	const bool displayOnlyPreviewRelatedFiles = m_pShowCurrentPreviewFilesOnlyCheckbox->GetChecked();
	m_fileChangeWriter.SetFilterFilesByControllerDef(displayOnlyPreviewRelatedFiles);

	UpdateFileRecords();
}

