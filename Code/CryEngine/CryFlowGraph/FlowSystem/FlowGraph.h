// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "FlowSystem.h"
#include "FlowData.h"
#include <CryString/StringUtils.h>

#include <CryFlowGraph/IFlowGraphDebugger.h>

// class CFlowSystem;

//#undef  FLOW_DEBUG_PENDING_UPDATES

#define MAX_GRAPH_ENTITIES 2
#define ALLOW_MULTIPLE_PORT_ACTIVATIONS_PER_UPDATE

//////////////////////////////////////////////////////////////////////////
class CFlowGraphBase : public IFlowGraph
{
public:
	CFlowGraphBase(CFlowSystem* pSys);
	virtual ~CFlowGraphBase();

	// IFlowGraph
	virtual void          AddRef();
	virtual void          Release();
	virtual IFlowGraphPtr Clone();
	virtual void          Clear();
	virtual void RegisterHook(IFlowGraphHookPtr);
	virtual void UnregisterHook(IFlowGraphHookPtr);
	virtual IFlowNodeIteratorPtr       CreateNodeIterator();
	virtual IFlowEdgeIteratorPtr       CreateEdgeIterator();
	virtual void                       SetUserData(TFlowNodeId id, const XmlNodeRef& data);
	virtual XmlNodeRef                 GetUserData(TFlowNodeId id);
	virtual void                       SetGraphEntity(EntityId id, int nIndex = 0);
	virtual EntityId                   GetGraphEntity(int nIndex) const;
	virtual SFlowAddress               ResolveAddress(const char* addr, bool isOutput);
	virtual TFlowNodeId                ResolveNode(const char* name);
	virtual void                       GetNodeConfiguration(TFlowNodeId id, SFlowNodeConfig&);
	virtual bool                       LinkNodes(SFlowAddress from, SFlowAddress to);
	virtual void                       UnlinkNodes(SFlowAddress from, SFlowAddress to);
	virtual TFlowNodeId                CreateNode(TFlowNodeTypeId typeId, const char* name, void* pUserData = 0);
	virtual TFlowNodeId                CreateNode(const char* typeName, const char* name, void* pUserData = 0);
	virtual bool                       SetNodeName(TFlowNodeId id, const char* sName);
	virtual IFlowNodeData*             GetNodeData(TFlowNodeId id);
	virtual const char*                GetNodeName(TFlowNodeId id);
	virtual TFlowNodeTypeId            GetNodeTypeId(TFlowNodeId id);
	virtual const char*                GetNodeTypeName(TFlowNodeId id);
	virtual void                       RemoveNode(const char* name);
	virtual void                       RemoveNode(TFlowNodeId id);
	virtual void                       SetEnabled(bool bEnabled);
	virtual bool                       IsEnabled() const { return m_bEnabled; }
	virtual void                       SetActive(bool bActive);
	virtual bool                       IsActive() const  { return m_bActive; }
	virtual void                       UnregisterFromFlowSystem();

	virtual void                       SetType(IFlowGraph::EFlowGraphType type) { m_Type = type; }
	virtual IFlowGraph::EFlowGraphType GetType() const                          { return m_Type; }

	virtual void                       RegisterFlowNodeActivationListener(SFlowNodeActivationListener* listener);
	virtual void                       RemoveFlowNodeActivationListener(SFlowNodeActivationListener* listener);

	virtual void                       Update();
	virtual void                       InitializeValues();
	virtual bool                       SerializeXML(const XmlNodeRef& root, bool reading);
	virtual void                       Serialize(TSerialize ser);
	virtual void                       PostSerialize();
	virtual void                       SetRegularlyUpdated(TFlowNodeId id, bool regularly);
	virtual void RequestFinalActivation(TFlowNodeId);
	virtual void                       ActivateNode(TFlowNodeId id) { ActivateNodeInt(id); }
	virtual void                       ActivatePortAny(SFlowAddress output, const TFlowInputData&);
	virtual void                       ActivatePortCString(SFlowAddress output, const char* cstr);
	virtual bool                       SetInputValue(TFlowNodeId node, TFlowPortId port, const TFlowInputData&);
	virtual bool                       IsOutputConnected(SFlowAddress output);
	virtual const TFlowInputData*      GetInputValue(TFlowNodeId node, TFlowPortId port);
	virtual bool                       GetActivationInfo(const char* nodeName, IFlowNode::SActivationInfo& actInfo);
	virtual void     SetEntityId(TFlowNodeId, EntityId);
	virtual EntityId GetEntityId(TFlowNodeId);

