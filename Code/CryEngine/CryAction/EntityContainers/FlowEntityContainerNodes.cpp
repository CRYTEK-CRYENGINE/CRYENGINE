// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// Description: Nodes related to Entity Containers
// - 10/02/2016 Created by Dario Sancho

#include "StdAfx.h"

#include "EntityContainers/EntityContainerMgr.h"
#include "EntityContainers/IEntityContainerListener.h"
#include <CryFlowGraph/IFlowBaseNode.h>
#include "CryAction.h"



class CFlowNode_EntityContainerContMgr_AddRemove : public CFlowBaseNode<eNCT_Singleton>
{
	enum EINPUTS
	{
		eI_InputEntityId = 0,
		eI_AddEntity,
		eI_RemoveEntity,
		eI_SrcContainerId,
		eI_AddContainerOfEntities,
		eI_RemoveContainerOfEntities,
	};

	enum EOUTPUTS
	{
		eO_ContainerSize,
	};

public:
	CFlowNode_EntityContainerContMgr_AddRemove(SActivationInfo* pActInfo) { }
	~CFlowNode_EntityContainerContMgr_AddRemove() { }

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] =
		{
			InputPortConfig<EntityId>("InputEntityId",           _HELP("Id of the Entity to Add/Remove")),
			InputPortConfig_AnyType("AddEntity",                 _HELP("Adds the selected entity to the selected container")),
			InputPortConfig_AnyType("RemoveEntity",              _HELP("Removes the selected entity to the selected container")),
			InputPortConfig<EntityId>("SrcContainerId",          _HELP("Id of the container containing the list of entitites to be added/removed from the selected container")),
			InputPortConfig_AnyType("AddContainerOfEntities",    _HELP("Adds all the entities in SrcContainerId container into the selected container")),
			InputPortConfig_AnyType("RemoveContainerOfEntities", _HELP("Removes all the entities in SrcContainerId container from the selected container")),
			{ 0 }
		};
		static const SOutputPortConfig outputs[] =
		{
			OutputPortConfig<int>("ContainerSize", _HELP("Container size after the operation completed")),
			{ 0 }
		};

		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("FlowNode to add/remove entities to an entity container");
		config.SetCategory(EFLN_ADVANCED);
	}

	void Serialize(SActivationInfo*, TSerialize ser) { }

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			break;
		case eFE_Activate:
			{
				IF_UNLIKELY (pActInfo->pEntity == NULL)
					return;

				if (CCryAction* pCryAction = CCryAction::GetCryAction())
				{
					CEntityContainerMgr& containerManager = CCryAction::GetCryAction()->GetEntityContainerMgr();
					EntityId containerId = pActInfo->pEntity->GetId();

					if (IsPortActive(pActInfo, eI_AddEntity))
					{
						size_t groupSize = containerManager.AddEntity(containerId, GetPortEntityId(pActInfo, eI_InputEntityId));
						ActivateOutput(pActInfo, eO_ContainerSize, (int)groupSize);
					}
					if (IsPortActive(pActInfo, eI_RemoveEntity))
					{
						size_t groupSize = containerManager.RemoveEntity(containerId, GetPortEntityId(pActInfo, eI_InputEntityId));
						ActivateOutput(pActInfo, eO_ContainerSize, (int)groupSize);
					}
					if (IsPortActive(pActInfo, eI_AddContainerOfEntities))
					{
						size_t groupSize = containerManager.AddContainerOfEntitiesToContainer(containerId, GetPortEntityId(pActInfo, eI_SrcContainerId));
						ActivateOutput(pActInfo, eO_ContainerSize, (int)groupSize);
					}
					if (IsPortActive(pActInfo, eI_RemoveContainerOfEntities))
					{
						size_t groupSize = containerManager.RemoveContainerOfEntitiesFromContainer(containerId, GetPortEntityId(pActInfo, eI_SrcContainerId));
						ActivateOutput(pActInfo, eO_ContainerSize, (int)groupSize);
					}
				}
			}
			break;
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

};


class CFlowNode_EntityContainerContMgr_Merge : public CFlowBaseNode<eNCT_Singleton>
{
	enum EINPUTS
	{
		eI_SrcContainerId,
		eI_Merge,
		eI_Copy,
	};

