// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "LevelIndependentFileMan.h"
#include "HyperGraph/FlowGraph.h"
#include <CryFlowGraph/IFlowGraphModuleManager.h>

class CEditorFlowGraphModuleManager : public ILevelIndependentFileModule, public IFlowGraphModuleListener
{
public:
	CEditorFlowGraphModuleManager();
	virtual ~CEditorFlowGraphModuleManager();

	//ILevelIndependentFileModule
	virtual bool PromptChanges();
	//~ILevelIndependentFileModule

	//IFlowGraphModuleListener
	virtual void OnModuleInstanceCreated(IFlowGraphModule* module, TModuleInstanceId instanceID)   {}
	virtual void OnModuleInstanceDestroyed(IFlowGraphModule* module, TModuleInstanceId instanceID) {}
	virtual void OnModuleDestroyed(IFlowGraphModule* module);
	virtual void OnRootGraphChanged(IFlowGraphModule* module, ERootGraphChangeReason reason);
	virtual void OnModulesScannedAndReloaded();
	//~IFlowGraphModuleListener

	void          Init();

	bool          NewModule(string& filename, bool bGlobal = true, CHyperGraph** pHyperGraph = nullptr);

	IFlowGraphPtr GetModuleFlowGraph(int index) const;
	IFlowGraphPtr GetModuleFlowGraph(const char* name) const;
	void          DeleteModuleFlowGraph(CHyperFlowGraph* pGraph);

	void          CreateModuleNodes(const char* moduleName);

	bool          SaveModuleGraph(CHyperFlowGraph* pFG);

private:

	bool HasModifications();
	void SaveChangedGraphs();
	void DiscardChangedGraphs();
	void CreateEditorFlowgraphs();

	typedef std::vector<CHyperFlowGraph*> TFlowGraphs;
	TFlowGraphs m_FlowGraphs;
};

