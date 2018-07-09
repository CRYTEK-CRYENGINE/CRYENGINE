// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "MannAdvancedPasteDialog.h"

//////////////////////////////////////////////////////////////////////////
void CreateTagControl(CTagSelectionControl& tagControl, const int tagControlId, CStatic& hostGroupBox, const CTagDefinition* const pTagDef, const TagState& defaultTagState)
{
	tagControl.Create(IDD_DATABASE, &hostGroupBox);
	tagControl.SetDlgCtrlID(tagControlId);
	CRect rc;
	hostGroupBox.GetClientRect(rc);
	rc.DeflateRect(8, 16, 8, 8);
	::SetWindowPos(tagControl.GetSafeHwnd(), NULL, rc.left, rc.top, rc.Width(), rc.Height(), SWP_NOZORDER);
	tagControl.ShowWindow(SW_SHOW);

	tagControl.SetTagDef(pTagDef);
	tagControl.SetTagState(defaultTagState);
}

//////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CMannAdvancedPasteDialog, CXTResizeDialog)
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
CMannAdvancedPasteDialog::CMannAdvancedPasteDialog(CWnd* const pParent, const IAnimationDatabase& database, const FragmentID fragmentID, const SFragTagState& tagState)
	: CXTResizeDialog(CMannAdvancedPasteDialog::IDD, pParent)
	, m_fragmentID(fragmentID)
	, m_database(database)
	, m_tagState(tagState)
{
}

//////////////////////////////////////////////////////////////////////////
void CMannAdvancedPasteDialog::DoDataExchange(CDataExchange* const pDX)
{
	__super::DoDataExchange(pDX);

	DDX_Control(pDX, IDC_GROUPBOX_GLOBALTAGS, m_globalTagsGroupBox);
	DDX_Control(pDX, IDC_GROUPBOX_FRAGMENTTAGS, m_fragTagsGroupBox);
}

//////////////////////////////////////////////////////////////////////////
BOOL CMannAdvancedPasteDialog::OnInitDialog()
{
	__super::OnInitDialog();

	const stack_string fragmentName = m_database.GetFragmentDefs().GetTagName(m_fragmentID);
	const stack_string windowName = stack_string().Format("Pasting fragments in '%s'", fragmentName.c_str());
	SetWindowText(windowName.c_str());

	// Create the scope alias selection control
	CreateTagControl(m_globalTagsSelectionControl, IDC_GROUPBOX_GLOBALTAGS, m_globalTagsGroupBox, &m_database.GetTagDefs(), m_tagState.globalTags);
	CreateTagControl(m_fragTagsSelectionControl, IDC_GROUPBOX_FRAGMENTTAGS, m_fragTagsGroupBox, m_database.GetTagDefs().GetSubTagDefinition(m_fragmentID), m_tagState.fragmentTags);

	return TRUE;
}

void CMannAdvancedPasteDialog::OnOK()
{
	m_tagState.globalTags = m_globalTagsSelectionControl.GetTagState();
	m_tagState.fragmentTags = m_fragTagsSelectionControl.GetTagState();

	CXTResizeDialog::OnOK();
}

