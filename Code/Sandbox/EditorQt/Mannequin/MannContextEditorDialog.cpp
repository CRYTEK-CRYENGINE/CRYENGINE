// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MannContextEditorDialog.h"
#include "MannequinDialog.h"
#include "MannNewContextDialog.h"
#include "Controls/MannImportBackgroundDialog.h"

#include <CryString/StringUtils.h>

namespace
{
inline string BoolToString(bool b)
{
	return b ? "True" : "False";
}

inline string VecToString(const Vec3& v)
{
	char szBuf[128];
	cry_sprintf(szBuf, "%g, %g, %g", v.x, v.y, v.z);
	return szBuf;
}

inline string AngToString(const Ang3& a)
{
	char szBuf[128];
	cry_sprintf(szBuf, "%g, %g, %g", a.x, a.y, a.z);
	return szBuf;
}
}

//////////////////////////////////////////////////////////////////////////

enum EContextReportColumn
{
	CONTEXTCOLUMN_INDEX,
	CONTEXTCOLUMN_NAME,
	CONTEXTCOLUMN_ENABLED,
	CONTEXTCOLUMN_DATABASE,
	CONTEXTCOLUMN_CONTEXTID,
	CONTEXTCOLUMN_TAGS,
	CONTEXTCOLUMN_FRAGTAGS,
	CONTEXTCOLUMN_MODEL,
	CONTEXTCOLUMN_ATTACHMENT,
	CONTEXTCOLUMN_ATTACHMENTCONTEXT,
	CONTEXTCOLUMN_ATTACHMENTHELPER,
	CONTEXTCOLUMN_STARTPOSITION,
	CONTEXTCOLUMN_STARTROTATION,
	CONTEXTCOLUMN_SLAVE_CONTROLLER_DEF,
	CONTEXTCOLUMN_SLAVE_DATABASE,
};

//////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CMannContextEditorDialog, CDialog)
ON_NOTIFY(NM_DBLCLK, ID_REPORT_CONTROL, OnReportItemDblClick)
ON_NOTIFY(XTP_NM_REPORT_SELCHANGED, ID_REPORT_CONTROL, OnReportSelChanged)
ON_NOTIFY(NM_KEYDOWN, ID_REPORT_CONTROL, OnReportKeyDown)

ON_BN_CLICKED(IDC_MANN_NEW_CONTEXT, &CMannContextEditorDialog::OnNewContext)
ON_BN_CLICKED(IDC_MANN_EDIT_CONTEXT, &CMannContextEditorDialog::OnEditContext)
ON_BN_CLICKED(IDC_MANN_CLONE_CONTEXT, &CMannContextEditorDialog::OnCloneAndEditContext)
ON_BN_CLICKED(IDC_MANN_DELETE_CONTEXT, &CMannContextEditorDialog::OnDeleteContext)
ON_BN_CLICKED(IDC_MANN_MOVE_UP, &CMannContextEditorDialog::OnMoveUp)
ON_BN_CLICKED(IDC_MANN_MOVE_DOWN, &CMannContextEditorDialog::OnMoveDown)

ON_BN_CLICKED(IDC_MANN_IMPORT_BACKGROUND, &CMannContextEditorDialog::OnImportBackground)
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
// Derived from CXTPReport so that it can populate the m_wndReport member
// of CMannContextEditorDialog when passed an SScopeContextData struct.
class CXTPMannContextRecord : public CXTPReportRecord
{
	DECLARE_DYNAMIC(CXTPMannContextRecord)
public:
	CXTPMannContextRecord(SScopeContextData& contextData)
		: m_contextData(contextData)
	{
		const SControllerDef* controllerDef = CMannequinDialog::GetCurrentInstance()->Contexts()->m_controllerDef;

		AddItem(new CXTPReportRecordItemNumber(m_contextData.dataID));                                                                                                 // CONTEXTCOLUMN_INDEX
		AddItem(new CXTPReportRecordItemText(m_contextData.name));                                                                                                     // CONTEXTCOLUMN_NAME
		AddItem(new CXTPReportRecordItemText(BoolToString(m_contextData.startActive)));                                                                                // CONTEXTCOLUMN_ENABLED
		AddItem(new CXTPReportRecordItemText(m_contextData.database ? m_contextData.database->GetFilename() : ""));                                                    // CONTEXTCOLUMN_DATABASE
		AddItem(new CXTPReportRecordItemText(m_contextData.contextID == CONTEXT_DATA_NONE ? "" : controllerDef->m_scopeContexts.GetTagName(m_contextData.contextID))); // CONTEXTCOLUMN_CONTEXTID

		CryStackStringT<char, 512> sTags;
		controllerDef->m_tags.FlagsToTagList(m_contextData.tags, sTags);
		AddItem(new CXTPReportRecordItemText(sTags.c_str()));

		AddItem(new CXTPReportRecordItemText(m_contextData.fragTags));

		CString modelName;
		if (m_contextData.viewData[0].charInst)
			modelName = m_contextData.viewData[0].charInst->GetFilePath();
		else if (m_contextData.viewData[0].pStatObj)
			modelName = m_contextData.viewData[0].pStatObj->GetFilePath();
		AddItem(new CXTPReportRecordItemText(modelName));

		AddItem(new CXTPReportRecordItemText(m_contextData.attachment));
		AddItem(new CXTPReportRecordItemText(m_contextData.attachmentContext));
		AddItem(new CXTPReportRecordItemText(m_contextData.attachmentHelper));
		AddItem(new CXTPReportRecordItemText(VecToString(m_contextData.startLocation.t)));
		AddItem(new CXTPReportRecordItemText(AngToString(m_contextData.startRotationEuler)));

		AddItem(new CXTPReportRecordItemText(m_contextData.pControllerDef ? m_contextData.pControllerDef->m_filename.c_str() : "")); // CONTEXTCOLUMN_SLAVE_CONTROLLERDEF
		AddItem(new CXTPReportRecordItemText(m_contextData.pSlaveDatabase ? m_contextData.pSlaveDatabase->GetFilename() : ""));      // CONTEXTCOLUMN_SLAVE_DATABASE
	}