	enum EOUTPUTS
	{
		eO_ContainerSize,
	};

public:
	CFlowNode_EntityContainerContMgr_Merge(SActivationInfo* pActInfo) { }
	~CFlowNode_EntityContainerContMgr_Merge() { }

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] =
		{
			InputPortConfig<EntityId>("SrcContainerId", _HELP("Id of the Source Container")),
			InputPortConfig_AnyType("Merge",        _HELP("Moves all source container's elements to the destination container. Src container will be empty after the process is done")),
			InputPortConfig_AnyType("Copy",         _HELP("Copies all source container's elements to the destination container")),
			{ 0 }
		};
		static const SOutputPortConfig outputs[] =
		{
			OutputPortConfig<int>("GroupSize", _HELP("Size of the destination container after the merge is done")),
			{ 0 }
		};

		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("FlowNode to move the entities of a source container into this node's container");
		config.SetCategory(EFLN_ADVANCED);
	}

	void Serialize(SActivationInfo*, TSerialize ser) { }

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			break;
		case eFE_Activate:
			{
				CEntityContainerMgr& containerManager = CCryAction::GetCryAction()->GetEntityContainerMgr();
				if (IsPortActive(pActInfo, eI_Copy) && pActInfo->pEntity)
				{
					EntityId dstContainerId = pActInfo->pEntity->GetId();
					size_t dstSize = containerManager.MergeContainers(GetPortEntityId(pActInfo, eI_SrcContainerId), dstContainerId, false);
					ActivateOutput(pActInfo, eO_ContainerSize, (int)dstSize);
				}
				else if (IsPortActive(pActInfo, eI_Merge) && pActInfo->pEntity)
				{
					EntityId dstContainerId = pActInfo->pEntity->GetId();
					size_t dstSize = containerManager.MergeContainers(GetPortEntityId(pActInfo, eI_SrcContainerId), dstContainerId, true);
					ActivateOutput(pActInfo, eO_ContainerSize, (int)dstSize);
				}
			}
			break;
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};


class CFlowNode_EntityContainerContMgr_Clear : public CFlowBaseNode<eNCT_Singleton>
{
	enum EInputs
	{
		eI_ClearContainer,
	};

	enum EOutputs
	{
		eO_Done,
	};

public:
	CFlowNode_EntityContainerContMgr_Clear(SActivationInfo* pActInfo) { }
	~CFlowNode_EntityContainerContMgr_Clear() { }

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] =
		{
			InputPortConfig_AnyType("ClearContainer", _HELP("Removes all entities from the selected container")),
			{ 0 }
		};

		static const SOutputPortConfig outputs[] =
		{
			OutputPortConfig_AnyType("Done", _HELP("The container has been cleared")),
			{ 0 }
		};

		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("FlowNode to remove all entities from an entity container");
		config.SetCategory(EFLN_ADVANCED);
	}

	void Serialize(SActivationInfo*, TSerialize ser) { }

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			break;
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, eI_ClearContainer) && pActInfo->pEntity)
				{
					EntityId containerId = pActInfo->pEntity->GetId();
					CEntityContainerMgr& containerManager = CCryAction::GetCryAction()->GetEntityContainerMgr();
					containerManager.ClearContainer(containerId);
					ActivateOutput(pActInfo, eO_Done, true);
				}
			}
			break;
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

};


class CFlowNode_EntityContainerContMgr_Actions : public CFlowBaseNode<eNCT_Instanced>
{
	enum EInputs
	{
		eI_HideEntities,
		eI_NumUnhideEntitiesPerFrame,
		eI_UnhideEntities,
		eI_RemoveEntitiesFromGame,
		eI_RunModule,
		eI_ModuleName,
	};

