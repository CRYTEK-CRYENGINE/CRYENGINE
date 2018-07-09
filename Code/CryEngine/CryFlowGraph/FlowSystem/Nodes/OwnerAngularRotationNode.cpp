// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include <CryFlowGraph/IFlowBaseNode.h>

class CRotateEntity : public CFlowBaseNode<eNCT_Instanced>
{
	Quat       m_worldRot;
	Quat       m_localRot;
	CTimeValue m_lastTime;
	bool       m_bActive;

public:
	CRotateEntity(SActivationInfo* pActInfo) : m_lastTime(0.0f), m_bActive(false)
	{
		m_worldRot.SetIdentity();
		m_localRot.SetIdentity();
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CRotateEntity(pActInfo);
	}

	void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
		ser.BeginGroup("Local");
		ser.Value("m_worldRot", m_worldRot);
		ser.Value("m_localRot", m_localRot);
		ser.Value("m_lastTime", m_lastTime);
		ser.Value("m_bActive", m_bActive);
		ser.EndGroup();
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<Vec3>("Speed",  _HELP("Speed (degrees/sec)")),
			InputPortConfig<bool>("Paused", _HELP("Pause the motion when true")),
			InputPortConfig<int>("Ref",     0,                                   _HELP("RotationAxis in World or Local Space"),_HELP("CoordSys"), _UICONFIG("enum_int:World=0,Local=1")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<Vec3>("Current",    _HELP("Current Rotation in Degrees")),
			OutputPortConfig<Vec3>("CurrentRad", _HELP("Current Rotation in Radian")),
			{ 0 }
		};
		config.sDescription = _HELP("Rotate at a defined speed");
		config.nFlags |= EFLN_TARGET_ENTITY | EFLN_AISEQUENCE_SUPPORTED;
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			{
				m_lastTime = gEnv->pTimer->GetFrameStartTime();
				m_bActive = !GetPortBool(pActInfo, 1);
				pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, m_bActive);

				if (!m_bActive)
				{
					ResetVelocity(pActInfo);
				}
			}

			break;
		case eFE_Initialize:
			{
				if (pActInfo->pEntity)
					m_localRot = pActInfo->pEntity->GetRotation();
				else
					m_localRot.SetIdentity();
				m_worldRot.SetIdentity();
				m_lastTime = gEnv->pTimer->GetFrameStartTime();
				m_bActive = !GetPortBool(pActInfo, 1);
				pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, m_bActive);
				break;
			}

		case eFE_Update:
			{
				if (m_bActive && pActInfo->pEntity)
				{
					CTimeValue time = gEnv->pTimer->GetFrameStartTime();
					float timeDifference = (time - m_lastTime).GetSeconds();
					m_lastTime = time;

					IEntity* pEntity = pActInfo->pEntity;
					const bool bUseWorld = GetPortInt(pActInfo, 2) == 0 ? true : false;
					Vec3 speed = GetPortVec3(pActInfo, 0);
					speed *= timeDifference;
					Quat deltaRot(Ang3(DEG2RAD(speed)));
					Quat finalRot;
					if (bUseWorld == false)
					{
						finalRot = pEntity->GetRotation();
						finalRot *= deltaRot;
					}
					else
					{
						m_worldRot *= deltaRot;
						finalRot = m_worldRot;
						finalRot *= m_localRot;
					}
					finalRot.NormalizeSafe();

					IPhysicalEntity* piPhysEnt = pEntity->GetPhysics();
					if (piPhysEnt && piPhysEnt->GetType() != PE_STATIC)
					{
						if (timeDifference > 0.0001f)
						{
							pe_action_set_velocity asv;
							asv.w = Quat::log(deltaRot) * (2.f / timeDifference);
							asv.bRotationAroundPivot = 1;
							pEntity->GetPhysics()->Action(&asv);
						}
					}
					else
					{
						pEntity->SetRotation(finalRot);
					}
					Ang3 currentAng = Ang3(finalRot);
					ActivateOutput(pActInfo, 1, Vec3(currentAng));
					currentAng = RAD2DEG(currentAng);
					ActivateOutput(pActInfo, 0, Vec3(currentAng));
				}
				break;
			}
		}
		;
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

private:

	void ResetVelocity(SActivationInfo* pActInfo)
	{
		IEntity* pGraphEntity = pActInfo->pEntity;
		if (pGraphEntity)
		{
			IPhysicalEntity* pPhysicalEntity = pGraphEntity->GetPhysics();
			if (pPhysicalEntity && pPhysicalEntity->GetType() != PE_STATIC)
			{
				pe_action_set_velocity setVel;
				setVel.v.zero();
				setVel.w.zero();
				pPhysicalEntity->Action(&setVel);
			}
		}
	}
};

REGISTER_FLOW_NODE("Movement:RotateEntity", CRotateEntity);
