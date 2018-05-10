// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include <CryFlowGraph/IFlowBaseNode.h>
#include <CryEntitySystem/IEntitySystem.h>

#define ADD_BASE_INPUTS_ENTITY_ITER()                                                                                           \
  InputPortConfig_Void("Start", _HELP("Call to start iterating through all entities inside the defined sphere")),               \
  InputPortConfig_Void("Next", _HELP("Call to get next entity found")),                                                         \
  InputPortConfig<int>("Limit", 0, _HELP("Limit how many entities are returned. Use 0 to get all entities")),                   \
  InputPortConfig<bool>("Immediate", false, _HELP("TRUE to iterate immediately through results, without having to call Next")), \
  InputPortConfig<int>("Type", 0, _HELP("Type of entity to iterate"), 0, _UICONFIG(ENTITY_TYPE_ENUM))

#define ADD_BASE_OUTPUTS_ENTITY_ITER()                                                                                      \
  OutputPortConfig<EntityId>("EntityId", _HELP("Called each time an entity is found, with the Id of the entity returned")), \
  OutputPortConfig<int>("Count", _HELP("Called each time an entity is found, with the current count returned")),            \
  OutputPortConfig_Void("Done", _HELP("Called when all entities have been found, false if none"))

#define ENTITY_TYPE_ENUM ("enum_int:All=0,AI=1,Actor=2,Vehicle=3,Item=4,Other=5")

enum EEntityType
{
	eET_Unknown = 0x00,
	eET_Valid   = 0x01,
	eET_AI      = 0x02,
	eET_Actor   = 0x04,
	eET_Vehicle = 0x08,
	eET_Item    = 0x10,
};

bool IsValidType(int requested, const EEntityType& type)
{
	bool bValid = false;

	if (requested == 0)
	{
		bValid = (type & eET_Valid) == eET_Valid;
	}
	else if (requested == 1)
	{
		bValid = (type & eET_AI) == eET_AI;
	}
	else if (requested == 2)
	{
		bValid = (type & eET_Actor) == eET_Actor;
	}
	else if (requested == 3)
	{
		bValid = (type & eET_Vehicle) == eET_Vehicle;
	}
	else if (requested == 4)
	{
		bValid = (type & eET_Item) == eET_Item;
	}
	else if (requested == 5)
	{
		bValid = (type == eET_Valid);
	}

	return bValid;
}

EEntityType GetEntityType(EntityId id)
{
	int type = eET_Unknown;

	IEntitySystem* pEntitySystem = gEnv->pEntitySystem;
	if (pEntitySystem)
	{
		IEntity* pEntity = pEntitySystem->GetEntity(id);
		if (pEntity)
		{
			type = eET_Valid;

			IEntityClass* pClass = pEntity->GetClass();
			if (pClass)
			{
				const char* className = pClass->GetName();

				// Check AI
				if (pEntity->GetAI())
				{
					type |= eET_AI;
				}

				// Check actor
				IActorSystem* pActorSystem = gEnv->pGameFramework->GetIActorSystem();
				if (pActorSystem)
				{
					IActor* pActor = pActorSystem->GetActor(id);
					if (pActor)
					{
						type |= eET_Actor;
					}
				}

				// Check vehicle
				IVehicleSystem* pVehicleSystem = gEnv->pGameFramework->GetIVehicleSystem();
				if (pVehicleSystem)
				{
					if (pVehicleSystem->IsVehicleClass(className))
					{
						type |= eET_Vehicle;
					}
				}

				// Check item
				IItemSystem* pItemSystem = gEnv->pGameFramework->GetIItemSystem();
				if (pItemSystem)
				{
					if (pItemSystem->IsItemClass(className))
					{
						type |= eET_Item;
					}
				}
			}
		}
	}
	return (EEntityType)type;
}

//-------------------------------------------------------------------------------------
class CFlowBaseIteratorNode : public CFlowBaseNode<eNCT_Instanced>
{
protected:
	enum EInputPorts
	{
		EIP_Start,
		EIP_Next,
		EIP_Limit,
		EIP_Immediate,
		EIP_Type,
		EIP_CustomStart,
	};

