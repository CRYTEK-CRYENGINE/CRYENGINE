// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "MannTagEditorDialog.h"
#include "MannequinDialog.h"
#include "MannTagDefEditorDialog.h"
#include <ICryMannequinEditor.h>
#include <CryGame/IGameFramework.h>
#include "Controls/QuestionDialog.h"
#include "FilePathUtil.h"

enum
{
	IDC_SCOPE_TAGS_CONTROL = AFX_IDW_PANE_FIRST
};

//////////////////////////////////////////////////////////////////////////
static CString GetNormalizedFilenameString(const CString& filename)
{
	string normalizedFilename = PathUtil::AbsolutePathToGamePath(filename.GetString());
	normalizedFilename = PathUtil::ToUnixPath(normalizedFilename);
	return normalizedFilename.c_str();
}

//////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CMannTagEditorDialog, CXTResizeDialog)
ON_BN_CLICKED(IDC_EDIT_TAGS, OnEditTagDefs)
ON_CBN_SELCHANGE(IDC_FRAGFILE_COMBO, &CMannTagEditorDialog::OnCbnSelchangeFragfileCombo)
ON_BN_CLICKED(IDC_CREATE_ADB_FILE, &CMannTagEditorDialog::OnBnClickedCreateAdbFile)
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
CMannTagEditorDialog::CMannTagEditorDialog(CWnd* pParent, IAnimationDatabase* animDB, FragmentID fragmentID)
	: CXTResizeDialog(CMannTagEditorDialog::IDD, pParent)
	, m_contexts(NULL)
	, m_animDB(animDB)
	, m_fragmentID(fragmentID)
	, m_fragmentName("")
	, m_nOldADBFileIndex(0)
{
}

//////////////////////////////////////////////////////////////////////////
CMannTagEditorDialog::~CMannTagEditorDialog()
{
}

//////////////////////////////////////////////////////////////////////////
void CMannTagEditorDialog::DoDataExchange(CDataExchange* pDX)
{
	__super::DoDataExchange(pDX);

	DDX_Control(pDX, IDC_FRAGMENT_ID_NAME_EDIT, m_nameEdit);
	DDX_Control(pDX, IDC_IS_PERSISTENT, m_btnIsPersistent);
	DDX_Control(pDX, IDC_AUTO_REINSTALL_BEST_MATCH, m_btnAutoReinstall);
	DDX_Control(pDX, IDC_GROUPBOX_SCOPETAGS, m_scopeTagsGroupBox);
	DDX_Control(pDX, IDC_TAGDEF_COMBO, m_tagDefComboBox);
	DDX_Control(pDX, IDC_FRAGFILE_COMBO, m_FragFileComboBox);
}

