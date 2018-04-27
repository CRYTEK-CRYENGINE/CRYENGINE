// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include <CryMath/Cry_Math.h>
#include <CryFlowGraph/IFlowBaseNode.h>

//////////////////////////////////////////////////////////////////////////
//! Normalize the val to 0-360 range.
inline float Snap_s360(float val)
{
	if (val < 0.0f)
		val = float(360.0f + fmod_tpl(val, 360.0f));
	else if (val >= 360.0f)
		val = float(fmod_tpl(val, 360.0f));
	return val;
}


class CRotateEntityToExNode : public CFlowBaseNode<eNCT_Instanced>
{
	enum EValueType
	{
		VT_SPEED = 0,
		VT_TIME
	};
	enum ECoordSys
	{
		CS_PARENT = 0,
		CS_WORLD,
		CS_LOCAL
	};

	CTimeValue m_startTime;
	CTimeValue m_endTime;
	CTimeValue m_localStartTime;   // updated every time that the target rotation is dynamically updated in middle of the movement. When there is no update, is equivalent to m_startTime
	Quat       m_targetQuat;
	Quat       m_sourceQuat;
	ECoordSys  m_coorSys;
	EValueType m_valueType;
	bool       m_bIsMoving;

public:
	CRotateEntityToExNode(SActivationInfo* pActInfo)
		: m_coorSys(CS_WORLD),
		m_valueType(VT_SPEED),
		m_bIsMoving(false)
	{
		m_targetQuat.SetIdentity();
		m_sourceQuat.SetIdentity();
	};

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CRotateEntityToExNode(pActInfo);
	}

	void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
		ser.BeginGroup("Local");
		ser.Value("isMoving", m_bIsMoving);
		if (m_bIsMoving)
		{
			ser.Value("startTime", m_startTime);
			ser.Value("endTime", m_endTime);
			ser.Value("localStartTime", m_localStartTime);
			ser.Value("sourceQuat", m_sourceQuat);
			ser.Value("targetQuat", m_targetQuat);
		}
		ser.EndGroup();

		if (ser.IsReading())
		{
			m_coorSys = (ECoordSys)GetPortInt(pActInfo, IN_COORDSYS);
			m_valueType = (EValueType)GetPortInt(pActInfo, IN_VALUETYPE);
		}
	}

	enum EInPorts
	{
		IN_DEST = 0,
		IN_DYN_DEST,
		IN_VALUETYPE,
		IN_VALUE,
		IN_COORDSYS,
		IN_START,
		IN_STOP
	};
	enum EOutPorts
	{
		OUT_CURRENT = 0,
		OUT_CURRENT_RAD,
		OUT_START,
		OUT_STOP,
		OUT_FINISH,
		OUT_DONE
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<Vec3>("Destination",   _HELP("Destination (degrees on each axis)")),
			InputPortConfig<bool>("DynamicUpdate", true,                                        _HELP("If dynamic update of Destination [follow-during-rotation] is allowed or not"),       _HELP("DynamicUpdate")),
			InputPortConfig<int>("ValueType",      0,                                           _HELP("Defines if the 'Value' input will be interpreted as speed or as time (duration)."),  _HELP("ValueType"),     _UICONFIG("enum_int:speed=0,time=1")),
			InputPortConfig<float>("Value",        0.f,                                         _HELP("Speed (Degrees/sec) or Time (duration in secs), depending on 'ValueType' input.")),
			InputPortConfig<int>("CoordSys",       0,                                           _HELP("Destination in Parent relative, World (absolute) or Local relative angles"),         _HELP("CoordSys"),      _UICONFIG("enum_int:Parent=0,World=1,Local=2")),
			InputPortConfig_Void("Start",          _HELP("Trigger this to start the movement")),
			InputPortConfig_Void("Stop",           _HELP("Trigger this to stop the movement")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<Vec3>("Current",    _HELP("Current Rotation in Degrees")),
			OutputPortConfig<Vec3>("CurrentRad", _HELP("Current Rotation in Radian")),
			OutputPortConfig_Void("Start",       _HELP("Triggered when start or reset inputs are triggered")),
			OutputPortConfig_Void("Stop",        _HELP("Triggered when stop input is triggered")),
			OutputPortConfig_Void("Finish",      _HELP("Triggered when destination is reached")),
			OutputPortConfig_Void("Done",        _HELP("Triggered when destination rotation is reached or rotation stopped")),
			{ 0 }
		};
		config.sDescription = _HELP("Rotate an entity during a defined period of time or with a defined speed");
		config.nFlags |= EFLN_TARGET_ENTITY | EFLN_AISEQUENCE_SUPPORTED;
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	void DegToQuat(const Vec3& rotation, Quat& destQuat)
	{
		Ang3 temp;

		temp.x = Snap_s360(rotation.x);
		temp.y = Snap_s360(rotation.y);
		temp.z = Snap_s360(rotation.z);
		const Ang3 angRad = DEG2RAD(temp);
		destQuat.SetRotationXYZ(angRad);
	}

	void SetRawEntityRot(IEntity* pEntity, const Quat& quat)
	{
		if (m_coorSys == CS_WORLD && pEntity->GetParent())
		{
			Matrix34 tm(quat);
			tm.SetTranslation(pEntity->GetWorldPos());
			pEntity->SetWorldTM(tm);
		}
		else
			pEntity->SetRotation(quat);
	}

	void UpdateCurrentRotOutputs(SActivationInfo* pActInfo, const Quat& quat)
	{
		Ang3 angles(quat);
		Ang3 anglesDeg = RAD2DEG(angles);
		ActivateOutput(pActInfo, OUT_CURRENT_RAD, Vec3(angles));
		ActivateOutput(pActInfo, OUT_CURRENT, Vec3(anglesDeg));
	}

	void PhysicStop(IEntity* pEntity, const Quat* forcedOrientation = NULL)
	{
		if (pEntity->GetPhysics())
		{
			pe_action_set_velocity asv;
			asv.w.zero();
			asv.bRotationAroundPivot = 1;
			pEntity->GetPhysics()->Action(&asv);

			pe_params_pos pos;
			pos.pos = pEntity->GetWorldPos();
			if (forcedOrientation == NULL)
			{
				// This might be one frame behind?
				pos.q = pEntity->GetWorldRotation();
			}
			else
			{
				pos.q = *forcedOrientation;
			}
			pEntity->GetPhysics()->SetParams(&pos);
		}
	}

	void PhysicSetApropiateSpeed(IEntity* pEntity)
	{
		if (pEntity->GetPhysics())
		{
			Quat targetQuat = (m_sourceQuat | m_targetQuat) < 0 ? -m_targetQuat : m_targetQuat;
			Vec3 rotVel = Quat::log(targetQuat * !m_sourceQuat) * (2.0f / max(0.01f, (m_endTime - m_localStartTime).GetSeconds()));

			pe_action_set_velocity asv;
			asv.w = rotVel;
			asv.bRotationAroundPivot = 1;
			pEntity->GetPhysics()->Action(&asv);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			{
				m_coorSys = (ECoordSys)GetPortInt(pActInfo, IN_COORDSYS);
				m_valueType = (EValueType)GetPortInt(pActInfo, IN_VALUETYPE);
				pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
				m_bIsMoving = false;
				break;
			}

		case eFE_Activate:
			{
				if (!pActInfo->pEntity)
					break;

				if (IsPortActive(pActInfo, IN_STOP))
				{
					pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
					ActivateOutput(pActInfo, OUT_DONE, true);
					ActivateOutput(pActInfo, OUT_STOP, true);
					PhysicStop(pActInfo->pEntity);
					m_bIsMoving = false;
				}
				// when the target is updated in middle of the movement, we can not just continue the interpolation only changing the target because that would cause some snaps.
				// so we do sort of a new start but keeping the initial time into account, so we can still finish at the right time.
				if (IsPortActive(pActInfo, IN_DEST) && GetPortBool(pActInfo, IN_DYN_DEST) == true && m_bIsMoving)
				{
					ReadSourceAndTargetQuats(pActInfo);
					m_localStartTime = gEnv->pTimer->GetFrameStartTime();
					CalcEndTime(pActInfo);
					if (m_valueType == VT_TIME)
					{
						PhysicSetApropiateSpeed(pActInfo->pEntity);
					}
				}
				if (IsPortActive(pActInfo, IN_START))
				{
					bool triggerStartOutput = true;
					Start(pActInfo, triggerStartOutput);
				}
				if (IsPortActive(pActInfo, IN_VALUE))
				{
					if (m_bIsMoving)
					{
						bool triggerStartOutput = false;
						Start(pActInfo, triggerStartOutput);
					}
				}

				// we dont support dynamic change of those inputs
				assert(!IsPortActive(pActInfo, IN_COORDSYS));
				assert(!IsPortActive(pActInfo, IN_VALUETYPE));

				break;
			}

		case eFE_Update:
			{
				if (!pActInfo->pEntity)
				{
					pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
					m_bIsMoving = false;
					break;
				}

				CTimeValue curTime = gEnv->pTimer->GetFrameStartTime();
				const float fDuration = (m_endTime - m_localStartTime).GetSeconds();
				float fPosition;
				if (fDuration <= 0.0)
					fPosition = 1.0;
				else
				{
					fPosition = (curTime - m_localStartTime).GetSeconds() / fDuration;
					fPosition = clamp_tpl(fPosition, 0.0f, 1.0f);
				}
				if (curTime >= m_endTime)
				{
					if (m_bIsMoving)
					{
						m_bIsMoving = false;
						ActivateOutput(pActInfo, OUT_DONE, true);
						ActivateOutput(pActInfo, OUT_FINISH, true);
					}

					// By delaying the shutdown of regular updates, we generate
					// the physics/entity stop + snap request multiple times. This seems
					// to fix a weird glitch in Jailbreak whereby doors sometimes seem to
					// overshoot their end orientation (physics proxies seem to be correct
					// but the render proxies are off).
					static const float s_Crysis3HacDeactivateDelay = 1.0f;
					if (curTime >= (m_endTime + s_Crysis3HacDeactivateDelay))
					{
						pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
					}
					SetRawEntityRot(pActInfo->pEntity, m_targetQuat);
					PhysicStop(pActInfo->pEntity, &m_targetQuat);
					UpdateCurrentRotOutputs(pActInfo, m_targetQuat);
				}
				else
				{
					Quat quat;
					quat.SetSlerp(m_sourceQuat, m_targetQuat, fPosition);
					SetRawEntityRot(pActInfo->pEntity, quat);
					UpdateCurrentRotOutputs(pActInfo, quat);
				}
				break;
			}
		}
	}

	void Start(SActivationInfo* pActInfo, bool triggerStartOutput)
	{
		m_bIsMoving = true;

		ReadSourceAndTargetQuats(pActInfo);

		m_startTime = gEnv->pTimer->GetFrameStartTime();
		m_localStartTime = m_startTime;
		CalcEndTime(pActInfo);

		PhysicSetApropiateSpeed(pActInfo->pEntity);

		pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
		if (triggerStartOutput)
			ActivateOutput(pActInfo, OUT_START, true);
	}

	void ReadSourceAndTargetQuats(SActivationInfo* pActInfo)
	{
		m_sourceQuat = m_coorSys == CS_WORLD ? pActInfo->pEntity->GetWorldRotation() : pActInfo->pEntity->GetRotation();
		m_sourceQuat.Normalize();

		const Vec3& destRotDeg = GetPortVec3(pActInfo, IN_DEST);
		if (m_coorSys == CS_LOCAL)
		{
			Quat rotQuat;
			DegToQuat(destRotDeg, rotQuat);

			m_targetQuat = m_sourceQuat * rotQuat;
		}
		else
		{
			DegToQuat(destRotDeg, m_targetQuat);
		}
	}

	void CalcEndTime(SActivationInfo* pActInfo)
	{
		if (m_valueType == VT_TIME)
		{
			CTimeValue timePast = m_localStartTime - m_startTime;
			float duration = GetPortFloat(pActInfo, IN_VALUE) - timePast.GetSeconds();
			m_endTime = m_startTime + duration;
		}
		else // when is speed-driven, we just calculate the apropiate endTime and from that on, everything else is the same
		{
			Quat qOriInv = m_sourceQuat.GetInverted();
			Quat qDist = qOriInv * m_targetQuat;
			qDist.NormalizeSafe();

			float angDiff = RAD2DEG(2 * acosf(qDist.w));
			if (angDiff > 180.f)
			{
				angDiff = 360.0f - angDiff;
			}

			float desiredSpeed = fabsf(GetPortFloat(pActInfo, IN_VALUE));
			if (desiredSpeed < 0.0001f)
				desiredSpeed = 0.0001f;
			m_endTime = m_localStartTime + angDiff / desiredSpeed;
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

};

REGISTER_FLOW_NODE("Movement:RotateEntityToEx", CRotateEntityToExNode);
