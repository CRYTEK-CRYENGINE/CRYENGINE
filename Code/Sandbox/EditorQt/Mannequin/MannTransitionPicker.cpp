// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MannTransitionPicker.h"
#include "MannequinDialog.h"

#include <ICryMannequin.h>
#include <CryGame/IGameFramework.h>
#include <ICryMannequinEditor.h>

BEGIN_MESSAGE_MAP(CMannTransitionPickerDlg, CDialog)
ON_BN_CLICKED(IDOK, &CMannTransitionPickerDlg::OnOk)
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
CMannTransitionPickerDlg::CMannTransitionPickerDlg(FragmentID& fromFragID, FragmentID& toFragID, SFragTagState& fromFragTag, SFragTagState& toFragTag, CWnd* pParent)
	: CXTResizeDialog(IDD_MANN_TRANSITIONPICKER, pParent), m_IDFrom(fromFragID), m_IDTo(toFragID), m_TagsFrom(fromFragTag), m_TagsTo(toFragTag)
{
}

//////////////////////////////////////////////////////////////////////////
CMannTransitionPickerDlg::~CMannTransitionPickerDlg()
{
}

//////////////////////////////////////////////////////////////////////////
void CMannTransitionPickerDlg::DoDataExchange(CDataExchange* pDX)
{
	__super::DoDataExchange(pDX);

	DDX_Control(pDX, IDC_FRAGMENTID_FROM_COMBO, m_fromComboBox);
	DDX_Control(pDX, IDC_FRAGMENTID_TO_COMBO, m_toComboBox);
}

//////////////////////////////////////////////////////////////////////////
BOOL CMannTransitionPickerDlg::OnInitDialog()
{
	__super::OnInitDialog();

	PopulateComboBoxes();

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
void CMannTransitionPickerDlg::UpdateFragmentIDs()
{
	// Setup the combobox lookup values
	CMannequinDialog* pMannequinDialog = CMannequinDialog::GetCurrentInstance();
	SMannequinContexts* pContexts = pMannequinDialog->Contexts();
	const SControllerDef* pControllerDef = pContexts->m_controllerDef;

	// retrieve per-detail values now
	{
		const int selIndex = m_fromComboBox.GetCurSel();
		CString fromName;
		if (selIndex >= 0 && selIndex < m_fromComboBox.GetCount())
		{
			m_fromComboBox.GetLBText(selIndex, fromName);
			m_IDFrom = pControllerDef->m_fragmentIDs.Find(fromName.GetString());
		}
	}

	{
		const int selIndex = m_toComboBox.GetCurSel();
		CString fromName;
		if (selIndex >= 0 && selIndex < m_toComboBox.GetCount())
		{
			m_toComboBox.GetLBText(selIndex, fromName);
			m_IDTo = pControllerDef->m_fragmentIDs.Find(fromName.GetString());
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMannTransitionPickerDlg::PopulateComboBoxes()
{
	CMannequinDialog* pMannequinDialog = CMannequinDialog::GetCurrentInstance();
	SMannequinContexts* pContexts = pMannequinDialog->Contexts();
	const SControllerDef* pControllerDef = pContexts->m_controllerDef;

	// figure out what scopes we've got available
	ActionScopes scopesFrom = ACTION_SCOPES_NONE;
	ActionScopes scopesTo = ACTION_SCOPES_NONE;
	if (TAG_ID_INVALID != m_IDFrom)
		scopesFrom = pControllerDef->GetScopeMask(m_IDFrom, m_TagsFrom);
	if (TAG_ID_INVALID != m_IDTo)
		scopesTo = pControllerDef->GetScopeMask(m_IDTo, m_TagsTo);

	const ActionScopes scopes = ((scopesFrom | scopesTo) == ACTION_SCOPES_NONE) ? ACTION_SCOPES_ALL : (scopesFrom | scopesTo);

	// Add two blank strings
	m_fromComboBox.AddString("");
	m_toComboBox.AddString("");

	// populate the combo boxes filtered by the scopes
	const TagID numTags = pControllerDef->m_fragmentIDs.GetNum();
	for (TagID itag = 0; itag < numTags; itag++)
	{
		const bool bValidFromScope = (pControllerDef->GetScopeMask(itag, m_TagsFrom) & scopes) != ACTION_SCOPES_NONE;
		const bool bValidToScope = (pControllerDef->GetScopeMask(itag, m_TagsTo) & scopes) != ACTION_SCOPES_NONE;
		const char* pName = pControllerDef->m_fragmentIDs.GetTagName(itag);

		if (bValidFromScope)
			m_fromComboBox.AddString(pName);
		if (bValidToScope)
			m_toComboBox.AddString(pName);
	}

	// select the current fragment ID if there's a valid one
	if (TAG_ID_INVALID != m_IDFrom)
	{
		m_fromComboBox.SelectString(-1, pControllerDef->m_fragmentIDs.GetTagName(m_IDFrom));
		GetDlgItem(IDC_FRAGMENTID_FROM_COMBO)->EnableWindow(false);
	}

	if (TAG_ID_INVALID != m_IDTo)
	{
		m_toComboBox.SelectString(-1, pControllerDef->m_fragmentIDs.GetTagName(m_IDTo));
		GetDlgItem(IDC_FRAGMENTID_TO_COMBO)->EnableWindow(false);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMannTransitionPickerDlg::OnOk()
{
	UpdateFragmentIDs();

	// Validate and provide feedback on what went wrong
	if (m_IDTo == TAG_ID_INVALID && m_IDFrom == TAG_ID_INVALID)
	{
		MessageBox("Please choose fragment IDs to preview the transition to and from.", "Validation", MB_OK | MB_ICONWARNING);
	}
	else if (m_IDFrom == TAG_ID_INVALID)
	{
		MessageBox("Please choose a fragment ID to preview the transition from.", "Validation", MB_OK | MB_ICONWARNING);
	}
	else if (m_IDTo == TAG_ID_INVALID)
	{
		MessageBox("Please choose a fragment ID to preview the transition to.", "Validation", MB_OK | MB_ICONWARNING);
	}
	else
	{
		// if everything is ok with the data entered.
		__super::OnOK();
	}
}

