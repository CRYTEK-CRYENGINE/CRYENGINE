// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "SandboxAPI.h"

#include "HyperGraphManager.h"
#include "FlowGraphMigrationHelper.h"

struct IAIAction;
struct IUIAction;
struct IFlowNode;
struct IFlowNodeData;
struct IFlowGraph;
struct IFlowGraphModule;
struct SInputPortConfig;

class CEntityObject;
class CHyperFlowGraph;
class CFlowNode;
class CPrefabObject;

//////////////////////////////////////////////////////////////////////////
class SANDBOX_API CFlowGraphManager : public CHyperGraphManager
{
public:
	//////////////////////////////////////////////////////////////////////////
	// CHyperGraphManager implementation.
	//////////////////////////////////////////////////////////////////////////
	virtual ~CFlowGraphManager();

	virtual void Init() override;
	virtual void ReloadClasses() override;
	virtual void ReloadGraphs() override;
	virtual void UpdateNodePrototypeWithoutReload(const char* nodeClassName) override;

	virtual CHyperNode*  CreateNode(CHyperGraph* pGraph, const char* sNodeClass, HyperNodeID nodeId, const Gdiplus::PointF& pos, CBaseObject* pObj = nullptr, bool bAllowMissing = false) override;
	virtual CHyperGraph* CreateGraph() override;
	//////////////////////////////////////////////////////////////////////////

	void OpenFlowGraphView(IFlowGraph* pIFlowGraph);

	// Create graph for specific entity.
	CHyperFlowGraph* CreateGraphForEntity(CEntityObject* pEntity, const char* sGroupName = "");

	// Create graph for specific UI Action.
	CHyperFlowGraph* CreateGraphForUIAction(IUIAction* pUIAction);
	CHyperFlowGraph* FindGraphForUIAction(IUIAction* pUIAction);

	// Create graph for specific AI Action.
	CHyperFlowGraph* CreateGraphForAction(IAIAction* pAction);

	CHyperFlowGraph* FindGraphForAction(IAIAction* pAction);

	// Deletes all graphs created for AI Actions
	void FreeGraphsForActions();

	// Create graph for specific Custom Action.
	CHyperFlowGraph* CreateGraphForCustomAction(ICustomAction* pCustomAction);

	// Create graph for specific Material FX Graphs.
	CHyperFlowGraph* CreateGraphForMatFX(IFlowGraphPtr pFG, const CString& filepath);

	// Finds graph for custom action
	CHyperFlowGraph* FindGraphForCustomAction(ICustomAction* pCustomAction);

	// Deletes all graphs created for Custom Actions
	void FreeGraphsForCustomActions();

	// Creates the editor graph (with description, comment nodes, etc) based on the root graph (runtime CFlowGraphBase for cloning instances) for the given module
	CHyperFlowGraph* CreateEditorGraphForModule(IFlowGraphModule* pModule);

	CHyperFlowGraph* FindGraph(IFlowGraphPtr pFG);
	CHyperFlowGraph* FindGraphByName(const char* flowGraphName);

	void             OnEnteringGameMode(bool inGame = true);

	//////////////////////////////////////////////////////////////////////////
	// Create special nodes.
	CFlowNode* CreateSelectedEntityNode(CHyperFlowGraph* pFlowGraph, CBaseObject* pSelObj);
	CFlowNode* CreateEntityNode(CHyperFlowGraph* pFlowGraph, CEntityObject* pEntity);
	CFlowNode* CreatePrefabInstanceNode(CHyperFlowGraph* pFlowGraph, CPrefabObject* pPrefabObj);

	// Getters
	size_t           GetFlowGraphCount() const { return m_graphs.size(); }
	CHyperFlowGraph* GetFlowGraph(size_t nIndex)  { return m_graphs[nIndex]; };
	void             GetAvailableGroups(std::set<CString>& outGroups, bool bActionGraphs = false);

	CFlowGraphMigrationHelper& GetMigrationHelper() { return m_migrationHelper; }

	void  UpdateLayerName(const CString& oldName, const CString& name);

	void RegisterGraph(CHyperFlowGraph* pGraph);
	void UnregisterGraph(CHyperFlowGraph* pGraph);

private:
	friend class CHyperFlowGraph;
	friend class CUndoFlowGraph;

	void       GetNodeConfig(IFlowNodeData* pSrcNode, CFlowNode* pFlowNode);
	IVariable* MakeInVar(const SInputPortConfig* pConfig, uint32 portId, CFlowNode* pFlowNode);
	IVariable* MakeSimpleVarFromFlowType(int type);

	bool OnSelectGameToken(const string& oldValue, string& newValue);
	bool OnSelectCustomAction(const string& oldValue, string& newValue);

private:
	// All graphs currently created.
	std::vector<CHyperFlowGraph*> m_graphs;
	CFlowGraphMigrationHelper     m_migrationHelper;
};