	enum EOutputPorts
	{
		EOP_EntityId,
		EOP_Count,
		EOP_Done,
		EOP_CustomStart,
	};

	typedef std::list<EntityId> IterList;
	IterList::iterator m_ListIter;
	IterList           m_List;

	SActivationInfo*   m_pActInfo;
	bool               m_bImmediate;
	bool               m_bRestart;
	int                m_Count;
	int                m_limit;
	int                m_Iter;
	int                m_type;
	Vec3               m_min;
	Vec3               m_max;

	void Reset()
	{
		m_Iter = 0;
		m_Count = 0;
		m_min.zero();
		m_max.zero();
		m_List.clear();
		m_bRestart = false;
		m_type = GetPortInt(m_pActInfo, EIP_Type);
		m_limit = GetPortInt(m_pActInfo, EIP_Limit);
		m_bImmediate = GetPortBool(m_pActInfo, EIP_Immediate);
	}

	void Output()
	{
		if (m_Count > 0)
		{
			const int outputCount = m_bImmediate ? m_Count : m_Iter + 1;
			while (m_Iter < outputCount)
			{
				if (m_limit == 0 || m_Iter <= m_limit)
				{
					ActivateOutput(m_pActInfo, EOP_EntityId, *m_ListIter++);
					ActivateOutput(m_pActInfo, EOP_Count, ++m_Iter);
				}
			}
		}

		if (m_Iter == m_Count || m_Iter == m_limit)
		{
			ActivateOutput(m_pActInfo, EOP_Done, 1);
			m_bRestart = true;
		}
	}

public:
	CFlowBaseIteratorNode(SActivationInfo* pActInfo)
		: m_pActInfo(pActInfo)
		, m_bImmediate(false)
		, m_bRestart(false)
		, m_Count(0)
		, m_limit(0)
		, m_Iter(0)
		, m_type(0)
	{
	}

	virtual ~CFlowBaseIteratorNode() {};
	virtual bool GetCustomConfiguration(SFlowNodeConfig& config)      { return false; };
	virtual void CalculateMinMax()                                    {};
	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser) {};
	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		// Define input ports here, in same order as EInputPorts
		static const SInputPortConfig inputs[] =
		{
			ADD_BASE_INPUTS_ENTITY_ITER(),
			{ 0 }
		};

		// Define output ports here, in same order as EOutputPorts
		static const SOutputPortConfig outputs[] =
		{
			ADD_BASE_OUTPUTS_ENTITY_ITER(),
			{ 0 }
		};

		if (!GetCustomConfiguration(config))
		{
			config.pInputPorts = inputs;
			config.pOutputPorts = outputs;
			config.sDescription = _HELP("Base iterator");
			config.SetCategory(EFLN_APPROVED);
		}
	}

	virtual void GetEntities()
	{
		IPhysicalEntity** ppList = NULL;
		IPhysicalWorld* pWorld = gEnv->pPhysicalWorld;

		const int entityCount = pWorld->GetEntitiesInBox(m_min, m_max, ppList, ent_all);

		for (int i = 0; i < entityCount; ++i)
		{
			const int id = pWorld->GetPhysicalEntityId(ppList[i]);
			if (id > 0)
			{
				const EntityId entityId = (EntityId)id;
				const EEntityType entityType = GetEntityType((EntityId)id);
				if (IsValidType(m_type, GetEntityType(entityId)))
				{
					m_List.push_back(entityId);
					++m_Count;
				}
			}
		}
		m_ListIter = m_List.begin();
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		m_pActInfo = pActInfo;

		switch (event)
		{
		case eFE_Activate:
			{
				if (IsPortActive(m_pActInfo, EIP_Start) || m_bRestart)
				{
					Reset();
					CalculateMinMax();
					GetEntities();
				}
				Output();
				break;
			}
		}
	}

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowBaseIteratorNode(pActInfo);
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//-------------------------------------------------------------------------------------
class CFlowNode_GetEntitiesInSphere : public CFlowBaseIteratorNode
{
	enum ECustomInputPorts
	{
		EIP_Pos = EIP_CustomStart,
		EIP_Range,
	};

public:
	CFlowNode_GetEntitiesInSphere(SActivationInfo* pActInfo) : CFlowBaseIteratorNode(pActInfo) {}
	virtual ~CFlowNode_GetEntitiesInSphere() {}

