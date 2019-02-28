// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FlowGraphModuleDialogs.h"

#include <regex>

#include "HyperGraph/FlowGraphModuleManager.h"
#include "HyperGraph/FlowGraphManager.h"
#include "HyperGraph/FlowGraph.h"
#include "HyperGraphEditorWnd.h"


//////////////////////////////////////////////////////////////////////////
// CFlowGraphEditModuleDlg
//////////////////////////////////////////////////////////////////////////

std::map<EFlowDataTypes, CString> CFlowGraphEditModuleDlg::m_flowDataTypes;

IMPLEMENT_DYNAMIC(CFlowGraphEditModuleDlg, CDialog)

CFlowGraphEditModuleDlg::CFlowGraphEditModuleDlg(IFlowGraphModule* pModule, CWnd* pParent /*=NULL*/)
	: CDialog(CFlowGraphEditModuleDlg::IDD, pParent)
	, m_inputListCtrl(functor(*this, &CFlowGraphEditModuleDlg::OnCommand_ReorderInput))
	, m_outputListCtrl(functor(*this, &CFlowGraphEditModuleDlg::OnCommand_ReorderOutput))
	, m_pModule(pModule)
{
	assert(m_pModule);

	// extract copy of ports from graph, so we can edit locally yet still cancel changes
	const size_t inPortCount = m_pModule->GetModuleInputPortCount();
	m_inputs.reserve(inPortCount);
	for(size_t i = 0; i < inPortCount; ++i)
	{
		m_inputs.push_back(*(m_pModule->GetModuleInputPort(i)));
	}

	const size_t outPortCount = m_pModule->GetModuleOutputPortCount();
	m_outputs.reserve(outPortCount);
	for (size_t i = 0; i <outPortCount; ++i)
	{
		m_outputs.push_back(*(m_pModule->GetModuleOutputPort(i)));
	}
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphEditModuleDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_FG_MODULE_INPUT_LIST, m_inputListCtrl);
	DDX_Control(pDX, IDC_FG_MODULE_OUTPUT_LIST, m_outputListCtrl);
}

//////////////////////////////////////////////////////////////////////////
BOOL CFlowGraphEditModuleDlg::OnInitDialog()
{
	CDialog::OnInitDialog();
	AddDataTypes();
	RefreshControl();

	return TRUE;
}

void CFlowGraphEditModuleDlg::AddDataTypes()
{
	m_flowDataTypes[eFDT_Any] = "Any";
	m_flowDataTypes[eFDT_Int] = "Int";
	m_flowDataTypes[eFDT_Float] = "Float";
	m_flowDataTypes[eFDT_String] = "String";
	m_flowDataTypes[eFDT_Vec3] = "Vec3";
	m_flowDataTypes[eFDT_EntityId] = "EntityId";
	m_flowDataTypes[eFDT_Bool] = "Bool";
}

