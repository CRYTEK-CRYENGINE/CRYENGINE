// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MannNewSubADBFilterDialog.h"
#include "MannequinDialog.h"

#include <ICryMannequin.h>
#include <CryGame/IGameFramework.h>
#include <ICryMannequinEditor.h>

float Edit2f(const CEdit& edit)
{
	CString tempString;
	edit.GetWindowText(tempString);
	return atof(tempString);
}

namespace
{
const char* adb_noDB = "<no animation database (adb)>";
const char* adb_DBext = ".adb";
};

BEGIN_MESSAGE_MAP(CMannNewSubADBFilterDialog, CDialog)
ON_BN_CLICKED(IDOK, &CMannNewSubADBFilterDialog::OnOk)
ON_BN_CLICKED(IDC_FRAG2USED, &CMannNewSubADBFilterDialog::OnFrag2Used)
ON_BN_CLICKED(IDC_USED2FRAG, &CMannNewSubADBFilterDialog::OnUsed2Frag)

ON_EN_CHANGE(IDC_NAME_EDIT, &CMannNewSubADBFilterDialog::OnFilenameChanged)
ON_NOTIFY(LVN_ITEMCHANGED, IDC_FRAG_ID_LIST, &CMannNewSubADBFilterDialog::OnFragListItemChanged)
ON_NOTIFY(LVN_ITEMCHANGED, IDC_FRAG_USED_ID_LIST, &CMannNewSubADBFilterDialog::OnUsedListItemChanged)

