// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PerceptionFlowNodes.h"

#include "PerceptionManager.h"

#include <CryAISystem/IAIActor.h>
#include <CryAISystem/IAIObject.h>

//////////////////////////////////////////////////////////////////////////
// PerceptionState
//////////////////////////////////////////////////////////////////////////

void CFlowNode_PerceptionState::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	switch (event)
	{
	case eFE_Activate:
	{
		if (!pActInfo->pEntity)
			return;

		IAIObject* pAIObject = pActInfo->pEntity->GetAI();
		IAIActor* pAIActor = pAIObject->CastToIAIActor();
		if (pAIActor)
		{
			if (IsPortActive(pActInfo, IN_ENABLE))
			{
				CPerceptionManager::GetInstance()->SetActorState(pAIObject, true);
			}
			else if (IsPortActive(pActInfo, IN_DISABLE))
			{
				CPerceptionManager::GetInstance()->SetActorState(pAIObject, false);
			}
			else if (IsPortActive(pActInfo, IN_RESET))
			{
				CPerceptionManager::GetInstance()->ResetActor(pAIObject);
				pAIActor->ResetPerception();
			}
		}
		break;
	}
	case eFE_Initialize:
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
// AIEventListener
//////////////////////////////////////////////////////////////////////////

CFlowNode_AIEventListener::~CFlowNode_AIEventListener()
{
	if (IPerceptionManager::GetInstance())
	{
		IPerceptionManager::GetInstance()->UnregisterAIStimulusEventListener(functor(*this, &CFlowNode_AIEventListener::OnAIEvent));
	}
}

void CFlowNode_AIEventListener::Serialize(SActivationInfo* pActInfo, TSerialize ser)
{
	ser.Value("pos", m_pos);
	ser.Value("radius", m_radius);
	ser.Value("m_thresholdSound", m_thresholdSound);
	ser.Value("m_thresholdCollision", m_thresholdCollision);
	ser.Value("m_thresholdBullet", m_thresholdBullet);
	ser.Value("m_thresholdExplosion", m_thresholdExplosion);
}

void CFlowNode_AIEventListener::GetConfiguration(SFlowNodeConfig& config)
{
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig_Void("Sound"),
		OutputPortConfig_Void("Collision"),
		OutputPortConfig_Void("Bullet"),
		OutputPortConfig_Void("Explosion"),
		{ 0 }
	};
	static const SInputPortConfig in_config[] = {
		InputPortConfig<Vec3>("Pos",                 _HELP("Position of the listener")),
		InputPortConfig<float>("Radius",             0.0f,                              _HELP("Radius of the listener")),
		InputPortConfig<float>("ThresholdSound",     0.5f,                              _HELP("Sensitivity of the sound output")),
		InputPortConfig<float>("ThresholdCollision", 0.5f,                              _HELP("Sensitivity of the collision output")),
		InputPortConfig<float>("ThresholdBullet",    0.5f,                              _HELP("Sensitivity of the bullet output")),
		InputPortConfig<float>("ThresholdExplosion", 0.5f,                              _HELP("Sensitivity of the explosion output")),
		{ 0 }
	};
	config.sDescription = _HELP("The highest level of alertness of any agent in the level");
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.SetCategory(EFLN_ADVANCED);
}

