// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FlowGraphSearchCtrl.h"

#include <CryAISystem/IAIAction.h>
#include "Util/CryMemFile.h"          // CCryMemFile

#include "HyperGraph/FlowGraphManager.h"
#include "HyperGraph/FlowGraphHelpers.h"
#include "HyperGraph/FlowGraphNode.h"
#include "HyperGraph/Nodes/CommentNode.h"
#include "HyperGraphEditorWnd.h"

#include "Objects/EntityObject.h"


#define MAX_HISTORY_ENTRIES 20

CFlowGraphSearchOptions* CFlowGraphSearchOptions::GetSearchOptions()
{
	static CFlowGraphSearchOptions options;
	return &options;
}

CFlowGraphSearchOptions::CFlowGraphSearchOptions(const CFlowGraphSearchOptions& other)
{
	assert(0);
}

CFlowGraphSearchOptions::CFlowGraphSearchOptions()
{
	this->m_strFind = "";
	this->m_bIncludePorts = FALSE;
	this->m_bIncludeValues = FALSE;
	this->m_bIncludeEntities = FALSE;
	this->m_bIncludeIDs = FALSE;
	this->m_bExactMatch = FALSE;      // not serialized!
	this->m_LookinIndex = eFL_Current;
	this->m_findSpecial = eFLS_None;
}

void CFlowGraphSearchOptions::Serialize(bool bLoading)
{
	CXTRegistryManager regMgr;
	CString str;
	const CString strSection = _T("FlowGraphSearchHistory");

	if (!bLoading)
	{
		regMgr.WriteProfileInt(strSection, _T("IncludePorts"), m_bIncludePorts);
		regMgr.WriteProfileInt(strSection, _T("IncludeValues"), m_bIncludeValues);
		regMgr.WriteProfileInt(strSection, _T("IncludeEntities"), m_bIncludeEntities);
		regMgr.WriteProfileInt(strSection, _T("IncludeIDs"), m_bIncludeIDs);
		regMgr.WriteProfileInt(strSection, _T("LookIn"), m_LookinIndex);
		regMgr.WriteProfileInt(strSection, _T("Special"), m_findSpecial);
		regMgr.WriteProfileString(strSection, _T("LastFind"), m_strFind);
		regMgr.WriteProfileInt(strSection, _T("Count"), m_lstFindHistory.GetCount());
		POSITION pos = m_lstFindHistory.GetHeadPosition();
		int i = 0;
		while (pos)
		{
			str.Format(_T("Item%i"), i);
			regMgr.WriteProfileString(strSection, str, m_lstFindHistory.GetNext(pos));
			++i;
		}
	}
	else
	{
		m_bIncludePorts = regMgr.GetProfileInt(strSection, _T("IncludePorts"), FALSE);
		m_bIncludeValues = regMgr.GetProfileInt(strSection, _T("IncludeValues"), FALSE);
		m_bIncludeEntities = regMgr.GetProfileInt(strSection, _T("IncludeEntities"), FALSE);
		m_bIncludeIDs = regMgr.GetProfileInt(strSection, _T("IncludeIDs"), FALSE);
		m_strFind = regMgr.GetProfileString(strSection, _T("LastFind"), "");
		m_lstFindHistory.RemoveAll();
		m_LookinIndex = regMgr.GetProfileInt(strSection, _T("LookIn"), eFL_Current);
		m_findSpecial = regMgr.GetProfileInt(strSection, _T("Special"), eFLS_None);
		int count = regMgr.GetProfileInt(strSection, _T("Count"), 0);
		for (int i = 0; i < count; ++i)
		{
			str.Format(_T("Item%i"), i);
			CString item = regMgr.GetProfileString(strSection, str, "");
			if (item.IsEmpty())
				break;
			m_lstFindHistory.AddTail(item);
		}
	}
}

void AFXAPI _DDX_CBString(CDataExchange* pDX, int nIDC, CString& value)
{
	HWND hWndCtrl = pDX->PrepareCtrl(nIDC);
	if (pDX->m_bSaveAndValidate)
	{
		// Get current edit item text (or drop list static)
		int nLen = ::GetWindowTextLength(hWndCtrl);
		if (nLen != -1 && nLen != 0xFFFF)
		{
			// Get known length
			::GetWindowText(hWndCtrl, value.GetBufferSetLength(nLen), nLen + 1);
		}
		else
		{
			// GetWindowTextLength does not work for drop lists assume
			// max of 255 characters
			::GetWindowText(hWndCtrl, value.GetBuffer(255), 255 + 1);
		}
		value.ReleaseBuffer();
	}
	else
	{
		// Set current selection based on model string
		if (::SendMessage(hWndCtrl, CB_SELECTSTRING, (WPARAM)-1,
		                  (LPARAM)(LPCTSTR)value) == CB_ERR)
		{
			// Set the edit text (will be ignored if DROPDOWNLIST)
			::SetWindowText(hWndCtrl, value);
		}
	}
}

