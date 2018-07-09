// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Game.h"
#include "Player.h"

#include "Nodes/GameFlowBaseNode.h"
#include "Actor2/Actor2.h"
#include "Actor2/ComponentDataRegistry.h"
#include "Actor2/Component_Coordination_SimpleNavigation.h"
#include "EngineFacade/GameFacade.h"

class CFlowSmartObjectNode : public CGameFlowBaseNode, public IActor2Listener
{
public:
	CFlowSmartObjectNode( SActivationInfo * pActInfo ) 
	: m_smartObjectId (0)
	{
	}

	~CFlowSmartObjectNode()
	{
		RemoveAsActor2Listener();
	}

	IFlowNodePtr Clone( SActivationInfo * pActInfo )
	{
		return new CFlowSmartObjectNode(pActInfo);
	}

	enum EInputPorts
	{
	};

	enum EOutputPorts
	{
		EOP_Triggered = 0,
		EOP_TriggeredReverse,
		EOP_UserId,
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			{0}
		};
		static const SOutputPortConfig outputs[] = {
			OutputPortConfig_Void  ("Triggered", _HELP("Triggered when the smartobject is used")),
			OutputPortConfig_Void  ("Triggered_Reverse", _HELP("Triggered when the smartobject is used")),
			OutputPortConfig<EntityId> ("UserId", _HELP("Entity that is using the smartobject")),
			{0}
		};
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("SmartObject Node");
		config.SetCategory(EFLN_APPROVED);
	}
	
	CActor2* GetActor( EntityId entityId )
	{
		return g_pGame->GetEnvironment().GetGame().GetActor2( ComponentEntityID( entityId ));
	}
	

	void RemoveAsActor2Listener()
	{
		CActor2* pActor = GetActor( m_smartObjectId );
		if (pActor)
		{
			pActor->UnregisterListener( this );
		}
		m_smartObjectId = 0;
	}

	void AddAsActor2Listener()
	{
		CActor2* pActor = GetActor( m_smartObjectId );
		if (pActor)
		{
			pActor->RegisterListener( this );
		}
	}

	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		switch (event)
		{
			case eFE_Initialize:
			case eFE_SetEntityId:
			{
				RemoveAsActor2Listener();
				m_smartObjectId = pActInfo->pEntity ? pActInfo->pEntity->GetId() : 0;
				m_actInfo = *pActInfo;
				AddAsActor2Listener();
			}

			case eFE_Activate:
			{
			}
			break;
		}
	}
	
	void Actor2ListenerNotify( const SSmartObjectUseEvent& info )
	{
		if (info.m_isReversePath)
			ActivateOutput(&m_actInfo, EOP_TriggeredReverse, true);
		else
			ActivateOutput(&m_actInfo, EOP_Triggered, true);
		EntityId entityId = info.m_userID;
		ActivateOutput(&m_actInfo, EOP_UserId, entityId );
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

	SActivationInfo m_actInfo;
	EntityId        m_smartObjectId;
};


REGISTER_FLOW_NODE("Actor2:SmartObject", CFlowSmartObjectNode);