	enum EOutputs
	{
		eO_Hidden,
		eO_Unhidden,
		eO_Removed,
		eO_ModuleRun
	};

public:
	CFlowNode_EntityContainerContMgr_Actions(SActivationInfo* pActInfo) { }
	~CFlowNode_EntityContainerContMgr_Actions() { }

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] =
		{
			InputPortConfig_AnyType("HideEntities",           _HELP("Hides all entities in the container"), _HELP("Hide Entities")),
			InputPortConfig<int>(   "NumEntitiesFrame", 1,    _HELP("Number of entities to unhide per frame. Set to -1 to unhide all at once, otherwise tweak according to the cost of the entities in the container for performance resaons."), _HELP("Num Unhide Entities Per Frame")),
			InputPortConfig_AnyType("UnhideEntities",         _HELP("Unhides all entities in the container"), _HELP("Unhide Entities")),
			InputPortConfig_AnyType("RemoveEntitiesFromGame", _HELP("Removes all entities in the container from the game"), _HELP("Remove Entities From Game")),
			InputPortConfig_AnyType("RunModule",              _HELP("Run the selected Module on the entities in the container"), _HELP("Run Module")),
			InputPortConfig<string>("ModuleName",             _HELP("Module to be run on the entities"), _HELP("Module Name"), _UICONFIG("dt=fgmodules")),
			{ 0 }
		};

		static const SOutputPortConfig outputs[] =
		{
			OutputPortConfig_Void("Hidden",    _HELP("Hide done")),
			OutputPortConfig_Void("Unhidden",  _HELP("Unhide done")),
			OutputPortConfig_Void("Removed",   _HELP("Remove done")),
			OutputPortConfig_Void("ModuleRun", _HELP("Module Started for each entity in the container"), _HELP("Module Run")),
			{ 0 }
		};

		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("FlowNode to perform actions on all the entities in the container");
		config.SetCategory(EFLN_ADVANCED);
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNode_EntityContainerContMgr_Actions(pActInfo);
	}

	void Serialize(SActivationInfo*, TSerialize ser) { }

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			{
				m_entitiesToUnhide.clear();
			}
			break;
		case eFE_Activate:
			{
				if (!pActInfo->pEntity)
					return;

				EntityId containerId = pActInfo->pEntity->GetId();
				CEntityContainerMgr& containerManager = CCryAction::GetCryAction()->GetEntityContainerMgr();
				if (IsPortActive(pActInfo, eI_HideEntities))
				{
					containerManager.SetHideEntities(containerId, true);
					ActivateOutput(pActInfo, eO_Hidden, true);
				}
				if (IsPortActive(pActInfo, eI_UnhideEntities))
				{
					m_numEntitiesToUnhidePerFrame = GetPortInt(pActInfo, eI_NumUnhideEntitiesPerFrame);
					containerManager.GetEntitiesInContainer(containerId, m_entitiesToUnhide);
					if (m_numEntitiesToUnhidePerFrame <= 0)
					{
						// un-hide all entities immediately
						containerManager.SetHideEntities(containerId, false);
						ActivateOutput(pActInfo, eO_Unhidden, true);
					}
					else
					{
						// Un-hide a group in a time-sliced fashion
						containerManager.GetEntitiesInContainer(containerId, m_entitiesToUnhide);
						pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
					}
				}
				if (IsPortActive(pActInfo, eI_RemoveEntitiesFromGame))
				{
					containerManager.RemoveEntitiesFromTheGame(containerId);
					ActivateOutput(pActInfo, eO_Removed, true);
				}
				if (IsPortActive(pActInfo, eI_RunModule))
				{
					const string& moduleName = GetPortString(pActInfo, eI_ModuleName);
					bool bSuccess = containerManager.RunModule(containerId, moduleName.c_str());
					ActivateOutput(pActInfo, eO_ModuleRun, bSuccess);
				}
			}
			break;
		case eFE_Update:
			{
				if (m_entitiesToUnhide.empty() == false)
				{
					// hide entities
					int lowIndex = m_numEntitiesToUnhidePerFrame >= m_entitiesToUnhide.size() ? 0 : m_entitiesToUnhide.size() - m_numEntitiesToUnhidePerFrame;
					for (int i = m_entitiesToUnhide.size() - 1; i >= lowIndex; --i)
					{
						if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_entitiesToUnhide[i].id))
						{
							pEntity->Hide(false);
						}
					}
					if (lowIndex == 0)
					{
						m_entitiesToUnhide.clear();
					}
					else
					{
						m_entitiesToUnhide.resize(lowIndex);
					}
				}

				if (m_entitiesToUnhide.empty())
				{
					// as soon as there are no entities to un-hide we stop the update and output result
					if (pActInfo && pActInfo->pGraph)
					{
						ActivateOutput(pActInfo, eO_Unhidden, true);
						pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
					}
				}
			}
			break;
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

private:
	CEntityContainer::TEntitiesInContainer m_entitiesToUnhide;
	int m_numEntitiesToUnhidePerFrame;
};


class CFlowNode_EntityContainerContMgr_QueryContainerSize : public CFlowBaseNode<eNCT_Singleton>
{

	enum EInputs
	{
		eI_DoQuery,
	};

	enum EOutputs
	{
		eO_ContainerSize,
	};

public:
	CFlowNode_EntityContainerContMgr_QueryContainerSize(SActivationInfo* pActInfo) { }
	~CFlowNode_EntityContainerContMgr_QueryContainerSize() { }

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] =
		{
			InputPortConfig_AnyType("DoQuery", _HELP("Request the container's size")),
			{ 0 }
		};

		static const SOutputPortConfig outputs[] =
		{
			OutputPortConfig<int>("ContainerSize", _HELP("Container size")),
			{ 0 }
		};

		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("FlowNode to query the size of an entity container");
		config.SetCategory(EFLN_ADVANCED);
	}

	void Serialize(SActivationInfo*, TSerialize ser) { }

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			break;
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, eI_DoQuery) && pActInfo->pEntity)
				{
					EntityId containerId = pActInfo->pEntity->GetId();
					CEntityContainerMgr& containerManager = CCryAction::GetCryAction()->GetEntityContainerMgr();
					size_t size = containerManager.GetContainerSize(containerId);
					ActivateOutput(pActInfo, eO_ContainerSize, (int)size);
				}
			}
			break;
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};


class CFlowNode_EntityContainerContMgr_QueryRandomEntity : public CFlowBaseNode<eNCT_Singleton>
{

	enum EInputs
	{
		eI_DoQuery,
	};