void AddComboHistory(CComboBox& cmb, CString strText, CStringList& lstHistory)
{
	if (strText.IsEmpty())
		return;

	int nIndex = cmb.FindString(-1, strText);
	if (nIndex == -1)
	{
		cmb.InsertString(0, strText);
		lstHistory.AddHead(strText);
		while (lstHistory.GetCount() > MAX_HISTORY_ENTRIES)
			lstHistory.RemoveTail();
		while (cmb.GetCount() > MAX_HISTORY_ENTRIES)
			cmb.DeleteString(cmb.GetCount() - 1);
	}
}

void RestoryCombo(CComboBox& cmb, CStringList& lstHistory, LPCTSTR lpszDefault = 0)
{
	if (lstHistory.GetCount() > 0)
	{
		POSITION pos = lstHistory.GetHeadPosition();
		int count = 0;
		while (pos && count < MAX_HISTORY_ENTRIES)
		{
			cmb.AddString(lstHistory.GetNext(pos));
			++count;
		}
	}
	else if (lpszDefault)
	{
		cmb.AddString(lpszDefault);
		lstHistory.AddTail(lpszDefault);
	}
}

IMPLEMENT_DYNAMIC(CFlowGraphSearchCtrl, CXTResizeDialog)

//////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CFlowGraphSearchCtrl, CXTResizeDialog)
ON_BN_CLICKED(IDC_BUTTON_FINDALL, OnButtonFindAll)
ON_CBN_SELENDOK(IDC_COMBO_FIND, OnOptionChanged)
ON_CBN_SELENDOK(IDC_COMBO_LOOKIN, OnOptionChanged)
ON_CBN_SELENDOK(IDC_COMBO_SPECIAL, OnOptionChanged)

ON_BN_CLICKED(IDC_CHECK_INCLUDE_PORTS, OnIncludePortsClicked)
ON_BN_CLICKED(IDC_CHECK_INCLUDE_VALUES, OnIncludeValuesClicked)
ON_BN_CLICKED(IDC_CHECK_INCLUDE_ENTITIES, OnIncludeEntitiesClicked)
ON_BN_CLICKED(IDC_CHECK_INCLUDE_IDS, OnIncludeIDsClicked)

