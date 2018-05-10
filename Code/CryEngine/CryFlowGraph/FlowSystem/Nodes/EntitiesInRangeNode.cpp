// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

////////////////////////////////////////////////////////////////////////////
//
// -------------------------------------------------------------------------
//  File name:   EntitiesInRange.cpp
//  Version:     v1.00
//  Created:     Oct 24th 2005 by AlexL
//  Compilers:   Visual Studio.NET 2003
//  Description:
// -------------------------------------------------------------------------
//  History:
//    - Sept. 2010      Jens Schöbel added Output for square dist and vector
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include <CryFlowGraph/IFlowBaseNode.h>

class CFlowNode_EntitiesInRange : public CFlowBaseNode<eNCT_Instanced>
{
public:
	CFlowNode_EntitiesInRange(SActivationInfo* pActInfo)
		: m_errorLogSent(false)
	{
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNode_EntitiesInRange(pActInfo);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig_Void("Trigger",      _HELP("Trigger this port to check if entities are in range")),
			InputPortConfig<EntityId>("Entity1", _HELP("Entity 1")),
			InputPortConfig<EntityId>("Entity2", _HELP("Entity 2")),
			InputPortConfig<float>("Range",      1.0f,                                                         _HELP("Range to check")),
			{ 0 }
		};
		static const SOutputPortConfig outputs[] = {
			OutputPortConfig<bool>("InRange",   _HELP("True if Entities are in Range")),
			OutputPortConfig_Void("False",      _HELP("Triggered if Entities are not in Range")),
			OutputPortConfig_Void("True",       _HELP("Triggered if Entities are in Range")),
			OutputPortConfig<float>("Distance", _HELP("Distance between the two entities.")),
			OutputPortConfig<Vec3>("DistVec",   _HELP("Vector from the first to the second entity")),
			{ 0 }
		};
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.nFlags |= EFLN_AISEQUENCE_SUPPORTED;
		config.sDescription = _HELP("Check if two entities are in range, activated by Trigger");
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			m_errorLogSent = false;
			break;

		case eFE_Activate:
			if (IsPortActive(pActInfo, 0))
			{
				IEntitySystem* pESys = gEnv->pEntitySystem;
				EntityId id1 = GetPortEntityId(pActInfo, 1);
				EntityId id2 = GetPortEntityId(pActInfo, 2);
				IEntity* pEnt1 = pESys->GetEntity(id1);
				IEntity* pEnt2 = pESys->GetEntity(id2);
				IEntity* pGraphEntity = pESys->GetEntity(pActInfo->pGraph->GetGraphEntity(0));
				if (pEnt1 == NULL || pEnt2 == NULL)
				{
					if (!m_errorLogSent)
					{
						GameWarning("[flow] Entity::EntitiesInRange - flowgraph entity: %d:'%s' - at least one of the input entities is invalid!. Entity1: %d:'%s'    Entity2:  %d:'%s'",
						            pActInfo->pGraph->GetGraphEntity(0), pGraphEntity ? pGraphEntity->GetName() : "<NULL>",
						            id1, pEnt1 ? pEnt1->GetName() : "<NULL>", id2, pEnt2 ? pEnt2->GetName() : "<NULL>");
						m_errorLogSent = true;
					}
				}
				else
				{
					const float range = GetPortFloat(pActInfo, 3);
					const float distance = pEnt1->GetWorldPos().GetDistance(pEnt2->GetWorldPos());
					const bool inRange = (distance <= range);
					ActivateOutput(pActInfo, 0, inRange);
					ActivateOutput(pActInfo, 1 + (inRange ? 1 : 0), true);
					ActivateOutput(pActInfo, 3, distance);

					const Vec3 distVector = pEnt2->GetWorldPos() - pEnt1->GetWorldPos();
					ActivateOutput(pActInfo, 4, distVector);
					m_errorLogSent = false;
				}
			}
			break;
		}
	}

private:
	bool m_errorLogSent;
};

REGISTER_FLOW_NODE("Entity:EntitiesInRange", CFlowNode_EntitiesInRange);