	enum EOutputs
	{
		eO_RandomEntityId,
	};

public:
	CFlowNode_EntityContainerContMgr_QueryRandomEntity(SActivationInfo* pActInfo) { }
	~CFlowNode_EntityContainerContMgr_QueryRandomEntity() { }

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] =
		{
			InputPortConfig_AnyType("DoQuery", _HELP("Activates the Query")),
			{ 0 }
		};

		static const SOutputPortConfig outputs[] =
		{
			OutputPortConfig<uint32>("RandomEntityId", _HELP("A random element from the container")),
			{ 0 }
		};

		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("FlowNode to retrieve a random element from the container");
		config.SetCategory(EFLN_ADVANCED);
	}

	void Serialize(SActivationInfo*, TSerialize ser) { }

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			break;
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, eI_DoQuery) && pActInfo->pEntity)
				{
					EntityId containerId = pActInfo->pEntity->GetId();
					CEntityContainerMgr& containerManager = CCryAction::GetCryAction()->GetEntityContainerMgr();
					EntityId entityId = containerManager.GetRandomEntity(containerId);
					ActivateOutput(pActInfo, eO_RandomEntityId, (int)entityId);
				}
			}
			break;
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};


class CFlowNode_EntityContainerContMgr_QueryContainerId : public CFlowBaseNode<eNCT_Singleton>
{
	enum EInputs
	{
		eI_DoQuery,
	};

	enum EOutputs
	{
		eO_ContainerCount,
		eO_FIRST_CONTAINERID,
		eO_ContainerId1 = eO_FIRST_CONTAINERID,
		eO_ContainerId2,
		eO_ContainerId3,
		eO_ContainerId4,
		eO_LAST_CONTAINERID = eO_ContainerId4
	};

public:
	CFlowNode_EntityContainerContMgr_QueryContainerId(SActivationInfo* pActInfo) {}
	~CFlowNode_EntityContainerContMgr_QueryContainerId() {}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] =
		{
			InputPortConfig_AnyType("DoQuery", _HELP("Request the ID of all the containers that the entity belongs to")),
			{ 0 }
		};

		static const SOutputPortConfig outputs[] =
		{
			OutputPortConfig<int>("ContainerCount", _HELP("Number of containers that the entity belongs to")),
			OutputPortConfig<int>("ContainerId1",   _HELP("First Container Id that the entity belongs to")),
			OutputPortConfig<int>("ContainerId2",   _HELP("Second Container Id that the entity belongs to")),
			OutputPortConfig<int>("ContainerId3",   _HELP("Third Container Id that the entity belongs to")),
			OutputPortConfig<int>("ContainerId4",   _HELP("Fourth Container Id that the entity belongs to")),
			{ 0 }
		};

		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("FlowNode to query which containers the given Entity Id belongs to");
		config.SetCategory(EFLN_ADVANCED);
	}

	void Serialize(SActivationInfo*, TSerialize ser) {}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			break;
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, eI_DoQuery) && pActInfo->pEntity)
				{
					EntityId entityId = pActInfo->pEntity->GetId();
					CEntityContainerMgr& containerManager = CCryAction::GetCryAction()->GetEntityContainerMgr();
					CEntityContainer::TEntitiesInContainer groupIdArray;

					containerManager.GetEntitysContainerIds(entityId, groupIdArray);

					const size_t numOutputs = eO_LAST_CONTAINERID - eO_FIRST_CONTAINERID + 1;
					const size_t numElements = groupIdArray.size();
					for (size_t i = 0; i < numOutputs; ++i)
					{
						ActivateOutput(pActInfo, eO_FIRST_CONTAINERID + i, i < numElements ? groupIdArray[i].id : 0);
					}
					ActivateOutput(pActInfo, eO_ContainerCount, (int)numElements);
				}
			}
			break;
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

};


class CFlowNode_EntityContainerContMgr_QueryIsInContainer : public CFlowBaseNode<eNCT_Instanced>, IEntityContainerListener
{
	enum EInputs
	{
		eI_DoQuery,
		eI_EntityId,
		eI_AutomaticCheck
	};

	enum EOutputs
	{
		eO_Result,
		eO_True,
		eO_False,
	};

public:
	CFlowNode_EntityContainerContMgr_QueryIsInContainer(SActivationInfo* pActInfo) {}
	~CFlowNode_EntityContainerContMgr_QueryIsInContainer()
	{
		UnregisterListener();
	}
	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNode_EntityContainerContMgr_QueryIsInContainer(pActInfo);
	}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] =
		{
			InputPortConfig_AnyType("DoQuery", _HELP("Checks if the given Entity ID belongs to the selected container")),
			InputPortConfig<EntityId>("Id", 0, _HELP("Entity ID to check")),
			InputPortConfig<bool>("AutomaticCheck", false, _HELP("If True, the node will automatically fire its outputs if the selected entity is added/removed to the given container")),
			{ 0 }
		};

		static const SOutputPortConfig outputs[] =
		{
			OutputPortConfig<bool>("Result", _HELP("Result of the query as True or False")),
			OutputPortConfig_Void("True",    _HELP("Entity ID is in the selected container")),
			OutputPortConfig_Void("False",   _HELP("Entity ID is NOT in the selected container")),
			{ 0 }
		};

		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("FlowNode to query if an entity is in an entity container");
		config.SetCategory(EFLN_ADVANCED);
	}

	void Serialize(SActivationInfo*, TSerialize ser) {}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
		case eFE_SetEntityId:
			{
				m_actInfo = *pActInfo;
				UpdateListenerState(pActInfo);
			}
			break;
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, eI_DoQuery) && pActInfo->pEntity)
				{
					CEntityContainerMgr& containerManager = CCryAction::GetCryAction()->GetEntityContainerMgr();
					const EntityId containerId = pActInfo->pEntity->GetId();
					const EntityId entityId = GetPortEntityId(pActInfo, eI_EntityId);
					CEntityContainer::TEntitiesInContainer containerIdArray;

					containerManager.GetEntitysContainerIds(entityId, containerIdArray);

					auto findIfLambda = [containerId](const CEntityContainer::TSentityInfoParam& info) { return info.id == containerId; };
					const bool bFound = std::find_if(containerIdArray.begin(), containerIdArray.end(), findIfLambda) != containerIdArray.end();

					ActivateOutput(pActInfo, eO_Result, bFound);
					ActivateOutput(pActInfo, bFound ? eO_True : eO_False, true);
				}

				if (IsPortActive(pActInfo, eI_AutomaticCheck))
				{
					UpdateListenerState(pActInfo);
				}
			}
			break;
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

