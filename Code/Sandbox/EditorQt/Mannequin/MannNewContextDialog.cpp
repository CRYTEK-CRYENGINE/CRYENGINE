// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MannNewContextDialog.h"
#include "MannequinDialog.h"

#include "Dialogs/QStringDialog.h"

#include <ICryMannequin.h>
#include <CryGame/IGameFramework.h>
#include <ICryMannequinEditor.h>

float Edit2f(const CEdit& edit);

namespace
{

const char* noDB = "<no animation database (adb)>";
const char* noControllerDef = "<no controller def>";
};

BEGIN_MESSAGE_MAP(CMannNewContextDialog, CDialog)
ON_BN_CLICKED(IDOK, &CMannNewContextDialog::OnOk)
ON_BN_CLICKED(IDC_BROWSEMODEL_BUTTON, &CMannNewContextDialog::OnBrowsemodelButton)
ON_BN_CLICKED(IDC_NEW_ADB_BUTTON, &CMannNewContextDialog::OnNewAdbButton)
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
CMannNewContextDialog::CMannNewContextDialog(SScopeContextData* context, const EContextDialogModes mode, CWnd* pParent)
	: CXTResizeDialog(IDD_MANN_NEW_CONTEXT, pParent), m_mode(mode)
{
	switch (m_mode)
	{
	case eContextDialog_New:
		m_pData = new SScopeContextData;
		break;
	case eContextDialog_Edit:
		assert(context);
		m_pData = context;
		break;
	case eContextDialog_CloneAndEdit:
		assert(context);
		m_pData = new SScopeContextData;
		CloneScopeContextData(context);
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
CMannNewContextDialog::~CMannNewContextDialog()
{
	if (m_mode == eContextDialog_Edit)
	{
		m_pData = nullptr;
	}
	else
	{
		SAFE_DELETE(m_pData); // ?? not sure if i want to be doing this, revist after adding new contexts to the dialog
	}
}

//////////////////////////////////////////////////////////////////////////
void CMannNewContextDialog::DoDataExchange(CDataExchange* pDX)
{
	__super::DoDataExchange(pDX);

	DDX_Control(pDX, IDC_STARTACTIVE_CHECKBOX, m_startActiveCheckBox);
	DDX_Control(pDX, IDC_NAME_EDIT, m_nameEdit);
	DDX_Control(pDX, IDC_CONTEXT_COMBO, m_contextCombo);
	DDX_Control(pDX, IDC_FRAGTAGS_EDIT, m_fragTagsEdit);
	DDX_Control(pDX, IDC_MODEL_EDIT, m_modelEdit);
	DDX_Control(pDX, IDC_BROWSEMODEL_BUTTON, m_browseButton);
	DDX_Control(pDX, IDC_DATABASE_COMBO, m_databaseCombo);
	DDX_Control(pDX, IDC_ATTACHMENTCONTEXT_COMBO, m_attachmentContextCombo);
	DDX_Control(pDX, IDC_ATTACHMENT_EDIT, m_attachmentEdit);
	DDX_Control(pDX, IDC_ATTACHMENTHELPER_EDIT, m_attachmentHelperEdit);
	DDX_Control(pDX, IDC_STARTPOSX_EDIT, m_startPosXEdit);
	DDX_Control(pDX, IDC_STARTPOSY_EDIT, m_startPosYEdit);
	DDX_Control(pDX, IDC_STARTPOSZ_EDIT, m_startPosZEdit);
	DDX_Control(pDX, IDC_STARTROTX_EDIT, m_startRotXEdit);
	DDX_Control(pDX, IDC_STARTROTY_EDIT, m_startRotYEdit);
	DDX_Control(pDX, IDC_STARTROTZ_EDIT, m_startRotZEdit);
	DDX_Control(pDX, IDC_SLAVE_DATABASE_COMBO, m_slaveDatabaseCombo);
	DDX_Control(pDX, IDC_SLAVE_CONTROLLER_DEF_COMBO, m_slaveControllerDefCombo);
}

//////////////////////////////////////////////////////////////////////////
BOOL CMannNewContextDialog::OnInitDialog()
{
	__super::OnInitDialog();

	m_tagVars.reset(new CVarBlock());
	m_tagsPanel.SetParent(this);
	m_tagsPanel.MoveWindow(370, 33, 325, 320);
	m_tagsPanel.ShowWindow(SW_SHOW);

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

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
void CMannNewContextDialog::CloneScopeContextData(const SScopeContextData* context)
{
	// Don't ever start active by default because there should only be one default (i.e. startActive) option
	// If there's an already someone marked as startActive then you won't be able to switch to them.
	m_pData->startActive = false;//context->startActive;
	m_pData->name = "";//context->name;

	// fill in per detail values now
	m_pData->contextID = context->contextID;
	m_pData->fragTags = context->fragTags;

	for (int i = 0; i < eMEM_Max; i++)
	{
		if (context->viewData[i].charInst)
		{
			m_pData->viewData[i].charInst = context->viewData[i].charInst.get();
		}
		else if (context->viewData[i].pStatObj)
		{
			m_pData->viewData[i].pStatObj = context->viewData[i].pStatObj;
		}
		m_pData->viewData[i].enabled = false;
	}

	m_pData->database = context->database;
	m_pData->attachmentContext = context->attachmentContext;
	m_pData->attachment = context->attachment;
	m_pData->attachmentHelper = context->attachmentHelper;
	m_pData->startLocation = context->startLocation;
	m_pData->startRotationEuler = context->startRotationEuler;
	m_pData->pControllerDef = context->pControllerDef;
	m_pData->pSlaveDatabase = context->pSlaveDatabase;

	m_pData->tags = context->tags;
}

//////////////////////////////////////////////////////////////////////////
void CMannNewContextDialog::PopulateControls()
{
	// Setup the combobox lookup values
	CMannequinDialog* pMannequinDialog = CMannequinDialog::GetCurrentInstance();
	SMannequinContexts* pContexts = pMannequinDialog->Contexts();
	const SControllerDef* pControllerDef = pContexts->m_controllerDef;
	if (pControllerDef == NULL)
	{
		return;
	}

	// Context combos
	for (int i = 0; i < pControllerDef->m_scopeContexts.GetNum(); i++)
	{
		m_contextCombo.AddString(pControllerDef->m_scopeContexts.GetTagName(i));
		m_attachmentContextCombo.AddString(pControllerDef->m_scopeContexts.GetTagName(i));
	}
	m_contextCombo.AddString("");
	// additional string that will be ignored by the validation checking in ::OnOK() function
	const char* noAttachmentContext = "<no attachment context>";
	m_attachmentContextCombo.AddString(noAttachmentContext);

	PopulateDatabaseCombo();
	PopulateControllerDefCombo();

	// fill in per detail values now
	m_startActiveCheckBox.SetCheck(m_pData->startActive);
	m_nameEdit.SetWindowText(m_pData->name);
	m_contextCombo.SelectString(0, m_pData->contextID == CONTEXT_DATA_NONE ? "" : pControllerDef->m_scopeContexts.GetTagName(m_pData->contextID));
	m_fragTagsEdit.SetWindowText(m_pData->fragTags);

	if (m_pData->viewData[eMEM_FragmentEditor].charInst)
		m_modelEdit.SetWindowText(m_pData->viewData[eMEM_FragmentEditor].charInst->GetFilePath());
	else if (m_pData->viewData[eMEM_FragmentEditor].pStatObj)
		m_modelEdit.SetWindowText(m_pData->viewData[eMEM_FragmentEditor].pStatObj->GetFilePath());

	if (m_pData->database)
		m_databaseCombo.SelectString(0, m_pData->database->GetFilename());
	else
		m_databaseCombo.SelectString(0, noDB);

	if (m_pData->attachmentContext.length() > 0)
		m_attachmentContextCombo.SelectString(0, m_pData->attachmentContext);
	else
		m_attachmentContextCombo.SelectString(0, noAttachmentContext);

	m_attachmentEdit.SetWindowText(m_pData->attachment.c_str());
	m_attachmentHelperEdit.SetWindowText(m_pData->attachmentHelper.c_str());
	m_startPosXEdit.SetWindowText(ToString(m_pData->startLocation.t.x));
	m_startPosYEdit.SetWindowText(ToString(m_pData->startLocation.t.y));
	m_startPosZEdit.SetWindowText(ToString(m_pData->startLocation.t.z));
	m_startRotXEdit.SetWindowText(ToString(m_pData->startRotationEuler.x));
	m_startRotYEdit.SetWindowText(ToString(m_pData->startRotationEuler.y));
	m_startRotZEdit.SetWindowText(ToString(m_pData->startRotationEuler.z));

	if (m_pData->pSlaveDatabase)
		m_slaveDatabaseCombo.SelectString(0, m_pData->pSlaveDatabase->GetFilename());
	else
		m_slaveDatabaseCombo.SelectString(0, noDB);

	if (m_pData->pControllerDef)
		m_slaveControllerDefCombo.SelectString(0, m_pData->pControllerDef->m_filename.c_str());
	else
		m_slaveControllerDefCombo.SelectString(0, noControllerDef);

	// Tags
	m_tagVars->DeleteAllVariables();
	m_tagControls.Init(m_tagVars, pControllerDef->m_tags);
	m_tagControls.Set(m_pData->tags);

	m_tagsPanel.DeleteVars();
	m_tagsPanel.SetVarBlock(m_tagVars.get(), NULL, false);
}

void CMannNewContextDialog::OnOk()
{
	// Setup the combobox lookup values
	CMannequinDialog* pMannequinDialog = CMannequinDialog::GetCurrentInstance();
	SMannequinContexts* pContexts = pMannequinDialog->Contexts();
	const SControllerDef* pControllerDef = pContexts->m_controllerDef;

	// some of m_pData members are of type string (CryString<char>) so need to be retrieved via a temporary
	CString tempString;

	// retrieve per-detail values now
	m_pData->startActive = (m_startActiveCheckBox.GetCheck() == 1);
	m_nameEdit.GetWindowText(m_pData->name);

	m_pData->contextID = m_contextCombo.GetCurSel();
	if (m_pData->contextID >= pControllerDef->m_scopeContexts.GetNum())
	{
		m_pData->contextID = CONTEXT_DATA_NONE;
	}
	else
	{
		m_contextCombo.GetLBText(m_pData->contextID, tempString);
		const char* idname = pControllerDef->m_scopeContexts.GetTagName(m_pData->contextID);
		assert(stricmp(tempString, idname) == 0);
	}

	m_pData->tags = m_tagControls.Get();

	m_fragTagsEdit.GetWindowText(tempString);
	m_pData->fragTags = tempString;

	// lookup the model view data we've chosen
	m_modelEdit.GetWindowText(tempString);
	if (tempString[0])
	{
		const char* ext = PathUtil::GetExt(tempString);
		const bool isCharInst = ((stricmp(ext, "chr") == 0) || (stricmp(ext, "cdf") == 0) || (stricmp(ext, "cga") == 0));

		if (isCharInst)
		{
			if (!m_pData->CreateCharInstance(tempString))
			{
				CString strBuffer;
				strBuffer.Format("[CMannNewContextDialog::OnOk] Invalid file name %s, couldn't find character instance", tempString);

				MessageBox(strBuffer, "Missing file", MB_OK);
				return;
			}
		}
		else
		{
			if (!m_pData->CreateStatObj(tempString))
			{
				CString strBuffer;
				strBuffer.Format("[CMannNewContextDialog::OnOk] Invalid file name %s, couldn't find stat obj", tempString);

				MessageBox(strBuffer, "Missing file", MB_OK);
				return;
			}
		}
	}

	// need to dig around to get to the (animation) databases
	IMannequin& mannequinSys = gEnv->pGameFramework->GetMannequinInterface();
	IMannequinEditorManager* pManager = mannequinSys.GetMannequinEditorManager();
	DynArray<const IAnimationDatabase*> databases;
	pManager->GetLoadedDatabases(databases);
	const size_t dbSize = databases.size();
	const int dbSel = m_databaseCombo.GetCurSel();
	CString dbString;
	m_databaseCombo.GetLBText(dbSel, dbString);
	if (dbSel >= 0 && dbSel < dbSize)
		m_pData->database = const_cast<IAnimationDatabase*>(databases[dbSel]);
	else
		m_pData->database = NULL;

	const int attCont = m_attachmentContextCombo.GetCurSel();
	// NB: m_scopeContexts.GetNum() is poorly named but it means the number of tags within a scope context.
	if (attCont >= 0 && attCont < pControllerDef->m_scopeContexts.GetNum())
		m_pData->attachmentContext = pControllerDef->m_scopeContexts.GetTagName(attCont);
	else
		m_pData->attachmentContext = "";

	m_attachmentEdit.GetWindowText(tempString);
	m_pData->attachment = tempString;

	m_attachmentHelperEdit.GetWindowText(tempString);
	m_pData->attachmentHelper = tempString;

	m_pData->startLocation.t.x = Edit2f(m_startPosXEdit);
	m_pData->startLocation.t.y = Edit2f(m_startPosYEdit);
	m_pData->startLocation.t.z = Edit2f(m_startPosZEdit);

	m_pData->startRotationEuler.x = Edit2f(m_startRotXEdit);
	m_pData->startRotationEuler.y = Edit2f(m_startRotYEdit);
	m_pData->startRotationEuler.z = Edit2f(m_startRotZEdit);

	const int dbSlaveSel = m_slaveDatabaseCombo.GetCurSel();
	if (dbSlaveSel >= 0 && dbSlaveSel < dbSize)
		m_pData->pSlaveDatabase = const_cast<IAnimationDatabase*>(databases[dbSlaveSel]);
	else
		m_pData->pSlaveDatabase = NULL;

	DynArray<const SControllerDef*> controllerDefs;
	pManager->GetLoadedControllerDefs(controllerDefs);
	const size_t dbContDefSize = controllerDefs.size();
	const int dbSlaveContDefSel = m_slaveControllerDefCombo.GetCurSel();
	if (dbSlaveContDefSel >= 0 && dbSlaveContDefSel < dbContDefSize)
		m_pData->pControllerDef = controllerDefs[dbSlaveContDefSel];
	else
		m_pData->pControllerDef = NULL;

	if (m_pData->database)
	{
		pMannequinDialog->Validate(*m_pData);
	}

	if (m_mode != eContextDialog_Edit)
	{
		// dataID is initialised sequentially in MannequinDialog::LoadPreviewFile,
		// however deletions might have left gaps so I need to find the highest index
		// and add the ID after that.
		int newDataID = 0;
		for (int i = 0; i < pContexts->m_contextData.size(); i++)
		{
			const int currDataID = pContexts->m_contextData[i].dataID;
			if (newDataID <= currDataID)
			{
				newDataID = currDataID + 1;
			}
		}
		m_pData->dataID = newDataID;

		pContexts->m_contextData.push_back(*m_pData);
	}

	// if everything is ok with the data entered.
	__super::OnOK();
}

void CMannNewContextDialog::OnBrowsemodelButton()
{
	// Find a geometry type file for the model
	CString file("");
	CString initialDir("");
	m_modelEdit.GetWindowText(initialDir);
	if (initialDir.GetLength() > 0)
	{
		string tempDir(PathUtil::GetGameFolder());
		tempDir = PathUtil::AddSlash(tempDir);
		tempDir += PathUtil::GetPathWithoutFilename(initialDir);
		initialDir = tempDir;
	}
	// cdf, cga, chr are characters == viewData[0].charInst
	// cgf are static geometry		== viewData[0].pStatObj
	const CString fileFilter("Any Valid (*.cdf;*.cga;*.chr;*.cgf)|*.cdf;*.cga;*.chr;*.cgf|Animated (*.cdf;*.cga;*.chr)|*.cdf;*.cga;*.chr|Static (*.cgf)|*.cgf; *.cgf|All Files (*.*)|*.*||");
	if (CFileUtil::SelectSingleFile(EFILE_TYPE_GEOMETRY, file, fileFilter, initialDir))
	{
		m_modelEdit.SetWindowText(file);
	}
}

void CMannNewContextDialog::PopulateDatabaseCombo()
{
	// Populate database combo
	IMannequin& mannequinSys = gEnv->pGameFramework->GetMannequinInterface();
	IMannequinEditorManager* pManager = mannequinSys.GetMannequinEditorManager();
	DynArray<const IAnimationDatabase*> databases;
	pManager->GetLoadedDatabases(databases);
	m_databaseCombo.ResetContent();
	m_slaveDatabaseCombo.ResetContent();
	const int numEntries = m_databaseCombo.GetCount();
	assert(numEntries == 0);
	for (int i = 0; i < databases.size(); i++)
	{
		m_databaseCombo.AddString(databases[i]->GetFilename());
		m_slaveDatabaseCombo.AddString(databases[i]->GetFilename());
	}
	// additional string that will be ignored by the validation checking in ::OnOK() function
	m_databaseCombo.AddString(noDB);
	m_slaveDatabaseCombo.AddString(noDB);
}

void CMannNewContextDialog::PopulateControllerDefCombo()
{
	// Populate controller def combo
	IMannequin& mannequinSys = gEnv->pGameFramework->GetMannequinInterface();
	IMannequinEditorManager* pManager = mannequinSys.GetMannequinEditorManager();
	DynArray<const SControllerDef*> controllerDefs;
	pManager->GetLoadedControllerDefs(controllerDefs);
	m_slaveControllerDefCombo.ResetContent();
	const int numEntries = m_slaveControllerDefCombo.GetCount();
	assert(numEntries == 0);
	for (int i = 0; i < controllerDefs.size(); i++)
	{
		m_slaveControllerDefCombo.AddString(controllerDefs[i]->m_filename.c_str());
	}
	// additional string that will be ignored by the validation checking in ::OnOK() function
	m_slaveControllerDefCombo.AddString(noControllerDef);
}

void CMannNewContextDialog::OnNewAdbButton()
{
	// TODO: Add your control notification handler code here
	// Setup the combobox lookup values
	const SControllerDef* pControllerDef = CMannequinDialog::GetCurrentInstance()->Contexts()->m_controllerDef;
	IMannequin& mannequinSys = gEnv->pGameFramework->GetMannequinInterface();

	CString adbNameBuffer("");
	QStringDialog adbNameDialog("New Database Name");
	adbNameDialog.SetString(adbNameBuffer);
	if (adbNameDialog.exec() != QDialog::Accepted)
		return;

	// create new database name
	string adbFilenameBuffer("Animations/Mannequin/ADB/");
	adbNameBuffer = adbNameDialog.GetString();
	if (adbNameBuffer.GetLength() > 0)
	{
		// add the filename+extension onto the path.
		adbFilenameBuffer += adbNameBuffer.GetString();
		adbFilenameBuffer += ".adb";
	}
	else
	{
		return;
	}

	adbFilenameBuffer = PathUtil::MakeGamePath(adbFilenameBuffer);

	// add the new one, re-populate the database combo box...
	const IAnimationDatabase* pADB = mannequinSys.GetAnimationDatabaseManager().Load(adbFilenameBuffer);
	if (!pADB)
	{
		pADB = mannequinSys.GetAnimationDatabaseManager().Create(adbFilenameBuffer, pControllerDef->m_filename.c_str());
	}

	if (!pADB)
	{
		return;
	}

	PopulateDatabaseCombo();
	// ...and select it in the list
	m_databaseCombo.SelectString(0, pADB->GetFilename());
}