END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
CMannNewSubADBFilterDialog::CMannNewSubADBFilterDialog(SMiniSubADB* pData, IAnimationDatabase* animDB, const string& parentName, const EContextDialogModes mode, CWnd* pParent)
	: CXTResizeDialog(IDD_MANN_SUBADB_FILTER_EDITOR, pParent), m_mode(mode), m_pDataCopy(nullptr), m_animDB(animDB), m_parentName(parentName)
{
	switch (m_mode)
	{
	case eContextDialog_New:
		m_pData = new SMiniSubADB;
		m_pData->filename = adb_noDB;
		break;
	case eContextDialog_Edit:
		{
			assert(pData);
			m_pData = pData;
			CopySubADBData(pData);
			break;
		}
	case eContextDialog_CloneAndEdit:
		assert(pData);
		m_pData = new SMiniSubADB;
		m_pData->vFragIDs.clear();
		CloneSubADBData(pData);
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
CMannNewSubADBFilterDialog::~CMannNewSubADBFilterDialog()
{
	if (m_mode == eContextDialog_Edit)
	{
		m_pData = nullptr;
	}
	else
	{
		SAFE_DELETE(m_pData); // ?? not sure if i want to be doing this, revist after adding new contexts to the dialog
	}
	SAFE_DELETE(m_pDataCopy);
}

//////////////////////////////////////////////////////////////////////////
void CMannNewSubADBFilterDialog::DoDataExchange(CDataExchange* pDX)
{
	__super::DoDataExchange(pDX);

	DDX_Control(pDX, IDOK, m_okButton);
	DDX_Control(pDX, IDC_NAME_EDIT, m_nameEdit);
	DDX_Control(pDX, IDC_FRAG_ID_LIST, m_fragIDList);
	DDX_Control(pDX, IDC_FRAG_USED_ID_LIST, m_fragUsedIDList);
}

//////////////////////////////////////////////////////////////////////////
BOOL CMannNewSubADBFilterDialog::OnInitDialog()
{
	__super::OnInitDialog();

	m_tagVars.reset(new CVarBlock());
	m_tagsPanel.SetParent(this);
	m_tagsPanel.MoveWindow(500, 33, 325, 320);
	m_tagsPanel.ShowWindow(SW_SHOW);

	m_fragIDList.InsertColumn(0, "Frag ID", LVCFMT_LEFT, LVSCW_AUTOSIZE_USEHEADER);
	m_fragUsedIDList.InsertColumn(0, "Frag ID", LVCFMT_LEFT, LVSCW_AUTOSIZE_USEHEADER);

	PopulateFragIDList();
	PopulateControls();

	switch (m_mode)
	{
	case eContextDialog_Edit:
		SetWindowText(_T("Edit Context"));
		break;
	case eContextDialog_CloneAndEdit:
		SetWindowText(_T("Clone Context"));
		break;
	}

	m_nameEdit.SetFocus();
	EnableControls();

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
void CMannNewSubADBFilterDialog::CloneSubADBData(const SMiniSubADB* pData)
{
	m_pData->tags = pData->tags;
	m_pData->filename = adb_noDB; // don't clone the filename, force the user to replace it.
	m_pData->pFragDef = pData->pFragDef;
	m_pData->vFragIDs = pData->vFragIDs;
	m_pData->vSubADBs = pData->vSubADBs;
}

//////////////////////////////////////////////////////////////////////////
void CMannNewSubADBFilterDialog::CopySubADBData(const SMiniSubADB* pData)
{
	assert(nullptr == m_pDataCopy);
	m_pDataCopy = new SMiniSubADB;
	m_pDataCopy->tags = pData->tags;
	m_pDataCopy->filename = pData->filename;
	m_pDataCopy->pFragDef = pData->pFragDef;
	m_pDataCopy->vFragIDs = pData->vFragIDs;
	m_pDataCopy->vSubADBs = pData->vSubADBs;
}

//////////////////////////////////////////////////////////////////////////
void CMannNewSubADBFilterDialog::OnFilenameChanged()
{
	EnableControls();
}

//////////////////////////////////////////////////////////////////////////
void CMannNewSubADBFilterDialog::EnableControls()
{
	const bool isValidFilename = VerifyFilename();
	m_okButton.EnableWindow(isValidFilename);
}

//////////////////////////////////////////////////////////////////////////
bool CMannNewSubADBFilterDialog::VerifyFilename()
{
	CString tempString;
	m_nameEdit.GetWindowText(tempString);
	const string testString(tempString);
	return (testString != adb_noDB) && (testString.Right(4) == adb_DBext) && (testString.size() > 4);
}

//////////////////////////////////////////////////////////////////////////
void CMannNewSubADBFilterDialog::PopulateControls()
{
	// Setup the combobox lookup values
	CMannequinDialog* pMannequinDialog = CMannequinDialog::GetCurrentInstance();
	SMannequinContexts* pContexts = pMannequinDialog->Contexts();
	const SControllerDef* pControllerDef = pContexts->m_controllerDef;

	// fill in per detail values now
	m_nameEdit.SetWindowText(PathUtil::GetFile(m_pData->filename.c_str()));

	// Tags
	m_tagVars->DeleteAllVariables();
	m_tagControls.Init(m_tagVars, pControllerDef->m_tags);
	m_tagControls.Set(m_pData->tags);

	m_tagsPanel.DeleteVars();
	m_tagsPanel.SetVarBlock(m_tagVars.get(), NULL, false);
}

//////////////////////////////////////////////////////////////////////////
void CMannNewSubADBFilterDialog::PopulateFragIDList()
{
	m_fragUsedIDList.DeleteAllItems();
	m_fragUsedIDList.SetRedraw(FALSE);

	if (m_pData)
	{
		int nItem = m_fragUsedIDList.GetItemCount();

		for (SMiniSubADB::TFragIDArray::const_iterator it = m_pData->vFragIDs.begin(); it != m_pData->vFragIDs.end(); ++it)
		{
			CString text(m_pData->pFragDef->GetTagName(*it));
			m_fragUsedIDList.InsertItem(nItem, text);
		}
	}

	m_fragUsedIDList.SetColumnWidth(0, LVSCW_AUTOSIZE_USEHEADER);
	m_fragUsedIDList.SetRedraw(TRUE);

	m_fragIDList.DeleteAllItems();
	m_fragIDList.SetRedraw(FALSE);

	if (m_animDB)
	{
		const CTagDefinition& fragDefs = m_animDB->GetFragmentDefs();
		const uint32 numFrags = fragDefs.GetNum();

		int nItem = m_fragIDList.GetItemCount();
		for (uint32 i = 0; i < numFrags; i++)
		{
			if (!m_pData || std::find(m_pData->vFragIDs.begin(), m_pData->vFragIDs.end(), i) == m_pData->vFragIDs.end())
			{
				const string fragTagName = fragDefs.GetTagName(i);
				m_fragIDList.InsertItem(nItem, fragTagName);
			}
		}
	}

	m_fragIDList.SetColumnWidth(0, LVSCW_AUTOSIZE_USEHEADER);
	m_fragIDList.SetRedraw(TRUE);
}

//////////////////////////////////////////////////////////////////////////
void CMannNewSubADBFilterDialog::OnOk()
{
	// some of m_pData members are of type string (CryString<char>) so need to be retrieved via a temporary
	CString tempString;

	// retrieve per-detail values now
	m_nameEdit.GetWindowText(tempString);
	m_pData->filename = tempString;

	// verify that the string is ok - don't have to really since button should be disabled if its not
	assert(m_pData->filename != adb_noDB);

	m_pData->tags = m_tagControls.Get();

	// about to rebuild this so delete the old
	m_pData->vFragIDs.clear();
	const int nItems = m_fragUsedIDList.GetItemCount();
	for (int i = 0; i < nItems; ++i)
	{
		const CString fragmentIdName = m_fragUsedIDList.GetItemText(i, 0);
		const FragmentID fragId = m_animDB->GetFragmentID(fragmentIdName);
		m_pData->vFragIDs.push_back(fragId);
	}

	string copyOfFilename = m_pData->filename;
	const string pathToADBs("Animations/Mannequin/ADB/");
	const size_t pos = m_pData->filename.find(pathToADBs);
	if (!VerifyFilename())
	{
		copyOfFilename += adb_DBext;
	}
	const string finalFilename = (pos >= copyOfFilename.size()) ? (pathToADBs + copyOfFilename) : copyOfFilename;

	switch (m_mode)
	{
	case eContextDialog_CloneAndEdit:// fall through
	case eContextDialog_New:
		SaveNewSubADBFile(finalFilename);
		break;
	case eContextDialog_Edit:
		// delete current SubADB
		m_animDB->ClearSubADBFilter(m_pDataCopy->filename);

		// save "new" SubADB as above
		SaveNewSubADBFile(finalFilename);
		break;
	}

	// if everything is ok with the data entered.
	__super::OnOK();
}

//////////////////////////////////////////////////////////////////////////
void CMannNewSubADBFilterDialog::SaveNewSubADBFile(const string& filename)
{
	IMannequin& mannequinSys = gEnv->pGameFramework->GetMannequinInterface();
	IMannequinEditorManager* pMannequinEditorManager = mannequinSys.GetMannequinEditorManager();
	CRY_ASSERT(pMannequinEditorManager);

	m_animDB->AddSubADBTagFilter(m_parentName, filename, TAG_STATE_EMPTY); // TODO: Create SubADB file

	pMannequinEditorManager->SetSubADBTagFilter(m_animDB, filename.c_str(), m_pData->tags);

	// now add the fragment filters
	for (SMiniSubADB::TFragIDArray::const_iterator it = m_pData->vFragIDs.begin(); it != m_pData->vFragIDs.end(); ++it)
	{
		pMannequinEditorManager->AddSubADBFragmentFilter(m_animDB, filename.c_str(), *it);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMannNewSubADBFilterDialog::FindFragmentIDsMissing(SMiniSubADB::TFragIDArray& outFrags, const SMiniSubADB::TFragIDArray& AFrags, const SMiniSubADB::TFragIDArray& BFrags)
{
	// iterate through the "copy"
	for (SMiniSubADB::TFragIDArray::const_iterator Aiter = AFrags.begin(); Aiter != AFrags.end(); ++Aiter)
	{
		bool bFound = false;
		// iterate through the "original"
		for (SMiniSubADB::TFragIDArray::const_iterator Biter = BFrags.begin(); Biter != BFrags.end(); ++Biter)
		{
			if ((*Aiter) == (*Biter))
			{
				bFound = true;
				break;
			}
		}
		// if we didn't find it then it must have been removed
		if (!bFound)
		{
			outFrags.push_back(*Aiter);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMannNewSubADBFilterDialog::OnFragListItemChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
	NMLISTVIEW* pNMIA = reinterpret_cast<NMLISTVIEW*>(pNMHDR);

	if (m_fragIDList.GetSelectedCount() > 0)
		m_selectFraglistIndex = pNMIA->iItem;
	else
		m_selectFraglistIndex = -1;
}

//////////////////////////////////////////////////////////////////////////
void CMannNewSubADBFilterDialog::OnUsedListItemChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
	NMLISTVIEW* pNMIA = reinterpret_cast<NMLISTVIEW*>(pNMHDR);

	if (m_fragUsedIDList.GetSelectedCount() > 0)
		m_selectUsedlistIndex = pNMIA->iItem;
	else
		m_selectUsedlistIndex = -1;
}

//////////////////////////////////////////////////////////////////////////
void CMannNewSubADBFilterDialog::OnFrag2Used()
{
	// do we have a selection?
	if (m_selectFraglistIndex == -1)
		return;

	// grab the selections string
	const CString s1 = m_fragIDList.GetItemText(m_selectFraglistIndex, 0);

	// delete the selection
	m_fragIDList.DeleteItem(m_selectFraglistIndex);

	// add to the used list
	int nItem = m_fragUsedIDList.GetItemCount();
	m_fragUsedIDList.InsertItem(nItem, s1);

	EnableControls();
}

//////////////////////////////////////////////////////////////////////////
void CMannNewSubADBFilterDialog::OnUsed2Frag()
{
	// do we have a selection?
	if (m_selectUsedlistIndex == -1)
		return;

	// grab the selections string
	const CString s1 = m_fragUsedIDList.GetItemText(m_selectUsedlistIndex, 0);

	// delete the selection
	m_fragUsedIDList.DeleteItem(m_selectUsedlistIndex);

	// add to the frag list
	int nItem = m_fragIDList.GetItemCount();
	m_fragIDList.InsertItem(nItem, s1);

	EnableControls();
}