private:

	virtual void OnGroupChange(IEntityContainerListener::SChangeInfo& info)
	{
		if (m_actInfo.pEntity)
		{
			const EntityId entityId = GetPortEntityId(&m_actInfo, eI_EntityId);
			if (info.entityId == entityId)
			{
				const EntityId containerId = m_actInfo.pEntity->GetId();
				if (info.containerId == containerId)
				{
					switch (info.eventType)
					{
					case IEntityContainerListener::eET_EntityAdded:
						{
							ActivateOutput(&m_actInfo, eO_True, true);
							ActivateOutput(&m_actInfo, eO_Result, true);
						}
						break;
					case IEntityContainerListener::eET_EntityRemoved:
						{
							ActivateOutput(&m_actInfo, eO_False, true);
							ActivateOutput(&m_actInfo, eO_Result, false);
						}
						break;
					}
				}
			}
		}
	}

	void UnregisterListener()
	{
		if (CCryAction::GetCryAction())
		{
			CEntityContainerMgr& containerManager = CCryAction::GetCryAction()->GetEntityContainerMgr();
			containerManager.UnregisterListener(this);
		}
	}

	void RegisterListener(SActivationInfo* pActInfo)
	{
		if (pActInfo && pActInfo->pEntity)
		{
			const EntityId entityId = pActInfo->pEntity->GetId();
			CEntityContainerMgr& containerManager = CCryAction::GetCryAction()->GetEntityContainerMgr();
			stack_string listenerName = stack_string().Format("Groups:QueryIsInContainer(Id:%u)", entityId);
			containerManager.RegisterListener(this, listenerName.c_str(), false);
		}
	}

	void UpdateListenerState(SActivationInfo* pActInfo)
	{
		if (pActInfo && pActInfo->pEntity && GetPortBool(pActInfo, eI_AutomaticCheck))
		{
			RegisterListener(pActInfo);
		}
		else
		{
			UnregisterListener();
		}
	}

	SActivationInfo m_actInfo;
};


class CFlowNode_EntityContainerContMgr_QueryEntityByIndex : public CFlowBaseNode<eNCT_Singleton>
{
	enum EINPUTS
	{
		eI_DoQuery,
		eI_Index,
	};

	enum EOUTPUTS
	{
		eO_MemberID,
		eO_Success,
		eO_Error
	};

public:
	CFlowNode_EntityContainerContMgr_QueryEntityByIndex(SActivationInfo* pActInfo) {}
	~CFlowNode_EntityContainerContMgr_QueryEntityByIndex() = default;

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] =
		{
			InputPortConfig_Void("DoQuery", _HELP("Query the entity id of the n-th member of a container")),
			InputPortConfig<int>("Index", 0, _HELP("Index of the entity to query inside a group [index: from 1 to num entities in the group]")),
			{ 0 }
		};
		static const SOutputPortConfig outputs[] =
		{
			OutputPortConfig<EntityId>("MemberId", _HELP("Entity Id with the selected index in the group")),
			OutputPortConfig_Void("Success",       _HELP("A member with the given index was found")),
			OutputPortConfig_Void("Error",         _HELP("A member with the given index was NOT found")),
			{ 0 }
		};

		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("FlowNode to get the entity id of the n-th member of a group");
		config.SetCategory(EFLN_ADVANCED);
	}

	void Serialize(SActivationInfo*, TSerialize ser) {}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, eI_DoQuery) && pActInfo->pEntity)
				{
					const uint32 index = std::max(0, GetPortInt(pActInfo, eI_Index));

					CEntityContainerMgr& containerManager = CCryAction::GetCryAction()->GetEntityContainerMgr();
					const EntityId containerId = pActInfo->pEntity->GetId();
					CEntityContainer::TEntitiesInContainer containerIdArray;

					containerManager.GetEntitiesInContainer(containerId, containerIdArray);

					if (index > 0 && index <= containerIdArray.size())
					{
						ActivateOutput(pActInfo, eO_MemberID, containerIdArray[index - 1].id);
						ActivateOutput(pActInfo, eO_Success, true);
					}
					else
					{
						ActivateOutput(pActInfo, eO_MemberID, 0);
						ActivateOutput(pActInfo, eO_Error, true);
					}
				}
			}
			break;
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};


