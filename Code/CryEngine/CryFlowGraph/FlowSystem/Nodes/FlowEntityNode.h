// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryFlowGraph/IFlowBaseNode.h>

//////////////////////////////////////////////////////////////////////////
class CFlowEntityClass : public IFlowNodeFactory
{
public:
	CFlowEntityClass(IEntityClass* pEntityClass);
	~CFlowEntityClass();
	virtual IFlowNodePtr Create(IFlowNode::SActivationInfo*);

	virtual void         GetMemoryUsage(ICrySizer* s) const
	{
		SIZER_SUBCOMPONENT_NAME(s, "CFlowEntityClass");
		s->AddObject(this, sizeof(*this));
		s->AddObject(m_inputs);
		s->AddObject(m_outputs);
	}

	// Allow overriding since this is the default flow class
	virtual bool        AllowOverride() const { return true; }

	void        GetConfiguration(SFlowNodeConfig&);

	const char* CopyStr(const char* src);
	void        FreeStr(const char* src);

	size_t      GetNumOutputs() { return m_outputs.size() - 1; }

	void        Reset();

private:
	void GetInputsOutputs(IEntityClass* pEntityClass);
	friend class CFlowEntityNode;

	//string m_classname;
	IEntityClass*                  m_pEntityClass;
	std::vector<SInputPortConfig>  m_inputs;
	std::vector<SOutputPortConfig> m_outputs;
};

//////////////////////////////////////////////////////////////////////////
class CFlowEntityNodeBase : public CFlowBaseNode<eNCT_Instanced>, public IEntityEventListener
{
public:
	CFlowEntityNodeBase()
	{
		m_entityId = 0;
		m_event = ENTITY_EVENT_LAST;
	};
	~CFlowEntityNodeBase()
	{
		UnregisterEvent(m_event);
	}

	// deliberately don't implement Clone(), make sure derived classes do.

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (event != eFE_SetEntityId)
			return;

		EntityId newEntityId = GetEntityId(pActInfo);

		if (m_entityId && newEntityId)
			UnregisterEvent(m_event);

		if (newEntityId)
		{
			m_entityId = newEntityId;
			RegisterEvent(m_event);
		}
	}

	// Return entityId of this node
	EntityId GetEntityId(SActivationInfo* pActInfo) const
	{
		assert(pActInfo);

		EntityId entityId = m_entityId;

		if (pActInfo->pEntity)
		{
			entityId = pActInfo->pEntity->GetId();
		}

		return entityId;
	}

	inline IEntity* GetEntity(SActivationInfo* pActInfo)
	{
		if (pActInfo->pEntity)
			return pActInfo->pEntity;
		if (m_entityId != INVALID_ENTITYID)
		{
			return gEnv->pEntitySystem->GetEntity(m_entityId);
		}
		return nullptr;
	}

	void RegisterEvent(EEntityEvent event)
	{
		if (event != ENTITY_EVENT_LAST)
		{
			gEnv->pEntitySystem->AddEntityEventListener(m_entityId, event, this);
			gEnv->pEntitySystem->AddEntityEventListener(m_entityId, ENTITY_EVENT_DONE, this);
		}
	}
	void UnregisterEvent(EEntityEvent event)
	{
		if (m_entityId && event != ENTITY_EVENT_LAST)
		{
			IEntitySystem* pEntitySystem = gEnv->pEntitySystem;
			if (pEntitySystem)
			{
				pEntitySystem->RemoveEntityEventListener(m_entityId, event, this);
				pEntitySystem->RemoveEntityEventListener(m_entityId, ENTITY_EVENT_DONE, this);
			}
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

protected:
	EEntityEvent m_event; // This member must be set in derived class constructor.
	EntityId     m_entityId;
};

//////////////////////////////////////////////////////////////////////////
// Flow node for entity.
//////////////////////////////////////////////////////////////////////////
class CFlowEntityNode : public CFlowEntityNodeBase
{
public:
	CFlowEntityNode(CFlowEntityClass* pClass, SActivationInfo* pActInfo);
	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo);
	virtual void         GetConfiguration(SFlowNodeConfig&);
	virtual void         ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo);
	virtual bool         SerializeXML(SActivationInfo*, const XmlNodeRef&, bool);

	//////////////////////////////////////////////////////////////////////////
	// IEntityEventListener
	virtual void OnEntityEvent(IEntity* pEntity, const SEntityEvent& event);
	//////////////////////////////////////////////////////////////////////////

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

protected:
	void Serialize(SActivationInfo* pActInfo, TSerialize ser);
	void SendEventToEntity(SActivationInfo* pActInfo, IEntity* pEntity);

	IFlowGraph*                  m_pGraph;
	TFlowNodeId                  m_nodeID;
	_smart_ptr<CFlowEntityClass> m_pClass;
	int                          m_lastInitializeFrameId;
};
