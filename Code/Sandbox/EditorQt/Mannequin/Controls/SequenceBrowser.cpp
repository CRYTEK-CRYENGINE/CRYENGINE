// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SequenceBrowser.h"
#include "PreviewerPage.h"
#include "Controls/FolderTreeCtrl.h"
#include "../MannequinDialog.h"

IMPLEMENT_DYNAMIC(CSequenceBrowser, CXTResizeFormView)

// File name extension for sequence files
static const CString kSequenceFileNameSpec = "*.xml";

// Tree root element name
static const CString kRootElementName = "Sequences";

static CString GetSequenceFolder()
{
	CString sequenceFolder;

	ICVar* pSequencePathCVar = gEnv->pConsole->GetCVar("mn_sequence_path");
	if (pSequencePathCVar)
	{
		sequenceFolder = PathUtil::GetGameFolder() + "/" + pSequencePathCVar->GetString();
		sequenceFolder = PathUtil::ToUnixPath(PathUtil::RemoveSlash(sequenceFolder.GetString())).c_str();
	}

	return sequenceFolder;
}

//////////////////////////////////////////////////////////////////////////
// CSequenceBrowser
//////////////////////////////////////////////////////////////////////////
CSequenceBrowser::CSequenceBrowser(CWnd* parent, CPreviewerPage& previewerPage) : CXTResizeFormView(CSequenceBrowser::IDD),
	m_previewerPage(previewerPage)
{
	std::vector<CString> folders;
	folders.push_back(GetSequenceFolder());
	m_pTree = new CFolderTreeCtrl(folders, kSequenceFileNameSpec, kRootElementName),
	Create("ClassName", "SequenceBrowser", 0, CRect(0, 0, 100, 100), parent, 0, nullptr);
	ShowWindow(FALSE);
}

CSequenceBrowser::~CSequenceBrowser()
{
	if (m_pTree)
		delete m_pTree;
}

BEGIN_MESSAGE_MAP(CSequenceBrowser, CXTResizeFormView)
ON_NOTIFY(NM_DBLCLK, IDC_TREE, OnTreeDoubleClicked)
ON_COMMAND(IDC_OPEN, OnOpen)
END_MESSAGE_MAP()

void CSequenceBrowser::DoDataExchange(CDataExchange* pDX)
{
	__super::DoDataExchange(pDX);

	DDX_Control(pDX, IDC_TREE, *m_pTree);
}

BOOL CSequenceBrowser::OnInitDialog()
{
	__super::OnInitDialog();

	SetResize(IDC_TREE, SZ_RESIZE(1.0f));
	SetResize(IDC_OPEN, SZ_REPOS(1.0f));

	return TRUE;
}

void CSequenceBrowser::OnTreeDoubleClicked(NMHDR* pNMHDR, LRESULT* pResult)
{
	OnOpen();
}

void CSequenceBrowser::OnOpen()
{
	HTREEITEM selectedItem = m_pTree->GetSelectedItem();

	if (selectedItem == NULL)
		return;

	if (m_pTree->IsFile(selectedItem))
	{
		CString sequencePath = m_pTree->GetPath(selectedItem);
		m_previewerPage.LoadSequence(sequencePath);
		CMannequinDialog::GetCurrentInstance()->GetDockingPaneManager()->ShowPane(CMannequinDialog::IDW_PREVIEWER_PANE);
	}
}

