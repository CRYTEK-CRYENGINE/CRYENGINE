// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FlowGraphManager.h"

#include <CryGame/IGameFramework.h>
#include <CryEntitySystem/IEntitySystem.h>
#include <CryFlowGraph/IFlowSystem.h>
#include <CryFlowGraph/IFlowGraphModuleManager.h>
#include <CryAISystem/IAIAction.h>

#include "FlowGraph.h"
#include "FlowGraphNode.h"
#include "FlowGraphModuleManager.h"
#include "FlowGraphVariables.h"
#include "Nodes/TrackEventNode.h"
#include "Controls/HyperGraphEditorWnd.h"
#include "Controls/FlowGraphSearchCtrl.h"
#include "Controls/SelectGameTokenDialog.h"

#include "IconManager.h"
#include "GameEngine.h"

#include "Objects/EntityObject.h"
#include "Objects/PrefabObject.h"
#include "Prefabs/PrefabManager.h"
#include "Prefabs/PrefabEvents.h"
#include "Prefabs/PrefabItem.h"

#include "UIEnumsDatabase.h"
#include "Util/Variable.h"
#include "Util/BoostPythonHelpers.h"

//Includes for property selectors
#include <CrySandbox/CryFunction.h>
#include "Controls/PropertyItem.h"
#include "CustomActions/CustomActionDialog.h"

#define FG_INTENSIVE_DEBUG
#undef  FG_INTENSIVE_DEBUG

static XmlNodeRef g_flowgraphDocumentationData = NULL;

//////////////////////////////////////////////////////////////////////////
void WriteFlowgraphDocumentation(IConsoleCmdArgs* /* pArgs */)
{
	g_flowgraphDocumentationData = gEnv->pSystem->CreateXmlNode("FlowGraphDocumentationRoot");

	GetIEditorImpl()->GetFlowGraphManager()->ReloadClasses();
	g_flowgraphDocumentationData->saveToFile("USER\\flowgraphdocu.xml");

	g_flowgraphDocumentationData->removeAllChilds();
	g_flowgraphDocumentationData = NULL;
}

void ReloadFlowgraphClasses(IConsoleCmdArgs* /* pArgs */)
{
	GetIEditorImpl()->GetFlowGraphManager()->ReloadClasses();
}

void WriteFlownodeToDocumentation(IFlowNodeData* pFlowNodeData, const IFlowNodeTypeIterator::SNodeType& nodeType)
{
	SFlowNodeConfig config;
	pFlowNodeData->GetConfiguration(config);

	string name = nodeType.typeName;
	int seperator = name.find(':');
	if (seperator == 0)
		seperator = name.length();

	//create node entry, split class and name

	//find or add class
	XmlNodeRef flowNodeParent = g_flowgraphDocumentationData->findChild(name.substr(0, seperator));
	if (!flowNodeParent)
	{
		flowNodeParent = gEnv->pSystem->CreateXmlNode(name.substr(0, seperator));
		g_flowgraphDocumentationData->addChild(flowNodeParent);
	}

	//add new node with given name
	string nodeName = name.substr(seperator + 1, name.length() - seperator - 1);
	if (nodeName.empty())
		return;

	if (*nodeName.begin() >= '0' && *nodeName.begin() <= '9')
		nodeName = "_" + nodeName;

	XmlNodeRef flowNodeData = gEnv->pSystem->CreateXmlNode(nodeName.c_str());
	flowNodeData->setAttr("Description", config.sDescription ? config.sDescription : "");

	//Input subgroup
	XmlNodeRef inputs = gEnv->pSystem->CreateXmlNode("Inputs");
	int iInputs = pFlowNodeData->GetNumInputPorts();
	for (int input = 0; input < iInputs; ++input)
	{
		const SInputPortConfig& port = config.pInputPorts[input];
		if (!port.name)
			break;

		name = port.name;
		if (*name.begin() >= '0' && *name.begin() <= '9')
			name = "_" + name;
		name.replace(' ', '_');

		inputs->setAttr(name.c_str(), port.description ? port.description : "");
	}
	flowNodeData->addChild(inputs);

	//Output subgroup
	XmlNodeRef outputs = gEnv->pSystem->CreateXmlNode("Outputs");
	int iOutputs = pFlowNodeData->GetNumOutputPorts();
	for (int output = 0; output < iOutputs; ++output)
	{
		const SOutputPortConfig& port = config.pOutputPorts[output];
		if (!port.name)
			break;

		name = port.name;
		if (*name.begin() > '0' && *name.begin() <= '9')
			name = "_" + name;
		name.replace(' ', '_');

		outputs->setAttr(name.c_str(), port.description ? port.description : "");
	}
	flowNodeData->addChild(outputs);

	flowNodeParent->addChild(flowNodeData);
}

