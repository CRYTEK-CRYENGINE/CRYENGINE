// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   History:
   - 11:05:2010   Created by Ian Masters
*************************************************************************/

#include "StdAfx.h"

#include <CrySystem/ISystem.h>
#include <IWorldQuery.h>
#include <CryFlowGraph/IFlowBaseNode.h>

class CPressureWaveNode : public CFlowBaseNode<eNCT_Instanced>
{
	enum INPUTS
	{
		EIP_Trigger = 0,
		EIP_Normal,
		EIP_Speed,
		EIP_Force,
		EIP_Amplitude,
		EIP_Decay,
		EIP_RangeMin,
		EIP_RangeMax,
		EIP_Duration,
		EIP_Stop,
	};

	enum OUTPUTS
	{
		EOP_Time = 0,
	};

public:
	CPressureWaveNode(SActivationInfo* pActInfo)
		: m_triggered(false)
	{
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	~CPressureWaveNode()
	{
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CPressureWaveNode(pActInfo);
	}

	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
		ser.BeginGroup("Local");

		ser.Value("m_triggered", m_triggered);

		int64 time;

		time = m_startTime.GetValue();
		ser.Value("m_startTime", time);
		if (ser.IsReading()) m_startTime.SetValue(time);

		time = m_currentTime.GetValue();
		ser.Value("m_currentTime", time);
		if (ser.IsReading()) m_currentTime.SetValue(time);

		if (ser.IsReading())
		{
			if (m_triggered)
			{
				m_actInfo = *pActInfo;
				pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
			}
		}

		ser.EndGroup();
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("Trigger",     _HELP("Trigger the effect")),
			InputPortConfig<Vec3>("Normal",     Vec3(0.0f,                            0.0f,                                                                         1.0f), _HELP("Up normal of effect")),
			InputPortConfig<float>("Speed",     15.0f,                                _HELP("Initial speed of the effect radiating outward")),
			InputPortConfig<float>("Force",     1.0f,                                 _HELP("Outward force affecting entities")),
			InputPortConfig<float>("Amplitude", 30.0f,                                _HELP("Force affecting entities on along the normal")),
			InputPortConfig<float>("Decay",     1.0f,                                 _HELP("Decay over duration, 0 = none, 1 = linear, 2 = squared, 3 = cubic")),
			InputPortConfig<float>("RangeMin",  0.0f,                                 _HELP("Minimum effect range")),
			InputPortConfig<float>("RangeMax",  10.0f,                                _HELP("Maximum effect range")),
			InputPortConfig<float>("Duration",  1.0f,                                 _HELP("Duration of the effect in seconds")),
			InputPortConfig_Void("Stop",        _HELP("Stop the effect immediately")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<float>("Time", _HELP("Current time of the effect")),
			{ 0 }
		};
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.sDescription = _HELP("Plays a force wave that affects physicalized objects");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			{
				m_actInfo = *pActInfo;
				m_entitiesAffected.reserve(50);
				break;
			}

		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, EIP_Trigger))
				{
					if (pActInfo->pEntity)
					{
						if (!m_triggered)
						{
							pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
							m_triggered = true;
						}
						m_startTime = gEnv->pTimer->GetFrameStartTime();
						m_currentTime = m_startTime;
						m_entitiesAffected.resize(0);

						m_effectCenter = pActInfo->pEntity->GetWorldPos();
						Update(0.0f);
					}
				}
				else if (IsPortActive(pActInfo, EIP_Stop))
				{
					pActInfo->pGraph->SetRegularlyUpdated(m_actInfo.myID, false);
					m_triggered = false;
				}
				break;
			}

		case eFE_Update:
			{
				m_currentTime = gEnv->pTimer->GetFrameStartTime();
				float elapsed = m_currentTime.GetSeconds() - m_startTime.GetSeconds();
				Update(elapsed);
				ActivateOutput(&m_actInfo, EOP_Time, elapsed);
				break;
			}
		}
	}

	void Update(float elapsed)
	{
		float maxTime = GetPortFloat(&m_actInfo, EIP_Duration);
		float percent = maxTime > FLT_EPSILON ? elapsed / maxTime : 1.0f;
		if (percent >= 1.0f)
		{
			m_actInfo.pGraph->SetRegularlyUpdated(m_actInfo.myID, false);
			m_triggered = false;
			return;
		}

		Vec3 N = GetPortVec3(&m_actInfo, EIP_Normal);

		float rangeMin = GetPortFloat(&m_actInfo, EIP_RangeMin);
		float rangeMax = GetPortFloat(&m_actInfo, EIP_RangeMax);
		const float range = rangeMax - rangeMin;
		Vec3 boxDim(rangeMax, rangeMax, rangeMax);
		Vec3 ptmin = m_effectCenter - boxDim;
		Vec3 ptmax = m_effectCenter + boxDim;

		float speed = GetPortFloat(&m_actInfo, EIP_Speed);
		float waveFront = elapsed * speed;

		float decay = GetPortFloat(&m_actInfo, EIP_Decay);
		if (decay > FLT_EPSILON) decay = 1.0f / decay;

		float force = GetPortFloat(&m_actInfo, EIP_Force);
		force = pow_tpl(force * (1 - percent), decay);

		float amplitude = GetPortFloat(&m_actInfo, EIP_Amplitude);
		amplitude = pow_tpl(amplitude * (1 - percent), decay);

		if (gEnv->bMultiplayer) // Turned off for performance and network issues
		{
			return;
		}

		IPhysicalEntity** pEntityList = NULL;
		static const int iObjTypes = ent_rigid | ent_sleeping_rigid | ent_living;// | ent_allocate_list;
		int numEntities = gEnv->pPhysicalWorld->GetEntitiesInBox(ptmin, ptmax, pEntityList, iObjTypes);
		AABB bounds;
		for (int i = 0; i < numEntities; ++i)
		{
			IPhysicalEntity* pPhysicalEntity = pEntityList[i];
			IEntity* pEntity = static_cast<IEntity*>(pPhysicalEntity->GetForeignData(PHYS_FOREIGN_ID_ENTITY));

			// Has the entity already been affected?
			if (pEntity)
			{
				bool affected = stl::find(m_entitiesAffected, pEntity->GetId());
				if (!affected)
				{
					{
						pEntity->GetPhysicsWorldBounds(bounds);
						Vec3 p = bounds.GetCenter();
						Vec3 v = p - m_effectCenter;
						float distFromCenter = v.GetLength() + FLT_EPSILON;
						if (distFromCenter < rangeMax)
						{
							if (waveFront > distFromCenter) // has the wavefront passed the entity?
							{
								//pPhysicalEntity->GetStatus(&dyn);

								// normalize v, cheaper than another sqrt
								v /= distFromCenter;

								Vec3 dir = N + v * force;
								static bool usePos = false;
								float impulse = 1.0f - (max(0.0f, distFromCenter - rangeMin) / range);
								impulse = impulse * amplitude;// / dyn.mass;
								if (impulse > FLT_EPSILON)
								{
									pEntity->AddImpulse(-1, p, dir * impulse, usePos, 1.0f);
									m_entitiesAffected.push_back(pEntity->GetId());
								}
							}
						}
					}
				}
			}
		}
	}

private:

	SActivationInfo       m_actInfo;
	CTimeValue            m_startTime;
	CTimeValue            m_currentTime;
	bool                  m_triggered;
	Vec3                  m_effectCenter;
	std::vector<EntityId> m_entitiesAffected; // this would be faster with a set, but someone (no names!) will whinge about fragmentation
};

REGISTER_FLOW_NODE("Physics:PressureWave", CPressureWaveNode);