class CFlowNode_EntityContainerContMgr_Listener : public CFlowBaseNode<eNCT_Instanced>, public IEntityContainerListener
{

	enum EOutputs
	{
		eO_ContainerSize,
		eO_EventEntityAdded,
		eO_EventEntityRemoved,
		eO_EventContainerEmpty,
	};

public:
	CFlowNode_EntityContainerContMgr_Listener(SActivationInfo* pActInfo) {}
	~CFlowNode_EntityContainerContMgr_Listener()
	{
		UnregisterListener();
	}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SOutputPortConfig outputs[] =
		{
			OutputPortConfig<int>("ContainerSize",       _HELP("Container size after the operation completed")),
			OutputPortConfig_Void("EventEntityAdded",    _HELP("Entity added event received")),
			OutputPortConfig_Void("EventEntityRemoved",  _HELP("Entity removed event received")),
			OutputPortConfig_Void("EventContainerEmpty", _HELP("Empty container event received (due to removing entities). A new container will not fire this event.")),
			{ 0 }
		};

		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("FlowNode that outputs the number of elements on an entity container everytime elements are added or removed to it");
		config.SetCategory(EFLN_ADVANCED);
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNode_EntityContainerContMgr_Listener(pActInfo);
	}

	void Serialize(SActivationInfo*, TSerialize ser) { }

	virtual void OnGroupChange(IEntityContainerListener::SChangeInfo& info)
	{
		if (m_actInfo.pEntity)
		{
			const EntityId myContainerId = m_actInfo.pEntity->GetId();
			if (info.containerId == myContainerId)
			{
				CEntityContainerMgr& containerManager = CCryAction::GetCryAction()->GetEntityContainerMgr();
				size_t size = containerManager.GetContainerSize(info.containerId);
				ActivateOutput(&m_actInfo, eO_ContainerSize, (int)size);
				switch (info.eventType)
				{
				case IEntityContainerListener::eET_EntityAdded:
					ActivateOutput(&m_actInfo, eO_EventEntityAdded, true);
					break;
				case IEntityContainerListener::eET_EntityRemoved:
					ActivateOutput(&m_actInfo, eO_EventEntityRemoved, true);
					break;
				case IEntityContainerListener::eET_ContainerEmpty:
					ActivateOutput(&m_actInfo, eO_EventContainerEmpty, true);
					break;
				}
			}
		}
	}

	void UnregisterListener()
	{
		if (CCryAction::GetCryAction())
		{
			CEntityContainerMgr& containerManager = CCryAction::GetCryAction()->GetEntityContainerMgr();
			containerManager.UnregisterListener(this);
		}
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
		case eFE_SetEntityId:
			{
				bool bHasSameEntity = (m_actInfo.pEntity == pActInfo->pEntity);

				// Keep a copy of the activation info to be able to use the output ports
				m_actInfo = *pActInfo;

				if (!bHasSameEntity)
				{
					UnregisterListener();

					if (pActInfo->pEntity)
					{
						// Register listener
						EntityId entityId = pActInfo->pEntity->GetId();
						CEntityContainerMgr& containerManager = CCryAction::GetCryAction()->GetEntityContainerMgr();
						stack_string listenerName = stack_string().Format("Containers:Listener(Id:%u)", entityId);
						containerManager.RegisterListener(this, listenerName.c_str(), false);
					}
				}
			}
			break;
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

private:
	SActivationInfo m_actInfo;
};


class CFlowNode_EntityContainerContMgr_Filters : public CFlowBaseNode<eNCT_Singleton>
{
	enum EInputs
	{
		eI_Apply,
		eI_Filters,
		eI_FilterValue,
		eI_RefEntityId,
		eI_RefPosition,
		eI_OutputContainerId,
		eI_OutputSize,
		eI_ClearOutputContainer,
		eI_SplitContainer,
	};

	enum EOutputs
	{
		eO_Done    = 0,
		eO_FIRST_MEMBER_OUTPUT,
		eO_Member1 = eO_FIRST_MEMBER_OUTPUT,
		eO_Member2,
		eO_Member3,
		eO_Member4,
		eO_Member5,
		eO_Member6,
		eO_LAST_MEMBER_OUTPUT = eO_Member6,
	};

	enum EFilterType
	{
		eFT_None = 0,
		eFT_Closest,
		eFT_Furthest,
		eFT_Random,
	};