	SScopeContextData& ContextData() const { return m_contextData; }

private:
	SScopeContextData& m_contextData;
};

//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNAMIC(CXTPMannContextRecord, CXTPReportRecord)

//////////////////////////////////////////////////////////////////////////
CMannContextEditorDialog::CMannContextEditorDialog(CWnd* pParent)
	: CXTResizeDialog(IDD_MANN_CONTEXT_EDITOR, pParent)
{
	// flush changes so nothing is lost.
	CMannequinDialog::GetCurrentInstance()->FragmentEditor()->TrackPanel()->FlushChanges();
}

//////////////////////////////////////////////////////////////////////////
CMannContextEditorDialog::~CMannContextEditorDialog()
{
	CMannequinDialog::GetCurrentInstance()->LoadNewPreviewFile(NULL);
	CMannequinDialog::GetCurrentInstance()->StopEditingFragment();
}

//////////////////////////////////////////////////////////////////////////
void CMannContextEditorDialog::DoDataExchange(CDataExchange* pDX)
{
	__super::DoDataExchange(pDX);

	DDX_Control(pDX, IDC_MANN_NEW_CONTEXT, m_newContext);
	DDX_Control(pDX, IDC_MANN_EDIT_CONTEXT, m_editContext);
	DDX_Control(pDX, IDC_MANN_CLONE_CONTEXT, m_cloneContext);
	DDX_Control(pDX, IDC_MANN_DELETE_CONTEXT, m_deleteContext);
	DDX_Control(pDX, IDC_MANN_MOVE_UP, m_moveUp);
	DDX_Control(pDX, IDC_MANN_MOVE_DOWN, m_moveDown);
}

