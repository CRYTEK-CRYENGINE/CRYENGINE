// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FlowScriptedNode.h"
#include <CryScriptSystem/IScriptSystem.h>

namespace
{
bool GetCategory(const char* catName, uint32& outCategory)
{
	if (stricmp(catName, "approved") == 0)
		outCategory = EFLN_APPROVED;
	else if (stricmp(catName, "advanced") == 0)
		outCategory = EFLN_ADVANCED;
	else if (stricmp(catName, "debug") == 0)
		outCategory = EFLN_DEBUG;
	else if (stricmp(catName, "legacy") == 0)
		outCategory = EFLN_OBSOLETE;
	else if (stricmp(catName, "obsolete") == 0)
		outCategory = EFLN_OBSOLETE;
	else if (stricmp(catName, "wip") == 0)
		outCategory = EFLN_ADVANCED;
	else
	{
		//outCategory = EFLN_NOCATEGORY;
		outCategory = EFLN_DEBUG;
		return false;
	}
	return true;
}
}

class CFlowDataToScriptDataVisitor
{
public:
	CFlowDataToScriptDataVisitor(IScriptTable* pTable, const char* name) : m_pTable(pTable), m_name(name) {}

	template<class T>
	void Visit(const T& value)
	{
		m_pTable->SetValue(m_name, value);
	}

	void Visit(const TFlowInputDataVariant& var)
	{
		VisitVariant(var);
	}

	void Visit(EntityId id)
	{
		ScriptHandle hdl;
		hdl.n = id;
		m_pTable->SetValue(m_name, hdl);
	}

	void Visit(const string& str)
	{
		m_pTable->SetValue(m_name, str.c_str());
	}

	void Visit(SFlowSystemVoid)
	{
		m_pTable->SetToNull(m_name);
	}

	template<class T>
	void operator()(T& type)
	{
		Visit(type);
	}

private:
	template<size_t I = 0>
	void VisitVariant(const TFlowInputDataVariant& var)
	{
		if (var.index() == I)
		{
			Visit(stl::get<I>(var));
		}
		else
		{
			VisitVariant<I + 1>(var);
		}
	}

	IScriptTable* m_pTable;
	const char*   m_name;
};
template<>
void CFlowDataToScriptDataVisitor::VisitVariant<stl::variant_size<TFlowInputDataVariant>::value>(const TFlowInputDataVariant& var)
{
	CRY_ASSERT_MESSAGE(false, "Invalid variant index.");
}

class CFlowDataToScriptParamVisitor
{
public:
	CFlowDataToScriptParamVisitor(IScriptSystem* pSS) : m_pSS(pSS) {}

	template<class T>
	void Visit(const T& value)
	{
		m_pSS->PushFuncParam(value);
	}

	void Visit(const TFlowInputDataVariant& var)
	{
		VisitVariant(var);
	}

	void Visit(EntityId id)
	{
		ScriptHandle hdl;
		hdl.n = id;
		m_pSS->PushFuncParam(hdl);
	}

	void Visit(const string& str)
	{
		m_pSS->PushFuncParam(str.c_str());
	}

	void Visit(SFlowSystemVoid)
	{
		ScriptAnyValue n(svtNull);
		m_pSS->PushFuncParamAny(n);
	}

	template<class T>
	void operator()(T& type)
	{
		Visit(type);
	}

private:
	template<size_t I = 0>
	void VisitVariant(const TFlowInputDataVariant& var)
	{
		if (var.index() == I)
		{
			Visit(stl::get<I>(var));
		}
		else
		{
			VisitVariant<I + 1>(var);
		}
	}

	IScriptSystem* m_pSS;
};
template<>
void CFlowDataToScriptParamVisitor::VisitVariant<stl::variant_size<TFlowInputDataVariant>::value>(const TFlowInputDataVariant& var)
{
	CRY_ASSERT_MESSAGE(false, "Invalid variant index.");
}

/*
 * node
 */

CFlowScriptedNode::CFlowScriptedNode(const SActivationInfo* pInfo, CFlowScriptedNodeFactoryPtr pFactory, SmartScriptTable table) :
	m_info(*pInfo), m_table(table), m_pFactory(pFactory)
{
	ScriptHandle thisHandle;
	thisHandle.ptr = this;
	m_table->SetValue("__this", thisHandle);
}

CFlowScriptedNode::~CFlowScriptedNode()
{
}

void CFlowScriptedNode::GetMemoryUsage(ICrySizer* s) const
{
	s->Add(*this);
}