	virtual IFlowGraphPtr                  GetClonedFlowGraph() const { return m_pClonedFlowGraph; }

	virtual void                           PrecacheResources();

	virtual size_t                         GetGraphTokenCount() const;
	virtual const IFlowGraph::SGraphToken* GetGraphToken(size_t index) const;
	virtual const char*                    GetGlobalNameForGraphToken(const char* tokenName) const;
	virtual bool                           AddGraphToken(const IFlowGraph::SGraphToken& token);
	virtual void                           RemoveGraphTokens();

	virtual TFlowGraphId                   GetGraphId() const { return m_graphId; }
	virtual bool                           IsInInitializationPhase() const { return m_bNeedsInitialize; }

	virtual void                           EnsureSortedEdges()
	{
		if (!m_bEdgesSorted)
			SortEdges();
	}
	// ~IFlowGraph

	virtual void GetMemoryUsage(ICrySizer* s) const;

	// temporary solutions [ ask Dejan ]

	// Suspended flow graphs were needed for AI Action flow graphs.
	// Suspended flow graphs aren't updated...
	// Nodes in suspended flow graphs should ignore OnEntityEvent notifications!
	virtual void SetSuspended(bool suspend = true);
	virtual bool IsSuspended() const;

	// AI action related
	virtual void       SetAIAction(IAIAction* pAIAction);
	virtual IAIAction* GetAIAction() const;

	// Custom action related
	virtual void           SetCustomAction(ICustomAction* pCustomAction);
	virtual ICustomAction* GetCustomAction() const;
	IFlowGraphPtr          GetClonedFlowGraph() { return m_pClonedFlowGraph; }

	//////////////////////////////////////////////////////////////////////////
	IEntity* GetIEntityForNode(TFlowNodeId id);

	// Called only from CFlowSystem::~CFlowSystem()
	void                                       NotifyFlowSystemDestroyed();

	void                                       RegisterInspector(IFlowGraphInspectorPtr pInspector);
	void                                       UnregisterInspector(IFlowGraphInspectorPtr pInspector);
	const std::vector<IFlowGraphInspectorPtr>& GetInspectors() const { return m_inspectors; }

	CFlowSystem*                               GetSys() const        { return m_pSys; }

	// get some more stats
	void GetGraphStats(int& nodeCount, int& edgeCount);

	void OnEntityReused(IEntity* pEntity, SEntitySpawnParams& params);
	void OnEntityIdChanged(EntityId oldId, EntityId newId);
	void UpdateForwardings();

	bool NotifyFlowNodeActivationListeners(TFlowNodeId srcNode, TFlowPortId srcPort, TFlowNodeId toNode, TFlowPortId toPort, const char* value);

protected:

	// helper to broadcast an activation
	template<class T>
	void PerformActivation(const SFlowAddress, const T& value);

	void CloneInner(CFlowGraphBase* pClone);

private:
	void ResetGraphToken(const IFlowGraph::SGraphToken& token);
	void UnregisterGraphTokens();

	class CNodeIterator;
	class CEdgeIterator;

	void       FlowLoadError(const char* format, ...) PRINTF_PARAMS(2, 3);

