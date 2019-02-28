// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FlowGraphTokensDialogs.h"

#include "HyperGraph/FlowGraph.h"
#include "HyperGraphEditorWnd.h"

#include "Controls/QuestionDialog.h"

#define IDC_GRAPH_PROPERTIES 6

namespace
{
CString typeNames[] =
{
	"Void",       //  eFDT_Void
	"Int",        //  eFDT_Int
	"Float",      //  eFDT_Float
	"EntityId",   //  eFDT_EntityId
	"Vec3",       //  eFDT_Vec3
	"String",     //  eFDT_String
	"Bool",       //  eFDT_Bool
};
};

//////////////////////////////////////////////////////////////////////////
// CFlowGraphTokensCtrl
//////////////////////////////////////////////////////////////////////////

IMPLEMENT_DYNAMIC(CFlowGraphTokensCtrl, CWnd)

CFlowGraphTokensCtrl::CFlowGraphTokensCtrl(CHyperGraphDialog* pParent, CXTPTaskPanel* pPanel)
	: m_pParent(pParent),
	m_pTaskPanel(pPanel),
	m_pGraph(0),
	m_bUpdate(false)
{
}

CFlowGraphTokensCtrl::~CFlowGraphTokensCtrl()
{
}

BEGIN_MESSAGE_MAP(CFlowGraphTokensCtrl, CWnd)
END_MESSAGE_MAP()

BOOL CFlowGraphTokensCtrl::Create(DWORD dwStyle, const CRect& rc, CWnd* pParentWnd, UINT nID)
{
	__super::Create(NULL, NULL, dwStyle, rc, pParentWnd, nID);
	Init(nID);
	return TRUE;
}

void CFlowGraphTokensCtrl::Init(int nID)
{
	m_pGroup = m_pTaskPanel->AddGroup(nID);
	m_pGroup->SetCaption(_T("Graph Tokens"));
	m_pGroup->SetItemLayout(xtpTaskItemLayoutDefault);

	m_graphProps.Create(WS_CHILD | WS_VISIBLE, CRect(0, 0, 100, 100), this, IDC_GRAPH_PROPERTIES);
	m_graphProps.ModifyStyleEx(0, WS_EX_CLIENTEDGE);
	m_graphProps.SetParent(m_pTaskPanel);
	m_graphProps.SetItemHeight(16);
	m_graphProps.SetFlags(m_graphProps.GetFlags() | CPropertyCtrl::F_READ_ONLY);

	m_pPropItem = m_pGroup->AddControlItem(m_graphProps.GetSafeHwnd());

	FillProps();

	ResizeProps(true);
	m_pTaskPanel->ExpandGroup(m_pGroup, TRUE);
}

void CFlowGraphTokensCtrl::SetGraph(CHyperGraph* pGraph)
{
	if (pGraph && pGraph->IsFlowGraph())
	{
		m_pGraph = static_cast<CHyperFlowGraph*>(pGraph);
	}
	else
		m_pGraph = 0;

	if (m_pGraph)
	{
		FillProps();
	}
	else
	{
		m_graphProps.RemoveAllItems();
		ResizeProps(true);
	}
}

void CFlowGraphTokensCtrl::FillProps()
{
	if (m_pGraph == 0)
		return;

	CVarBlockPtr pVB = new CVarBlock;

	if (m_pGraph)
	{
		IFlowGraph* pIFlowGraph = m_pGraph->GetIFlowGraph();
		if (pIFlowGraph)
		{
			size_t gtCount = pIFlowGraph->GetGraphTokenCount();
			for (size_t i = 0; i < gtCount; ++i)
			{
				const IFlowGraph::SGraphToken* pToken = pIFlowGraph->GetGraphToken(i);
				if (pToken)
				{
					pVB->AddVariable(CreateVariable(pToken->name.c_str(), pToken->type), pToken->name.c_str());
				}
			}
		}
	}

	m_graphProps.RemoveAllItems();
	m_graphProps.AddVarBlock(pVB);

	ResizeProps(pVB->IsEmpty());
}

void CFlowGraphTokensCtrl::ResizeProps(bool bHide)
{
	CRect rc;
	m_pTaskPanel->GetClientRect(rc);
	// Resize to fit properties.
	int h = m_graphProps.GetVisibleHeight();
	m_graphProps.SetWindowPos(NULL, 0, 0, rc.right, h + 1 + 4, SWP_NOMOVE);
	m_pPropItem->SetControlHandle(m_graphProps.GetSafeHwnd());
	m_pPropItem->SetVisible(!bHide);
	m_pTaskPanel->Reposition(false);
}

void CFlowGraphTokensCtrl::UpdateGraphProperties(CHyperGraph* pGraph)
{
	if (m_bUpdate)
		return;

	if (pGraph && pGraph->IsFlowGraph())
	{
		CHyperFlowGraph* pFG = static_cast<CHyperFlowGraph*>(pGraph);
		if (pFG == m_pGraph)
		{
			FillProps();
		}
	}
}

CSmartVariableEnum<CString> CFlowGraphTokensCtrl::CreateVariable(const char* name, EFlowDataTypes type)
{
	CSmartVariableEnum<CString> variable;
	variable->SetDescription(name);
	variable->Set(typeNames[type]);

	return variable;
}

//////////////////////////////////////////////////////////////////////////
// CFlowGraphEditTokensDlg
//////////////////////////////////////////////////////////////////////////

IMPLEMENT_DYNAMIC(CFlowGraphTokensDlg, CDialog)