IFlowNodePtr CFlowScriptedNode::Clone(SActivationInfo* pActInfo)
{
	SmartScriptTable newTable(m_table->GetScriptSystem());
	newTable->Clone(m_table.GetPtr(), true);
	return new CFlowScriptedNode(pActInfo, m_pFactory, newTable);
}

void CFlowScriptedNode::GetConfiguration(SFlowNodeConfig& config)
{
	m_pFactory->GetConfiguration(config);
}

void CFlowScriptedNode::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	if (event != eFE_Activate)
		return;

	// pass 1: update data
	for (size_t i = 0; i < m_pFactory->NumInputs(); i++)
	{
		if (IsPortActive(pActInfo, i))
		{
			//TODO
			//pActInfo->pInputPorts[i].Visit( CFlowDataToScriptDataVisitor(m_table.GetPtr(), m_pFactory->InputName(i)) );
		}
	}

	// pass 2: call OnActivate functions
	for (size_t i = 0; i < m_pFactory->NumInputs(); i++)
	{
		if (IsPortActive(pActInfo, i))
		{
			static char buffer[256] = "OnActivate_";
			strcpy(buffer + 11, m_pFactory->InputName(i));
			Script::CallMethod(m_table.GetPtr(), buffer);
		}
	}
}

void CFlowScriptedNode::Serialize(SActivationInfo* pActInfo, TSerialize ser)
{
	ser.Value("ScriptData", m_table);
}

bool CFlowScriptedNode::SerializeXML(SActivationInfo*, const XmlNodeRef& root, bool reading)
{
	// TODO: when we have Lua-XML serialization, add a call here ;)
	return true;
}

int CFlowScriptedNode::ActivatePort(IFunctionHandler* pH, size_t nOutput, const TFlowInputData& data)
{
	m_info.pGraph->ActivatePort(SFlowAddress(m_info.myID, static_cast<TFlowPortId>(nOutput), true), data);
	return pH->EndFunction();
}

/*
 * factory
 */

CFlowScriptedNodeFactory::CFlowScriptedNodeFactory()
{
}

CFlowScriptedNodeFactory::~CFlowScriptedNodeFactory()
{
}

bool CFlowScriptedNodeFactory::Init(const char* path, const char* nodeName)
{
	static SInputPortConfig termInput = { 0 };
	static SOutputPortConfig termOutput = { 0 };

	IScriptSystem* pSS = gEnv->pScriptSystem;
	if (!pSS->ExecuteFile(path))
		return false;
	if (!pSS->GetGlobalValue(nodeName, m_table))
		return false;

	SmartScriptTable inputs;
	if (m_table->GetValue("Inputs", inputs))
	{
		int n = inputs->Count();
		for (int i = 1; i <= n; i++)
		{
			const char* cname;
			if (inputs->GetAt(i, cname))
			{
				const string& name = AddString(cname);
				m_inputs.push_back(InputPortConfig_AnyType(name.c_str()));
			}
		}
	}
	m_inputs.push_back(termInput);
	SmartScriptTable outputs;
	if (m_table->GetValue("Outputs", outputs))
	{
		int n = outputs->Count();
		for (int i = 1; i <= n; i++)
		{
			const char* cname;
			if (outputs->GetAt(i, cname))
			{
				const string& name = AddString(cname);
				size_t idx = m_outputs.size();
				m_outputs.push_back(OutputPortConfig_AnyType(name.c_str()));

				string activateName = "Activate_" + name;

				IScriptTable::SUserFunctionDesc fd;
				fd.pUserDataFunc = ActivateFunction;
				fd.sFunctionName = activateName.c_str();
				fd.nDataSize = sizeof(size_t);
				fd.pDataBuffer = &idx;
				fd.sGlobalName = name.c_str();
				fd.sFunctionParams = "value";

				m_table->AddFunction(fd);
			}
		}
	}
	m_outputs.push_back(termOutput);

	char* categoryString = 0;
	m_table->GetValue("Category", categoryString);
	if (categoryString)
	{
		if (GetCategory(categoryString, m_category) == false)
		{
			GameWarning("[flow] Unrecognized Category '%s' in ScriptedFlowNode '%s'", categoryString, nodeName);
		}
	}
	else
		m_category = EFLN_DEBUG;
	//m_category = EFLN_NOCATEGORY;

	return true;
}