	ILINE void NeedUpdate()
	{
		m_bNeedsUpdating = true;
	}
	ILINE void ActivateNodeInt(TFlowNodeId id)
	{
		if (m_modifiedNodes[id] == NOT_MODIFIED)
		{
			m_modifiedNodes[id] = m_firstModifiedNode;
			m_firstModifiedNode = id;
		}
		if (!m_bInUpdate)
			NeedUpdate();
	}
	IFlowNodePtr CreateNodeOfType(IFlowNode::SActivationInfo* pInfo, TFlowNodeTypeId typeId);
	void         SortEdges();
	TFlowNodeId  AllocateId();
	void DeallocateId(TFlowNodeId);
	bool ValidateAddress(SFlowAddress);
	bool ValidateNode(TFlowNodeId) const;
	bool                               ValidateLink(SFlowAddress& from, SFlowAddress& to);
	static void                        RemoveNodeFromActivationArray(TFlowNodeId id, TFlowNodeId& front, std::vector<TFlowNodeId>& array);
	void                               Cleanup();
	bool                               ReadXML(const XmlNodeRef& root);
	bool                               WriteXML(const XmlNodeRef& root);
	std::pair<CFlowData*, TFlowNodeId> CreateNodeInt(TFlowNodeTypeId typeId, const char* name, void* pUserData = 0);
	string                             PrettyAddress(SFlowAddress addr);
	SFlowAddress                       ResolveAddress(const string& node, const string& port, bool isOutput)
	{
		return ResolveAddress(node.c_str(), port.c_str(), isOutput);
	}
	SFlowAddress ResolveAddress(const char* node, const char* port, bool isOutput);
	void         DoUpdate(IFlowNode::EFlowEvent event);
	void         NeedInitialize()
	{
		m_bNeedsInitialize = true;
		NeedUpdate();
	}

	template<class T>
	inline string ToString(const T& value);

	template<class T>
	bool        NotifyFlowNodeActivationListeners(TFlowNodeId srcNode, TFlowPortId srcPort, TFlowNodeId toNode, TFlowPortId toPort, const T& value);

#if defined (FLOW_DEBUG_PENDING_UPDATES)
	void DebugPendingActivations();
#endif

	const char* GetDebugName() const;
	void SetDebugName(const char* sName);
	void CreateDebugName();


	// the set of modified nodes, not modified marker
	static const TFlowNodeId NOT_MODIFIED;
	// end of modified list marker
	static const TFlowNodeId END_OF_MODIFIED_LIST;


	// PerformActivation works with this
	std::vector<TFlowNodeId>                  m_modifiedNodes;
	// This list is used for flowgraph debugging
	std::vector<SFlowNodeActivationListener*> m_flowNodeActivationListeners;
	// and Update swaps modifiedNodes and this so that we don't get messed
	// up during the activation sweep
	std::vector<TFlowNodeId> m_activatingNodes;
	// and this is the head of m_modifiedNodes
	TFlowNodeId              m_firstModifiedNode;
	// and this is the head of m_activatingNodes
	TFlowNodeId              m_firstActivatingNode;
	// are we in an update loop?
	bool                     m_bInUpdate;
	// Activate may request a final activation; these get inserted here, and we
	// sweep through it at the end of the update process
	std::vector<TFlowNodeId> m_finalActivatingNodes;
	TFlowNodeId              m_firstFinalActivatingNode;

	// all of the node data
	std::vector<CFlowData>   m_flowData;
	// deallocated id's waiting to be reused
	std::vector<TFlowNodeId> m_deallocatedIds;

	// a link between nodes
	struct SEdge
	{
		ILINE SEdge() : fromNode(InvalidFlowNodeId), toNode(InvalidFlowNodeId), fromPort(InvalidFlowPortId), toPort(InvalidFlowPortId) {}
		ILINE SEdge(SFlowAddress from, SFlowAddress to) : fromNode(from.node), toNode(to.node), fromPort(from.port), toPort(to.port)
		{
			CRY_ASSERT(from.isOutput);
			CRY_ASSERT(!to.isOutput);
		}
		ILINE SEdge(TFlowNodeId fromNode_, TFlowPortId fromPort_, TFlowNodeId toNode_, TFlowPortId toPort_) : fromNode(fromNode_), toNode(toNode_), fromPort(fromPort_), toPort(toPort_) {}

		TFlowNodeId fromNode;
		TFlowNodeId toNode;
		TFlowPortId fromPort;
		TFlowPortId toPort;