	virtual bool GetCustomConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] =
		{
			ADD_BASE_INPUTS_ENTITY_ITER(),
			InputPortConfig<Vec3>("Center",_HELP("Center point of sphere")),
			InputPortConfig<float>("Range",0.f,                              _HELP("Range i.e., radius of sphere - Distance from center to check for entities")),
			{ 0 }
		};

		static const SOutputPortConfig outputs[] =
		{
			ADD_BASE_OUTPUTS_ENTITY_ITER(),
			{ 0 }
		};

		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Finds and returns all entities that are inside the defined sphere");
		config.SetCategory(EFLN_APPROVED);

		return true;
	}

	virtual void CalculateMinMax()
	{
		const Vec3& center(GetPortVec3(m_pActInfo, EIP_Pos));
		const float range = GetPortFloat(m_pActInfo, EIP_Range);
		const float rangeSq = range * range;

		m_min(center.x - range, center.y - range, center.z - range);
		m_max(center.x + range, center.y + range, center.z + range);
	}

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNode_GetEntitiesInSphere(pActInfo);
	}
};

//-------------------------------------------------------------------------------------
class CFlowNode_GetEntitiesInBoxByClass : public CFlowBaseNode<eNCT_Instanced>
{
protected:
	enum EInputPorts
	{
		EIP_Start,
		EIP_Next,
		EIP_Limit,
		EIP_Immediate,
		EIP_ClassName,
		EIP_Pos,
		EIP_Size
	};

	enum EOutputPorts
	{
		EOP_EntityId,
		EOP_Count,
		EOP_Done
	};

	typedef std::list<EntityId> IterList;
	IterList::iterator m_listIter;
	IterList           m_list;

	SActivationInfo*   m_pActInfo;
	bool               m_bImmediate;
	bool               m_bRestart;
	int                m_count;
	int                m_limit;
	int                m_iter;
	string             m_className;

	void Reset()
	{
		m_iter = 0;
		m_count = 0;
		m_list.clear();
		m_bRestart = false;
		m_className = GetPortString(m_pActInfo, EIP_ClassName);
		m_limit = GetPortInt(m_pActInfo, EIP_Limit);
		m_bImmediate = GetPortBool(m_pActInfo, EIP_Immediate);
	}

	void Output()
	{
		if (m_count > 0)
		{
			const int outputCount = m_bImmediate ? m_count : m_iter + 1;
			while (m_iter < outputCount)
			{
				if (m_limit == 0 || m_iter <= m_limit)
				{
					ActivateOutput(m_pActInfo, EOP_EntityId, *m_listIter++);
					ActivateOutput(m_pActInfo, EOP_Count, ++m_iter);
				}
			}
		}

		if (m_iter == m_count || m_iter == m_limit)
		{
			ActivateOutput(m_pActInfo, EOP_Done, 1);
			m_bRestart = true;
		}
	}

public:
	CFlowNode_GetEntitiesInBoxByClass(SActivationInfo* pActInfo) :
		m_pActInfo(pActInfo),
		m_bImmediate(false),
		m_bRestart(false),
		m_count(0),
		m_limit(0),
		m_iter(0)
	{
	};

