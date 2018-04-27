// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*
 * A Module is a container and interface for a graph.
 * The module has a set of instances that can run independently.
 * Modules have an external 'call' node and internally a 'start' and an 'end'. - see FlowModuleNodes.h
 * Modules are managed by an editor manager Editor:FlowGraphModuleManager.h and a runtime manager - CryAction:ModuleManager.h
 */

#pragma once

#include <CryFlowGraph/IFlowGraphModuleManager.h>
#include <CryCore/Containers/VectorMap.h>

class CFlowModuleCallNode;
class CFlowModuleStartNode;
class CFlowModuleEndNode;

// notes on Instance IDs:
// - a Module runs a set of instances with unique IDs that are associated with an entity (or with invalid_entityid)
// - if a call node has a manually specified instance ID different from '-1', it will be used
// - the first of the same specific IID to be called will create the instance which in turn will output for all all call nodes with that IID.
// - IIDs specified at runtime may refer to the "offline" manually specified IIDs.
// - an IID generated at runtime will not collide with one manually specified (the ID generator starts from the biggest manual ID found)
// - an IID '-1' generates new instances on every call if there is no entity associated
// - the call node will hold the outputs of the last generated instance until it is called again
// - if there is an entity id specified, the IID will refer within that entity, meaning entities can have their own separate instance 1,2,3.. of the same module
// - an IID -1 with a specified entity ID will not generate new IID. '-1' is the instance ID.

struct SModuleInstance : public IModuleInstance
{
	SModuleInstance(IFlowGraphModule* pModule, TModuleInstanceId id)
		: IModuleInstance(pModule, id)
		, m_entityId(INVALID_ENTITYID)
		, m_pStartNode(nullptr)
	{}

	EntityId m_entityId;

	CFlowModuleStartNode* GetStartNode() const                           { return m_pStartNode; }
	void                  SetStartNode(CFlowModuleStartNode* pStartNode) { m_pStartNode = pStartNode; }

protected:
	CFlowModuleStartNode* m_pStartNode;
};

class CFlowGraphModule : public IFlowGraphModule
{
	friend class CFlowGraphModuleManager;

public:

	CFlowGraphModule(TModuleId moduleId);
	virtual ~CFlowGraphModule();

	// IFlowGraphModule
	virtual const char*                                GetName() const                       { return m_name.c_str(); }
	virtual const char*                                GetPath() const                       { return m_fileName.c_str(); }
	virtual TModuleId                                  GetId() const                         { return m_Id; }

	virtual IFlowGraphModule::EType                    GetType() const                       { return m_type; }
	virtual void                                       SetType(IFlowGraphModule::EType type) { m_type = type; }

	virtual IFlowGraph*                                GetRootGraph() const                  { return m_pRootGraph; }
	bool                                               HasInstanceGraph(IFlowGraphPtr pGraph);

	virtual size_t                                     GetModuleInputPortCount() const;
	virtual size_t                                     GetModuleOutputPortCount() const;
	virtual const IFlowGraphModule::SModulePortConfig* GetModuleInputPort(size_t index) const;
	virtual const IFlowGraphModule::SModulePortConfig* GetModuleOutputPort(size_t index) const;
	virtual bool                                       AddModulePort(const SModulePortConfig& port);
	virtual void                                       RemoveModulePorts();

	virtual IModuleInstanceIteratorPtr                 CreateInstanceIterator();
	virtual size_t                                     GetRunningInstancesCount() const;

	//! Create and call a new Instance associated with and Entity.
	virtual void                                       CallDefaultInstanceForEntity(IEntity* pEntity);
	// ~IFlowGraphModule

	inline SModuleInstance* GetInstance(EntityId entityId, TModuleInstanceId instanceId);
	bool                    HasRunningInstances() const;

	// Call Nodes for Instances Registry
	void RegisterCallNodeForInstance(EntityId entityId, TModuleInstanceId instanceId, uint callNodeId, CFlowModuleCallNode* pCallNode);
	void UnregisterCallNodeForInstance(EntityId entityId, TModuleInstanceId instanceId, uint callNodeId);
	void ChangeRegisterCallNodeForInstance(
	  EntityId oldEntityId, TModuleInstanceId oldInstanceId,
	  EntityId newEntityId, TModuleInstanceId newInstanceId,
	  uint callNodeId, CFlowModuleCallNode* pCallNode);
	void ClearCallNodesForInstances();

	// Global Control Nodes Registry
	void RegisterGlobalControlNode(uint callNodeId, CFlowModuleCallNode* pCallNode);
	void UnregisterGlobalControlNode(uint callNodeId);
	void ClearGlobalControlNodes();

	//! Create a new Instance, register it in the running instances and pass event to call nodes, global controllers and listeners
	void CreateInstance(EntityId entityId, TModuleInstanceId runningInstanceId);

	//! Call/Update an Instance with inputs. InstanceId may be invalid in which case a new instance will be generated
	TModuleInstanceId CallModuleInstance(EntityId entityId, TModuleInstanceId instanceId, const TModuleParams& params, CFlowModuleCallNode* pCallingNode);

	//! Call/Update all Instances with inputs
	void CallAllModuleInstances(const TModuleParams& params, CFlowModuleCallNode* pCallingNode);

	//! Activate the cancel port in the start node of the instance
	void CancelInstance(EntityId entityId, TModuleInstanceId instanceId);