//////////////////////////////////////////////////////////////////////////
BOOL CMannContextEditorDialog::OnInitDialog()
{
	__super::OnInitDialog();

	CRect rc;
	GetClientRect(&rc);

	CRect rcReport = rc;
	rcReport.top += 45;
	rcReport.bottom -= 45;
	VERIFY(m_wndReport.Create(WS_CHILD | WS_TABSTOP | WS_VISIBLE | WM_VSCROLL, rcReport, this, ID_REPORT_CONTROL));

	CXTPReportColumn* pColID = m_wndReport.AddColumn(new CXTPReportColumn(CONTEXTCOLUMN_INDEX, _T("Index"), 30, false, -1, true, true));
	m_wndReport.AddColumn(new CXTPReportColumn(CONTEXTCOLUMN_NAME, _T("Name"), 120));
	m_wndReport.AddColumn(new CXTPReportColumn(CONTEXTCOLUMN_ENABLED, _T("Start On"), 50));
	m_wndReport.AddColumn(new CXTPReportColumn(CONTEXTCOLUMN_DATABASE, _T("Database"), 120));
	CXTPReportColumn* pColContext = m_wndReport.AddColumn(new CXTPReportColumn(CONTEXTCOLUMN_CONTEXTID, _T("Context"), 80));
	m_wndReport.AddColumn(new CXTPReportColumn(CONTEXTCOLUMN_TAGS, _T("Tags"), 120));
	m_wndReport.AddColumn(new CXTPReportColumn(CONTEXTCOLUMN_FRAGTAGS, _T("FragTags"), 80));
	m_wndReport.AddColumn(new CXTPReportColumn(CONTEXTCOLUMN_MODEL, _T("Model"), 120));
	m_wndReport.AddColumn(new CXTPReportColumn(CONTEXTCOLUMN_ATTACHMENT, _T("Attachment"), 80));
	m_wndReport.AddColumn(new CXTPReportColumn(CONTEXTCOLUMN_ATTACHMENTCONTEXT, _T("Att. Context"), 80));
	m_wndReport.AddColumn(new CXTPReportColumn(CONTEXTCOLUMN_ATTACHMENTHELPER, _T("Att. Helper"), 80));
	m_wndReport.AddColumn(new CXTPReportColumn(CONTEXTCOLUMN_STARTPOSITION, _T("Start Position"), 80));
	m_wndReport.AddColumn(new CXTPReportColumn(CONTEXTCOLUMN_STARTROTATION, _T("Start Rotation"), 80));
	m_wndReport.AddColumn(new CXTPReportColumn(CONTEXTCOLUMN_SLAVE_CONTROLLER_DEF, _T("Slave Controller Def"), 80));
	m_wndReport.AddColumn(new CXTPReportColumn(CONTEXTCOLUMN_SLAVE_DATABASE, _T("Slave Database"), 80));

	m_wndReport.GetColumns()->GetGroupsOrder()->Add(pColContext);
	m_wndReport.SetMultipleSelection(false);
	m_wndReport.GetColumns()->SetSortColumn(pColContext, true);
	m_wndReport.GetColumns()->InsertSortColumn(pColID);

	SetResize(ID_REPORT_CONTROL, SZ_RESIZE(1));
	SetResize(IDOK, SZ_REPOS(1));

	PopulateReport();

	EnableControls();

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
void CMannContextEditorDialog::PopulateReport()
{
	m_wndReport.BeginUpdate();

	m_wndReport.ResetContent();

	SMannequinContexts* pContexts = CMannequinDialog::GetCurrentInstance()->Contexts();
	for (size_t i = 0; i < pContexts->m_contextData.size(); i++)
	{
		CXTPMannContextRecord* pRecord = new CXTPMannContextRecord(pContexts->m_contextData[i]);
		pRecord->SetEditable();
		m_wndReport.AddRecord(pRecord);
	}

	m_wndReport.Populate();

	if (!pContexts->m_contextData.empty())
	{
		m_wndReport.SetFocusedRow(m_wndReport.GetRows()->GetAt(0));
	}

	m_wndReport.EndUpdate();
}

//////////////////////////////////////////////////////////////////////////
void CMannContextEditorDialog::EnableControls()
{
	const CXTPReportRow* pRow = m_wndReport.GetFocusedRow();
	const bool isValidRowSelected = pRow && !pRow->IsGroupRow();
	const int index = isValidRowSelected ? pRow->GetIndex() - pRow->GetParentRow()->GetIndex() - 1 : 0;

	m_newContext.EnableWindow(true);
	m_editContext.EnableWindow(isValidRowSelected);
	m_cloneContext.EnableWindow(isValidRowSelected);
	m_deleteContext.EnableWindow(isValidRowSelected);
	m_moveUp.EnableWindow(isValidRowSelected && index > 0);
	m_moveDown.EnableWindow(isValidRowSelected && index < (pRow->GetParentRow()->GetChilds()->GetCount() - 1));
}

//////////////////////////////////////////////////////////////////////////
CXTPMannContextRecord* CMannContextEditorDialog::GetFocusedRecord()
{
	return DYNAMIC_DOWNCAST(CXTPMannContextRecord, m_wndReport.GetFocusedRow()->GetRecord());
}

//////////////////////////////////////////////////////////////////////////
void CMannContextEditorDialog::OnReportSelChanged(NMHDR* pNotifyStruct, LRESULT* result)
{
	EnableControls();
}

//////////////////////////////////////////////////////////////////////////
void CMannContextEditorDialog::OnReportItemDblClick(NMHDR* pNotifyStruct, LRESULT* result)
{
	XTP_NM_REPORTRECORDITEM* pItemNotify = (XTP_NM_REPORTRECORDITEM*) pNotifyStruct;

	if (!pItemNotify->pRow || pItemNotify->pRow->IsGroupRow())
		return;

	OnEditContext();
}

//////////////////////////////////////////////////////////////////////////
void CMannContextEditorDialog::OnReportKeyDown(NMHDR* pNotifyStruct, LRESULT* /*result*/)
{
	LPNMKEY lpNMKey = (LPNMKEY)pNotifyStruct;

	if (!m_wndReport.GetFocusedRow() || m_wndReport.GetFocusedRow()->IsGroupRow())
		return;

	if (lpNMKey->nVKey == VK_RETURN)
	{
		OnEditContext();
	}
}

//////////////////////////////////////////////////////////////////////////
void CMannContextEditorDialog::OnNewContext()
{
	CMannNewContextDialog dialog(nullptr, CMannNewContextDialog::eContextDialog_New, this);
	if (dialog.DoModal() == IDOK)
	{
		PopulateReport();
	}
}

//////////////////////////////////////////////////////////////////////////
void CMannContextEditorDialog::OnEditContext()
{
	CXTPMannContextRecord* pRecord = GetFocusedRecord();
	if (pRecord)
	{
		CMannNewContextDialog dialog(&pRecord->ContextData(), CMannNewContextDialog::eContextDialog_Edit, this);
		if (dialog.DoModal() == IDOK)
		{
			PopulateReport();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMannContextEditorDialog::OnCloneAndEditContext()
{
	CXTPMannContextRecord* pRecord = GetFocusedRecord();
	if (pRecord)
	{
		CMannNewContextDialog dialog(&pRecord->ContextData(), CMannNewContextDialog::eContextDialog_CloneAndEdit, this);
		if (dialog.DoModal() == IDOK)
		{
			PopulateReport();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMannContextEditorDialog::OnDeleteContext()
{
	if (CXTPMannContextRecord* pRecord = GetFocusedRecord())
	{
		CString message;
		message.Format("Are you sure you want to delete \"%s\"?\nYou will not be able to undo this change.", pRecord->ContextData().name);

		if (MessageBox(message.GetBuffer(), "CryMannequin", MB_YESNO | MB_ICONQUESTION) == IDYES)
		{
			SMannequinContexts* pContexts = CMannequinDialog::GetCurrentInstance()->Contexts();
			std::vector<SScopeContextData>::const_iterator iter = pContexts->m_contextData.begin();
			while (iter != pContexts->m_contextData.end())
			{
				if ((*iter).dataID == pRecord->ContextData().dataID)
				{
					pContexts->m_contextData.erase(iter);
					break;
				}
				++iter;
			}
			PopulateReport();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMannContextEditorDialog::OnMoveUp()
{
	if (CXTPReportRow* pRow = m_wndReport.GetFocusedRow())
	{
		CXTPMannContextRecord* pRec = (CXTPMannContextRecord*)pRow->GetRecord();
		CXTPMannContextRecord* pAbove = (CXTPMannContextRecord*)m_wndReport.GetRows()->GetPrev(pRow, false)->GetRecord();

		SMannequinContexts& contexts = (*CMannequinDialog::GetCurrentInstance()->Contexts());
		std::vector<SScopeContextData>::iterator recIt = std::find(contexts.m_contextData.begin(), contexts.m_contextData.end(), pRec->ContextData());
		std::vector<SScopeContextData>::iterator abvIt = std::find(contexts.m_contextData.begin(), contexts.m_contextData.end(), pAbove->ContextData());
		if (recIt != contexts.m_contextData.end() && abvIt != contexts.m_contextData.end())
		{
			std::swap(*recIt, *abvIt);
			std::swap((*recIt).dataID, (*abvIt).dataID);
		}

		const uint32 tempDataID = (*abvIt).dataID;
		PopulateReport();
		SetFocusedRecord((*abvIt).dataID);
		m_wndReport.SetFocus();

		EnableControls();
	}
}

//////////////////////////////////////////////////////////////////////////
void CMannContextEditorDialog::OnMoveDown()
{
	if (CXTPReportRow* pRow = m_wndReport.GetFocusedRow())
	{
		CXTPMannContextRecord* pRec = (CXTPMannContextRecord*)pRow->GetRecord();
		CXTPMannContextRecord* pBelow = (CXTPMannContextRecord*)m_wndReport.GetRows()->GetNext(pRow, false)->GetRecord();

		SMannequinContexts& contexts = (*CMannequinDialog::GetCurrentInstance()->Contexts());
		std::vector<SScopeContextData>::iterator recIt = std::find(contexts.m_contextData.begin(), contexts.m_contextData.end(), pRec->ContextData());
		std::vector<SScopeContextData>::iterator blwIt = std::find(contexts.m_contextData.begin(), contexts.m_contextData.end(), pBelow->ContextData());
		if (recIt != contexts.m_contextData.end() && blwIt != contexts.m_contextData.end())
		{
			std::swap(*recIt, *blwIt);
			std::swap((*recIt).dataID, (*blwIt).dataID);
		}

		PopulateReport();
		SetFocusedRecord((*blwIt).dataID);
		m_wndReport.SetFocus();

		EnableControls();
	}
}

//////////////////////////////////////////////////////////////////////////
void CMannContextEditorDialog::SetFocusedRecord(const uint32 dataID)
{
	if (CXTPReportRow* pRow = m_wndReport.GetFocusedRow())
	{
		CXTPReportRow* pNextRow = m_wndReport.GetRows()->GetNext(pRow, false);
		while (pNextRow != NULL)
		{
			CXTPMannContextRecord* pRecord = (CXTPMannContextRecord*)pNextRow->GetRecord();
			if (pRecord && dataID == pRecord->ContextData().dataID)
			{
				m_wndReport.SetFocusedRow(pNextRow);
				break;
			}

			pNextRow = m_wndReport.GetRows()->GetNext(pNextRow, false);
		}
	}
}

const char* OBJECT_WHITELIST[] =
{
	"Brush",
	"Entity",
	"PrefabInstance",
	"Solid",
	"TagPoint"
};
const uint32 NUM_WHITELISTED_OBJECTS = ARRAYSIZE(OBJECT_WHITELIST);

bool IsObjectWhiteListed(const char* objectType)
{
	for (uint32 i = 0; i < NUM_WHITELISTED_OBJECTS; i++)
	{
		if (strcmp(OBJECT_WHITELIST[i], objectType) == 0)
			return true;
	}

	return false;
}

const char* ENTITY_WHITELIST[] =
{
	"RigidBodyEx",
	"Light",
	"Switch"
};
const uint32 NUM_WHITELISTED_ENTITIES = ARRAYSIZE(ENTITY_WHITELIST);

bool IsEntityWhiteListed(const char* entityClass)
{
	for (uint32 i = 0; i < NUM_WHITELISTED_ENTITIES; i++)
	{
		if (strcmp(ENTITY_WHITELIST[i], entityClass) == 0)
			return true;
	}

	return false;
}

//--- Remove any objects that aren't relevant/won't work in the Mannequin editor
void StripBackgroundXML(XmlNodeRef xmlRoot)
{
	if (xmlRoot)
	{
		uint32 numChildren = xmlRoot->getChildCount();
		for (int32 i = numChildren - 1; i >= 0; i--)
		{
			XmlNodeRef xmlObject = xmlRoot->getChild(i);
			const char* type = xmlObject->getAttr("Type");
			if (!IsObjectWhiteListed(type))
			{
				xmlRoot->removeChild(xmlObject);
			}
			else if (strcmp(type, "Entity") == 0)
			{
				const char* entClass = xmlObject->getAttr("EntityClass");
				if (!IsEntityWhiteListed(entClass))
				{
					xmlRoot->removeChild(xmlObject);
				}
			}
		}
	}
}

afx_msg void CMannContextEditorDialog::OnImportBackground()
{
	const char* setupFileFilter = "Select an object group file (*.grp)|*.grp";
	CString filename;

	const bool userSelectedPreviewFile = CFileUtil::SelectSingleFile(EFILE_TYPE_ANY, filename, setupFileFilter, PathUtil::GetGameFolder().c_str());
	if (!userSelectedPreviewFile)
	{
		return;
	}

	XmlNodeRef xmlData = GetISystem()->LoadXmlFromFile(filename);
	if (xmlData)
	{
		StripBackgroundXML(xmlData);

		const uint32 numObjects = xmlData->getChildCount();
		std::vector<CString> objectNames;
		objectNames.resize(numObjects);
		for (uint32 i = 0; i < numObjects; i++)
		{
			XmlNodeRef child = xmlData->getChild(i);
			objectNames[i] = child->getAttr("Name");
		}

		CMannImportBackgroundDialog dlgImportBackground(objectNames);
		if (dlgImportBackground.DoModal() == IDOK)
		{
			int curRoot = dlgImportBackground.GetCurrentRoot();
			QuatT rootTran(IDENTITY);

			if (curRoot >= 0)
			{
				XmlNodeRef child = xmlData->getChild(curRoot);
				child->getAttr("Pos", rootTran.t);
				child->getAttr("Rotate", rootTran.q);
			}

			CMannequinDialog* pMannequinDialog = CMannequinDialog::GetCurrentInstance();
			SMannequinContexts* pContexts = pMannequinDialog->Contexts();
			pContexts->backGroundObjects = xmlData;
			pContexts->backgroundRootTran = rootTran;
		}
	}
}

