// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   FlowEntityNode.h
//  Version:     v1.00
//  Created:     23/5/2005 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __FlowEntityNode_h__
#define __FlowEntityNode_h__
#pragma once

#include "FlowBaseNode.h"

//////////////////////////////////////////////////////////////////////////
class CFlowEntityClass : public IFlowNodeFactory
{
public:
	CFlowEntityClass(IEntityClass* pEntityClass);
	~CFlowEntityClass();
	virtual void         AddRef()  { m_nRefCount++; }
	virtual void         Release() { if (0 == --m_nRefCount) delete this; }
	virtual IFlowNodePtr Create(IFlowNode::SActivationInfo*);

	virtual void         GetMemoryUsage(ICrySizer* s) const
	{
		SIZER_SUBCOMPONENT_NAME(s, "CFlowEntityClass");
		s->AddObject(this, sizeof(*this));
		s->AddObject(m_inputs);
		s->AddObject(m_outputs);
	}

	void        GetConfiguration(SFlowNodeConfig&);

	const char* CopyStr(const char* src);
	void        FreeStr(const char* src);

	size_t      GetNumOutputs() { return m_outputs.size() - 1; }

	void        Reset();

private:
	void GetInputsOutputs(IEntityClass* pEntityClass);
	friend class CFlowEntityNode;

	int                            m_nRefCount;
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

	// Return entity of this node.
	ILINE IEntity* GetEntity() const
	{
		return gEnv->pEntitySystem->GetEntity(m_entityId);
	}

	// Return entityId of this node
	EntityId GetEntityId(SActivationInfo* pActInfo) const
	{
		assert(pActInfo);

		EntityId entityId = 0;

		if (pActInfo->pEntity)
		{
			entityId = pActInfo->pEntity->GetId();
		}

		return entityId;
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
	virtual void OnEntityEvent(IEntity* pEntity, SEntityEvent& event);
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

#endif // __FlowEntityNode_h__