	enum ESortingDirection
	{
		eSD_Ascending = 0,
		eSD_Descending,
	};

public:
	CFlowNode_EntityContainerContMgr_Filters(SActivationInfo* pActInfo) { }
	~CFlowNode_EntityContainerContMgr_Filters() { }

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] =
		{
			InputPortConfig_AnyType("Apply", _HELP("Apply filter")),
			InputPortConfig<int>("Filters", _HELP("Filters Set"), 0, _UICONFIG("enum_int:None=0,Closest=1,Farthest=2,Random=3")),
			InputPortConfig<float>("FilterValue", -1.f, _HELP("A value that can modify the filtering results. For 'Closest/Farthest', the value will be a minimum/maximum distance (<0 -> no modification)")),
			InputPortConfig<EntityId>("RefEntityId", _HELP("Reference entity for distance measurement. It overrides the Reference Position value")),
			InputPortConfig<Vec3>("RefPosition", Vec3(ZERO), _HELP("Reference position for distance measurement. Only used when RefEntityId is note selected or ZERO")),
			InputPortConfig<EntityId>("OutputContainerId", 0, _HELP("The filtering result will be outputted into the entity container with this ID")),
			InputPortConfig<int>("OutputSize", -1, _HELP("How many elements will be outputted into OutputContainerId (-1: ALL)")),
			InputPortConfig<bool>("ClearOutputContainer", true, _HELP("Should the contents of OutputContainerId be cleared before adding entities to it?")),
			InputPortConfig<bool>("SplitContainer", false, _HELP("Should the items be MOVED from the original group into the OutputContainerId?")),
			{ 0 }
		};

		static const SOutputPortConfig outputs[] =
		{
			OutputPortConfig_Void("Done",         _HELP("Operation done")),
			OutputPortConfig<EntityId>("Member1", _HELP("First member's ID")),
			OutputPortConfig<EntityId>("Member2", _HELP("Second member's ID")),
			OutputPortConfig<EntityId>("Member3", _HELP("Third member's ID")),
			OutputPortConfig<EntityId>("Member4", _HELP("Fourth member's ID")),
			OutputPortConfig<EntityId>("Member5", _HELP("Fifth member's ID")),
			OutputPortConfig<EntityId>("Member6", _HELP("Sixth member's ID")),
			{ 0 }
		};

		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("FlowNode to filter the components of an entity container, producing a sorted output");
		config.SetCategory(EFLN_ADVANCED);
	}

	void Serialize(SActivationInfo*, TSerialize ser) { }

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			break;
		case eFE_Activate:
			{
				if (!pActInfo->pEntity)
					return;

				if (IsPortActive(pActInfo, eI_Apply))
				{
					const EntityId containerId = pActInfo->pEntity->GetId();
					CEntityContainerMgr& containerManager = CCryAction::GetCryAction()->GetEntityContainerMgr();

					// Get all the entities in the group
					CEntityContainer::TEntitiesInContainer ids;
					containerManager.GetEntitiesInContainer(containerId, ids);

					// Read node common params
					int filterType = GetPortInt(pActInfo, eI_Filters);
					const int kSelectedOutputSize = GetPortInt(pActInfo, eI_OutputSize);
					const EntityId kOutputContainerId = GetPortEntityId(pActInfo, eI_OutputContainerId);

					const bool bOutputToContainer = containerManager.IsContainer(kOutputContainerId);
					const uint32 kNumFgOutputs = eO_LAST_MEMBER_OUTPUT - eO_FIRST_MEMBER_OUTPUT + 1; // number of fixed result outputs in FG node
					const uint32 kElementsToOutputByNode = std::min(static_cast<uint32>(ids.size()), kNumFgOutputs);

					uint32 numElementsToOutputToContainer = UpdateNumElementsToOutputToGroup(kSelectedOutputSize, ids);

					// Filtering by distance
					if (filterType == eFT_Closest || filterType == eFT_Furthest)
					{
						const float threshold = GetPortFloat(pActInfo, eI_FilterValue);
						const float thresholdSqr = threshold * threshold;

						// Select the reference point
						Vec3 refPoint = GetPortVec3(pActInfo, eI_RefPosition);
						EntityId refEntityId = GetPortEntityId(pActInfo, eI_RefEntityId);
						if (refEntityId != 0)
						{
							if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(refEntityId))
								refPoint = pEntity->GetWorldPos();
						}

						// lambda definition for remove_if
						auto remove_if_lambda = [refPoint, thresholdSqr, filterType](CEntityContainer::TSentityInfoParam entityInfo)
						{
							const IEntity* pThisEntity = gEnv->pEntitySystem->GetEntity(entityInfo.id);
							const float distSqr = pThisEntity ? (refPoint - pThisEntity->GetWorldPos()).GetLengthSquared() : 0.f;
							return pThisEntity && (filterType == eFT_Closest ? thresholdSqr<distSqr : thresholdSqr> distSqr);
						};

						// remove elements based on selected threshold
						if (threshold >= 0.f)
						{
							ids.erase(std::remove_if(ids.begin(), ids.end(), remove_if_lambda), ids.end());
						}

						numElementsToOutputToContainer = UpdateNumElementsToOutputToGroup(kSelectedOutputSize, ids);
						// partial sort needs to cover all the FG outputs
						const uint32 numElementsForPartialSort = std::max(numElementsToOutputToContainer, kElementsToOutputByNode);

						if (!ids.empty())
						{
							// Run filter
							if (filterType == eFT_Furthest)
							{
								SClosestDistanceComparison<eSD_Descending> comp(refPoint);
								std::partial_sort(ids.begin(), ids.begin() + std::min(numElementsForPartialSort, static_cast<uint32>(ids.size())), ids.end(), comp);
							}
							else
							{
								SClosestDistanceComparison<eSD_Ascending> comp(refPoint);
								std::partial_sort(ids.begin(), ids.begin() + std::min(numElementsForPartialSort, static_cast<uint32>(ids.size())), ids.end(), comp);
							}
						}
					}
					// Filtering Random
					else if (filterType == eFT_Random && kSelectedOutputSize != 0)
					{
						std::random_shuffle(ids.begin(), ids.end());
					}

					// Output filter results into a container
					if (bOutputToContainer)
					{
						const bool clearGroup = GetPortBool(pActInfo, eI_ClearOutputContainer);
						if (clearGroup)
						{
							containerManager.ClearContainer(kOutputContainerId);
						}

						const bool splitGroup = GetPortBool(pActInfo, eI_SplitContainer);
						for (size_t i = 0; i < numElementsToOutputToContainer; ++i)
						{
							containerManager.AddEntity(kOutputContainerId, ids[i].id);
							if (splitGroup)
							{
								containerManager.RemoveEntity(containerId, ids[i].id);
							}
						}
					}

					// Done output
					ActivateOutput(pActInfo, eO_Done, true);

					const uint32 kOutputSize = std::min(kElementsToOutputByNode, static_cast<uint32>(ids.size()));
					if (!ids.empty())
					{
						// Active outputs
						for (uint32 i = 0; i < kOutputSize; ++i)
							ActivateOutput(pActInfo, i + eO_FIRST_MEMBER_OUTPUT, ids[i].id);
					}
					// Clean up unused outputs
					for (uint32 i = kOutputSize; i < kNumFgOutputs; ++i)
						ActivateOutput(pActInfo, i + eO_FIRST_MEMBER_OUTPUT, 0);
				}
			}
			break;
		}
	}

	inline uint32 UpdateNumElementsToOutputToGroup(int outputSize, CEntityContainer::TEntitiesInContainer ids)
	{
		// The number of elements to output depends on the elements in the container and the output size selected in flog graph by the user
		return (outputSize < 0) ? ids.size() : std::min(static_cast<uint32>(ids.size()), static_cast<uint32>(outputSize));
	}

	template<ESortingDirection kSortDir>
	struct SClosestDistanceComparison
	{
		SClosestDistanceComparison(Vec3 refPoint) : m_refPoint(refPoint) {}

		bool operator()(CEntityContainer::TSentityInfoParam entityInfo1, CEntityContainer::TSentityInfoParam entityInfo2)
		{
			const IEntity* pEntity1 = gEnv->pEntitySystem->GetEntity(entityInfo1.id);
			const IEntity* pEntity2 = gEnv->pEntitySystem->GetEntity(entityInfo2.id);

			if (pEntity1 && pEntity2)
			{
				float dist1 = (m_refPoint - pEntity1->GetWorldPos()).GetLengthSquared();
				float dist2 = (m_refPoint - pEntity2->GetWorldPos()).GetLengthSquared();

				return kSortDir == eSD_Ascending ? (dist1 < dist2) : (dist2 < dist1);
			}

			return false;
		}

		Vec3 m_refPoint;
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

};


