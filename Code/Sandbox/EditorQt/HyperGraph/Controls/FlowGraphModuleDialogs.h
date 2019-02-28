// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryFlowGraph/IFlowGraphModuleManager.h>
#include "resource.h"
#include "DragNDropListBox.h"

class CHyperGraphDialog;
class CXTPTaskPanel;
class CHyperGraph;


typedef std::vector<IFlowGraphModule::SModulePortConfig> TPorts;

//////////////////////////////////////////////////////////////////////////
//! CFlowGraphEditModuleDlg - popup dialog allowing adding/removing/editing module inputs + outputs
class CFlowGraphEditModuleDlg : public CDialog
{
	DECLARE_DYNAMIC(CFlowGraphEditModuleDlg)

public:
	CFlowGraphEditModuleDlg(IFlowGraphModule* pModule, CWnd* pParent = NULL);   // standard constructor

	static CString        GetDataTypeName(EFlowDataTypes type);
	static EFlowDataTypes GetDataType(const CString& itemType);
	static void           FillDataTypes(CComboBox& comboBox);

protected:

	// Dialog Data
	enum { IDD = IDD_FG_EDIT_MODULE };

	CDragNDropListBox m_inputListCtrl;
	CDragNDropListBox m_outputListCtrl;

	void         RefreshControl();

	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();

	afx_msg void OnCommand_NewInput();
	afx_msg void OnCommand_DeleteInput();
	afx_msg void OnCommand_EditInput();
	void OnCommand_ReorderInput(int oldPos, int newPos);

	afx_msg void OnCommand_NewOutput();
	afx_msg void OnCommand_DeleteOutput();
	afx_msg void OnCommand_EditOutput();
	void OnCommand_ReorderOutput(int oldPos, int newPos);

	virtual void OnOK();

	DECLARE_MESSAGE_MAP()
	
	IFlowGraphModule* m_pModule;
	TPorts m_inputs;
	TPorts m_outputs;

private:
	void AddDataTypes();
	typedef std::map<EFlowDataTypes, CString> TDataTypes;
	static TDataTypes m_flowDataTypes;
};

//////////////////////////////////////////////////////////////////////////
//! Popup dialog creating a new module input/output or editing an existing one
class CFlowGraphNewModuleInputDlg : public CDialog
{
	DECLARE_DYNAMIC(CFlowGraphNewModuleInputDlg)

public:
	CFlowGraphNewModuleInputDlg(IFlowGraphModule::SModulePortConfig* pPort, TPorts* pOtherPorts, CWnd* pParent = NULL);   // standard constructor
	virtual ~CFlowGraphNewModuleInputDlg() {}

protected:

	// Dialog Data
	enum { IDD = IDD_FG_NEW_MODULE_INPUT };

	CComboBox m_inputTypesCtrl;
	CEdit     m_inputNameCtrl;

	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();

	virtual void OnOK();

	DECLARE_MESSAGE_MAP()

	void RefreshControl();

	IFlowGraphModule::SModulePortConfig* m_pPort;
	TPorts* m_pOtherPorts;
};