//////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CFlowGraphEditModuleDlg, CDialog)
ON_COMMAND(IDC_ADD_MODULE_INPUT, OnCommand_NewInput)
ON_COMMAND(IDC_DELETE_MODULE_INPUT, OnCommand_DeleteInput)
ON_COMMAND(IDC_EDIT_MODULE_INPUT, OnCommand_EditInput)
ON_LBN_DBLCLK(IDC_FG_MODULE_INPUT_LIST, OnCommand_EditInput)
ON_COMMAND(IDC_ADD_MODULE_OUTPUT, OnCommand_NewOutput)
ON_COMMAND(IDC_DELETE_MODULE_OUTPUT, OnCommand_DeleteOutput)
ON_COMMAND(IDC_EDIT_MODULE_OUTPUT, OnCommand_EditOutput)
ON_LBN_DBLCLK(IDC_FG_MODULE_OUTPUT_LIST, OnCommand_EditOutput)
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
void CFlowGraphEditModuleDlg::OnCommand_NewInput()
{
	IFlowGraphModule::SModulePortConfig input;
	input.input = true;
	input.type = eFDT_Any;
	CFlowGraphNewModuleInputDlg dlg(&input, &m_inputs);
	if(dlg.DoModal() == IDOK)
	{
		m_inputs.push_back(input);
	}

	RefreshControl();
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphEditModuleDlg::OnCommand_DeleteInput()
{
	int selected = m_inputListCtrl.GetSelCount();
	CArray<int, int> listSelections;
	listSelections.SetSize(selected);
	m_inputListCtrl.GetSelItems(selected, listSelections.GetData());

	for (int i = 0; i < selected; ++i)
	{
		int item = listSelections[i];

		CString temp;
		m_inputListCtrl.GetText(item, temp);
		m_inputs[item] = IFlowGraphModule::SModulePortConfig();
	}

	stl::find_and_erase_all(m_inputs, IFlowGraphModule::SModulePortConfig());
	RefreshControl();
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphEditModuleDlg::OnCommand_EditInput()
{
	int selected = m_inputListCtrl.GetSelCount();

	if (selected == 1)
	{
		CArray<int, int> listSelections;
		listSelections.SetSize(selected);
		m_inputListCtrl.GetSelItems(selected, listSelections.GetData());

		int item = listSelections[0];

		CFlowGraphNewModuleInputDlg dlg(&m_inputs[item], &m_inputs);
		dlg.DoModal();
		RefreshControl();
	}
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphEditModuleDlg::OnCommand_ReorderInput(int oldPos, int newPos)
{
	if (oldPos >= 0 && oldPos < m_inputs.size() && newPos >= 0 && newPos <= m_inputs.size())
	{
		IFlowGraphModule::SModulePortConfig portConfig(m_inputs[oldPos]);
		TPorts::iterator it = m_inputs.begin();
		m_inputs.insert(it + newPos, std::move(portConfig));
		it = m_inputs.begin();
		if (newPos <= oldPos) oldPos++;
		m_inputs.erase(it + oldPos);
	}
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphEditModuleDlg::OnCommand_NewOutput()
{
	IFlowGraphModule::SModulePortConfig output;
	output.input = false;
	output.type = eFDT_Any;
	CFlowGraphNewModuleInputDlg dlg(&output, &m_outputs);
	if (dlg.DoModal() == IDOK)
	{
		m_outputs.push_back(output);
	}

	RefreshControl();
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphEditModuleDlg::OnCommand_DeleteOutput()
{
	int selected = m_outputListCtrl.GetSelCount();
	CArray<int, int> listSelections;
	listSelections.SetSize(selected);
	m_outputListCtrl.GetSelItems(selected, listSelections.GetData());

	for (int i = 0; i < selected; ++i)
	{
		int item = listSelections[i];

		CString temp;
		m_outputListCtrl.GetText(item, temp);

		m_outputs[item] = IFlowGraphModule::SModulePortConfig();
	}

	stl::find_and_erase_all(m_outputs, IFlowGraphModule::SModulePortConfig());
	RefreshControl();
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphEditModuleDlg::OnCommand_EditOutput()
{
	int selected = m_outputListCtrl.GetSelCount();

	if (selected == 1)
	{
		CArray<int, int> listSelections;
		listSelections.SetSize(selected);
		m_outputListCtrl.GetSelItems(selected, listSelections.GetData());

		int item = listSelections[0];

		CFlowGraphNewModuleInputDlg dlg(&m_outputs[item], &m_outputs);
		dlg.DoModal();
		RefreshControl();
	}
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphEditModuleDlg::OnCommand_ReorderOutput(int oldPos, int newPos)
{
	if (oldPos >= 0 && oldPos < m_outputs.size() && newPos >= 0 && newPos <= m_outputs.size())
	{
		IFlowGraphModule::SModulePortConfig portConfig(m_outputs[oldPos]);
		TPorts::iterator it = m_outputs.begin();
		m_outputs.insert(it + newPos, std::move(portConfig));
		it = m_outputs.begin();
		if (newPos <= oldPos) oldPos++;
		m_outputs.erase(it + oldPos);
	}
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphEditModuleDlg::RefreshControl()
{
	m_inputListCtrl.ResetContent();
	m_outputListCtrl.ResetContent();

	for (TPorts::const_iterator it = m_inputs.begin(), end = m_inputs.end(); it != end; ++it)
	{
		assert(it->input);

		CString text = it->name;
		text += " (";
		text += GetDataTypeName(it->type);
		text += ")";
		m_inputListCtrl.AddString(text.GetBuffer());
	}

	for (TPorts::const_iterator it = m_outputs.begin(), end = m_outputs.end(); it != end; ++it)
	{
		assert(!it->input);

		CString text = it->name;
		text += " (";
		text += GetDataTypeName(it->type);
		text += ")";
		m_outputListCtrl.AddString(text.GetBuffer());
	}
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphEditModuleDlg::OnOK()
{
	if (m_pModule)
	{
		// write inputs/outputs back to graph
		m_pModule->RemoveModulePorts();

		for (TPorts::const_iterator it = m_inputs.begin(), end = m_inputs.end(); it != end; ++it)
		{
			assert(it->input);
			m_pModule->AddModulePort(*it);
		}

		for (TPorts::const_iterator it = m_outputs.begin(), end = m_outputs.end(); it != end; ++it)
		{
			assert(!it->input);
			m_pModule->AddModulePort(*it);
		}

		GetIEditorImpl()->GetFlowGraphModuleManager()->CreateModuleNodes(m_pModule->GetName());

		if (CHyperFlowGraph* pModuleFlowGraph = GetIEditorImpl()->GetFlowGraphManager()->FindGraph(m_pModule->GetRootGraph()))
		{
			XmlNodeRef tempNode = gEnv->pSystem->CreateXmlNode("Graph");
			pModuleFlowGraph->Serialize(tempNode, false);
			pModuleFlowGraph->SetModified(true);
		}
	}

	CDialog::OnOK();
}

CString CFlowGraphEditModuleDlg::GetDataTypeName(EFlowDataTypes type)
{
	return stl::find_in_map(m_flowDataTypes, type, "");
}

EFlowDataTypes CFlowGraphEditModuleDlg::GetDataType(const CString& itemType)
{
	for (TDataTypes::iterator iter = m_flowDataTypes.begin(); iter != m_flowDataTypes.end(); ++iter)
	{
		if (iter->second == itemType)
		{
			return iter->first;
		}
	}
	return eFDT_Void;
}

void CFlowGraphEditModuleDlg::FillDataTypes(CComboBox& comboBox)
{
	for (TDataTypes::iterator iter = m_flowDataTypes.begin(); iter != m_flowDataTypes.end(); ++iter)
	{
		comboBox.AddString(iter->second);
	}
}

//////////////////////////////////////////////////////////////////////////
// CFlowGraphNewModuleInputDlg
//////////////////////////////////////////////////////////////////////////

IMPLEMENT_DYNAMIC(CFlowGraphNewModuleInputDlg, CDialog)

CFlowGraphNewModuleInputDlg::CFlowGraphNewModuleInputDlg(IFlowGraphModule::SModulePortConfig* pPort, TPorts* pOtherPorts, CWnd* pParent /*=NULL*/)
	: CDialog(CFlowGraphNewModuleInputDlg::IDD, pParent)
	, m_pPort(pPort)
	, m_pOtherPorts(pOtherPorts)
{
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphNewModuleInputDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_MODULE_INPUT_TYPE, m_inputTypesCtrl);
	DDX_Control(pDX, IDC_MODULE_INPUT_NAME, m_inputNameCtrl);
}

//////////////////////////////////////////////////////////////////////////
BOOL CFlowGraphNewModuleInputDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	CFlowGraphEditModuleDlg::FillDataTypes(m_inputTypesCtrl);

	m_inputNameCtrl.SetWindowText(m_pPort->name.c_str());
	m_inputTypesCtrl.SetWindowText(CFlowGraphEditModuleDlg::GetDataTypeName(m_pPort->type));

	if (m_pPort->input)
		SetWindowText("Edit Module Input");
	else
		SetWindowText("Edit Module Output");

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CFlowGraphNewModuleInputDlg, CDialog)
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
void CFlowGraphNewModuleInputDlg::OnOK()
{
	if (m_pPort)
	{
		CString itemName;
		m_inputNameCtrl.GetWindowText(itemName);
		string dialogName = itemName.GetBuffer();

		CString itemType;
		m_inputTypesCtrl.GetWindowText(itemType);
		EFlowDataTypes dialogType = CFlowGraphEditModuleDlg::GetDataType(itemType);


		if (dialogType == eFDT_Void)
		{
			CryMessageBox("The port can not have type Void.", "Error Invalid Port Type", eMB_Error);
			return;
		}

		if (dialogName.empty())
		{
			CryMessageBox("The port name can not be empty.", "Error Invalid Port Name", eMB_Error);
			return;
		}

		// check to give an obvious message, the regex would get this
		if (isdigit(dialogName[0]) || dialogName[0] == ':' || dialogName[0] == '_' )
		{
			CryMessageBox("The port name must start with an A-Z letter, ':' or '_'.", "Error Invalid Port Name", eMB_Error);
			return;
		}

		std::regex validXml("^[a-zA-Z_:][-a-zA-Z0-9_:.]*$"); // Matches valid XML attribute names
		if (!std::regex_match(dialogName.c_str(), validXml))
		{
			MessageBox("Invalid port name! Only letters, numbers, and '-' '_' ':' '.' are allowed.", "Error Invalid Port Name", eMB_Error);
			return;
		}

		if (m_pPort->input)
		{
			// reject port names that are fixed in the Module Call and Start nodes
			if ( dialogName.compareNoCase("Call") == 0 // no case because it saves the casing but then doesn't use it most of the time -.-
				|| dialogName.compareNoCase("Start") == 0
				|| dialogName.compareNoCase("Update") == 0
				|| dialogName.compareNoCase("Cancel") == 0
				|| dialogName.compareNoCase("GlobalController") == 0
				|| dialogName.compareNoCase("EntityId") == 0
				|| dialogName.compareNoCase("ContinuousInput") == 0
				|| dialogName.compareNoCase("ContinuousOutput") == 0
				)
			{
				CryMessageBox("Invalid port name! A default port with that name already exists.", "Error Invalid Port Name", eMB_Error);
				return;
			}
		}
		else
		{
			// reject port names that are fixed in the Module Call and End nodes
			if ( dialogName.compareNoCase("OnCalled") == 0
				|| dialogName.compareNoCase("Done") == 0
				|| dialogName.compareNoCase("Success") == 0
				|| dialogName.compareNoCase("Cancel") == 0
				)
			{
				CryMessageBox("Invalid port name! A default port with that name already exists.", "Error Invalid Port Name", eMB_Error);
				return;
			}
		}

		for (TPorts::const_iterator it = m_pOtherPorts->begin(), end = m_pOtherPorts->end(); it != end; ++it)
		{
			if (dialogName.compareNoCase(it->name) == 0)
			{
				CryMessageBox("Invalid port name! A port with that name already exists.", "Error Invalid Port Name", eMB_Error);
				return;
			}
		}

		m_pPort->name = dialogName;
		m_pPort->type = dialogType;
	}

	CDialog::OnOK();
}

