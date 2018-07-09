// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "ReportDialog.h"
#include "Util/CryMemFile.h"
#include "Resource.h"

#define ID_REPORT_CONTROL 100

IMPLEMENT_DYNAMIC(CReportDialog, CXTResizeDialog)

BEGIN_MESSAGE_MAP(CReportDialog, CXTResizeDialog)
//ON_NOTIFY(NM_DBLCLK, IDC_ERRORS, OnNMDblclkErrors)
ON_BN_CLICKED(IDC_SELECTOBJECTS, OnSelectObjects)
ON_WM_SYSCOMMAND()
ON_WM_SIZE()

ON_NOTIFY(NM_CLICK, ID_REPORT_CONTROL, OnReportItemClick)
ON_NOTIFY(NM_RCLICK, ID_REPORT_CONTROL, OnReportItemRClick)
ON_NOTIFY(NM_DBLCLK, ID_REPORT_CONTROL, OnReportItemDblClick)
ON_NOTIFY(XTP_NM_REPORT_HEADER_RCLICK, ID_REPORT_CONTROL, OnReportColumnRClick)
ON_NOTIFY(XTP_NM_REPORT_HYPERLINK, ID_REPORT_CONTROL, OnReportHyperlink)
ON_NOTIFY(NM_KEYDOWN, ID_REPORT_CONTROL, OnReportKeyDown)
END_MESSAGE_MAP()

CReportDialog::CReportDialog(const char* title, CWnd* pParent)
	: CXTResizeDialog(IDD_ERROR_REPORT, pParent),
	m_title(title)
{
	m_profileTitle = title;
	m_profileTitle.replace(' ', '_');
}

CReportDialog::~CReportDialog()
{
}

void CReportDialog::Open(CReport* report)
{
	if (!GetSafeHwnd())
		Create(IDD_ERROR_REPORT, AfxGetMainWnd());
	Load(report);
	ShowWindow(SW_SHOW);
}

void CReportDialog::Close()
{
	CCryMemFile memFile(new BYTE[256], 256, 256);
	CArchive ar(&memFile, CArchive::store);
	m_wndReport.SerializeState(ar);
	ar.Close();

	UINT nSize = (UINT)memFile.GetLength();
	LPBYTE pbtData = memFile.GetMemPtr();
	CXTRegistryManager regManager;
	regManager.WriteProfileBinary((string("Dialogs\\Report_") + m_profileTitle).c_str(), "Configuration", pbtData, nSize);

	DestroyWindow();
}

void CReportDialog::OnOK()
{
	DestroyWindow();
}

void CReportDialog::OnCancel()
{
	DestroyWindow();
}

void CReportDialog::DoDataExchange(CDataExchange* pDX)
{
	__super::DoDataExchange(pDX);
}

BOOL CReportDialog::OnInitDialog()
{
	__super::OnInitDialog();

	VERIFY(m_wndReport.Create(WS_CHILD | WS_TABSTOP | WS_VISIBLE | WM_VSCROLL, CRect(0, 0, 0, 0), this, ID_REPORT_CONTROL));

	SetWindowText(m_title.c_str());

	AutoLoadPlacement((string("Dialogs\\Report_") + m_profileTitle).c_str());

	UINT nSize = 0;
	LPBYTE pbtData = NULL;
	CXTRegistryManager regManager;
	if (regManager.GetProfileBinary((string("Dialogs\\Report_") + m_profileTitle).c_str(), "Configuration", &pbtData, &nSize))
	{
		CCryMemFile memFile(pbtData, nSize);
		CArchive ar(&memFile, CArchive::load);
		m_wndReport.SerializeState(ar);
	}

	m_wndReport.RedrawControl();

	RedrawWindow();

	CRect rc;
	GetClientRect(rc);
	m_wndReport.MoveWindow(rc);

	return TRUE;
}

void CReportDialog::PostNcDestroy()
{
	__super::PostNcDestroy();
}

void CReportDialog::OnSelectObjects()
{
}

void CReportDialog::OnSize(UINT nType, int cx, int cy)
{
	__super::OnSize(nType, cx, cy);

	if (m_wndReport)
	{
		CRect rc;
		GetClientRect(rc);
		m_wndReport.MoveWindow(rc);
	}
}