void CFlowNode_AIEventListener::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	const int stimFlags = (1 << AISTIM_SOUND) | (1 << AISTIM_EXPLOSION) | (1 << AISTIM_BULLET_HIT) | (1 << AISTIM_COLLISION);

	if (event == eFE_Initialize)
	{
		m_pos = GetPortVec3(pActInfo, 0);
		m_radius = GetPortFloat(pActInfo, 1);

		if (IPerceptionManager::GetInstance())
		{
			SAIStimulusEventListenerParams params;
			params.pos = m_pos;
			params.radius = m_radius;
			params.flags = stimFlags;
			IPerceptionManager::GetInstance()->RegisterAIStimulusEventListener(functor(*this, &CFlowNode_AIEventListener::OnAIEvent), params);
		}

		m_thresholdSound = GetPortFloat(pActInfo, 2);
		m_thresholdCollision = GetPortFloat(pActInfo, 3);
		m_thresholdBullet = GetPortFloat(pActInfo, 4);
		m_thresholdExplosion = GetPortFloat(pActInfo, 5);
	}

	if (event == eFE_Activate)
	{
		if (IsPortActive(pActInfo, 0))
		{
			// Change pos
			Vec3 pos = GetPortVec3(pActInfo, 0);
			if (Distance::Point_PointSq(m_pos, pos) > sqr(0.01f))
			{
				m_pos = pos;

				if (IPerceptionManager::GetInstance())
				{
					SAIStimulusEventListenerParams params;
					params.pos = m_pos;
					params.radius = m_radius;
					params.flags = stimFlags;
					IPerceptionManager::GetInstance()->RegisterAIStimulusEventListener(functor(*this, &CFlowNode_AIEventListener::OnAIEvent), params);
				}
			}
		}
		else if (IsPortActive(pActInfo, 1))
		{
			// Change radius
			float rad = GetPortFloat(pActInfo, 1);
			if (fabsf(rad - m_radius) > 0.0001f)
			{
				m_radius = rad;

				if (IPerceptionManager::GetInstance())
				{
					SAIStimulusEventListenerParams params;
					params.pos = m_pos;
					params.radius = m_radius;
					params.flags = stimFlags;
					IPerceptionManager::GetInstance()->RegisterAIStimulusEventListener(functor(*this, &CFlowNode_AIEventListener::OnAIEvent), params);
				}
			}
		}
		else if (IsPortActive(pActInfo, 2))
		{
			// Change threshold
			m_thresholdSound = GetPortFloat(pActInfo, 2);
		}
		else if (IsPortActive(pActInfo, 3))
		{
			// Change threshold
			m_thresholdCollision = GetPortFloat(pActInfo, 3);
		}
		else if (IsPortActive(pActInfo, 4))
		{
			// Change threshold
			m_thresholdBullet = GetPortFloat(pActInfo, 4);
		}
		else if (IsPortActive(pActInfo, 5))
		{
			// Change threshold
			m_thresholdExplosion = GetPortFloat(pActInfo, 5);
		}
	}
}

void CFlowNode_AIEventListener::GetMemoryUsage(ICrySizer * sizer) const
{
	sizer->Add(*this);
}

void CFlowNode_AIEventListener::OnAIEvent(const SAIStimulusParams& params)
{
	CRY_ASSERT(m_pGraph->IsEnabled() && !m_pGraph->IsSuspended() && m_pGraph->IsActive());

	float dist = Distance::Point_Point(m_pos, params.position);
	float u = 0.0f;

	if (params.radius > 0.001f)
		u = (1.0f - clamp_tpl((dist - m_radius) / params.radius, 0.0f, 1.0f)) * params.threat;

	float thr = 0.0f;

	switch (params.type)
	{
	case AISTIM_SOUND:
		thr = m_thresholdSound;
		break;
	case AISTIM_COLLISION:
		thr = m_thresholdCollision;
		break;
	case AISTIM_BULLET_HIT:
		thr = m_thresholdBullet;
		break;
	case AISTIM_EXPLOSION:
		thr = m_thresholdExplosion;
		break;
	default:
		return;
	}

	if (thr > 0.0f && u > thr)
	{
		int i = 0;
		switch (params.type)
		{
		case AISTIM_SOUND:
			i = 0;
			break;
		case AISTIM_COLLISION:
			i = 1;
			break;
		case AISTIM_BULLET_HIT:
			i = 2;
			break;
		case AISTIM_EXPLOSION:
			i = 3;
			break;
		default:
			return;
		}
		SFlowAddress addr(m_nodeID, i, true);
		m_pGraph->ActivatePort(addr, true);
	}
}

REGISTER_FLOW_NODE("AI:PerceptionState", CFlowNode_PerceptionState)
REGISTER_FLOW_NODE("AI:EventListener", CFlowNode_AIEventListener)