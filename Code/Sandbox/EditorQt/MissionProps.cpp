// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "MissionProps.h"
#include "CryEditDoc.h"
#include "mission.h"
#include "missionscript.h"
#include <FilePathUtil.h>
#include "Controls/PropertyItem.h"

// CMissionProps dialog

IMPLEMENT_DYNAMIC(CMissionProps, CPropertyPage)
CMissionProps::CMissionProps()
	: CPropertyPage(CMissionProps::IDD)
{
}

CMissionProps::~CMissionProps()
{
}

void CMissionProps::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_SCRIPT, m_btnScript);
	DDX_Control(pDX, IDC_EQUIPPACK, m_btnEquipPack);
	DDX_Control(pDX, IDC_METHODS, m_lstMethods);
	DDX_Control(pDX, IDC_EVENTS, m_lstEvents);
}

BEGIN_MESSAGE_MAP(CMissionProps, CPropertyPage)
ON_BN_CLICKED(IDC_EQUIPPACK, OnBnClickedEquippack)
ON_BN_CLICKED(IDC_BROWSE, OnBnClickedBrowse)
ON_BN_CLICKED(IDC_REMOVE, OnBnClickedRemove)
ON_BN_CLICKED(IDC_CREATE, OnBnClickedCreate)
ON_BN_CLICKED(IDC_EDIT, OnBnClickedEdit)
ON_BN_CLICKED(IDC_RELOADSCRIPT, OnBnClickedReload)
END_MESSAGE_MAP()

void CMissionProps::LoadScript(CString sFilename)
{
	CMissionScript* pScript = GetIEditorImpl()->GetDocument()->GetCurrentMission()->GetScript();
	pScript->SetScriptFile(sFilename);
	pScript->Load();
	m_btnScript.SetWindowText(sFilename);
	m_lstMethods.ResetContent();
	for (int i = 0; i < pScript->GetMethodCount(); i++)
	{
		m_lstMethods.AddString(pScript->GetMethod(i));
	}
	m_lstEvents.ResetContent();
	for (int i = 0; i < pScript->GetEventCount(); i++)
	{
		m_lstEvents.AddString(pScript->GetEvent(i));
	}
}

// CMissionProps message handlers

void CMissionProps::OnBnClickedEquippack()
{
	CString sText;
	string newValue;
	m_btnEquipPack.GetWindowText(sText);

	if (!GetIEditorImpl()->EditDeprecatedProperty(ePropertyEquip, sText.GetString(), newValue))
		return;

	m_btnEquipPack.SetWindowText(newValue.GetString());
	GetIEditorImpl()->GetDocument()->GetCurrentMission()->SetPlayerEquipPack(newValue.GetString());
}

BOOL CMissionProps::OnInitDialog()
{
	CPropertyPage::OnInitDialog();

	LoadScript(GetIEditorImpl()->GetDocument()->GetCurrentMission()->GetScript()->GetFilename());
	m_btnScript.SetWindowText(GetIEditorImpl()->GetDocument()->GetCurrentMission()->GetScript()->GetFilename());
	m_btnEquipPack.SetWindowText(GetIEditorImpl()->GetDocument()->GetCurrentMission()->GetPlayerEquipPack());
	m_btnEquipPack.ShowWindow(SW_HIDE);

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

//////////////////////////////////////////////////////////////////////////
void CMissionProps::OnBnClickedBrowse()
{
	CString file;
	char szFilters[] = "Script Files (*.lua)|*.lua||";
	if (CFileUtil::SelectSingleFile(EFILE_TYPE_ANY, file, szFilters, "Scripts"))
	{
		LoadScript(file);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMissionProps::OnBnClickedRemove()
{
	LoadScript("");
}

//////////////////////////////////////////////////////////////////////////
void CMissionProps::OnBnClickedCreate()
{
	CString file;
	char szFilters[] = "Script Files (*.lua)|*.lua||";
	//CFileDialog Dlg(FALSE, "lua", CString(GetIEditorImpl()->GetMasterCDFolder())+"scripts\\*.lua", OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR|OFN_ENABLESIZING|OFN_EXPLORER, szFilters);
	if (CFileUtil::SelectSaveFile(szFilters, "*.lua", "Scripts", file))
	{
		// copy from template...
		if (!CopyFile("Editor\\MissionTemplate.lua", file, FALSE))
		{
			Warning(CString("MissionScript-Template (expected in ") + "Editor\\MissionTemplate.lua" + ") couldn't be found !");
			return;
		}
		SetFileAttributes(file, FILE_ATTRIBUTE_NORMAL);
		file = PathUtil::AbsolutePathToCryPakPath(file.GetString());
		LoadScript(file);
	}
}

void CMissionProps::OnBnClickedReload()
{
	CString sName = GetIEditorImpl()->GetDocument()->GetCurrentMission()->GetScript()->GetFilename();
	if (sName.GetLength())
		LoadScript(sName);
}

void CMissionProps::OnBnClickedEdit()
{
	GetIEditorImpl()->GetDocument()->GetCurrentMission()->GetScript()->Edit();
}