		ILINE bool operator<(const SEdge& rhs) const
		{
			if (fromNode < rhs.fromNode)
				return true;
			else if (fromNode > rhs.fromNode)
				return false;
			else if (fromPort < rhs.fromPort)
				return true;
			else if (fromPort > rhs.fromPort)
				return false;
			else if (toNode < rhs.toNode)
				return true;
			else if (toNode > rhs.toNode)
				return false;
			else if (toPort < rhs.toPort)
				return true;
			else
				return false;
		}
		ILINE bool operator==(const SEdge& rhs) const
		{
			return fromNode == rhs.fromNode && fromPort == rhs.fromPort &&
			       toNode == rhs.toNode && toPort == rhs.toPort;
		}

		void GetMemoryUsage(ICrySizer* pSizer) const {}
	};
	class SEdgeHasNode;
	std::vector<SEdge> m_edges;
	bool               m_bEnabled;
	bool               m_bActive;
	bool               m_bEdgesSorted;
	bool               m_bNeedsInitialize;
	bool               m_bNeedsUpdating;
	CFlowSystem*       m_pSys;

	// all of the regularly updated nodes (there aught not be too many)
	typedef std::vector<TFlowNodeId> RegularUpdates;
	RegularUpdates m_regularUpdates;
	RegularUpdates m_activatingUpdates;
	EntityId       m_graphEntityId[MAX_GRAPH_ENTITIES];

	// reference count
	int m_nRefs;

	// nodes -> id resolution
	std::map<string, TFlowNodeId>       m_nodeNameToId;
	// hooks
	std::vector<IFlowGraphHookPtr>      m_hooks;
	// user data for editor
	std::map<TFlowNodeId, XmlNodeRef>   m_userData;
	// inspectors
	std::vector<IFlowGraphInspectorPtr> m_inspectors;

	IFlowGraphDebuggerPtr               m_pFlowGraphDebugger;

	IEntitySystem*                      m_pEntitySystem;

	// temporary solutions [ ask Dejan ]
	bool m_bSuspended;
	bool m_bIsAIAction; // flag that this FlowGraph is an AIAction
	//                     first and only time set in SetAIAction call with an action != 0
	//                     it is never reset. needed when activations are pending which is o.k. for Actiongraphs
	IAIAction* m_pAIAction;

	bool       m_bIsCustomAction; // flag that this FlowGraph is an AIAction
	//                     first and only time set in SetAIAction call with an action != 0
	//                     it is never reset. needed when activations are pending which is o.k. for Actiongraphs
	ICustomAction*             m_pCustomAction;

	IFlowGraphPtr              m_pClonedFlowGraph;

	bool                       m_bRegistered;

	TFlowGraphId               m_graphId;

	IFlowGraph::EFlowGraphType m_Type;
#if !defined(RELEASE)
	string                     m_debugName; // name used for more useful warnings, debugging and profiling
#endif
	typedef std::vector<IFlowGraph::SGraphToken> TGraphTokens;
	TGraphTokens               m_graphTokens; //! definition (name and type) of the game tokens local to this graph


#if defined(ALLOW_MULTIPLE_PORT_ACTIVATIONS_PER_UPDATE)
	struct SCachedActivation
	{
		SCachedActivation()
			: portID(InvalidFlowPortId)
		{}

		template<class T>
		SCachedActivation(const TFlowPortId& inputPortID, const T& inputValue)
			: portID(inputPortID)
			, value(inputValue)
		{}

		TFlowPortId    portID;
		TFlowInputData value;
	};

	typedef std::vector<SCachedActivation>                TCachedPortActivations;
	typedef std::map<TFlowNodeId, TCachedPortActivations> TCachedActivations;
	TCachedActivations m_cachedActivations;
#endif
};

template<class T>
inline string CFlowGraphBase::ToString(const T& value)
{
	return CryStringUtils::toString(value);
}

template<>
inline string CFlowGraphBase::ToString(const string& value)
{
	return value;
}

template<>
inline string CFlowGraphBase::ToString(const SFlowSystemVoid& value)
{
	return "unknown";
}

