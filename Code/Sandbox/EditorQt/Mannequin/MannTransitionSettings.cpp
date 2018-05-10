// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MannTransitionSettings.h"
#include "MannequinDialog.h"

#include <ICryMannequin.h>
#include <CryGame/IGameFramework.h>
#include <ICryMannequinEditor.h>

BEGIN_MESSAGE_MAP(CMannTransitionSettingsDlg, CDialog)
ON_BN_CLICKED(IDOK, &CMannTransitionSettingsDlg::OnOk)
ON_CBN_SELENDOK(IDC_FRAGMENTID_FROM_COMBO, &CMannTransitionSettingsDlg::OnComboChange)
ON_CBN_SELENDOK(IDC_FRAGMENTID_TO_COMBO, &CMannTransitionSettingsDlg::OnComboChange)
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
CMannTransitionSettingsDlg::CMannTransitionSettingsDlg(const CString& windowCaption, FragmentID& fromFragID, FragmentID& toFragID, SFragTagState& fromFragTag, SFragTagState& toFragTag, CWnd* pParent)
	: CXTResizeDialog(IDD_MANN_TRANSITIONSETTINGS, pParent), m_IDFrom(fromFragID), m_IDTo(toFragID), m_TagsFrom(fromFragTag), m_TagsTo(toFragTag)
{
	m_windowCaption = windowCaption;
}

//////////////////////////////////////////////////////////////////////////
CMannTransitionSettingsDlg::~CMannTransitionSettingsDlg()
{
}

//////////////////////////////////////////////////////////////////////////
void CMannTransitionSettingsDlg::DoDataExchange(CDataExchange* pDX)
{
	__super::DoDataExchange(pDX);

	DDX_Control(pDX, IDC_FRAGMENTID_FROM_COMBO, m_fromComboBox);
	DDX_Control(pDX, IDC_FRAGMENTID_TO_COMBO, m_toComboBox);
}

//////////////////////////////////////////////////////////////////////////
BOOL CMannTransitionSettingsDlg::OnInitDialog()
{
	__super::OnInitDialog();

	SetWindowText(m_windowCaption);

	m_tagFromVars.reset(new CVarBlock());
	m_tagsFromPanel.SetParent(this);
	m_tagsFromPanel.MoveWindow(10, 43, 325, 380);
	m_tagsFromPanel.ShowWindow(SW_SHOW);

	m_tagToVars.reset(new CVarBlock());
	m_tagsToPanel.SetParent(this);
	m_tagsToPanel.MoveWindow(437, 43, 325, 380);
	m_tagsToPanel.ShowWindow(SW_SHOW);

	PopulateControls();

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
void CMannTransitionSettingsDlg::UpdateFragmentIDs()
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
void CMannTransitionSettingsDlg::PopulateComboBoxes()
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
	}

	if (TAG_ID_INVALID != m_IDTo)
	{
		m_toComboBox.SelectString(-1, pControllerDef->m_fragmentIDs.GetTagName(m_IDTo));
	}
}

//////////////////////////////////////////////////////////////////////////
void CMannTransitionSettingsDlg::PopulateTagPanels()
{
	// Setup the combobox lookup values
	CMannequinDialog* pMannequinDialog = CMannequinDialog::GetCurrentInstance();
	SMannequinContexts* pContexts = pMannequinDialog->Contexts();
	const SControllerDef* pControllerDef = pContexts->m_controllerDef;

	UpdateFragmentIDs();

	// Tags
	m_tagFromVars->DeleteAllVariables();
	m_tagFromControls.Init(m_tagFromVars, pControllerDef->m_tags);
	m_tagFromControls.Set(m_TagsFrom.globalTags);

	const CTagDefinition* pFromFragTags = (m_IDFrom == FRAGMENT_ID_INVALID) ? NULL : pControllerDef->GetFragmentTagDef(m_IDFrom);
	if (pFromFragTags)
	{
		m_tagFromFragControls.Init(m_tagFromVars, *pFromFragTags);
		m_tagFromFragControls.Set(m_TagsFrom.fragmentTags);
	}

	m_tagsFromPanel.DeleteVars();
	m_tagsFromPanel.SetVarBlock(m_tagFromVars.get(), NULL, false);

	m_tagToVars->DeleteAllVariables();
	m_tagToControls.Init(m_tagToVars, pControllerDef->m_tags);
	m_tagToControls.Set(m_TagsTo.globalTags);

	const CTagDefinition* pToFragTags = (m_IDTo == FRAGMENT_ID_INVALID) ? NULL : pControllerDef->GetFragmentTagDef(m_IDTo);
	if (pToFragTags)
	{
		m_tagToFragControls.Init(m_tagToVars, *pToFragTags);
		m_tagToFragControls.Set(m_TagsTo.fragmentTags);
	}

	m_tagsToPanel.DeleteVars();
	m_tagsToPanel.SetVarBlock(m_tagToVars.get(), NULL, false);
}

//////////////////////////////////////////////////////////////////////////
void CMannTransitionSettingsDlg::PopulateControls()
{
	PopulateComboBoxes();
	PopulateTagPanels();
}

//////////////////////////////////////////////////////////////////////////
void CMannTransitionSettingsDlg::OnOk()
{
	UpdateFragmentIDs();

	// retrieve per-detail values now
	m_TagsFrom.globalTags = m_tagFromControls.Get();
	m_TagsFrom.fragmentTags = m_tagFromFragControls.Get();

	m_TagsTo.globalTags = m_tagToControls.Get();
	m_TagsTo.fragmentTags = m_tagToFragControls.Get();

	// if everything is ok with the data entered.
	__super::OnOK();
}

void CMannTransitionSettingsDlg::OnComboChange()
{
	UpdateFragmentIDs();
	PopulateTagPanels();
}