	virtual ~CFlowNode_GetEntitiesInBoxByClass() {};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] =
		{
			InputPortConfig_Void("Start",        _HELP("Call to start iterating through all entities inside the defined sphere")),
			InputPortConfig_Void("Next",         _HELP("Call to get next entity found")),
			InputPortConfig<int>("Limit",        0,                                                                               _HELP("Limit how many entities are returned. Use 0 to get all entities")),
			InputPortConfig<bool>("Immediate",   false,                                                                           _HELP("TRUE to iterate immediately through results, without having to call Next")),
			InputPortConfig<string>("ClassName", _HELP("Class name of entity to iterate")),
			InputPortConfig<Vec3>("Center",      _HELP("Center point of the box")),
			InputPortConfig<float>("Size",       0.f,                                                                             _HELP("Size of the box (from center to any edge) where to check for entities")),
			{ 0 }
		};

		static const SOutputPortConfig outputs[] =
		{
			ADD_BASE_OUTPUTS_ENTITY_ITER(),
			{ 0 }
		};

		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Finds and returns all entities of the given class that are inside the defined box");
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser) {};

	virtual void GetEntities()
	{
		const Vec3& center(GetPortVec3(m_pActInfo, EIP_Pos));
		const float range = GetPortFloat(m_pActInfo, EIP_Size);

		Vec3 boxMin(center.x - range, center.y - range, center.z - range);
		Vec3 boxMax(center.x + range, center.y + range, center.z + range);

		IPhysicalEntity** ppList = NULL;
		const int size = gEnv->pPhysicalWorld->GetEntitiesInBox(boxMin, boxMax, ppList, ent_all);
		for (int i = 0; i < size; ++i)
		{
			const int id = gEnv->pPhysicalWorld->GetPhysicalEntityId(ppList[i]);
			if (id > 0)
			{
				const IEntitySystem* const pEntitySystem = gEnv->pEntitySystem;
				if (pEntitySystem)
				{
					const EntityId entityId = (EntityId)id;
					const IEntity* const pEntity = pEntitySystem->GetEntity(entityId);
					if (pEntity)
					{
						const IEntityClass* const pClass = pEntity->GetClass();
						if (pClass)
						{
							if (m_className.find(pClass->GetName()) != -1)
							{
								m_list.push_back(entityId);
								++m_count;
							}
						}
					}
				}
			}
		}
		m_listIter = m_list.begin();
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		m_pActInfo = pActInfo;

		switch (event)
		{
		case eFE_Activate:
			{
				if (IsPortActive(m_pActInfo, EIP_Start) || m_bRestart)
				{
					Reset();
					GetEntities();
				}
				Output();
				break;
			}
		}
	}

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNode_GetEntitiesInBoxByClass(pActInfo);
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//-------------------------------------------------------------------------------------
class CFlowNode_GetEntitiesInBox : public CFlowBaseIteratorNode
{
	enum ECustomInputPorts
	{
		EIP_Min = EIP_CustomStart,
		EIP_Max,
	};

public:
	CFlowNode_GetEntitiesInBox(SActivationInfo* pActInfo) : CFlowBaseIteratorNode(pActInfo) {}

	virtual ~CFlowNode_GetEntitiesInBox() {}

	virtual bool GetCustomConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] =
		{
			ADD_BASE_INPUTS_ENTITY_ITER(),
			InputPortConfig<Vec3>("Min",  _HELP("AABB min")),
			InputPortConfig<Vec3>("Max",  _HELP("AABB max")),
			{ 0 }
		};

		static const SOutputPortConfig outputs[] =
		{
			ADD_BASE_OUTPUTS_ENTITY_ITER(),
			{ 0 }
		};

		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Finds and returns all entities that are inside the defined AABB");
		config.SetCategory(EFLN_APPROVED);

		return true;
	}

	virtual void CalculateMinMax()
	{
		m_min = GetPortVec3(m_pActInfo, EIP_Min);
		m_max = GetPortVec3(m_pActInfo, EIP_Max);
	}

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNode_GetEntitiesInBox(pActInfo);
	}
};

//-------------------------------------------------------------------------------------
class CFlowNode_GetEntitiesInArea : public CFlowBaseIteratorNode
{
	enum ECustomInputPorts
	{
		EIP_Area = EIP_CustomStart,
	};

public:
	CFlowNode_GetEntitiesInArea(SActivationInfo* pActInfo) : CFlowBaseIteratorNode(pActInfo) {}
	virtual ~CFlowNode_GetEntitiesInArea() {}