	//! Activate the cancel port in the start node of all the running instances of this module
	void CancelAllInstances();

	//! Send one input port to the instance's Start node
	void UpdateInstanceInputPort(EntityId entityId, TModuleInstanceId instanceId, size_t paramIdx, const TFlowInputData& value);

	//! Send one input port to the Start node of all running instances of this module
	void UpdateAllInstancesInputPort(size_t paramIdx, const TFlowInputData& value);

	//! Send one output port of the instance to all its Call nodes
	void UpdateInstanceOutputPort(EntityId entityId, TModuleInstanceId instanceId, size_t paramIdx, const TFlowInputData& value);

	//! Update call nodes with final outputs and destroy instance
	void OnInstanceFinished(EntityId entityId, TModuleInstanceId instanceId, bool bSuccess, const TModuleParams& params);

	// For setting up connections between the instance and it's nodes at creation time
	TModuleInstanceId GetInstanceIdOfInstanceBeingCreated() const { return m_pInstanceBeingCreated->m_instanceId; }
	EntityId          GetEntityOfInstanceBeingCreated() const     { return m_pInstanceBeingCreated->m_entityId; }
	void              RegisterStartNodeForInstanceBeingCreated(CFlowModuleStartNode* pStartNode);

private:

	bool PreLoadModule(const char* fileName);                           //! load initial data, create nodes
	bool LoadModuleGraph(const char* moduleName, const char* fileName); //! load actual flowgraph
	bool SaveModuleXml(XmlNodeRef saveTo);

	void Destroy();

	// create flowgraph nodes (Start, Return, Call) for this module
	void RegisterNodes();
	void UnregisterNodes();

	// Instance handling
	void DeactivateInstanceGraph(SModuleInstance* pInstance) const;
	bool DestroyInstance(EntityId entityId, TModuleInstanceId instanceId);
	void RemoveCompletedInstances();
	void RemoveAllInstances();

	// attributes
	TModuleId               m_Id;
	IFlowGraphModule::EType m_type;
	string                  m_name;
	string                  m_fileName;
	IFlowGraphPtr           m_pRootGraph; //! Root graph all others are cloned from

	// Inputs and outputs for this module
	typedef std::vector<SModulePortConfig> TModulePorts;
	TModulePorts m_moduleInpPorts;
	TModulePorts m_moduleOutPorts;

	// Instance Registries
	typedef std::map<TModuleInstanceId, SModuleInstance> TInstanceMap;
	typedef std::map<EntityId, TInstanceMap>             TEntityInstanceMap;
	TEntityInstanceMap m_runningInstances; //! Instances of this module currently running

	typedef VectorMap<uint, CFlowModuleCallNode*>       TCallNodesMap;
	typedef VectorMap<TModuleInstanceId, TCallNodesMap> TInstanceCallNodesMap;
	typedef VectorMap<EntityId, TInstanceCallNodesMap>  TEntityInstancesToCallNodes;
	TEntityInstancesToCallNodes m_requestedInstanceIds;  //! map for Instance IDs that are manually requested in the Call nodes at game start
	TModuleInstanceId           m_nextInstanceId;        //! ID for the next generated instance. Starts after the last manually requested ID.

	SModuleInstance*            m_pInstanceBeingCreated; //! for Start and Return nodes to read when they are being cloned.

	// Global control nodes registry
	TCallNodesMap m_globalControlNodes; //! Instances of this module currently running

	// flow node registration
	TFlowNodeTypeId m_startNodeTypeId;
	TFlowNodeTypeId m_returnNodeTypeId;
	TFlowNodeTypeId m_callNodeTypeId;

	// Instance Iterator
	class CInstanceIterator : public IFlowGraphModuleInstanceIterator
	{
	public:
		CInstanceIterator(CFlowGraphModule* pFM)
		{
			m_pModule = pFM;
			m_curEnt = m_pModule->m_runningInstances.begin();
			if (m_curEnt != m_pModule->m_runningInstances.end())
			{
				m_curInst = m_curEnt->second.begin();
			}
			m_nRefs = 0;
		}

		void AddRef()
		{
			++m_nRefs;
		}

		void Release()
		{
			if (--m_nRefs <= 0)
			{
				this->~CInstanceIterator();
				m_pModule->m_iteratorPool.push_back(this);
			}
		}

		IModuleInstance* Next()
		{
			if (m_curEnt == m_pModule->m_runningInstances.end())
			{
				return nullptr;
			}

			IModuleInstance* pCurInst = &(m_curInst->second);

			//advance
			++m_curInst;
			if (m_curInst == m_curEnt->second.end())
			{
				++m_curEnt;
				if (m_curEnt != m_pModule->m_runningInstances.end())
				{
					m_curInst = m_curEnt->second.begin();
				}
			}

			return pCurInst;
		}

		size_t Count()
		{
			return m_pModule->GetRunningInstancesCount();
		}

		CFlowGraphModule*            m_pModule;
		TInstanceMap::iterator       m_curInst;
		TEntityInstanceMap::iterator m_curEnt;
		int                          m_nRefs;
	};

	std::vector<CInstanceIterator*> m_iteratorPool;

};

TYPEDEF_AUTOPTR(CFlowGraphModule);
typedef CFlowGraphModule_AutoPtr CFlowGraphModulePtr;