REGISTER_FLOW_NODE("GameEntity:Containers:Add/Remove Entity", CFlowNode_EntityContainerContMgr_AddRemove);
REGISTER_FLOW_NODE("GameEntity:Containers:Merge", CFlowNode_EntityContainerContMgr_Merge);
REGISTER_FLOW_NODE("GameEntity:Containers:Clear", CFlowNode_EntityContainerContMgr_Clear);
REGISTER_FLOW_NODE("GameEntity:Containers:Actions", CFlowNode_EntityContainerContMgr_Actions);
REGISTER_FLOW_NODE("GameEntity:Containers:QueryContainerSize", CFlowNode_EntityContainerContMgr_QueryContainerSize);
REGISTER_FLOW_NODE("GameEntity:Containers:QueryRandomEntity", CFlowNode_EntityContainerContMgr_QueryRandomEntity);
REGISTER_FLOW_NODE("GameEntity:Containers:QueryContainerId", CFlowNode_EntityContainerContMgr_QueryContainerId);
REGISTER_FLOW_NODE("GameEntity:Containers:QueryIsInContainer", CFlowNode_EntityContainerContMgr_QueryIsInContainer);
REGISTER_FLOW_NODE("GameEntity:Containers:QueryEntityByIndex", CFlowNode_EntityContainerContMgr_QueryEntityByIndex);
REGISTER_FLOW_NODE("GameEntity:Containers:Listener", CFlowNode_EntityContainerContMgr_Listener);
REGISTER_FLOW_NODE("GameEntity:Containers:Filters", CFlowNode_EntityContainerContMgr_Filters);