CFlowGraphTokensDlg::CFlowGraphTokensDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CFlowGraphTokensDlg::IDD, pParent)
{
}

void CFlowGraphTokensDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_ADD_GRAPHTOKEN, m_newTokenButton);
	DDX_Control(pDX, IDC_DELETE_GRAPHTOKEN, m_deleteTokenButton);
	DDX_Control(pDX, IDC_GRAPH_TOKEN_LIST, m_tokenListCtrl);
}

BOOL CFlowGraphTokensDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	RefreshControl();

	return TRUE;
}

BEGIN_MESSAGE_MAP(CFlowGraphTokensDlg, CDialog)
ON_COMMAND(IDC_ADD_GRAPHTOKEN, OnCommand_NewToken)
ON_COMMAND(IDC_DELETE_GRAPHTOKEN, OnCommand_DeleteToken)
ON_COMMAND(IDC_EDIT_GRAPHTOKEN, OnCommand_EditToken)
END_MESSAGE_MAP()

void CFlowGraphTokensDlg::OnCommand_NewToken()
{
	STokenData temp;
	temp.type = eFDT_Bool;
	CFlowGraphNewTokenDlg dlg(&temp, m_tokens);
	if (dlg.DoModal() == IDOK && temp.name != "" && temp.type != eFDT_Void)
	{
		m_tokens.push_back(temp);
	}

	RefreshControl();
}

void CFlowGraphTokensDlg::OnCommand_DeleteToken()
{
	int selected = m_tokenListCtrl.GetSelCount();
	std::vector<int> listSelections(selected);
	m_tokenListCtrl.GetSelItems(selected, &listSelections[0]);

	for (int i = 0; i < selected; ++i)
	{
		int item = listSelections[i];

		CString temp;
		m_tokenListCtrl.GetText(item, temp);

		m_tokens[item].name = "";
		m_tokens[item].type = eFDT_Void;
	}

	stl::find_and_erase_all(m_tokens, STokenData());

	RefreshControl();
}

void CFlowGraphTokensDlg::OnCommand_EditToken()
{
	int selected = m_tokenListCtrl.GetSelCount();

	if (selected == 1)
	{
		std::vector<int> listSelections(selected);
		m_tokenListCtrl.GetSelItems(selected, &listSelections[0]);

		int item = listSelections[0];

		CFlowGraphNewTokenDlg dlg(&m_tokens[item], m_tokens);
		dlg.DoModal();

		RefreshControl();
	}
}

void CFlowGraphTokensDlg::RefreshControl()
{
	m_tokenListCtrl.ResetContent();
	int i = 0;
	std::sort(m_tokens.begin(), m_tokens.end(), SortByAsc);
	for (TTokens::const_iterator it = m_tokens.begin(), end = m_tokens.end(); it != end; ++it, ++i)
	{
		CString text = it->name;
		text += " (";
		text += typeNames[it->type];
		text += ")";
		m_tokenListCtrl.AddString(text.GetBuffer());
	}
}

//////////////////////////////////////////////////////////////////////////
// CFlowGraphNewTokenDlg
//////////////////////////////////////////////////////////////////////////

IMPLEMENT_DYNAMIC(CFlowGraphNewTokenDlg, CDialog)

CFlowGraphNewTokenDlg::CFlowGraphNewTokenDlg(STokenData* pTokenData, TTokens tokenList, CWnd* pParent /*=NULL*/)
	: CDialog(CFlowGraphNewTokenDlg::IDD, pParent)
{
	m_pTokenData = pTokenData;
	m_tokens = tokenList;
}

void CFlowGraphNewTokenDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_GRAPH_TOKEN_TYPE, m_tokenTypesCtrl);
	DDX_Control(pDX, IDC_GRAPH_TOKEN_NAME, m_tokenNameCtrl);
}

BOOL CFlowGraphNewTokenDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// start at 1 to skip 'void' type
	for (int i = 1; i < sizeof(typeNames) / sizeof(CString); ++i)
	{
		m_tokenTypesCtrl.AddString(typeNames[i]);
		m_tokenTypesCtrl.SetItemData(i, i);
	}

	m_tokenNameCtrl.SetWindowText(m_pTokenData->name.GetBuffer());

	m_tokenTypesCtrl.SetWindowText(typeNames[m_pTokenData->type]);

	return TRUE;
}

BEGIN_MESSAGE_MAP(CFlowGraphNewTokenDlg, CDialog)
END_MESSAGE_MAP()

void CFlowGraphNewTokenDlg::OnOK()
{
	// set token data from controls; close
	if (m_pTokenData)
	{
		CString newTokenName;
		m_tokenNameCtrl.GetWindowText(newTokenName);

		// handle 'edit' or 'new' token command
		if (m_pTokenData->name == newTokenName || !DoesTokenExist(newTokenName))
		{
			m_pTokenData->name = newTokenName;

			CString itemType;
			m_tokenTypesCtrl.GetWindowText(itemType);

			for (int i = 0; i < sizeof(typeNames) / sizeof(CString); ++i)
			{
				if (itemType == typeNames[i])
				{
					m_pTokenData->type = (EFlowDataTypes)i;
					break;
				}
			}

			CDialog::OnOK();
		}
		else
		{
			CQuestionDialog::SWarning(QObject::tr(""), QObject::tr("Token name already in use"));
		}
	}
}

bool CFlowGraphNewTokenDlg::DoesTokenExist(CString tokenName)
{
	for (int i = 0; i < m_tokens.size(); ++i)
	{
		if (m_tokens[i].name == tokenName)
		{
			return true;
		}
	}
	return false;
}