IFlowNodePtr CFlowScriptedNodeFactory::Create(IFlowNode::SActivationInfo* pInfo)
{
	SmartScriptTable newTable(m_table->GetScriptSystem());
	newTable->Clone(m_table.GetPtr(), true);
	return new CFlowScriptedNode(pInfo, this, newTable);
}

void CFlowScriptedNodeFactory::GetConfiguration(SFlowNodeConfig& config)
{
	config.pInputPorts = &m_inputs[0];
	config.pOutputPorts = &m_outputs[0];
	config.SetCategory(m_category);
}

const string& CFlowScriptedNodeFactory::AddString(const char* c_str)
{
	string str = c_str;
	return *m_stringTable.insert(str).first;
}

int CFlowScriptedNodeFactory::ActivateFunction(IFunctionHandler* pH, void* pBuffer, int nSize)
{
	// TODO: log errors
	IScriptSystem* pSS = pH->GetIScriptSystem();

	CRY_ASSERT(nSize == sizeof(size_t));
	size_t nOutput = *(size_t*)pBuffer;

	ScriptHandle hdl;
	SmartScriptTable tblNode;

	SmartScriptTable pTable = NULL;
	ScriptHandle pVoidNode;
	if (!pH->GetParam(1, pTable) || !pTable)
		return pH->EndFunction();
	if (!pTable->GetValue("__this", pVoidNode) || !pVoidNode.ptr)
		return pH->EndFunction();
	CFlowScriptedNode* pNode = (CFlowScriptedNode*) pVoidNode.ptr;

	static const int IDX = 2;
	switch (pH->GetParamType(IDX))
	{
	case svtNull:
		return pNode->ActivatePort(pH, nOutput, TFlowInputData());
	case svtString:
		{
			const char* temp;
			pH->GetParam(IDX, temp);
			return pNode->ActivatePort(pH, nOutput, TFlowInputData(string(temp)));
		}
	case svtNumber:
		{
			float temp;
			pH->GetParam(IDX, temp);
			return pNode->ActivatePort(pH, nOutput, TFlowInputData(temp));
		}
	case svtBool:
		{
			bool temp;
			pH->GetParam(IDX, temp);
			return pNode->ActivatePort(pH, nOutput, TFlowInputData(temp));
		}
	default:
		pSS->RaiseError("Unknown or unsupported ScriptVarType %d", pH->GetParamType(IDX));
	}

	return pH->EndFunction();
}

void CFlowScriptedNodeFactory::GetMemoryUsage(ICrySizer* s) const
{
	SIZER_SUBCOMPONENT_NAME(s, "CFlowScriptedNodeFactory");
	s->Add(*this);
	s->AddObject(m_stringTable);
	s->AddObject(m_inputs);
	s->AddObject(m_outputs);
	for (std::set<string>::const_iterator iter = m_stringTable.begin(); iter != m_stringTable.end(); ++iter)
		s->Add(*iter);
	for (size_t i = 0; i < m_inputs.size(); i++)
		m_inputs[i].defaultData.GetMemoryStatistics(s);
}

/*
 * CFlowSimpleScriptedNode
 */

CFlowSimpleScriptedNode::CFlowSimpleScriptedNode(const SActivationInfo* pInfo, CFlowSimpleScriptedNodeFactoryPtr pFactory) :
	m_pFactory(pFactory)
{
}

CFlowSimpleScriptedNode::~CFlowSimpleScriptedNode()
{
}

IFlowNodePtr CFlowSimpleScriptedNode::Clone(SActivationInfo* pActInfo)
{
	return new CFlowSimpleScriptedNode(pActInfo, m_pFactory);
}

void CFlowSimpleScriptedNode::GetConfiguration(SFlowNodeConfig& config)
{
	m_pFactory->GetConfiguration(config);
}

void CFlowSimpleScriptedNode::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	IScriptSystem* pSS = gEnv->pScriptSystem;
	switch (event)
	{
	case eFE_Activate:

		for (size_t i = 0; i < m_pFactory->NumInputs(); i++)
		{
			if (IsPortActive(pActInfo, i) && (m_pFactory->GetActivateFlags() & (1 << i)))
			{
				pActInfo->pGraph->RequestFinalActivation(pActInfo->myID);
				break;
			}
		}
		break;
	case eFE_Initialize:
		break;
	case eFE_FinalActivate:
		m_pFactory->CallFunction(pActInfo);
		break;
	}
}

void CFlowSimpleScriptedNode::Serialize(SActivationInfo* pActInfo, TSerialize ser)
{
}