template<>
inline string CFlowGraphBase::ToString(const TFlowInputData& value)
{
	switch (value.GetType())
	{
	case eFDT_EntityId:
		return ToString(*value.GetPtr<EntityId>());
	case eFDT_String:
		return ToString(*value.GetPtr<string>());
	case eFDT_Float:
		return ToString(*value.GetPtr<float>());
	case eFDT_Vec3:
		return ToString(*value.GetPtr<Vec3>());
	case eFDT_Bool:
		return ToString(*value.GetPtr<bool>());
	case eFDT_Int:
		return ToString(*value.GetPtr<int>());
	default:
		return "unknown";
	}
}

template<class T>
bool CFlowGraphBase::NotifyFlowNodeActivationListeners(TFlowNodeId srcNode, TFlowPortId srcPort, TFlowNodeId toNode, TFlowPortId toPort, const T& value)
{
	const string val = ToString(value);
	return NotifyFlowNodeActivationListeners(srcNode, srcPort, toNode, toPort, val.c_str());
}

// this function is only provided to assist implementation of ActivatePort()
// force it inline as it is only instantiated once per template type used
template<class T>
ILINE void CFlowGraphBase::PerformActivation(const SFlowAddress addr, const T& value)
{
	// CRY_PROFILE_FUNCTION_ARG(PROFILE_ACTION, m_debugName); // enable only when needed, it increases FG update total time
	CRY_ASSERT(ValidateAddress(addr));

	if (m_bActive == false || m_bEnabled == false)
		return;

	static ICVar* pToggleDebugger = nullptr;
	if (!pToggleDebugger)
		pToggleDebugger = gEnv->pConsole->GetCVar("fg_iEnableFlowgraphNodeDebugging");

	if (addr.isOutput)
	{
		EnsureSortedEdges();

		TFlowInputData valueData;
		valueData.SetUserFlag(true);
		valueData.SetValueWithConversion(value);

		int edgeIndex = 0;
		const bool bFlowGraphDebuggerEnabled = (pToggleDebugger && pToggleDebugger->GetIVal() > 0);
		const bool notify = gEnv->IsEditor() && m_pFlowGraphDebugger && bFlowGraphDebuggerEnabled;
		const int firstEdgeIndex = m_flowData[addr.node].GetOutputFirstEdge(addr.port);

		// check activations even on unconnected outputs for trace and breakpoints
		if (notify && !IsOutputConnected(addr))
		{
			if (!m_pFlowGraphDebugger->PerformActivation(this, addr, valueData))
				return; // for breakpoint hits
		}

#if defined(ALLOW_MULTIPLE_PORT_ACTIVATIONS_PER_UPDATE)
		std::vector<TFlowNodeId> tempNodeIDs;
#endif
		std::vector<SEdge>::const_iterator iter = m_edges.begin() + firstEdgeIndex;
		std::vector<SEdge>::const_iterator iterEnd = m_edges.end();
		while (iter != iterEnd && iter->fromNode == addr.node && iter->fromPort == addr.port)
		{
			if (notify)
			{
				SFlowAddress fromAddr;
				fromAddr.node = iter->fromNode;
				fromAddr.port = iter->fromPort;
				fromAddr.isOutput = true;

				SFlowAddress toAddr;
				toAddr.node = iter->toNode;
				toAddr.port = iter->toPort;
				toAddr.isOutput = false;

				if (!m_pFlowGraphDebugger->PerformActivation(this, edgeIndex, fromAddr, toAddr, valueData))
					return; // for breakpoint hits
			}
#if defined(ALLOW_MULTIPLE_PORT_ACTIVATIONS_PER_UPDATE)
			if (!m_bNeedsInitialize && m_flowData[iter->toNode].GetInputPort(iter->toPort)->IsUserFlagSet())
			{
				tempNodeIDs.push_back(iter->toNode);
				// cache multiple port activations per update
				TCachedActivations::iterator cachedIter = m_cachedActivations.find(iter->toNode);

				if (cachedIter != m_cachedActivations.end())
				{
					SCachedActivation cachedActivation(iter->toPort, value);

					// Check if this port is already in the cache
					TCachedPortActivations::iterator dupCheckIter = cachedIter->second.begin();
					TCachedPortActivations::iterator dupCheckIterEnd = cachedIter->second.end();
					for (; dupCheckIter != dupCheckIterEnd; ++dupCheckIter)
					{
						if (iter->toPort == dupCheckIter->portID)
						{
							break;
						}
					}
					// If it is, we're probably in a loop
					if (dupCheckIter == dupCheckIterEnd)
					{
						cachedIter->second.push_back(cachedActivation);
					}
				}
				else
				{
					SCachedActivation cachedActivation(iter->toPort, value);

					TCachedPortActivations activation;
					activation.push_back(cachedActivation);
					m_cachedActivations.insert(std::make_pair(iter->toNode, activation));
				}
			}
			else
#endif
			{
				m_flowData[iter->toNode].ActivateInputPort(iter->toPort, value);
				// see if we need to insert this node into the modified list
				ActivateNodeInt(iter->toNode);
			}

			if (notify)
				NotifyFlowNodeActivationListeners(iter->fromNode, iter->fromPort, iter->toNode, iter->toPort, value);

			if (m_pSys && m_pSys->IsInspectingEnabled())
				m_pSys->NotifyFlow(this, addr, SFlowAddress(iter->toNode, iter->toPort, false));

			++iter;
			++edgeIndex;
		}
#if defined(ALLOW_MULTIPLE_PORT_ACTIVATIONS_PER_UPDATE)
		if (!m_cachedActivations.empty())
		{
			// add a separator for each cached activation so we can send the activate event after the port values were set
			std::vector<TFlowNodeId>::const_iterator nodeIter = tempNodeIDs.begin();
			for (; nodeIter != tempNodeIDs.end(); ++nodeIter)
			{
				TCachedActivations::iterator cachedIter = m_cachedActivations.find(*nodeIter);

				if (cachedIter != m_cachedActivations.end())
				{
					SCachedActivation invalidCachedActivation;
					// If last cache entry is already a separator, don't add another one
					if (cachedIter->second.rbegin()->portID != invalidCachedActivation.portID)
						cachedIter->second.push_back(invalidCachedActivation);
				}
			}
		}
#endif
	}
	else
	{
		if (m_flowData[addr.node].GetImpl())
		{
			m_flowData[addr.node].ActivateInputPort(addr.port, value);
			ActivateNodeInt(addr.node);
		}
	}
}

