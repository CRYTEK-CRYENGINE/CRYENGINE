// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "HyperGraph.h"
#include "SandboxAPI.h"

#include <CryEntitySystem/IEntitySystem.h>
#include <CryFlowGraph/IFlowSystem.h>

class CEntityObject;
class CFlowNode;
struct IAIAction;
struct IUIAction;
struct ICustomAction;

//////////////////////////////////////////////////////////////////////////
class CFlowEdge : public CHyperEdge
{
public:
	SFlowAddress addr_in;
	SFlowAddress addr_out;
};

//////////////////////////////////////////////////////////////////////////
// Specialization of HyperGraph to handle logical Flow Graphs.
//////////////////////////////////////////////////////////////////////////
class SANDBOX_API CHyperFlowGraph : public CHyperGraph, SFlowNodeActivationListener
{
public:
	CHyperFlowGraph(CHyperGraphManager* pManager, const char* sGroupName = "", CEntityObject* pEntity = nullptr);
	CHyperFlowGraph(CHyperGraphManager* pManager, IFlowGraph* pIFlowGraph);

	// CHyperGraph
	virtual IHyperGraph* Clone() override;
	virtual void         Serialize(XmlNodeRef& node, bool bLoading, CObjectArchive* ar = 0) override;
	virtual void         SetEnabled(bool bEnable) override;
	virtual bool         IsFlowGraph() override { return true; }
	virtual bool         Migrate(XmlNodeRef& node) override;
	virtual void         SetGroupName(const CString& sName) override;
	virtual void         PostClone(CBaseObject* pFromObject, CObjectCloneContext& ctx) override;
	virtual bool         CanConnectPorts(CHyperNode* pSrcNode, CHyperNodePort* pSrcPort, CHyperNode* pTrgNode, CHyperNodePort* pTrgPort, CHyperEdge* pExistingEdge = nullptr) override;
	// ~CHyperGraph

	//////////////////////////////////////////////////////////////////////////
	IFlowGraph* GetIFlowGraph();
	void        SetIFlowGraph(IFlowGraph* pFlowGraph);
	void        OnEnteringGameMode();

	// SFlowNodeActivationListener
	virtual bool OnFlowNodeActivation(IFlowGraphPtr pFlowGraph, TFlowNodeId srcNode, TFlowPortId srcPort, TFlowNodeId toNode, TFlowPortId toPort, const char* value);
	// ~SFlowNodeActivationListener

	//////////////////////////////////////////////////////////////////////////
	// Assigns current entity of the flow graph.
	// if bAdjustGraphNodes==true assigns this entity to all nodes which have t
	void           SetEntity(CEntityObject* pEntity, bool bAdjustGraphNodes = false);
	CEntityObject* GetEntity() const { return m_pEntity; }
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Assigns AI Action to the flow graph.
	void               SetAIAction(IAIAction* pAIAction);
	virtual IAIAction* GetAIAction() const { return m_pAIAction; }
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Set flow graph as UI Action.
	void               SetUIAction(IUIAction* pUIAction);
	virtual IUIAction* GetUIAction() const { return m_pUIAction; }
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Assigns Custom Action to the flow graph.
	void                   SetCustomAction(ICustomAction* pCustomAction);
	virtual ICustomAction* GetCustomAction() const { return m_pCustomAction; }
	//////////////////////////////////////////////////////////////////////////

	enum EMultiPlayerType
	{
		eMPT_ServerOnly = 0,
		eMPT_ClientOnly,
		eMPT_ClientServer
	};

	void SetMPType(EMultiPlayerType mpType)
	{
		m_mpType = mpType;
	}

	EMultiPlayerType GetMPType()
	{
		return m_mpType;
	}

	virtual void         ValidateEdges(CHyperNode* pNode);
	virtual IUndoObject* CreateUndo();
	virtual CFlowNode*   FindFlowNode(TFlowNodeId id) const;

	// Extract GUID remapping from the prefab if the FG is part of one
	// This is needed in order to properly deserialize
	void ExtractObjectsPrefabIdToGlobalIdMappingFromPrefab(CObjectArchive& archive);

protected:

	virtual ~CHyperFlowGraph();

	virtual void        RemoveEdge(CHyperEdge* pEdge);
	virtual CHyperEdge* MakeEdge(CHyperNode* pNodeOut, CHyperNodePort* pPortOut, CHyperNode* pNodeIn, CHyperNodePort* pPortIn, bool bEnabled);
	virtual void        OnPostLoad();
	virtual void        EnableEdge(CHyperEdge* pEdge, bool bEnable);
	virtual void        ClearAll();

	void                InitializeFromGame();

protected:
	IFlowGraphPtr  m_pGameFlowGraph;
	CEntityObject* m_pEntity;
	IAIAction*     m_pAIAction;
	IUIAction*     m_pUIAction;
	ICustomAction* m_pCustomAction;

	// MultiPlayer Type of this flowgraph (default ServerOnly)
	EMultiPlayerType m_mpType;
};