bool CFlowSimpleScriptedNode::SerializeXML(SActivationInfo*, const XmlNodeRef& root, bool reading)
{
	return true;
}

/*
 * CFlowSimpleScriptedNodeFactory
 */

CFlowSimpleScriptedNodeFactory::CFlowSimpleScriptedNodeFactory() : m_func(0), activateFlags(0)
{
}

CFlowSimpleScriptedNodeFactory::~CFlowSimpleScriptedNodeFactory()
{
	if (m_func)
		gEnv->pScriptSystem->ReleaseFunc(m_func);
}

bool CFlowSimpleScriptedNodeFactory::Init(const char* path, const char* nodeName)
{
	static SInputPortConfig termInput = { 0 };
	static SOutputPortConfig termOutput = { 0 };
	activateFlags = 0;

	if (const char* p = strchr(nodeName, ':'))
		nodeName = p + 1;

	IScriptSystem* pSS = gEnv->pScriptSystem;
	if (!pSS->ExecuteFile(path))
		return false;
	SmartScriptTable pTable;
	if (!pSS->GetGlobalValue(nodeName, pTable))
		return false;

	SmartScriptTable inputs;
	if (pTable->GetValue("Inputs", inputs))
	{
		int nItems = inputs->Count();
		for (int i = 1; i <= nItems; i++)
		{
			SmartScriptTable tbl;
			if (inputs->GetAt(i, tbl))
			{
				const char* cname;
				const char* dname;
				const char* enumStr;
				const char* type;
				if (!tbl->GetAt(1, cname) || !tbl->GetAt(2, type))
				{
					GameWarning("Element %d is incorrect - should be {name,type} or {name,type,help}", i);
					return false;
				}
				const string& name = AddString(cname);

				// find which ports will trigger an activation
				// their names start by 't_'
				activateFlags |= ((name[0] == 't') && (name[1] == '_')) ? (1 << (i - 1)) : 0;

				if (tbl->GetAt(3, dname)) // Get description if supplied
					dname = AddString(dname).c_str();
				else
					dname = 0;

				if (tbl->GetAt(4, enumStr)) // Get enumeration string if supplied
					enumStr = AddString(enumStr).c_str();
				else
					enumStr = 0;

				if (0 == strcmp("string", type))
					m_inputs.push_back(InputPortConfig<string>(name.c_str(), dname, NULL, enumStr));
				else if (0 == strcmp("bool", type))
					m_inputs.push_back(InputPortConfig<bool>(name.c_str(), dname, NULL, enumStr));
				else if (0 == strcmp("entityid", type))
					m_inputs.push_back(InputPortConfig<EntityId>(name.c_str(), dname, NULL, enumStr));
				else if (0 == strcmp("int", type))
					m_inputs.push_back(InputPortConfig<int>(name.c_str(), dname, NULL, enumStr));
				else if (0 == strcmp("float", type))
					m_inputs.push_back(InputPortConfig<float>(name.c_str(), dname, NULL, enumStr));
				else if (0 == strcmp("vec3", type))
					m_inputs.push_back(InputPortConfig<Vec3>(name.c_str(), dname, NULL, enumStr));
				else
				{
					GameWarning("Unknown type %s", type);
					return false;
				}
			}
			else
			{
				GameWarning("Table is of incorrect format");
				return false;
			}
		}
	}
	m_inputs.push_back(termInput);
	SmartScriptTable outputs;
	if (pTable->GetValue("Outputs", outputs))
	{
		int nItems = outputs->Count();
		for (int i = 1; i <= nItems; i++)
		{
			SmartScriptTable tbl;
			if (outputs->GetAt(i, tbl))
			{
				const char* cname;
				const char* dname;
				const char* type;
				if (!tbl->GetAt(1, cname) || !tbl->GetAt(2, type))
				{
					GameWarning("Element %d is incorrect - should be {name,type} or {name,type,help}", i);
					return false;
				}
				const string& name = AddString(cname);

				if (tbl->GetAt(3, dname)) // Get description if supplied
					dname = AddString(dname).c_str();
				else
					dname = 0;

				if (0 == strcmp("string", type))
					m_outputs.push_back(OutputPortConfig<string>(name.c_str(), dname));
				else if (0 == strcmp("bool", type))
					m_outputs.push_back(OutputPortConfig<bool>(name.c_str(), dname));
				else if (0 == strcmp("entityid", type))
					m_outputs.push_back(OutputPortConfig<EntityId>(name.c_str(), dname));
				else if (0 == strcmp("int", type))
					m_outputs.push_back(OutputPortConfig<int>(name.c_str(), dname));
				else if (0 == strcmp("float", type))
					m_outputs.push_back(OutputPortConfig<float>(name.c_str(), dname));
				else if (0 == strcmp("vec3", type))
					m_outputs.push_back(OutputPortConfig<Vec3>(name.c_str(), dname));
				else
				{
					GameWarning("Unknown type %s", type);
					return false;
				}
			}
			else
			{
				GameWarning("Table is of incorrect format");
				return false;
			}
		}
		m_outputValues.resize(m_outputs.size());
	}
	m_outputs.push_back(termOutput);

	if (!pTable->GetValue("Implementation", m_func))
	{
		return false;
	}

	char* categoryString = 0;
	pTable->GetValue("Category", categoryString);
	if (categoryString)
	{
		if (GetCategory(categoryString, m_category) == false)
		{
			GameWarning("[flow] Unrecognized Category '%s' in ScriptedFlowNode '%s'", categoryString, nodeName);
		}
	}
	else
		m_category = EFLN_DEBUG;
	//m_category = EFLN_NOCATEGORY;

	return true;
}