//////////////////////////////////////////////////////////////////////////
BOOL CMannTagEditorDialog::OnInitDialog()
{
	__super::OnInitDialog();

	m_contexts = CMannequinDialog::GetCurrentInstance()->Contexts();

	CRect rc;

	// Scope selection control
	m_scopeTagsControl.Create(IDD_DATABASE, &m_scopeTagsGroupBox);
	m_scopeTagsControl.SetDlgCtrlID(IDC_SCOPE_TAGS_CONTROL);
	m_scopeTagsGroupBox.GetClientRect(rc);
	rc.DeflateRect(8, 16, 8, 8);
	::SetWindowPos(m_scopeTagsControl.GetSafeHwnd(), NULL, rc.left, rc.top, rc.Width(), rc.Height(), SWP_NOZORDER);
	m_scopeTagsControl.ShowWindow(SW_SHOW);

	// Setup the dialog to edit the FragmentID
	const char* fragmentName = m_animDB->GetFragmentDefs().GetTagName(m_fragmentID);
	m_nameEdit.SetWindowText(fragmentName);

	const SControllerDef* pControllerDef = m_contexts->m_controllerDef;
	const CTagDefinition& tagDef = pControllerDef->m_scopeIDs;
	m_scopeTagsControl.SetTagDef(&tagDef);

	const SFragmentDef& fragmentDef = pControllerDef->GetFragmentDef(m_fragmentID);
	const CTagDefinition* pFragTagDef = pControllerDef->GetFragmentTagDef(m_fragmentID);

	const ActionScopes scopeMask = fragmentDef.scopeMaskList.GetDefault();
	STagState<sizeof(ActionScopes)> tsScopeMask;
	tsScopeMask.SetFromInteger(scopeMask);
	m_scopeTagsControl.SetTagState(STagStateBase(tsScopeMask));

	m_btnIsPersistent.SetCheck((fragmentDef.flags & SFragmentDef::PERSISTENT) != 0);
	m_btnAutoReinstall.SetCheck((fragmentDef.flags & SFragmentDef::AUTO_REINSTALL_BEST_MATCH) != 0);

	InitialiseFragmentTags(pFragTagDef);
	InitialiseFragmentADBs();

	AutoLoadPlacement("Dialogs\\MannequinTagEditor");

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
void CMannTagEditorDialog::AddFragADBInternal(const CString& ADBDisplayName, const CString& normalizedFilename)
{
	m_vFragADBFiles.push_back(SFragmentTagEntry(ADBDisplayName, normalizedFilename));

	int nIndex = m_FragFileComboBox.AddString(ADBDisplayName);
	m_FragFileComboBox.SetItemData(nIndex, m_vFragADBFiles.size() - 1);
}

//////////////////////////////////////////////////////////////////////////
void CMannTagEditorDialog::AddFragADB(const CString& filename)
{
	const CString ADBDisplayName = PathUtil::GetFile(filename);

	for (TEntryContainer::const_iterator it = m_vFragADBFiles.begin(); it != m_vFragADBFiles.end(); ++it)
		if (ADBDisplayName.CompareNoCase(it->displayName) == 0)
			return;

	const CString normalizedFilename = GetNormalizedFilenameString(filename);

	AddFragADBInternal(ADBDisplayName, normalizedFilename);
}

//////////////////////////////////////////////////////////////////////////
void CMannTagEditorDialog::InitialiseFragmentADBsRec(const CString& baseDir)
{
	ICryPak* pCryPak = gEnv->pCryPak;
	_finddata_t fd;

	intptr_t handle = pCryPak->FindFirst(baseDir + "*", &fd);
	if (handle != -1)
	{
		do
		{
			if (fd.name[0] == '.')
			{
				continue;
			}

			CString filename(baseDir + fd.name);
			if (fd.attrib & _A_SUBDIR)
			{
				InitialiseFragmentTagsRec(filename + "/");
			}
			else
			{
				if (filename.Right(4) == ".adb")
				{
					AddFragADB(filename);
				}
			}
		}
		while (pCryPak->FindNext(handle, &fd) >= 0);

		pCryPak->FindClose(handle);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMannTagEditorDialog::InitialiseFragmentADBs()
{
	InitialiseFragmentADBsRec(MANNEQUIN_FOLDER);

	const char* filename = m_animDB->FindSubADBFilenameForID(m_fragmentID);
	const CString cleanName = PathUtil::GetFile(filename);

	TEntryContainer::const_iterator it = m_vFragADBFiles.begin();
	for (; it != m_vFragADBFiles.end(); ++it)
		if (cleanName.CompareNoCase(it->displayName) == 0)
			break;

	if (it != m_vFragADBFiles.end())
	{
		m_nOldADBFileIndex = m_FragFileComboBox.FindStringExact(-1, it->displayName);
		if (m_nOldADBFileIndex != CB_ERR)
			m_FragFileComboBox.SetCurSel(m_nOldADBFileIndex);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMannTagEditorDialog::ResetFragmentADBs()
{
	m_FragFileComboBox.ResetContent();
	m_vFragADBFiles.clear();

	InitialiseFragmentADBs();
}

//////////////////////////////////////////////////////////////////////////
void CMannTagEditorDialog::InitialiseFragmentTagsRec(const CString& baseDir)
{
	ICryPak* pCryPak = gEnv->pCryPak;
	_finddata_t fd;

	intptr_t handle = pCryPak->FindFirst(baseDir + "*.xml", &fd);
	if (handle != -1)
	{
		do
		{
			if (fd.name[0] == '.')
			{
				continue;
			}

			CString filename(baseDir + fd.name);
			if (fd.attrib & _A_SUBDIR)
			{
				InitialiseFragmentTagsRec(filename + "/");
			}
			else
			{
				if (XmlNodeRef root = GetISystem()->LoadXmlFromFile(filename))
				{
					// Ignore non-tagdef XML files
					CString tag = CString(root->getTag());
					if (tag == "TagDefinition")
					{
						IAnimationDatabaseManager& db = gEnv->pGameFramework->GetMannequinInterface().GetAnimationDatabaseManager();
						const CTagDefinition* pTagDef = db.LoadTagDefs(filename, true);
						if (pTagDef)
						{
							AddTagDef(pTagDef->GetFilename());
						}
					}
				}
			}
		}
		while (pCryPak->FindNext(handle, &fd) >= 0);

		pCryPak->FindClose(handle);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMannTagEditorDialog::InitialiseFragmentTags(const CTagDefinition* pTagDef)
{
	m_entries.clear();
	AddTagDefInternal("", "");

	InitialiseFragmentTagsRec(MANNEQUIN_FOLDER);

	SelectTagDefByTagDef(pTagDef);
}

//////////////////////////////////////////////////////////////////////////
void CMannTagEditorDialog::OnOK()
{
	IMannequin& mannequinSys = gEnv->pGameFramework->GetMannequinInterface();
	IMannequinEditorManager* pManEditMan = mannequinSys.GetMannequinEditorManager();
	assert(pManEditMan);

	m_nameEdit.GetWindowText(m_fragmentName);

	const CString commonErrorMessage = "Failed to create FragmentID with name '" + m_fragmentName + "'.\n  Reason:\n\n  ";

	EModifyFragmentIdResult result = pManEditMan->RenameFragmentID(m_animDB->GetFragmentDefs(), m_fragmentID, m_fragmentName);

	if (result != eMFIR_Success)
	{
		switch (result)
		{
		case eMFIR_DuplicateName:
			CQuestionDialog::SCritical(QObject::tr(""), QObject::tr(commonErrorMessage + "There is already a FragmentID with this name."));
			break;

		case eMFIR_InvalidNameIdentifier:
			CQuestionDialog::SCritical(QObject::tr(""), QObject::tr(commonErrorMessage + "Invalid name identifier for a FragmentID.\n  A valid name can only use a-Z, A-Z, 0-9 and _ characters and cannot start with a digit."));
			break;

		case eMFIR_UnknownInputTagDefinition:
			CQuestionDialog::SCritical(QObject::tr(""), QObject::tr(commonErrorMessage + "Unknown input tag definition. Your changes cannot be saved."));
			break;

		case eMFIR_InvalidFragmentId:
			CQuestionDialog::SCritical(QObject::tr(""), QObject::tr(commonErrorMessage + "Invalid FragmentID. Your changes cannot be saved."));
			break;

		default:
			assert(false);
			break;
		}
		return;
	}

	ProcessFragmentTagChanges();
	ProcessFlagChanges();
	ProcessScopeTagChanges();

	// All validation has passed so let the dialog close
	__super::OnOK();
}

//////////////////////////////////////////////////////////////////////////
const CString CMannTagEditorDialog::GetCurrentADBFileSel() const
{
	int nIndex = m_FragFileComboBox.GetCurSel();
	if (nIndex == CB_ERR)
		return CString();

	if (m_nOldADBFileIndex == nIndex)
		return CString();

	return m_vFragADBFiles[m_FragFileComboBox.GetItemData(nIndex)].filename;
}

//////////////////////////////////////////////////////////////////////////
void CMannTagEditorDialog::ProcessFragmentADBChanges()
{
	const CString sADBFileName = GetCurrentADBFileSel();

	if (!sADBFileName || !sADBFileName[0])
		return;

	IMannequin& mannequinSys = gEnv->pGameFramework->GetMannequinInterface();
	IMannequinEditorManager* pMannequinEditorManager = mannequinSys.GetMannequinEditorManager();
	CRY_ASSERT(pMannequinEditorManager);

	const char* const currentSubADBFragmentFilterFilename = m_animDB->FindSubADBFilenameForID(m_fragmentID);
	if (currentSubADBFragmentFilterFilename && currentSubADBFragmentFilterFilename[0])
	{
		pMannequinEditorManager->RemoveSubADBFragmentFilter(m_animDB, currentSubADBFragmentFilterFilename, m_fragmentID);
	}
	pMannequinEditorManager->AddSubADBFragmentFilter(m_animDB, sADBFileName, m_fragmentID);

	CMannequinDialog::GetCurrentInstance()->FragmentBrowser()->SetADBFileNameTextToCurrent();
}

//////////////////////////////////////////////////////////////////////////
void CMannTagEditorDialog::ProcessFragmentTagChanges()
{
	const SControllerDef* pControllerDef = m_contexts->m_controllerDef;
	assert(pControllerDef);

	IMannequin& mannequinSys = gEnv->pGameFramework->GetMannequinInterface();

	const CString tagsFilename = GetSelectedTagDefFilename();
	const bool hasFilename = (tagsFilename && tagsFilename[0]);
	const CTagDefinition* pTagDef = hasFilename ? mannequinSys.GetAnimationDatabaseManager().LoadTagDefs(tagsFilename, true) : NULL;
	const CTagDefinition* pCurFragFragTagDef = pControllerDef->GetFragmentTagDef(m_fragmentID);

	if (pTagDef == pCurFragFragTagDef)
		return;

	IMannequinEditorManager* pManEditMan = mannequinSys.GetMannequinEditorManager();
	assert(pManEditMan);

	pManEditMan->SetFragmentTagDef(pControllerDef->m_fragmentIDs, m_fragmentID, pTagDef);
}

//////////////////////////////////////////////////////////////////////////
void CMannTagEditorDialog::ProcessFlagChanges()
{
	const SControllerDef* pControllerDef = m_contexts->m_controllerDef;
	assert(pControllerDef);
	SFragmentDef fragmentDef = pControllerDef->GetFragmentDef(m_fragmentID);

	IMannequin& mannequinSys = gEnv->pGameFramework->GetMannequinInterface();

	uint8 flags = 0;
	if (m_btnIsPersistent.GetCheck())
	{
		flags |= SFragmentDef::PERSISTENT;
	}
	if (m_btnAutoReinstall.GetCheck())
	{
		flags |= SFragmentDef::AUTO_REINSTALL_BEST_MATCH;
	}

	if (flags == fragmentDef.flags)
		return;

	fragmentDef.flags = flags;

	IMannequinEditorManager* pManEditMan = mannequinSys.GetMannequinEditorManager();
	assert(pManEditMan);

	pManEditMan->SetFragmentDef(*pControllerDef, m_fragmentID, fragmentDef);
}

//////////////////////////////////////////////////////////////////////////
void CMannTagEditorDialog::ProcessScopeTagChanges()
{
	const ActionScopes scopeMask = m_contexts->m_controllerDef->GetFragmentDef(m_fragmentID).scopeMaskList.GetDefault();
	const ActionScopes scopeTags = *((ActionScopes*)(&m_scopeTagsControl.GetTagState()));

	if (scopeMask == scopeTags)
		return;

	IMannequin& mannequinSys = gEnv->pGameFramework->GetMannequinInterface();
	IMannequinEditorManager* pManEditMan = mannequinSys.GetMannequinEditorManager();

	assert(pManEditMan);
	assert(m_contexts->m_controllerDef);

	SFragmentDef fragmentDef = m_contexts->m_controllerDef->GetFragmentDef(m_fragmentID);
	fragmentDef.scopeMaskList.Insert(SFragTagState(), scopeTags);

	pManEditMan->SetFragmentDef(*m_contexts->m_controllerDef, m_fragmentID, fragmentDef);

	CMannequinDialog::GetCurrentInstance()->FragmentEditor()->TrackPanel()->SetCurrentFragment();
}

//////////////////////////////////////////////////////////////////////////
void CMannTagEditorDialog::OnEditTagDefs()
{
	const int selIndex = m_tagDefComboBox.GetCurSel();
	CString displayName;
	if (selIndex >= 0 && selIndex < m_tagDefComboBox.GetCount())
	{
		m_tagDefComboBox.GetLBText(selIndex, displayName);
	}
	CMannTagDefEditorDialog dialog(displayName);
	dialog.DoModal();

	// Update the list of tag defs in the combo box in case new ones have been created
	m_tagDefComboBox.ResetContent();
	m_entries.clear();
	AddTagDefInternal("", "");

	IMannequinEditorManager* pManEditMan = gEnv->pGameFramework->GetMannequinInterface().GetMannequinEditorManager();
	DynArray<const CTagDefinition*> tagDefs;
	pManEditMan->GetLoadedTagDefs(tagDefs);

	for (DynArray<const CTagDefinition*>::const_iterator it = tagDefs.begin(), itEnd = tagDefs.end(); it != itEnd; ++it)
	{
		AddTagDef((*it)->GetFilename());
	}

	const CTagDefinition* pFragTagDef = m_contexts->m_controllerDef->GetFragmentTagDef(m_fragmentID);
	SelectTagDefByTagDef(pFragTagDef);
}

//////////////////////////////////////////////////////////////////////////
void CMannTagEditorDialog::AddTagDef(const CString& filename)
{
	const CString tagDefDisplayName = PathUtil::GetFile(filename);

	if (ContainsTagDefDisplayName(tagDefDisplayName))
		return;

	// Only add filenames with "tag" in it
	const bool isFragTagDefinitionFile = (strstri(filename, "tag") != 0);
	if (!isFragTagDefinitionFile)
		return;

	const CString normalizedFilename = GetNormalizedFilenameString(filename);

	IMannequin& mannequinSys = gEnv->pGameFramework->GetMannequinInterface();
	if (!mannequinSys.GetAnimationDatabaseManager().LoadTagDefs(filename, true))
		return;

	AddTagDefInternal(tagDefDisplayName, normalizedFilename);
}

//////////////////////////////////////////////////////////////////////////
void CMannTagEditorDialog::AddTagDefInternal(const CString& tagDefDisplayName, const CString& normalizedFilename)
{
	m_entries.push_back(SFragmentTagEntry(tagDefDisplayName, normalizedFilename));

	int nIndex = m_tagDefComboBox.AddString(tagDefDisplayName);
	m_tagDefComboBox.SetItemData(nIndex, m_entries.size() - 1);
}

//////////////////////////////////////////////////////////////////////////
bool CMannTagEditorDialog::ContainsTagDefDisplayName(const CString& tagDefDisplayNameToSearch) const
{
	const size_t entriesCount = m_entries.size();
	for (size_t i = 0; i < entriesCount; ++i)
	{
		const SFragmentTagEntry& entry = m_entries[i];
		if (tagDefDisplayNameToSearch.CompareNoCase(entry.displayName) == 0)
			return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
const CString CMannTagEditorDialog::GetSelectedTagDefFilename() const
{
	int nIndex = m_tagDefComboBox.GetCurSel();

	if (nIndex != CB_ERR)
		return m_entries[m_tagDefComboBox.GetItemData(nIndex)].filename;

	return "";
}

//////////////////////////////////////////////////////////////////////////
void CMannTagEditorDialog::SelectTagDefByFilename(const CString& filename)
{
	const CString normalizedFilename = GetNormalizedFilenameString(filename);

	const size_t entriesCount = m_entries.size();
	for (size_t i = 0; i < entriesCount; ++i)
	{
		const SFragmentTagEntry& entry = m_entries[i];
		if (normalizedFilename.CompareNoCase(entry.filename) == 0)
		{
			int nIndex = m_tagDefComboBox.FindStringExact(-1, entry.displayName);
			if (nIndex != CB_ERR)
				m_tagDefComboBox.SetCurSel(nIndex);
			return;
		}
	}

	m_tagDefComboBox.SetCurSel(0);
}

//////////////////////////////////////////////////////////////////////////
void CMannTagEditorDialog::SelectTagDefByTagDef(const CTagDefinition* pTagDef)
{
	if (!pTagDef)
	{
		m_tagDefComboBox.SetCurSel(0);
		return;
	}

	SelectTagDefByFilename(pTagDef->GetFilename());
}

void CMannTagEditorDialog::OnCbnSelchangeFragfileCombo()
{
	ProcessFragmentADBChanges();
}

void CMannTagEditorDialog::OnBnClickedCreateAdbFile()
{
	const char* fragName = (m_fragmentID != FRAGMENT_ID_INVALID) ? m_animDB->GetFragmentDefs().GetTagName(m_fragmentID) : "NoFragment";
	CFileDialog NewFileDiag
	(
	  false,
	  ".adb",
	  fragName,
	  OFN_ENABLESIZING | OFN_EXPLORER | OFN_PATHMUSTEXIST,
	  "ADB Files (*.adb)|*.adb; *.adb|All Files (*.*)|*.*||",
	  this
	);
	NewFileDiag.m_ofn.lpstrInitialDir = MANNEQUIN_FOLDER;

	if (IDOK == NewFileDiag.DoModal())
	{
		CString sADBFileName = NewFileDiag.GetFileName();
		sADBFileName.MakeLower();
		const CString sADBFolder = NewFileDiag.GetFolderPath();
		if (!sADBFileName || !sADBFileName[0])
			return;

		const CString sRealFileName = sADBFolder + "\\" + sADBFileName;

		CFile file(sRealFileName.GetString(), CFile::modeCreate | CFile::modeNoTruncate);
		file.Close();

		const CString normalizedFilename = GetNormalizedFilenameString(MANNEQUIN_FOLDER + sADBFileName);
		m_animDB->RemoveSubADBFragmentFilter(m_fragmentID);
		m_animDB->AddSubADBFragmentFilter(normalizedFilename.GetString(), m_fragmentID);

		ResetFragmentADBs();
	}
}

