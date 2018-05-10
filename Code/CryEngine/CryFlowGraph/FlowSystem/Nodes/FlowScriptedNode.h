// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __FLOWSCRIPTEDNODE_H__
#define __FLOWSCRIPTEDNODE_H__

#pragma once

#include <CryFlowGraph/IFlowSystem.h>

class CFlowScriptedNodeFactory;
TYPEDEF_AUTOPTR(CFlowScriptedNodeFactory);
typedef CFlowScriptedNodeFactory_AutoPtr CFlowScriptedNodeFactoryPtr;
class CFlowSimpleScriptedNodeFactory;
TYPEDEF_AUTOPTR(CFlowSimpleScriptedNodeFactory);
typedef CFlowSimpleScriptedNodeFactory_AutoPtr CFlowSimpleScriptedNodeFactoryPtr;

class CFlowScriptedNode : public IFlowNode
{
public:
	CFlowScriptedNode(const SActivationInfo*, CFlowScriptedNodeFactoryPtr, SmartScriptTable);
	~CFlowScriptedNode();

	// IFlowNode
	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo);
	virtual void         GetConfiguration(SFlowNodeConfig&);
	virtual void         ProcessEvent(EFlowEvent event, SActivationInfo*);
	virtual bool         SerializeXML(SActivationInfo*, const XmlNodeRef& root, bool reading);
	virtual void         Serialize(SActivationInfo*, TSerialize ser);
	virtual void         PostSerialize(SActivationInfo*) {}
	virtual void         GetMemoryUsage(ICrySizer* s) const;
	// ~IFlowNode

	int ActivatePort(IFunctionHandler* pH, size_t nOutput, const TFlowInputData& data);

private:
	SActivationInfo             m_info;
	SmartScriptTable            m_table;
	CFlowScriptedNodeFactoryPtr m_pFactory;
};

class CFlowScriptedNodeFactory : public IFlowNodeFactory
{
public:
	CFlowScriptedNodeFactory();
	~CFlowScriptedNodeFactory();

	bool                 Init(const char* path, const char* name);

	virtual IFlowNodePtr Create(IFlowNode::SActivationInfo*);

	ILINE size_t         NumInputs() const      { return m_inputs.size() - 1; }
	ILINE const char*    InputName(int n) const { return m_inputs[n].name; }
	void                 GetConfiguration(SFlowNodeConfig&);

	virtual void         GetMemoryUsage(ICrySizer* s) const;

	void                 Reset() {}

private:
	SmartScriptTable               m_table;

	std::set<string>               m_stringTable;
	std::vector<SInputPortConfig>  m_inputs;
	std::vector<SOutputPortConfig> m_outputs;
	uint32                         m_category;

	const string& AddString(const char* str);

	static int    ActivateFunction(IFunctionHandler* pH, void* pBuffer, int nSize);
};

class CFlowSimpleScriptedNode : public IFlowNode
{
public:
	CFlowSimpleScriptedNode(const SActivationInfo*, CFlowSimpleScriptedNodeFactoryPtr);
	~CFlowSimpleScriptedNode();

	// IFlowNode
	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo);
	virtual void         GetConfiguration(SFlowNodeConfig&);
	virtual void         ProcessEvent(EFlowEvent event, SActivationInfo*);
	virtual bool         SerializeXML(SActivationInfo*, const XmlNodeRef& root, bool reading);
	virtual void         Serialize(SActivationInfo*, TSerialize ser);
	virtual void         PostSerialize(SActivationInfo*) {};
	// ~IFlowNode

	int          ActivatePort(IFunctionHandler* pH, size_t nOutput, const TFlowInputData& data);

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

private:
	CFlowSimpleScriptedNodeFactoryPtr m_pFactory;
};

class CFlowSimpleScriptedNodeFactory : public IFlowNodeFactory
{
public:
	CFlowSimpleScriptedNodeFactory();
	~CFlowSimpleScriptedNodeFactory();

	bool                 Init(const char* path, const char* name);

	virtual IFlowNodePtr Create(IFlowNode::SActivationInfo*);

	ILINE size_t         NumInputs() const      { return m_inputs.size() - 1; }
	ILINE const char*    InputName(int n) const { return m_inputs[n].name; }
	ILINE int            GetActivateFlags()     { return activateFlags; }
	void                 GetConfiguration(SFlowNodeConfig&);

	bool                 CallFunction(IFlowNode::SActivationInfo* pInputData);
	HSCRIPTFUNCTION      GetFunction() { return m_func; }

	virtual void         GetMemoryUsage(ICrySizer* s) const;

	void                 Reset() {}

private:
	HSCRIPTFUNCTION                m_func;

	std::set<string>               m_stringTable;
	std::vector<SInputPortConfig>  m_inputs;
	std::vector<SOutputPortConfig> m_outputs;
	std::vector<ScriptAnyValue>    m_outputValues;
	uint32                         m_category;

	const string&                  AddString(const char* str);
	int                            activateFlags; // one bit per input port; true means a value change will activate the node
};

#endif
