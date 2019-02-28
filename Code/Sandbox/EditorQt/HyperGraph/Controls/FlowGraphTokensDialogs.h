// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Controls/PropertyCtrl.h"
#include "HyperGraph/FlowGraph.h"
#include "resource.h"

class CHyperGraphDialog;
class CXTPTaskPanel;
class CHyperGraph;


//////////////////////////////////////////////////////////////////////////
//!	Read-only display of graph tokens in the right-hand properties panel
class CFlowGraphTokensCtrl : public CWnd
{
public:
	DECLARE_DYNAMIC(CFlowGraphTokensCtrl)

	CFlowGraphTokensCtrl(CHyperGraphDialog* pParent, CXTPTaskPanel* pPanel);
	virtual ~CFlowGraphTokensCtrl();

	BOOL Create(DWORD dwStyle, const CRect& rc, CWnd* pParentWnd, UINT nID);

	void SetGraph(CHyperGraph* pGraph);
	void UpdateGraphProperties(CHyperGraph* pGraph);

protected:
	DECLARE_MESSAGE_MAP()

	void Init(int nID);
	void FillProps();
	void ResizeProps(bool bHide = false);

protected:

	CSmartVariableEnum<CString> CreateVariable(const char* name, EFlowDataTypes type);

	CHyperFlowGraph*        m_pGraph;
	CHyperGraphDialog*      m_pParent;
	CXTPTaskPanel*          m_pTaskPanel;
	CXTPTaskPanelGroup*     m_pGroup;
	CXTPTaskPanelGroupItem* m_pPropItem;

	CPropertyCtrl           m_graphProps;

	bool                    m_bUpdate;
};



struct STokenData
{
	STokenData() : type(eFDT_Void)
	{}

	inline bool operator==(const STokenData& other)
	{
		return (name == other.name) && (type == other.type);
	}

	CString        name;
	EFlowDataTypes type;
};
typedef std::vector<STokenData> TTokens;


//////////////////////////////////////////////////////////////////////////
//! Popup dialog allowing adding/removing/editing graph tokens
class CFlowGraphTokensDlg : public CDialog
{
	DECLARE_DYNAMIC(CFlowGraphTokensDlg)

public:
	CFlowGraphTokensDlg(CWnd* pParent = NULL);   // standard constructor

	// Dialog Data
	enum { IDD = IDD_FG_EDIT_GRAPH_TOKEN };

	CButton  m_newTokenButton;
	CButton  m_deleteTokenButton;
	CButton  m_editTokenButton;
	CListBox m_tokenListCtrl;

	void     RefreshControl();

	TTokens& GetTokenData()                      { return m_tokens; }
	void     SetTokenData(const TTokens& tokens) { m_tokens = tokens; }

protected:
	static bool  SortByAsc(const STokenData& lhs, const STokenData& rhs) { return lhs.name < rhs.name; }
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();

	afx_msg void OnCommand_NewToken();
	afx_msg void OnCommand_DeleteToken();
	afx_msg void OnCommand_EditToken();

	DECLARE_MESSAGE_MAP()

	TTokens m_tokens;
};

//////////////////////////////////////////////////////////////////////////
// CFlowGraphNewTokenDlg - popup dialog creating a new graph token
//	or editing an existing one
//////////////////////////////////////////////////////////////////////////

class CFlowGraphNewTokenDlg : public CDialog
{
	DECLARE_DYNAMIC(CFlowGraphNewTokenDlg)

public:
	CFlowGraphNewTokenDlg(STokenData* pTokenData, TTokens tokenList, CWnd* pParent = NULL);   // standard constructor
	virtual ~CFlowGraphNewTokenDlg() {}

	// Dialog Data
	enum { IDD = IDD_FG_NEW_GRAPH_TOKEN };

	CComboBox m_tokenTypesCtrl;
	CEdit     m_tokenNameCtrl;

protected:

	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();

	virtual void OnOK();

	DECLARE_MESSAGE_MAP()

	void RefreshControl();
	bool DoesTokenExist(CString tokenName);

	STokenData* m_pTokenData;
	TTokens     m_tokens;
};

