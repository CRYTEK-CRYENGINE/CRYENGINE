// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryFlowGraph/IFlowBaseNode.h>

struct IEntityClass;

typedef void (* FlowNodeInputFunction)(EntityId id, const TFlowInputData& data);
typedef void (* FlowNodeOnActivateFunction)(EntityId id, IFlowNode::SActivationInfo* pActInfo, const class CFlowGameEntityNode* pNode);

class CGameEntityNodeFactory : public IFlowNodeFactory
{
public:
	CGameEntityNodeFactory();
	virtual ~CGameEntityNodeFactory();

	virtual IFlowNodePtr Create(IFlowNode::SActivationInfo* pActInfo);

	virtual void         GetMemoryUsage(ICrySizer* s) const
	{
		SIZER_SUBCOMPONENT_NAME(s, "CFlowGameEntityFactory");
		s->AddObject(this, sizeof(*this));
		s->AddObject(m_inputs);
		s->AddObject(m_outputs);
	}

	void   GetConfiguration(SFlowNodeConfig& configuration);

	size_t GetNumOutputs() { return m_outputs.size() - 1; }

	void   Reset();

	void   AddInputs(const std::vector<SInputPortConfig>& inputs, FlowNodeOnActivateFunction callback);
	void   AddInput(IEntityClass::EventValueType type, const char* name, FlowNodeInputFunction callback);
	void   AddOutputs(const std::vector<SOutputPortConfig>& outputs);
	int    AddOutput(IEntityClass::EventValueType type, const char* name, const char* description = nullptr);
	void   Close();

private:
	void MakeHumanName(SInputPortConfig& config);

	friend class CFlowGameEntityNode;

	std::vector<FlowNodeInputFunction> m_callbacks;
	std::vector<SInputPortConfig>      m_inputs;
	std::vector<SOutputPortConfig>     m_outputs;

	FlowNodeOnActivateFunction         m_activateCallback;
};

class CFlowGameEntityNode : public CFlowBaseNode<eNCT_Instanced>, public IEntityEventListener
{
public:
	CFlowGameEntityNode(CGameEntityNodeFactory* pClass, SActivationInfo* pActInfo);
	virtual ~CFlowGameEntityNode()
	{
		UnregisterEvent();
	}
	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) override;
	virtual void         GetConfiguration(SFlowNodeConfig&) override;
	virtual void         ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo) override;
	virtual bool         SerializeXML(SActivationInfo* pActInfo, const XmlNodeRef&, bool) override;

	virtual void         GetMemoryUsage(ICrySizer* s) const override
	{
		s->Add(*this);
	}

	//IEntityEventListener
	virtual void OnEntityEvent(IEntity* pEntity, SEntityEvent& event) override;
	//~IEntityEventListener

protected:
	ILINE IEntity* GetEntity() const
	{
		return gEnv->pEntitySystem->GetEntity(m_entityId);
	}

	EntityId GetEntityId(SActivationInfo* pActInfo) const
	{
		assert(pActInfo);
		EntityId entityId = INVALID_ENTITYID;

		if (pActInfo && pActInfo->pEntity)
		{
			entityId = pActInfo->pEntity->GetId();
		}

		return entityId;
	}

	void RegisterEvent()
	{
		gEnv->pEntitySystem->AddEntityEventListener(m_entityId, ENTITY_EVENT_ACTIVATE_FLOW_NODE_OUTPUT, this);
		gEnv->pEntitySystem->AddEntityEventListener(m_entityId, ENTITY_EVENT_DONE, this);
	}

	void UnregisterEvent()
	{
		IEntitySystem* pEntitySystem = gEnv->pEntitySystem;
		if (pEntitySystem && m_entityId)
		{
			pEntitySystem->RemoveEntityEventListener(m_entityId, ENTITY_EVENT_ACTIVATE_FLOW_NODE_OUTPUT, this);
			pEntitySystem->RemoveEntityEventListener(m_entityId, ENTITY_EVENT_DONE, this);
		}
	}

	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser) override;

	IFlowGraph*                        m_pGraph;
	TFlowNodeId                        m_nodeID;
	_smart_ptr<CGameEntityNodeFactory> m_pClass;
	int                                m_lastInitializeFrameId;
	EntityId                           m_entityId;
};