void CReportDialog::OnSysCommand(UINT nID, LPARAM lParam)
{
	if (nID == SC_CLOSE)
	{
		Close();
	}
	else
	{
		CDialog::OnSysCommand(nID, lParam);
	}
}

void CReportDialog::OnReportItemClick(NMHDR* pNotifyStruct, LRESULT* result)
{
}

void CReportDialog::OnReportItemRClick(NMHDR* pNotifyStruct, LRESULT* result)
{
}

void CReportDialog::OnReportColumnRClick(NMHDR* pNotifyStruct, LRESULT* result)
{
}

void CReportDialog::OnReportItemDblClick(NMHDR* pNotifyStruct, LRESULT* result)
{
}

void CReportDialog::OnReportHyperlink(NMHDR* pNotifyStruct, LRESULT* result)
{
}

void CReportDialog::OnReportKeyDown(NMHDR* pNotifyStruct, LRESULT* result)
{
}

class StrLess : public std::less<const char*>
{
public:
	bool operator()(const char* s1, const char* s2) const
	{
		return stricmp(s1, s2) < 0;
	}
};

void CReportDialog::Load(CReport* report)
{
	typedef std::map<const char*, int, StrLess> ColumnMap;
	ColumnMap columnMap;

	m_wndReport.GetColumns()->Clear();
	m_wndReport.GetRecords()->RemoveAll();

	int numColumns = 0;
	for (int recordIndex = 0; report && recordIndex < report->GetRecordCount(); ++recordIndex)
	{
		IReportRecord* record = (report ? report->GetRecord(recordIndex) : 0);
		for (int fieldIndex = 0; record && fieldIndex < record->GetFieldCount(); ++fieldIndex)
		{
			const char* description = (record ? record->GetFieldDescription(fieldIndex) : 0);
			if (description && columnMap.insert(std::make_pair(description, numColumns)).second)
				++numColumns;
		}
	}

	{
		std::vector<const char*> columnDescriptions;
		columnDescriptions.resize(numColumns);
		for (ColumnMap::iterator column = columnMap.begin(); column != columnMap.end(); ++column)
			columnDescriptions[(*column).second] = (*column).first;
		for (std::vector<const char*>::iterator column = columnDescriptions.begin(); column != columnDescriptions.end(); ++column)
			m_wndReport.AddColumn(new CXTPReportColumn(column - columnDescriptions.begin(), *column, 40, TRUE));
	}

	{
		m_wndReport.BeginUpdate();

		std::vector<CXTPReportRecordItem*> items;
		items.resize(numColumns);
		for (int recordIndex = 0; report && recordIndex < report->GetRecordCount(); ++recordIndex)
		{
			IReportRecord* record = (report ? report->GetRecord(recordIndex) : 0);

			CXTPReportRecord* entry = new CXTPReportRecord();

			std::fill(items.begin(), items.end(), static_cast<CXTPReportRecordItem*>(0));
			for (int fieldIndex = 0; record && fieldIndex < record->GetFieldCount(); ++fieldIndex)
			{
				const char* description = (record ? record->GetFieldDescription(fieldIndex) : 0);
				const char* text = (record ? record->GetFieldText(fieldIndex) : 0);
				ColumnMap::iterator columnIterator = (description ? columnMap.find(description) : columnMap.end());
				int columnIndex = (columnIterator != columnMap.end() ? (*columnIterator).second : -1);
				if (columnIndex >= 0 && columnIndex < int(items.size()) && text)
					items[columnIndex] = new CXTPReportRecordItemText(text);
			}

			for (std::vector<CXTPReportRecordItem*>::iterator itemIterator = items.begin(); itemIterator != items.end(); ++itemIterator)
			{
				CXTPReportRecordItem* item = *itemIterator;
				item = (item ? item : new CXTPReportRecordItemText());
				if (entry)
					entry->AddItem(item);
			}

			if (entry)
				m_wndReport.AddRecord(entry);
		}

		m_wndReport.EndUpdate();
		m_wndReport.Populate();
	}
}