END_MESSAGE_MAP()
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
CFlowGraphSearchCtrl::CFlowGraphSearchCtrl(CHyperGraphDialog* pDialog)
	: CXTResizeDialog(CFlowGraphSearchCtrl::IDD, pDialog)
{
	m_pResultsCtrl = 0;
	m_pDialog = pDialog;
	Create(CFlowGraphSearchCtrl::IDD, pDialog);
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphSearchCtrl::DoDataExchange(CDataExchange* pDX)
{
	CXTResizeDialog::DoDataExchange(pDX);
	// controls
	DDX_Control(pDX, IDC_COMBO_LOOKIN, m_cmbLookin);
	DDX_Control(pDX, IDC_COMBO_FIND, m_cmbFind);
	DDX_Control(pDX, IDC_COMBO_SPECIAL, m_cmbSpecial);

	// data
	DDX_CBIndex(pDX, IDC_COMBO_LOOKIN, CFlowGraphSearchOptions::GetSearchOptions()->m_LookinIndex);
	DDX_CBIndex(pDX, IDC_COMBO_SPECIAL, CFlowGraphSearchOptions::GetSearchOptions()->m_findSpecial);
	DDX_CBString(pDX, IDC_COMBO_FIND, CFlowGraphSearchOptions::GetSearchOptions()->m_strFind);
	DDX_Check(pDX, IDC_CHECK_INCLUDE_PORTS, CFlowGraphSearchOptions::GetSearchOptions()->m_bIncludePorts);
	DDX_Check(pDX, IDC_CHECK_INCLUDE_VALUES, CFlowGraphSearchOptions::GetSearchOptions()->m_bIncludeValues);
	DDX_Check(pDX, IDC_CHECK_INCLUDE_ENTITIES, CFlowGraphSearchOptions::GetSearchOptions()->m_bIncludeEntities);
	DDX_Check(pDX, IDC_CHECK_INCLUDE_IDS, CFlowGraphSearchOptions::GetSearchOptions()->m_bIncludeIDs);
}

//////////////////////////////////////////////////////////////////////////
BOOL CFlowGraphSearchCtrl::OnInitDialog()
{
	CXTResizeDialog::OnInitDialog();

	// ModifyStyle(WS_CLIPSIBLINGS, WS_CLIPCHILDREN);
	SetFlag(xtResizeNoClipChildren);

	SetResize(IDC_COMBO_FIND, SZ_TOP_LEFT, SZ_TOP_RIGHT);
	SetResize(IDC_COMBO_LOOKIN, SZ_TOP_LEFT, SZ_TOP_RIGHT);
	SetResize(IDC_COMBO_SPECIAL, SZ_TOP_LEFT, SZ_TOP_RIGHT);
	SetResize(IDC_BUTTON_FINDALL, SZ_TOP_RIGHT, SZ_TOP_RIGHT);
	SetResize(IDC_CHECK_INCLUDE_PORTS, SZ_TOP_LEFT, SZ_TOP_RIGHT);
	SetResize(IDC_CHECK_INCLUDE_VALUES, SZ_TOP_LEFT, SZ_TOP_RIGHT);
	SetResize(IDC_CHECK_INCLUDE_ENTITIES, SZ_TOP_LEFT, SZ_TOP_RIGHT);
	SetResize(IDC_CHECK_INCLUDE_IDS, SZ_TOP_LEFT, SZ_TOP_RIGHT);

	RestoryCombo(m_cmbFind, CFlowGraphSearchOptions::GetSearchOptions()->m_lstFindHistory);

	// Order must match EFindLocation order in FlowGraphSearchCtrl.h
	m_cmbLookin.AddString(_T("Current"));
	m_cmbLookin.AddString(_T("AIActions"));
	m_cmbLookin.AddString(_T("CustomActions"));
	m_cmbLookin.AddString(_T("Entities"));
	m_cmbLookin.AddString(_T("All FlowGraphs"));

	// Order must match EFindSpecial order in FlowGraphSearchCtrl.h
	m_cmbSpecial.AddString(_T("---"));
	m_cmbSpecial.AddString(_T("No Entity"));
	m_cmbSpecial.AddString(_T("No Links"));
	m_cmbSpecial.AddString(_T("Cat: Approved"));
	m_cmbSpecial.AddString(_T("Cat: Advanced"));
	m_cmbSpecial.AddString(_T("Cat: Debug"));
	m_cmbSpecial.AddString(_T("Cat: Obsolete"));

	UpdateData(false);
	UpdateOptions();

	m_cmbFind.SetCurSel(-1);
	return FALSE;
}

//////////////////////////////////////////////////////////////////////////
class CSearchResult : public CXTPReportRecord
{
	// returns reference to a STATIC string to speed things up -> beware
	static const CString& GetFGName(CHyperFlowGraph* pFG)
	{
		static CString temp;
		CEntityObject* pEntity = pFG->GetEntity();
		IAIAction* pAIAction = pFG->GetAIAction();
		temp = pFG->GetGroupName();
		if (!temp.IsEmpty()) temp += ":";
		if (pEntity)
		{
			temp += pEntity->GetName();
		}
		else if (pAIAction)
		{
			temp += pAIAction->GetName();
		}
		else
		{
			temp += pFG->GetName();
		}
		return temp;
	}

public:
	CSearchResult(CHyperFlowGraph* pGraph, HyperNodeID nodeId, CString context)
	{
		assert(pGraph != 0);
		m_pGraph = pGraph;
		m_nodeId = nodeId;
		CString name;
		CHyperNode* pNode = (CHyperNode*) pGraph->FindNode(m_nodeId);
		assert(pNode != 0);
		AddItem(new CXTPReportRecordItemText(GetFGName(m_pGraph)));
		CString node = pNode->GetTitle();
		node.AppendFormat("  (ID=%d)", m_nodeId);
		AddItem(new CXTPReportRecordItemText(node));
		CXTPReportRecordItem* pItem = AddItem(new CXTPReportRecordItemText(context));
		CXTPReportRecordItemPreview* pPreviewItem = new CXTPReportRecordItemPreview(context);
		SetPreviewItem(pPreviewItem);

		const CString delimiter(_T(": "));
		int hyPos = context.Find(delimiter);
		if (hyPos >= 0)
		{
			hyPos += delimiter.GetLength();
			int len = context.GetLength() - hyPos;
			assert(hyPos + len == context.GetLength());
			pItem->AddHyperlink(new CXTPReportHyperlink(hyPos, len));
			pPreviewItem->AddHyperlink(new CXTPReportHyperlink(hyPos, len));
		}
	}

	CHyperFlowGraph* m_pGraph;
	HyperNodeID      m_nodeId;
};

uint32 Option2Cat(CFlowGraphSearchOptions::EFindSpecial o)
{
	switch (o)
	{
	case CFlowGraphSearchOptions::eFLS_Approved:
		return EFLN_APPROVED;
	case CFlowGraphSearchOptions::eFLS_Advanced:
		return EFLN_ADVANCED;
	case CFlowGraphSearchOptions::eFLS_Debug:
		return EFLN_DEBUG;
	//case CFlowGraphSearchOptions::eFLS_Legacy:
	//	return EFLN_LEGACY;
	//case CFlowGraphSearchOptions::eFLS_WIP:
	//	return EFLN_WIP;
	case CFlowGraphSearchOptions::eFLS_Obsolete:
		return EFLN_OBSOLETE;
	default:
		return EFLN_DEBUG;
	}
}

//////////////////////////////////////////////////////////////////////////
class CFinder
{
	struct Matcher
	{
		enum Method
		{
			SLOPPY = 0,
			EXACT_MATCH,
		};

		bool operator()(const char* a, const char* b) const
		{
			switch (m)
			{
			default:
			case SLOPPY:
				return strstri(a, b) != 0;
				break;
			case EXACT_MATCH:
				return strcmp(a, b) == 0;
				break;
			}
		}
		Matcher(Method m) : m(m) {}
		Method m;
	};

public:
	CFinder(CFlowGraphSearchOptions* pOptions, CFlowGraphSearchCtrl::ResultVec& resultVec)
		: m_pOptions(pOptions),
		m_resultVec(resultVec),
		m_matcher(pOptions->m_bExactMatch ? Matcher::EXACT_MATCH : Matcher::SLOPPY)
	{
		m_cat = Option2Cat((CFlowGraphSearchOptions::EFindSpecial) m_pOptions->m_findSpecial);
	}

	bool Accept(CHyperFlowGraph* pFG, CHyperNode* pNode, HyperNodeID id)
	{
		switch (m_pOptions->m_LookinIndex)
		{
		case CFlowGraphSearchOptions::eFL_AIActions:
			if (pFG->GetAIAction() == 0)
				return false;
			break;
		case CFlowGraphSearchOptions::eFL_CustomActions:
			if (pFG->GetCustomAction() == 0)
				return false;
			break;
		case CFlowGraphSearchOptions::eFL_Entities:
			if (pFG->GetEntity() == 0)
				return false;
			break;
		}

		const Matcher& matches = m_matcher; // only for nice wording "matches"

		CString context;
		const CString& nodeName = pNode->GetTitle();

		// check if it's an special editor node
		if (pNode->IsEditorSpecialNode())
		{
			if (m_pOptions->m_findSpecial == CFlowGraphSearchOptions::eFLS_None)
			{
				if (matches(nodeName.GetString(), m_pOptions->m_strFind.GetString()) != 0)
				{
					context.Format("%s: %s (ID=%d)", pNode->GetClassName(), nodeName.GetString(), id);
					m_resultVec.push_back(CFlowGraphSearchCtrl::Result(pFG, id, context));
					return true;
				}
			}
			return false;
		}

		// we can be sure it is a normal node.
		CFlowNode* pFlowNode = static_cast<CFlowNode*>(pNode);

		if (m_pOptions->m_findSpecial == CFlowGraphSearchOptions::eFLS_None)
		{
			if (matches(nodeName.GetString(), m_pOptions->m_strFind.GetString()) != 0)
			{
				context.Format("Node: %s (ID=%d)", nodeName.GetString(), id);
				m_resultVec.push_back(CFlowGraphSearchCtrl::Result(pFG, id, context));
			}

			if (m_pOptions->m_bIncludeIDs)
			{
				char ids[32];
				cry_sprintf(ids, "(ID=%d)", id);
				if (matches(ids, m_pOptions->m_strFind.GetString()) != 0)
				{
					context.Format("Node: %s (ID=%d)", nodeName.GetString(), id);
					m_resultVec.push_back(CFlowGraphSearchCtrl::Result(pFG, id, context));
				}
			}

			if (m_pOptions->m_bIncludeEntities)
			{
				CEntityObject* pEntity = pFlowNode->GetEntity();
				if (pEntity != 0 && matches(pEntity->GetName(), m_pOptions->m_strFind.GetString()) != 0)
				{
					context.Format("Entity: %s", pEntity->GetName());
					m_resultVec.push_back(CFlowGraphSearchCtrl::Result(pFG, id, context));
				}
			}

			if (m_pOptions->m_bIncludePorts)
			{
				const CHyperNode::Ports& inputs = *pNode->GetInputs();
				for (int i = 0; i < inputs.size(); ++i)
				{
					if (inputs[i].pVar != 0 && (matches(inputs[i].pVar->GetHumanName(), m_pOptions->m_strFind.GetString()) != 0))
					{
						context.Format("InPort: %s", inputs[i].pVar->GetHumanName());
						m_resultVec.push_back(CFlowGraphSearchCtrl::Result(pFG, id, context));
					}
				}
				const CHyperNode::Ports& outputs = *pNode->GetOutputs();
				for (int i = 0; i < outputs.size(); ++i)
				{
					if (outputs[i].pVar != 0 && (matches(outputs[i].pVar->GetHumanName(), m_pOptions->m_strFind.GetString()) != 0))
					{
						context.Format("OutPort: %s", outputs[i].pVar->GetHumanName());
						m_resultVec.push_back(CFlowGraphSearchCtrl::Result(pFG, id, context));
					}
				}
			}
			if (m_pOptions->m_bIncludeValues)
			{
				// check port values
				CString val;
				CVarBlock* pVarBlock = pNode->GetInputsVarBlock();
				if (pVarBlock != 0)
				{
					for (int i = 0; i < pVarBlock->GetNumVariables(); ++i)
					{
						IVariable* pVar = pVarBlock->GetVariable(i);
						pVar->Get(val);
						if (matches(val.GetString(), m_pOptions->m_strFind.GetString()) != 0)
						{
							context.Format("Value: %s=%s", pVar->GetHumanName(), val.GetString());
							m_resultVec.push_back(CFlowGraphSearchCtrl::Result(pFG, id, context));
						}
					}
					delete pVarBlock;
				}
			}
		}
		else
		{
			// special find (category or NoEntity)
			if (m_pOptions->m_findSpecial == CFlowGraphSearchOptions::eFLS_NoEntity)
			{
				if (pNode->CheckFlag(EHYPER_NODE_ENTITY))
				{
					bool bEntityPortConnected = false;
					if (pNode->GetInputs() && (*pNode->GetInputs())[0].nConnected != 0)
						bEntityPortConnected = true;

					bool valid = (pNode->CheckFlag(EHYPER_NODE_ENTITY_VALID) ||
					              pNode->CheckFlag(EHYPER_NODE_GRAPH_ENTITY) ||
					              pNode->CheckFlag(EHYPER_NODE_GRAPH_ENTITY2) ||
					              bEntityPortConnected);
					if (!valid)
					{
						context.Format("No TargetEntity");
						m_resultVec.push_back(CFlowGraphSearchCtrl::Result(pFG, id, context));
					}
				}
			}
			// special find (NoLinks), not that fast
			else if (m_pOptions->m_findSpecial == CFlowGraphSearchOptions::eFLS_NoLinks)
			{
				bool bConnected = false;
				for (int pass = 0; pass < 2; ++pass)
				{
					CHyperNode::Ports* pPorts = pass == 0 ? pNode->GetInputs() : pNode->GetOutputs();
					for (int i = 0; i < pPorts->size(); ++i)
					{
						if ((*pPorts)[i].nConnected != 0)
						{
							bConnected = true;
							break;
						}
					}
					if (bConnected)
						break;
				}
				if (bConnected == false)
				{
					context.Format("No Links");
					m_resultVec.push_back(CFlowGraphSearchCtrl::Result(pFG, id, context));
				}
			}
			else
			{
				// category
				if (pFlowNode->GetCategory() == m_cat)
				{
					context.Format("Category: %s", pFlowNode->GetCategoryName());
					m_resultVec.push_back(CFlowGraphSearchCtrl::Result(pFG, id, context));
				}
			}
		}
		return false;
	}

protected:
	CFlowGraphSearchOptions*         m_pOptions;
	CFlowGraphSearchCtrl::ResultVec& m_resultVec;
	Matcher                          m_matcher;
	uint32                           m_cat;
};

//////////////////////////////////////////////////////////////////////////
void CFlowGraphSearchCtrl::DoTheFind(ResultVec& vec)
{
	CFlowGraphSearchOptions* pOptions = CFlowGraphSearchOptions::GetSearchOptions();
	CFinder finder(pOptions, vec);

	if (pOptions->m_LookinIndex == CFlowGraphSearchOptions::eFL_Current)
	{
		CHyperFlowGraph* pFG = (CHyperFlowGraph*) m_pDialog->GetGraphView()->GetGraph();
		if (!pFG)
			return;

		IHyperGraphEnumerator* pEnum = pFG->GetNodesEnumerator();
		for (IHyperNode* pINode = pEnum->GetFirst(); pINode != 0; pINode = pEnum->GetNext())
		{
			finder.Accept(pFG, (CHyperNode*) pINode, pINode->GetId());
		}
		pEnum->Release();
	}
	else
	{
		CFlowGraphManager* pFGMgr = GetIEditorImpl()->GetFlowGraphManager();
		size_t numFG = pFGMgr->GetFlowGraphCount();
		for (size_t i = 0; i < numFG; ++i)
		{
			CHyperFlowGraph* pFG = pFGMgr->GetFlowGraph(i);
			IHyperGraphEnumerator* pEnum = pFG->GetNodesEnumerator();
			for (IHyperNode* pINode = pEnum->GetFirst(); pINode != 0; pINode = pEnum->GetNext())
			{
				finder.Accept(pFG, (CHyperNode*) pINode, pINode->GetId());
			}
			pEnum->Release();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphSearchCtrl::OnButtonFindAll()
{
	FindAll(false);
}
//////////////////////////////////////////////////////////////////////////
void CFlowGraphSearchCtrl::OnIncludePortsClicked()
{
	CFlowGraphSearchOptions::GetSearchOptions()->m_bIncludePorts = IsDlgButtonChecked(IDC_CHECK_INCLUDE_PORTS);
	CFlowGraphSearchOptions::GetSearchOptions()->Serialize(false);
}
//////////////////////////////////////////////////////////////////////////
void CFlowGraphSearchCtrl::OnIncludeValuesClicked()
{
	CFlowGraphSearchOptions::GetSearchOptions()->m_bIncludeValues = IsDlgButtonChecked(IDC_CHECK_INCLUDE_VALUES);
	CFlowGraphSearchOptions::GetSearchOptions()->Serialize(false);
}
//////////////////////////////////////////////////////////////////////////
void CFlowGraphSearchCtrl::OnIncludeEntitiesClicked()
{
	CFlowGraphSearchOptions::GetSearchOptions()->m_bIncludeEntities = IsDlgButtonChecked(IDC_CHECK_INCLUDE_ENTITIES);
	CFlowGraphSearchOptions::GetSearchOptions()->Serialize(false);
}
//////////////////////////////////////////////////////////////////////////
void CFlowGraphSearchCtrl::OnIncludeIDsClicked()
{
	CFlowGraphSearchOptions::GetSearchOptions()->m_bIncludeIDs = IsDlgButtonChecked(IDC_CHECK_INCLUDE_IDS);
	CFlowGraphSearchOptions::GetSearchOptions()->Serialize(false);
}
//////////////////////////////////////////////////////////////////////////
void CFlowGraphSearchCtrl::FindAll(bool bSelectFirst)
{
	assert(m_pResultsCtrl != 0);

	// update search options
	UpdateData();
	CFlowGraphSearchOptions* pOptions = CFlowGraphSearchOptions::GetSearchOptions();

	// add search string to history
	AddComboHistory(m_cmbFind, pOptions->m_strFind, pOptions->m_lstFindHistory);

	// perform search
	ResultVec resultVec;
	DoTheFind(resultVec);

	// update result control
	m_pResultsCtrl->BeginUpdate();
	CXTPReportRecords* pRecords = m_pResultsCtrl->GetRecords();
	pRecords->RemoveAll();
	ResultVec::iterator iter(resultVec.begin());
	while (iter != resultVec.end())
	{
		const Result& result = *iter;
		pRecords->Add(new CSearchResult(result.m_pGraph, result.m_nodeId, result.m_context));
		++iter;
	}
	m_pResultsCtrl->EndUpdate();
	m_pResultsCtrl->Populate();

	// force showing of results pane if we found something
	m_pDialog->ShowResultsPane(true, resultVec.size() != 0);

	if (bSelectFirst)
	{
		m_pResultsCtrl->SelectFirstResult();
		m_pResultsCtrl->ShowSelectedResult();
	}

	CFlowGraphSearchOptions::GetSearchOptions()->Serialize(false);

	// keep focus if no results
	// if (resultVec.size () == 0)
	//	GotoDlgCtrl(&m_cmbFind);
}

//////////////////////////////////////////////////////////////////////////
BOOL CFlowGraphSearchCtrl::PreTranslateMessage(MSG* pMsg)
{
#if 0
	if (pMsg->message == WM_KEYDOWN || pMsg->message == WM_SYSKEYDOWN)
	{
		if (::GetFocus() == m_hWnd)
		{
			if (IsDialogMessage(pMsg))
				return TRUE;
		}
		CDialog
	}
#endif
	return CXTResizeDialog::PreTranslateMessage(pMsg);
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphSearchCtrl::UpdateOptions()
{
	UpdateData();
	bool enable = CFlowGraphSearchOptions::GetSearchOptions()->m_findSpecial == CFlowGraphSearchOptions::eFLS_None;
	m_cmbFind.EnableWindow(enable);
	GetDlgItem(IDC_CHECK_INCLUDE_PORTS)->EnableWindow(enable);
	GetDlgItem(IDC_CHECK_INCLUDE_VALUES)->EnableWindow(enable);
	GetDlgItem(IDC_CHECK_INCLUDE_ENTITIES)->EnableWindow(enable);
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphSearchCtrl::OnOptionChanged()
{
	UpdateOptions();
	CFlowGraphSearchOptions::GetSearchOptions()->Serialize(false);
}

// called from CHyperGraphDialog...
void CFlowGraphSearchCtrl::SetResultsCtrl(CFlowGraphSearchResultsCtrl* pCtrl)
{
	m_pResultsCtrl = pCtrl;
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphSearchCtrl::Find(const CString& searchString, bool bSetTextOnly, bool bSelectFirst, bool bTempExactMatch)
{
	CFlowGraphSearchOptions::GetSearchOptions()->m_strFind = searchString;
	CFlowGraphSearchOptions::GetSearchOptions()->m_bExactMatch = bTempExactMatch ? TRUE : FALSE;
	UpdateData(false);
	UpdateOptions();
	if (bSetTextOnly == false)
	{
		FindAll(bSelectFirst);
	}
	CFlowGraphSearchOptions::GetSearchOptions()->m_bExactMatch = FALSE;
}

//////////////////////////////////////////////////////////////////////////
// CFlowGraphSearchResultsCtrl
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CFlowGraphSearchResultsCtrl, CXTPReportControl)
ON_WM_DESTROY()
ON_NOTIFY_REFLECT(NM_RETURN, OnReportItemDblClick)
ON_NOTIFY_REFLECT(NM_KEYDOWN, OnReportKeyDown)
ON_NOTIFY_REFLECT(NM_DBLCLK, OnReportItemDblClick)
ON_NOTIFY_REFLECT(XTP_NM_REPORT_HYPERLINK, OnReportItemHyperlink)
ON_NOTIFY_REFLECT(XTP_NM_REPORT_HEADER_RCLICK, OnReportColumnRClick)
END_MESSAGE_MAP()
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
CFlowGraphSearchResultsCtrl::CFlowGraphSearchResultsCtrl(CHyperGraphDialog* pDialog)
{
	class CMyPaintManager : public CXTPReportPaintManager
	{
	public:
		/*
		   virtual void DrawGroupRow( CDC* pDC, CXTPReportGroupRow* pRow, CRect rcRow)
		   {
		   HGDIOBJ oldFont = m_fontText.Detach();
		   m_fontText.Attach( m_fontBoldText.Detach() );
		   __super::DrawGroupRow( pDC, pRow, rcRow );
		   m_fontBoldText.Attach( m_fontText.Detach() );
		   m_fontText.Attach( oldFont );

		   if ( !pRow->GetTreeDepth() )
		   {
		    rcRow.bottom = rcRow.top + 1;
		    pDC->FillSolidRect( rcRow, 0 );
		   }
		   }
		   virtual void DrawFocusedRow( CDC* pDC, CRect rcRow )
		   {
		   }
		 */
		virtual int GetRowHeight(CDC* pDC, CXTPReportRow* pRow)
		{
			return __super::GetRowHeight(pDC, pRow) + (pRow->IsGroupRow() ? 2 : 0);
		}
		virtual int GetHeaderHeight()
		{
			return CXTPReportPaintManager::GetHeaderHeight();
		}
	};

	m_pDialog = pDialog;
	CXTPReportColumn* pGraphCol = AddColumn(new CXTPReportColumn(0, _T("Graph"), 50, TRUE, XTP_REPORT_NOICON, TRUE, TRUE));
	CXTPReportColumn* pNodeCol = AddColumn(new CXTPReportColumn(1, _T("Node"), 50, TRUE, XTP_REPORT_NOICON, TRUE, TRUE));
	CXTPReportColumn* pContextCol = AddColumn(new CXTPReportColumn(2, _T("Context"), 50, TRUE, XTP_REPORT_NOICON, TRUE, TRUE));
	GetReportHeader()->AllowColumnRemove(TRUE);
	ShadeGroupHeadings(TRUE);
	SkipGroupsFocus(TRUE);
	SetMultipleSelection(FALSE);
	SetPaintManager(new CMyPaintManager);
	GetPaintManager()->m_bShadeSortColumn = FALSE;
	GroupBy(pGraphCol);

	// Serialize states
	UINT nSize = 0;
	LPBYTE pbtData = NULL;
	CXTRegistryManager regManager;
	if (regManager.GetProfileBinary("Dialogs\\FlowGraphSearchResults", "Configuration", &pbtData, &nSize))
	{
		CCryMemFile memFile(pbtData, nSize);
		CArchive ar(&memFile, CArchive::load);
		SerializeState(ar);
		RedrawControl();
	}
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphSearchResultsCtrl::OnDestroy()
{
	CCryMemFile memFile(new BYTE[256], 256, 256);
	CArchive ar(&memFile, CArchive::store);
	SerializeState(ar);
	ar.Close();

	UINT nSize = (UINT)memFile.GetLength();
	LPBYTE pbtData = memFile.GetMemPtr();
	CXTRegistryManager regManager;
	regManager.WriteProfileBinary("Dialogs\\FlowGraphSearchResults", "Configuration", pbtData, nSize);

	__super::OnDestroy();
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphSearchResultsCtrl::OnReportKeyDown(NMHDR* pNotifyStruct, LRESULT* result)
{
	LPNMKEY lpNMKey = (LPNMKEY)pNotifyStruct;
	if (lpNMKey->nVKey == VK_RETURN)
		ShowSelectedResult();
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphSearchResultsCtrl::OnReportItemHyperlink(NMHDR* pNotifyStruct, LRESULT* result)
{
	XTP_NM_REPORTRECORDITEM* pItemNotify = (XTP_NM_REPORTRECORDITEM*) pNotifyStruct;
}

#define ID_GROUP_BY_THIS   1
#define ID_COLUMN_BEST_FIT 2
#define ID_SORT_ASC        3
#define ID_SORT_DESC       4
#define ID_SHOW_GROUPBOX   5
#define ID_UNGROUP         6
#define ID_ENABLE_PREVIEW  7

#define ID_COLUMN_SHOW     100

//////////////////////////////////////////////////////////////////////////
void CFlowGraphSearchResultsCtrl::OnReportColumnRClick(NMHDR* pNotifyStruct, LRESULT* result)
{
	XTP_NM_REPORTRECORDITEM* pItemNotify = (XTP_NM_REPORTRECORDITEM*) pNotifyStruct;
	ASSERT(pItemNotify->pColumn);
	CPoint ptClick = pItemNotify->pt;

	CMenu menu;
	VERIFY(menu.CreatePopupMenu());

	// create main menu items
	menu.AppendMenu(MF_STRING, ID_SORT_ASC, _T("Sort ascending"));
	menu.AppendMenu(MF_STRING, ID_SORT_DESC, _T("Sort descending"));
	menu.AppendMenu(MF_SEPARATOR, (UINT)-1, (LPCTSTR)NULL);
	menu.AppendMenu(MF_STRING, ID_GROUP_BY_THIS, _T("Group by this field"));
	menu.AppendMenu(MF_STRING, ID_UNGROUP, _T("Ungroup"));
	// menu.AppendMenu(MF_STRING, ID_SHOW_GROUPBOX, _T("Show GroupBy Box"));
	menu.AppendMenu(MF_SEPARATOR, (UINT)-1, (LPCTSTR)NULL);
	menu.AppendMenu(MF_STRING, ID_ENABLE_PREVIEW, _T("Enable Preview"));

	if (IsGroupByVisible())
	{
		menu.CheckMenuItem(ID_SHOW_GROUPBOX, MF_BYCOMMAND | MF_CHECKED);
	}
	if (GetReportHeader()->IsShowItemsInGroups())
	{
		menu.EnableMenuItem(ID_GROUP_BY_THIS, MF_BYCOMMAND | MF_DISABLED);
	}
	if (IsPreviewMode())
	{
		menu.CheckMenuItem(ID_ENABLE_PREVIEW, MF_BYCOMMAND | MF_CHECKED);
	}

	CXTPReportColumns* pColumns = GetColumns();
	CXTPReportColumn* pColumn = pItemNotify->pColumn;

	// create columns items
	int nColumnCount = pColumns->GetCount();
	int nColumn;
	CMenu menuColumns;
	VERIFY(menuColumns.CreatePopupMenu());
	for (nColumn = 0; nColumn < nColumnCount; nColumn++)
	{
		CXTPReportColumn* pCol = pColumns->GetAt(nColumn);
		CString sCaption = pCol->GetCaption();
		//if (!sCaption.IsEmpty())
		menuColumns.AppendMenu(MF_STRING, ID_COLUMN_SHOW + nColumn, sCaption);
		menuColumns.CheckMenuItem(ID_COLUMN_SHOW + nColumn,
		                          MF_BYCOMMAND | (pCol->IsVisible() ? MF_CHECKED : MF_UNCHECKED));
	}

	menu.InsertMenu(0, MF_BYPOSITION | MF_POPUP, (UINT_PTR) menuColumns.m_hMenu, _T("Columns"));

	// track menu
	int nMenuResult = CXTPCommandBars::TrackPopupMenu(&menu, TPM_NONOTIFY | TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON, ptClick.x, ptClick.y, this, NULL);

	// process column selection item
	if (nMenuResult >= ID_COLUMN_SHOW)
	{
		CXTPReportColumn* pCol = pColumns->GetAt(nMenuResult - ID_COLUMN_SHOW);
		if (pCol)
		{
			pCol->SetVisible(!pCol->IsVisible());
			// sanity check: at least one column must be visible
			int visible = 0;
			for (int i = 0; i < nColumnCount; ++i)
				visible += pColumns->GetAt(i)->IsVisible() ? 1 : 0;
			if (visible == 0)
				pCol->SetVisible(TRUE);
		}
	}

	// other general items
	switch (nMenuResult)
	{
	case ID_SORT_ASC:
	case ID_SORT_DESC:
		if (pColumn && pColumn->IsSortable())
		{
			pColumns->SetSortColumn(pColumn, nMenuResult == ID_SORT_ASC);
			Populate();
		}
		break;
	case ID_UNGROUP:
		GroupBy(0);
		break;
	case ID_GROUP_BY_THIS:
		GroupBy(pColumn);
		break;
	case ID_SHOW_GROUPBOX:
		ShowGroupBy(!IsGroupByVisible());
		break;
	case ID_COLUMN_BEST_FIT:
		GetColumns()->GetReportHeader()->BestFit(pColumn);
		break;
	case ID_ENABLE_PREVIEW:
		EnablePreviewMode(!IsPreviewMode());
		Populate();
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphSearchResultsCtrl::GroupBy(CXTPReportColumn* pColumn)
{
	CXTPReportColumns* pColumns = GetColumns();

	while (pColumns->GetGroupsOrder()->GetCount() > 0)
	{
		CXTPReportColumn* pTCol = pColumns->GetGroupsOrder()->GetAt(0);
		pTCol->SetVisible(TRUE);
		pColumns->GetGroupsOrder()->Remove(pTCol);
	}
	if (pColumn)
	{
		pColumns->GetGroupsOrder()->Add(pColumn);
		pColumn->SetVisible(FALSE);
	}
	Populate();
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphSearchResultsCtrl::OnReportItemDblClick(NMHDR* /* pNotifyStruct*/, LRESULT* /* result */)
{
	ShowSelectedResult();
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphSearchResultsCtrl::SelectFirstResult()
{
	CXTPReportRows* pRows = GetRows();
	if (pRows == 0)
		return;
	int count = pRows->GetCount();
	if (count <= 0)
		return;
	for (int i = 0; i < count; ++i)
	{
		CXTPReportRow* pRow = pRows->GetAt(i);
		if (pRow != 0)
		{
			CSearchResult* pRecord = DYNAMIC_DOWNCAST(CSearchResult, pRow->GetRecord());
			if (pRecord)
			{
				SetFocusedRow(pRow, true);
				return;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphSearchResultsCtrl::ShowSelectedResult()
{
	CXTPReportRow* pRow = this->GetFocusedRow();
	if (pRow != 0)
	{
		CSearchResult* pRecord = DYNAMIC_DOWNCAST(CSearchResult, pRow->GetRecord());
		if (pRecord)
		{
			// make sure graph still exists
			CFlowGraphManager* pFGMgr = GetIEditorImpl()->GetFlowGraphManager();
			size_t numFG = pFGMgr->GetFlowGraphCount();
			for (size_t i = 0; i < numFG; ++i)
			{
				CHyperFlowGraph* pFG = pFGMgr->GetFlowGraph(i);
				if (pFG == pRecord->m_pGraph)
				{
					m_pDialog->SetGraph(pRecord->m_pGraph);
					CHyperNode* pNode = (CHyperNode*) pRecord->m_pGraph->FindNode(pRecord->m_nodeId);
					if (pNode)
					{
						m_pDialog->GetGraphView()->ShowAndSelectNode(pNode, true);
					}
					break;
				}
			}
		}
	}
}