//////////////////////////////////////////////////////////////////////////
CFlowGraphManager::~CFlowGraphManager()
{
	while (!m_graphs.empty())
	{
		CHyperFlowGraph* pFG = m_graphs.back();
		if (pFG)
		{
			pFG->OnHyperGraphManagerDestroyed();
			UnregisterGraph(pFG);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphManager::Init()
{
	CHyperGraphManager::Init();

	REGISTER_COMMAND("fg_writeDocumentation", WriteFlowgraphDocumentation, VF_NULL, "Write flownode documentation file.");
	REGISTER_COMMAND("fg_reloadClasses", ReloadFlowgraphClasses, VF_NULL, "Reloads all flowgraph nodes.");

	// Enumerate all flow graph classes.
	ReloadClasses();

	//Register all deprecated property editors
	GetIEditorImpl()->RegisterDeprecatedPropertyEditor(ePropertyGameToken, WrapMemberFunction(this, &CFlowGraphManager::OnSelectGameToken));
	GetIEditorImpl()->RegisterDeprecatedPropertyEditor(ePropertyCustomAction, WrapMemberFunction(this, &CFlowGraphManager::OnSelectCustomAction));
}


bool CFlowGraphManager::OnSelectGameToken(const string& oldValue, string& newValue)
{
	CSelectGameTokenDialog gtDlg;
	gtDlg.PreSelectGameToken(CString(oldValue));
	if (gtDlg.DoModal() == IDOK)
	{
		newValue = gtDlg.GetSelectedGameToken();
		return true;
	}
	return false;
}

bool CFlowGraphManager::OnSelectCustomAction(const string& oldValue, string& newValue)
{
	CCustomActionDialog customActionDlg;
	customActionDlg.SetCustomAction(CString(oldValue));
	if (customActionDlg.DoModal() == IDOK)
	{
		newValue = customActionDlg.GetCustomAction();
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphManager::OnEnteringGameMode(bool inGame)
{
	if (!inGame)
		return;

	for (int i = 0; i < m_graphs.size(); ++i)
		m_graphs[i]->OnEnteringGameMode();
}

//////////////////////////////////////////////////////////////////////////
CHyperGraph* CFlowGraphManager::CreateGraph()
{
	return new CHyperFlowGraph(this);
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphManager::RegisterGraph(CHyperFlowGraph* pGraph)
{
	if (stl::find(m_graphs, pGraph) == true)
	{
		CryLogAlways("CFlowGraphManager::RegisterGraph: OLD Graph 0x%p", pGraph);
		assert(false);
	}
	else
	{
#ifdef FG_INTENSIVE_DEBUG
		CryLogAlways("CFlowGraphManager::RegisterGraph: NEW Graph 0x%p", pGraph);
#endif
		m_graphs.push_back(pGraph);
	}
	// assert (stl::find(m_graphs, pGraph) == false);
	SendNotifyEvent(EHG_GRAPH_ADDED, pGraph);
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphManager::UnregisterGraph(CHyperFlowGraph* pGraph)
{
#ifdef FG_INTENSIVE_DEBUG
	CryLogAlways("CFlowGraphManager::UnregisterGraph: Graph 0x%p", pGraph);
#endif
	stl::find_and_erase(m_graphs, pGraph);
	SendNotifyEvent(EHG_GRAPH_REMOVED, pGraph);
}

//////////////////////////////////////////////////////////////////////////
CHyperNode* CFlowGraphManager::CreateNode(CHyperGraph* pGraph, const char* sNodeClass, HyperNodeID nodeId, const Gdiplus::PointF& pos, CBaseObject* pObj, bool bAllowMissing)
{
	// AlexL: ignore pos, as a SetPos would screw up Undo. And it will be set afterwards anyway

	CHyperNode* pNode = 0;

	CHyperFlowGraph* pFlowGraph = (CHyperFlowGraph*)pGraph;
	// Check for special node classes.
	if (pObj)
	{
		pNode = CreateSelectedEntityNode(pFlowGraph, pObj);
	}
	else if (strcmp(sNodeClass, "default_entity") == 0)
	{
		pNode = CreateEntityNode(pFlowGraph, pFlowGraph->GetEntity());
	}
	else
	{
		pNode = __super::CreateNode(pGraph, sNodeClass, nodeId, pos, pObj, bAllowMissing);
	}

	return pNode;
}

void CFlowGraphManager::ReloadGraphs()
{
	LOADING_TIME_PROFILE_SECTION;

	// create undos
	std::vector<IUndoObject*> m_CurrentFgUndos;
	GetIEditor()->Notify(eNotify_OnBeginUndoRedo);
	m_CurrentFgUndos.resize(m_graphs.size());
	for (int i = 0; i < m_graphs.size(); ++i)
		m_CurrentFgUndos[i] = m_graphs[i]->CreateUndo();

	// restore graphs
	for (int i = 0; i < m_CurrentFgUndos.size(); ++i)
	{
		m_CurrentFgUndos[i]->Undo(false);
		delete m_CurrentFgUndos[i];
	}

	GetIEditor()->Notify(eNotify_OnEndUndoRedo); // will force the graph tree to reload and repaint
}

void CFlowGraphManager::UpdateNodePrototypeWithoutReload(const char* nodeClassName)
{
	IFlowSystem* const pFlowSystem = gEnv->pGameFramework->GetIFlowSystem();
	IFlowGraphPtr const pFlowGraph = pFlowSystem->CreateFlowGraph(); // dummy graph to create the node in, will be deleted

	CFlowNode* pProtoFlowNode = nullptr;
	if (strcmp(nodeClassName, TRACKEVENT_CLASS) == 0)
		pProtoFlowNode = new CTrackEventNode;
	else
		pProtoFlowNode = new CFlowNode;
	pProtoFlowNode->SetClass(nodeClassName);

	TFlowNodeId nodeId = pFlowGraph->CreateNode(nodeClassName, nodeClassName);
	IFlowNodeData* pFlowNodeData = pFlowGraph->GetNodeData(nodeId);
	GetNodeConfig(pFlowNodeData, pProtoFlowNode);

	m_prototypes[nodeClassName] = pProtoFlowNode;
}

void CFlowGraphManager::ReloadClasses()
{
	LOADING_TIME_PROFILE_SECTION;

	// rebuilds m_prototypes from where nodes are cloned when they are created.
	// since some nodes could be changed / removed
	// serialize all FlowGraphs, and then restore state after reloading

	// create undos
	std::vector<IUndoObject*> m_CurrentFgUndos;
	GetIEditor()->Notify(eNotify_OnBeginUndoRedo);
	m_CurrentFgUndos.resize(m_graphs.size());
	for (int i = 0; i < m_graphs.size(); ++i)
		m_CurrentFgUndos[i] = m_graphs[i]->CreateUndo();

	// rebuild node prototypes

	m_prototypes.clear();

	IFlowSystem* const pFlowSystem = gEnv->pGameFramework->GetIFlowSystem();
	IFlowGraphPtr const pFlowGraph = pFlowSystem->CreateFlowGraph(); // dummy graph to create all the nodes in. will be deleted

	IFlowNodeTypeIteratorPtr pTypeIterator = pFlowSystem->CreateNodeTypeIterator();
	IFlowNodeTypeIterator::SNodeType nodeType;
	while (pTypeIterator->Next(nodeType))
	{
		CFlowNode* pProtoFlowNode = nullptr;
		if (strcmp(nodeType.typeName, TRACKEVENT_CLASS) == 0)
			pProtoFlowNode = new CTrackEventNode;
		else
			pProtoFlowNode = new CFlowNode;
		pProtoFlowNode->SetClass(nodeType.typeName);
		m_prototypes[nodeType.typeName] = pProtoFlowNode;

		TFlowNodeId nodeId = pFlowGraph->CreateNode(nodeType.typeName, nodeType.typeName);
		IFlowNodeData* pFlowNodeData = pFlowGraph->GetNodeData(nodeId);
		assert(pFlowNodeData);
		GetNodeConfig(pFlowNodeData, pProtoFlowNode);

		if (g_flowgraphDocumentationData)
		{
			WriteFlownodeToDocumentation(pFlowNodeData, nodeType);
		}
	}

	// restore graphs
	for (int i = 0; i < m_CurrentFgUndos.size(); ++i)
	{
		m_CurrentFgUndos[i]->Undo(false);
		delete m_CurrentFgUndos[i];
	}

	GetIEditor()->Notify(eNotify_OnEndUndoRedo); // will force the graph tree to reload and repaint
}

enum UIEnumType
{
	eUI_None,
	eUI_Int,
	eUI_Float,
	eUI_String,
	eUI_GlobalEnum,
	eUI_GlobalEnumRef,
	eUI_GlobalEnumDef,
};

namespace
{

float GetValue(const CString& s, const char* token)
{
	float fValue = 0.0f;
	int pos = s.Find(token);
	if (pos >= 0)
	{
		// fill in Enum Pairs
		pos = s.Find('=', pos + 1);
		if (pos >= 0)
		{
			sscanf(s.GetString() + pos + 1, "%f", &fValue);
		}
	}
	return fValue;
}

float GetValue(const char* s, const char* token)
{
	return GetValue(CString(s), token);
}

}

UIEnumType ParseUIConfig(const char* sUIConfig, std::map<string, string>& outEnumPairs, Vec3& uiValueRanges)
{
	UIEnumType enumType = eUI_None;
	string uiConfig(sUIConfig);

	// ranges
	uiValueRanges.x = GetValue(uiConfig, "v_min");
	uiValueRanges.y = GetValue(uiConfig, "v_max");

	int pos = uiConfig.Find(':');
	if (pos <= 0)
		return enumType;

	string enumTypeS = uiConfig.Mid(0, pos);
	if (enumTypeS == "enum_string")
		enumType = eUI_String;
	else if (enumTypeS == "enum_int")
		enumType = eUI_Int;
	else if (enumTypeS == "enum_float")
		enumType = eUI_Float;
	else if (enumTypeS == "enum_global")
	{
		// don't do any tokenzing. "enum_global:ENUM_NAME"
		enumType = eUI_GlobalEnum;
		string enumName = uiConfig.Mid(pos + 1);
		if (enumName.IsEmpty() == false)
			outEnumPairs[enumName] = enumName;
		return enumType;
	}
	else if (enumTypeS == "enum_global_def")
	{
		// don't do any tokenzing. "enum_global_def:ENUM_NAME"
		enumType = eUI_GlobalEnumDef;
		string enumName = uiConfig.Mid(pos + 1);
		if (enumName.IsEmpty() == false)
			outEnumPairs[enumName] = enumName;
		return enumType;
	}
	else if (enumTypeS == "enum_global_ref")
	{
		// don't do any tokenzing. "enum_global_ref:ENUM_NAME_FORMAT_STRING:REF_PORT"
		enumType = eUI_GlobalEnumRef;
		int pos1 = uiConfig.Find(':', pos + 1);
		if (pos1 < 0)
		{
			Warning(_T("FlowGraphManager: Wrong enum_global_ref format while parsing UIConfig '%s'"), sUIConfig);
			return eUI_None;
		}

		string enumFormat = uiConfig.Mid(pos + 1, pos1 - pos - 1);
		string enumPort = uiConfig.Mid(pos1 + 1);
		if (enumFormat.IsEmpty() || enumPort.IsEmpty())
		{
			Warning(_T("FlowGraphManager: Wrong enum_global_ref format while parsing UIConfig '%s'"), sUIConfig);
			return eUI_None;
		}
		outEnumPairs[enumFormat] = enumPort;
		return enumType;

	}

	if (enumType != eUI_None)
	{
		// fill in Enum Pairs
		string values = uiConfig.Mid(pos + 1);
		pos = 0;
		string resToken = TokenizeString(values, " ,", pos);
		while (!resToken.IsEmpty())
		{
			string str = resToken;
			string value = str;
			int pos_e = str.Find('=');
			if (pos_e >= 0)
			{
				value = str.Mid(pos_e + 1);
				str = str.Mid(0, pos_e);
			}
			outEnumPairs[str] = value;
			resToken = TokenizeString(values, " ,", pos);
		}
		;
	}

	return enumType;
}

template<typename T>
void FillEnumVar(CVariableEnum<T>* pVar, const std::map<string, string>& nameValueMap)
{
	var_type::type_convertor convertor;
	T val;
	for (std::map<string, string>::const_iterator iter = nameValueMap.begin(); iter != nameValueMap.end(); ++iter)
	{
		convertor(iter->second, val);
		//int val = atoi (iter->second.GetString());
		pVar->AddEnumItem(iter->first, val);
	}
}

//////////////////////////////////////////////////////////////////////////
IVariable* CFlowGraphManager::MakeInVar(const SInputPortConfig* pPortConfig, uint32 portId, CFlowNode* pFlowNode)
{
	int type = pPortConfig->defaultData.GetType();
	if (!pPortConfig->defaultData.IsLocked())
		type = eFDT_Any;

	IVariable* pVar = 0;
	// create variable

	CFlowNodeGetCustomItemsBase* pGetCustomItems = 0;

	const char* name = pPortConfig->name;
	bool isEnumDataType = false;

	Vec3 uiValueRanges(ZERO);

	// UI Parsing
	if (pPortConfig->sUIConfig != 0)
	{
		isEnumDataType = true;
		std::map<string, string> enumPairs;
		UIEnumType type = ParseUIConfig(pPortConfig->sUIConfig, enumPairs, uiValueRanges);
		switch (type)
		{
		case eUI_Int:
			{
				CVariableFlowNodeEnum<int>* pEnumVar = new CVariableFlowNodeEnum<int>;
				FillEnumVar<int>(pEnumVar, enumPairs);
				pVar = pEnumVar;
			}
			break;
		case eUI_Float:
			{
				CVariableFlowNodeEnum<float>* pEnumVar = new CVariableFlowNodeEnum<float>;
				FillEnumVar<float>(pEnumVar, enumPairs);
				pVar = pEnumVar;
			}
			break;
		case eUI_String:
			{
				CVariableFlowNodeEnum<string>* pEnumVar = new CVariableFlowNodeEnum<string>;
				FillEnumVar<string>(pEnumVar, enumPairs);
				pVar = pEnumVar;
			}
			break;
		case eUI_GlobalEnum:
			{
				CVariableFlowNodeEnum<string>* pEnumVar = new CVariableFlowNodeEnum<string>;
				pEnumVar->SetFlags(IVariable::UI_USE_GLOBAL_ENUMS); // FIXME: is not really needed. may be dropped
				string globalEnumName;
				if (enumPairs.empty())
					globalEnumName = name;
				else
					globalEnumName = enumPairs.begin()->first;

				CVarGlobalEnumList* pEnumList = new CVarGlobalEnumList(globalEnumName);
				pEnumVar->SetEnumList(pEnumList);
				pVar = pEnumVar;
			}
			break;
		case eUI_GlobalEnumRef:
			{
				assert(enumPairs.size() == 1);
				CVariableFlowNodeDynamicEnum* pEnumVar = new CVariableFlowNodeDynamicEnum(enumPairs.begin()->first, enumPairs.begin()->second);
				pEnumVar->SetFlags(IVariable::UI_USE_GLOBAL_ENUMS); // FIXME: is not really needed. may be dropped
				pVar = pEnumVar;
				// take care of custom item base
				pGetCustomItems = new CFlowNodeGetCustomItemsBase;
				pGetCustomItems->SetUIConfig(pPortConfig->sUIConfig);
			}
			break;
		case eUI_GlobalEnumDef:
			{
				assert(enumPairs.size() == 1);
				CVariableFlowNodeDefinedEnum* pEnumVar = new CVariableFlowNodeDefinedEnum(enumPairs.begin()->first, portId);
				pEnumVar->SetFlags(IVariable::UI_USE_GLOBAL_ENUMS); // FIXME: is not really needed. may be dropped
				pVar = pEnumVar;
				// take care of custom item base
				pGetCustomItems = new CFlowNodeGetCustomItemsBase;
				pGetCustomItems->SetUIConfig(pPortConfig->sUIConfig);
			}
			break;
		default:
			isEnumDataType = false;
			break;
		}
	}

	// Make a simple var, it was no enum type
	if (pVar == 0)
	{
		pVar = MakeSimpleVarFromFlowType(type);
	}

	// Set ranges if applicable
	if (uiValueRanges.x != 0.0f || uiValueRanges.y != 0.0f)
		pVar->SetLimits(uiValueRanges.x, uiValueRanges.y, true, true);

	// Take care of predefined datatypes
	if (!isEnumDataType)
	{
		const FlowGraphVariables::MapEntry* mapEntry = 0;
		if (pPortConfig->sUIConfig != 0)
		{
			// parse UI config, if a certain datatype is specified
			// ui_dt=datatype
			CString uiConfig(pPortConfig->sUIConfig);
			const char* prefix = "dt=";
			const int pos = uiConfig.Find(prefix);
			if (pos >= 0)
			{
				CString dt = uiConfig.Mid(pos + 3); // 3==strlen(prefix)
				dt = dt.SpanExcluding(" ,");
				dt += "_";
				mapEntry = FlowGraphVariables::FindEntry(dt);
			}
		}
		if (mapEntry == 0)
		{
			mapEntry = FlowGraphVariables::FindEntry(pPortConfig->name);
			if (mapEntry != 0)
				name += strlen(mapEntry->sPrefix);
		}

		if (mapEntry != 0)
		{
			pVar->SetDataType(mapEntry->eDataType);
			if (mapEntry->eDataType == IVariable::DT_USERITEMCB)
			{
				assert(pGetCustomItems == 0);
				assert(mapEntry->pGetCustomItemsCreator != 0);
				pGetCustomItems = mapEntry->pGetCustomItemsCreator();
				pGetCustomItems->SetUIConfig(pPortConfig->sUIConfig);
			}
		}
	}

	// Set Name of Variable
	pVar->SetName(pPortConfig->name);   // ALEXL 08/11/05: from now on we always use the REAL port name

	// Set HumanName of Variable
	if (pPortConfig->humanName)
		pVar->SetHumanName(pPortConfig->humanName);
	else
		pVar->SetHumanName(name);  // if there is no human name we set the 'name' (stripped prefix if it was a predefined data type!)

	// Set variable description
	if (pPortConfig->description)
		pVar->SetDescription(pPortConfig->description);

	if (pGetCustomItems)
		pGetCustomItems->AddRef();

	pVar->SetUserData(pGetCustomItems);

	return pVar;
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphManager::GetNodeConfig(IFlowNodeData* pSrcNode, CFlowNode* pFlowNode)
{
	if (!pSrcNode)
		return;
	pFlowNode->ClearPorts();
	SFlowNodeConfig config;
	pSrcNode->GetConfiguration(config);

	if (config.nFlags & EFLN_TARGET_ENTITY)
	{
		pFlowNode->SetFlag(EHYPER_NODE_ENTITY, true);
	}
	if (config.nFlags & EFLN_HIDE_UI)
	{
		pFlowNode->SetFlag(EHYPER_NODE_HIDE_UI, true);
	}
	if (config.nFlags & EFLN_UNREMOVEABLE)
	{
		pFlowNode->SetFlag(EHYPER_NODE_UNREMOVEABLE, true);
	}

	pFlowNode->m_flowSystemNodeFlags = config.nFlags;
	if (pFlowNode->GetCategory() != EFLN_APPROVED)
	{
		pFlowNode->SetFlag(EHYPER_NODE_CUSTOM_COLOR1, true);
	}

	pFlowNode->m_szDescription = config.sDescription; // can be 0
	pFlowNode->m_szUIClassName = config.sUIClassName; // can be 0

	if (config.pInputPorts)
	{
		const SInputPortConfig* pPortConfig = config.pInputPorts;
		uint32 portId = 0;
		while (pPortConfig->name)
		{
			CHyperNodePort port;
			port.bInput = true;

			int type = pPortConfig->defaultData.GetType();
			if (!pPortConfig->defaultData.IsLocked())
				type = eFDT_Any;

			port.pVar = MakeInVar(pPortConfig, portId, pFlowNode);

			if (port.pVar)
			{
				switch (type)
				{
				case eFDT_Bool:
					port.pVar->Set(*pPortConfig->defaultData.GetPtr<bool>());
					break;
				case eFDT_Int:
					port.pVar->Set(*pPortConfig->defaultData.GetPtr<int>());
					break;
				case eFDT_Float:
					port.pVar->Set(*pPortConfig->defaultData.GetPtr<float>());
					break;
				case eFDT_EntityId:
					port.pVar->Set((int)*pPortConfig->defaultData.GetPtr<EntityId>());
					port.pVar->SetFlags(port.pVar->GetFlags() | IVariable::UI_DISABLED);
					break;
				case eFDT_Vec3:
					port.pVar->Set(*pPortConfig->defaultData.GetPtr<Vec3>());
					break;
				case eFDT_String:
					port.pVar->Set((pPortConfig->defaultData.GetPtr<string>())->c_str());
					break;
				}

				pFlowNode->AddPort(port);
			}
			++pPortConfig;
			++portId;
		}
	}
	if (config.pOutputPorts)
	{
		const SOutputPortConfig* pPortConfig = config.pOutputPorts;
		while (pPortConfig->name)
		{
			CHyperNodePort port;
			port.bInput = false;
			port.bAllowMulti = true;
			port.pVar = MakeSimpleVarFromFlowType(pPortConfig->type);
			if (port.pVar)
			{
				port.pVar->SetName(pPortConfig->name);
				if (pPortConfig->description)
					port.pVar->SetDescription(pPortConfig->description);
				if (pPortConfig->humanName)
					port.pVar->SetHumanName(pPortConfig->humanName);
				pFlowNode->AddPort(port);
			}
			++pPortConfig;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
IVariable* CFlowGraphManager::MakeSimpleVarFromFlowType(int type)
{
	switch (type)
	{
	case eFDT_Void:
		return new CVariableFlowNodeVoid;
		break;
	case eFDT_Bool:
		return new CVariableFlowNode<bool>;
		break;
	case eFDT_Int:
		return new CVariableFlowNode<int>;
		break;
	case eFDT_Float:
		return new CVariableFlowNode<float>;
		break;
	case eFDT_EntityId:
		return new CVariableFlowNode<int>;
		break;
	case eFDT_Vec3:
		return new CVariableFlowNode<Vec3>;
		break;
	case eFDT_String:
		return new CVariableFlowNode<string>;
		break;
	default:
		// Any type.
		return new CVariableFlowNodeVoid;
		break;
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
CHyperFlowGraph* CFlowGraphManager::CreateGraphForEntity(CEntityObject* pEntity, const char* sGroupName)
{
	CHyperFlowGraph* pFlowGraph = new CHyperFlowGraph(this, sGroupName, pEntity);
	pFlowGraph->SetName(pEntity->GetName());
#ifdef FG_INTENSIVE_DEBUG
	CryLogAlways("CFlowGraphManager::CreateGraphForEntity: graph=0x%p entity=0x%p guid=%s", pFlowGraph, pEntity, GuidUtil::ToString(pEntity->GetId()));
#endif
	return pFlowGraph;
}

//////////////////////////////////////////////////////////////////////////
CHyperFlowGraph* CFlowGraphManager::FindGraphForUIAction(IUIAction* pUIAction)
{
	for (int i = 0; i < m_graphs.size(); ++i)
		if (m_graphs[i]->GetUIAction() == pUIAction)
			return m_graphs[i];
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
CHyperFlowGraph* CFlowGraphManager::CreateGraphForUIAction(IUIAction* pUIAction)
{
	CHyperFlowGraph* pFlowGraph = new CHyperFlowGraph(this);
	pFlowGraph->SetUIAction(pUIAction);
	return pFlowGraph;
}

//////////////////////////////////////////////////////////////////////////
CHyperFlowGraph* CFlowGraphManager::FindGraphForAction(IAIAction* pAction)
{
	for (int i = 0; i < m_graphs.size(); ++i)
		if (m_graphs[i]->GetAIAction() == pAction)
			return m_graphs[i];
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
CHyperFlowGraph* CFlowGraphManager::CreateGraphForAction(IAIAction* pAction)
{
	CHyperFlowGraph* pFlowGraph = new CHyperFlowGraph(this);
	pFlowGraph->GetIFlowGraph()->SetSuspended(true);
	pFlowGraph->SetAIAction(pAction);
	return pFlowGraph;
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphManager::FreeGraphsForActions()
{
	int i = m_graphs.size();
	while (i--)
	{
		if (m_graphs[i]->GetAIAction())
		{
			// delete will call UnregisterGraph and remove itself from the vector
			//	assert( m_graphs[i]->NumRefs() == 1 );
			m_graphs[i]->SetAIAction(NULL);
			m_graphs[i]->SetName("");
			m_graphs[i]->SetName("Default");
			m_graphs[i]->Release();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CHyperFlowGraph* CFlowGraphManager::CreateEditorGraphForModule(IFlowGraphModule* pModule)
{
	LOADING_TIME_PROFILE_SECTION

	assert(pModule);
	if (pModule)
	{
		CString filename = pModule->GetPath();
		IFlowGraph* pIGraph = pModule->GetRootGraph();

		CHyperFlowGraph* pFlowGraph = new CHyperFlowGraph(this, pIGraph);
		pFlowGraph->GetIFlowGraph()->SetType(IFlowGraph::eFGT_Module);
		pFlowGraph->SetName(pModule->GetName());

		// load editor side data, such as comments and node positions plus the runtime data
		pFlowGraph->Load(filename.GetString());

		// if module is in a subfolder, extract the folder name
		// to use as a group in the FG editor window
		CString groupName = filename.MakeLower();
		CString moduleFolder = "flowgraphmodules\\";
		const int moduleFolderLength = moduleFolder.GetLength();
		int offset = groupName.Find(moduleFolder);
		int offset2 = groupName.Find("\\", offset + moduleFolderLength);
		if (offset != -1 && offset2 != -1)
		{
			groupName.Truncate(offset2);
			groupName = groupName.Right(offset2 - offset - moduleFolderLength);
			pFlowGraph->SetGroupName(groupName);
		}

		return pFlowGraph;
	}

	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
CHyperFlowGraph* CFlowGraphManager::FindGraph(IFlowGraphPtr pFG)
{
	LOADING_TIME_PROFILE_SECTION
	for (int i = 0; i < m_graphs.size(); ++i)
		if (m_graphs[i]->GetIFlowGraph() == pFG.get())
			return m_graphs[i];
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
CHyperFlowGraph* CFlowGraphManager::FindGraphForCustomAction(ICustomAction* pCustomAction)
{
	for (int i = 0; i < m_graphs.size(); ++i)
		if (m_graphs[i]->GetCustomAction() == pCustomAction)
			return m_graphs[i];
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
CHyperFlowGraph* CFlowGraphManager::CreateGraphForCustomAction(ICustomAction* pCustomAction)
{
	CHyperFlowGraph* pFlowGraph = new CHyperFlowGraph(this);
	pFlowGraph->GetIFlowGraph()->SetSuspended(true);
	pFlowGraph->SetCustomAction(pCustomAction);
	return pFlowGraph;
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphManager::FreeGraphsForCustomActions()
{
	int i = m_graphs.size();
	while (i--)
	{
		if (m_graphs[i]->GetCustomAction())
		{
			// delete will call UnregisterGraph and remove itself from the vector
			//	assert( m_graphs[i]->NumRefs() == 1 );
			m_graphs[i]->SetCustomAction(NULL);
			m_graphs[i]->SetName("");
			m_graphs[i]->SetName("Default");
			m_graphs[i]->Release();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CFlowNode* CFlowGraphManager::CreateSelectedEntityNode(CHyperFlowGraph* pFlowGraph, CBaseObject* pSelObj)
{
	if (pSelObj)
	{
		if (pSelObj->IsKindOf(RUNTIME_CLASS(CEntityObject)))
		{
			return CreateEntityNode(pFlowGraph, (CEntityObject*)pSelObj);
		}
		else if (pSelObj->IsKindOf(RUNTIME_CLASS(CPrefabObject)))
		{
			return CreatePrefabInstanceNode(pFlowGraph, (CPrefabObject*)pSelObj);
		}
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
CHyperFlowGraph* CFlowGraphManager::CreateGraphForMatFX(IFlowGraphPtr pFG, const CString& filepath)
{
	CHyperFlowGraph* pFlowGraph = new CHyperFlowGraph(this, pFG.get());
	IFlowGraph* const pRuntimeGraph = pFlowGraph->GetIFlowGraph();

	stack_string fileName = PathUtil::GetFileName(filepath.GetString());
	pFlowGraph->SetName(fileName);

	stack_string debugName = "[Material FX] ";
	debugName.append(fileName);
	pRuntimeGraph->SetDebugName(debugName);
	pRuntimeGraph->SetType(IFlowGraph::eFGT_MaterialFx);

	pFlowGraph->Load(filepath.GetString());

	return pFlowGraph;
}

//////////////////////////////////////////////////////////////////////////
CFlowNode* CFlowGraphManager::CreateEntityNode(CHyperFlowGraph* pFlowGraph, CEntityObject* pEntity)
{
	if (!pEntity || !pEntity->GetIEntity())
		return 0;

	string classname = string("entity:") + pEntity->GetIEntity()->GetClass()->GetName();
	CFlowNode* pNode = (CFlowNode*)__super::CreateNode(pFlowGraph, classname.c_str(), pFlowGraph->AllocateId());
	if (pNode)
	{
		// check if we add it to an AIAction
		if (pFlowGraph->GetAIAction() == 0)
			pNode->SetEntity(pEntity);
		else
			pNode->SetEntity(0); // AIAction -> reset entity

		IFlowNodeData* pFlowNodeData = pFlowGraph->GetIFlowGraph()->GetNodeData(pNode->GetFlowNodeId());
		GetNodeConfig(pFlowNodeData, pNode);
	}

	return pNode;
}

//////////////////////////////////////////////////////////////////////////
CFlowNode* CFlowGraphManager::CreatePrefabInstanceNode(CHyperFlowGraph* pFlowGraph, CPrefabObject* pPrefabObj)
{
	if (!pPrefabObj)
		return 0;

	CFlowNode* pNode = (CFlowNode*)__super::CreateNode(pFlowGraph, "Prefab:Instance", pFlowGraph->AllocateId());
	if (pNode)
	{
		// Associate flownode to the prefab
		CPrefabManager* pPrefabManager = GetIEditorImpl()->GetPrefabManager();
		CRY_ASSERT(pPrefabManager != NULL);
		if (pPrefabManager)
		{
			CPrefabEvents* pPrefabEvents = pPrefabManager->GetPrefabEvents();
			CRY_ASSERT(pPrefabEvents != NULL);
			bool bResult = pPrefabEvents->AddPrefabInstanceNodeFromSelection(pNode, pPrefabObj);
			if (!bResult)
			{
				Warning(_T("FlowGraphManager: Failed to add prefab instance node for prefab instance: %s"), pPrefabObj->GetName());
			}
		}
	}

	return pNode;
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphManager::OpenFlowGraphView(IFlowGraph* pIFlowGraph)
{
	assert(pIFlowGraph);
	CHyperFlowGraph* pFlowGraph = new CHyperFlowGraph(this, pIFlowGraph);
	OpenView(pFlowGraph);
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphManager::GetAvailableGroups(std::set<CString>& outGroups, bool bActionGraphs)
{
	std::vector<CHyperFlowGraph*>::iterator iter = m_graphs.begin();
	while (iter != m_graphs.end())
	{
		CHyperFlowGraph* pGraph = *iter;
		if (pGraph != 0)
		{
			if (bActionGraphs == false)
			{
				CEntityObject* pEntity = pGraph->GetEntity();
				if (pEntity && pEntity->CheckFlags(OBJFLAG_PREFAB) == false)
				{
					CString& groupName = pGraph->GetGroupName();
					if (groupName.IsEmpty() == false)
						outGroups.insert(groupName);
				}
			}
			else
			{
				IAIAction* pAction = pGraph->GetAIAction();
				if (pAction)
				{
					CString& groupName = pGraph->GetGroupName();
					if (groupName.IsEmpty() == false)
						outGroups.insert(groupName);
				}
			}
		}
		++iter;
	}
}

//////////////////////////////////////////////////////////////////////////
void CFlowGraphManager::UpdateLayerName(const CString& oldName, const CString& name)
{
	if (!gEnv->pFlowSystem)
		return;

	bool isChanged = false;
	const TFlowNodeId layerTypeId = gEnv->pFlowSystem->GetTypeId("Engine:LayerSwitch");
	size_t numGraphs = GetFlowGraphCount();
	for (size_t i = 0; i < numGraphs; ++i)
	{
		CHyperFlowGraph* pEditorFG = GetFlowGraph(i);
		IHyperGraphEnumerator* pEnum = pEditorFG->GetNodesEnumerator();
		for (IHyperNode* pINode = pEnum->GetFirst(); pINode; pINode = pEnum->GetNext())
		{
			if (!((CHyperNode*)pINode)->IsFlowNode() || ((CHyperNode*)pINode)->GetFlowNodeId() == InvalidFlowNodeId)
				continue;

			CFlowNode* pEditorFN = (CFlowNode*)pINode;
			if (pEditorFN->GetTypeId() == layerTypeId)
			{
				_smart_ptr<CVarBlock> pVarBlock = pEditorFN->GetInputsVarBlock();
				if (pVarBlock)
				{
					IVariable* pVar = pVarBlock->FindVariable("Layer");
					CString layerName;
					pVar->Get(layerName);
					if (layerName == oldName)
					{
						pVar->Set(name);
						isChanged = true;
					}
				}
			}
		}
		pEnum->Release();
	}

	if (isChanged)
		SendNotifyEvent(EHG_GRAPH_CLEAR_SELECTION);
}


CHyperFlowGraph* CFlowGraphManager::FindGraphByName(const char* flowGraphName)
{
	for (int i = 0; i < m_graphs.size(); ++i)
	{
		if (0 == strcmp(m_graphs[i]->GetName(), flowGraphName))
			return m_graphs[i];
	}
	return NULL;
}

namespace
{
void PyOpenFlowGraphView(const char* flowgraph)
{
	CHyperFlowGraph* pFlowGraph = GetIEditorImpl()->GetFlowGraphManager()->FindGraphByName(flowgraph);
	GetIEditorImpl()->GetFlowGraphManager()->OpenView(pFlowGraph);
}
void PyOpenFlowGraphViewAndSelect(const char* flowgraph, const char* entityName)
{
	CHyperFlowGraph* pFlowGraph = GetIEditorImpl()->GetFlowGraphManager()->FindGraphByName(flowgraph);
	CHyperGraphDialog* pDlg = GetIEditorImpl()->GetFlowGraphManager()->OpenView(pFlowGraph);
	if (pDlg)
	{
		CFlowGraphSearchCtrl* pSC = pDlg->GetSearchControl();
		if (pSC)
		{
			CFlowGraphSearchOptions* pOpts = CFlowGraphSearchOptions::GetSearchOptions();
			pOpts->m_bIncludeEntities = true;
			pOpts->m_findSpecial = CFlowGraphSearchOptions::eFLS_None;
			pOpts->m_LookinIndex = CFlowGraphSearchOptions::eFL_Current;
			pSC->Find(entityName, false, true, true);
		}
	}
}
}

DECLARE_PYTHON_MODULE(flowgraph)

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyOpenFlowGraphView, flowgraph, open_view,
                                     "Opens named Flow Graph",
                                     "flowgraph.open_view(str flowGraphName)");

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyOpenFlowGraphViewAndSelect, flowgraph, open_view_and_select,
                                     "Opens named Flow Graph and select entity node",
                                     "flowgraph.open_view_and_select(str flowGraphName,str entityName)");