	virtual bool GetCustomConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] =
		{
			ADD_BASE_INPUTS_ENTITY_ITER(),
			InputPortConfig<string>("Area",_HELP("Name of area shape"),  0, 0),
			{ 0 }
		};

		static const SOutputPortConfig outputs[] =
		{
			ADD_BASE_OUTPUTS_ENTITY_ITER(),
			{ 0 }
		};

		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Finds and returns all entities that are inside the area shape with the given name");
		config.SetCategory(EFLN_APPROVED);

		return true;
	}

	virtual void CalculateMinMax()
	{
		const string area = GetPortString(m_pActInfo, EIP_Area);

		IEntitySystem* pEntitySystem = gEnv->pEntitySystem;
		if (pEntitySystem)
		{
			IEntity* pArea = pEntitySystem->FindEntityByName(area);
			if (pArea)
			{
				IEntityAreaComponent* pAreaProxy = (IEntityAreaComponent*)pArea->GetProxy(ENTITY_PROXY_AREA);
				if (pAreaProxy)
				{
					Vec3 worldPos(pArea->GetWorldPos());
					EEntityAreaType areaType = pAreaProxy->GetAreaType();

					switch (areaType)
					{
					case ENTITY_AREA_TYPE_BOX:
						{
							pAreaProxy->GetBox(m_min, m_max);
							m_min += worldPos;
							m_max += worldPos;
							break;
						}
					case ENTITY_AREA_TYPE_SPHERE:
						{
							Vec3 center;
							float radius = 0.f;
							pAreaProxy->GetSphere(center, radius);

							m_min.Set(center.x - radius, center.y - radius, center.z - radius);
							m_max.Set(center.x + radius, center.y + radius, center.z + radius);
							break;
						}
					case ENTITY_AREA_TYPE_SHAPE:
						{
							const Vec3* points = pAreaProxy->GetPoints();
							const int count = pAreaProxy->GetPointsCount();
							if (count > 0)
							{
								Vec3 p = worldPos + points[0];
								m_min = p;
								m_max = p;
								for (int i = 1; i < count; ++i)
								{
									p = worldPos + points[i];
									if (p.x < m_min.x) m_min.x = p.x;
									if (p.y < m_min.y) m_min.y = p.y;
									if (p.z < m_min.z) m_min.z = p.z;
									if (p.x > m_max.x) m_max.x = p.x;
									if (p.y > m_max.y) m_max.y = p.y;
									if (p.z > m_max.z) m_max.z = p.z;
								}
							}
							break;
						}
					}
				}
			}
		}
	}

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNode_GetEntitiesInArea(pActInfo);
	}
};

//-------------------------------------------------------------------------------------
class CFlowNode_GetEntities : public CFlowBaseIteratorNode
{

public:
	CFlowNode_GetEntities(SActivationInfo* pActInfo) : CFlowBaseIteratorNode(pActInfo) {}
	virtual ~CFlowNode_GetEntities() {}

	virtual bool GetCustomConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] =
		{
			ADD_BASE_INPUTS_ENTITY_ITER(),
			{ 0 }
		};

		static const SOutputPortConfig outputs[] =
		{
			ADD_BASE_OUTPUTS_ENTITY_ITER(),
			{ 0 }
		};

		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Finds and returns all entities in the world");
		config.SetCategory(EFLN_APPROVED);

		return true;
	}

	virtual void GetEntities()
	{
		IEntitySystem* pEntitySystem = gEnv->pEntitySystem;
		if (pEntitySystem)
		{
			IEntityItPtr iter = pEntitySystem->GetEntityIterator();
			if (iter)
			{
				iter->MoveFirst();
				IEntity* pEntity = NULL;
				while (!iter->IsEnd())
				{
					pEntity = iter->Next();
					if (pEntity)
					{
						const EntityId id = pEntity->GetId();
						const EEntityType entityType = GetEntityType(id);
						if (IsValidType(m_type, entityType))
						{
							m_List.push_back(id);
							++m_Count;
						}
					}
				}
			}
		}
		m_ListIter = m_List.begin();
	}

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNode_GetEntities(pActInfo);
	}
};

REGISTER_FLOW_NODE("Iterator:GetEntitiesInSphere", CFlowNode_GetEntitiesInSphere);
REGISTER_FLOW_NODE("Iterator:GetEntitiesInBox", CFlowNode_GetEntitiesInBox);
REGISTER_FLOW_NODE("Iterator:GetEntitiesInArea", CFlowNode_GetEntitiesInArea);
REGISTER_FLOW_NODE("Iterator:GetEntities", CFlowNode_GetEntities);
REGISTER_FLOW_NODE("Iterator:GetEntitiesInBoxByClass", CFlowNode_GetEntitiesInBoxByClass);