class CFlowGraph : public CFlowGraphBase
{
public:
	CFlowGraph(CFlowSystem* pSys) : CFlowGraphBase(pSys) {}
	virtual void GetMemoryUsage(ICrySizer* s) const;

	virtual void DoActivatePort(const SFlowAddress address, const NFlowSystemUtils::Wrapper<SFlowSystemVoid>& value)
	{
		CFlowGraphBase::PerformActivation(address, value.value);
	}
	virtual void DoActivatePort(const SFlowAddress address, const NFlowSystemUtils::Wrapper<int>& value)
	{
		CFlowGraphBase::PerformActivation(address, value.value);
	}
	virtual void DoActivatePort(const SFlowAddress address, const NFlowSystemUtils::Wrapper<float>& value)
	{
		CFlowGraphBase::PerformActivation(address, value.value);
	}
	virtual void DoActivatePort(const SFlowAddress address, const NFlowSystemUtils::Wrapper<EntityId>& value)
	{
		CFlowGraphBase::PerformActivation(address, value.value);
	}
	virtual void DoActivatePort(const SFlowAddress address, const NFlowSystemUtils::Wrapper<Vec3>& value)
	{
		CFlowGraphBase::PerformActivation(address, value.value);
	}
	virtual void DoActivatePort(const SFlowAddress address, const NFlowSystemUtils::Wrapper<string>& value)
	{
		CFlowGraphBase::PerformActivation(address, value.value);
	}
	virtual void DoActivatePort(const SFlowAddress address, const NFlowSystemUtils::Wrapper<bool>& value)
	{
		CFlowGraphBase::PerformActivation(address, value.value);
	}
};