bool CFlowSimpleScriptedNodeFactory::CallFunction(IFlowNode::SActivationInfo* pActInfo)
{
	if (!m_func)
		return false;

	IScriptSystem* pSS = gEnv->pScriptSystem;

	pSS->BeginCall(m_func);
	for (size_t i = 0; i < m_inputs.size() - 1; i++)
	{
		CFlowDataToScriptParamVisitor visitor(pSS);
		pActInfo->pInputPorts[i].Visit(visitor);
	}

	if (pSS->EndCallAnyN(m_outputValues.size(), &m_outputValues[0]))
	{
		for (size_t i = 0; i < m_outputs.size() - 1; i++)
		{
			SFlowAddress port(pActInfo->myID, static_cast<TFlowPortId>(i), true);

			switch (m_outputValues[i].GetType())
			{
			case EScriptAnyType::Nil:
				pActInfo->pGraph->ActivatePort(port, SFlowSystemVoid());
				break;
			case EScriptAnyType::Boolean:
				pActInfo->pGraph->ActivatePort(port, m_outputValues[i].GetBool());
				break;
			case EScriptAnyType::Handle:
				pActInfo->pGraph->ActivatePort(port, EntityId(m_outputValues[i].GetUserData().nRef));
				break;
			case EScriptAnyType::Number:
				pActInfo->pGraph->ActivatePort(port, m_outputValues[i].GetNumber());
				break;
			case EScriptAnyType::String:
				pActInfo->pGraph->ActivatePort(port, string(m_outputValues[i].GetString()));
				break;
			case EScriptAnyType::Table:
				{
					Vec3 vec;
					IScriptTable* pTable = m_outputValues[i].GetScriptTable();
					if (pTable->GetValue("x", vec.x) && pTable->GetValue("y", vec.y) && pTable->GetValue("z", vec.z))
					{
						pActInfo->pGraph->ActivatePort(port, vec);
					}
				}
				break;
			case EScriptAnyType::Vector:
				{
					Vec3 v = m_outputValues[i].GetVector();
					pActInfo->pGraph->ActivatePort(port, v);
				}
				break;
			}
		}
	}
	return true;
}

IFlowNodePtr CFlowSimpleScriptedNodeFactory::Create(IFlowNode::SActivationInfo* pInfo)
{
	return new CFlowSimpleScriptedNode(pInfo, this);
}

void CFlowSimpleScriptedNodeFactory::GetConfiguration(SFlowNodeConfig& config)
{
	config.pInputPorts = &m_inputs[0];
	config.pOutputPorts = &m_outputs[0];
	config.SetCategory(m_category);
}

const string& CFlowSimpleScriptedNodeFactory::AddString(const char* c_str)
{
	string str = c_str;
	return *m_stringTable.insert(str).first;
}

void CFlowSimpleScriptedNodeFactory::GetMemoryUsage(ICrySizer* s) const
{
	SIZER_SUBCOMPONENT_NAME(s, "CFlowSimpleScriptedNodeFactory");
	s->Add(*this);
	s->AddObject(m_stringTable);
	s->AddObject(m_inputs);
	s->AddObject(m_outputs);
	s->AddObject(m_outputValues);
}
