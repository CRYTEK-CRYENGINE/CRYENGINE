// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include <CryFlowGraph/IFlowBaseNode.h>

class CMoveEntityTo : public CFlowBaseNode<eNCT_Instanced>
{
	enum EInPorts
	{
		IN_DEST = 0,
		IN_DYN_DEST,
		IN_VALUETYPE,
		IN_VALUE,
		IN_EASEIN,
		IN_EASEOUT,
		IN_COORDSYS,
		IN_START,
		IN_STOP
	};

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
	enum EOutPorts
	{
		OUT_CURRENT = 0,
		OUT_START,
		OUT_STOP,
		OUT_FINISH,
		OUT_DONE
	};

	Vec3       m_position;
	Vec3       m_destination;
	Vec3       m_startPos; // position the entity had when Start was last triggered
	float      m_lastFrameTime;
	float      m_topSpeed;
	float      m_easeOutDistance;
	float      m_easeInDistance;
	float      m_startTime;
	ECoordSys  m_coorSys;
	EValueType m_valueType;
	bool       m_bActive;
	bool       m_stopping; // this is used as a workaround for a physics problem: when the velocity is set to 0, the object still can move, for 1 more frame, with the previous velocity.
	// calling Action() with threadSafe=1 does not solve that.
	// TODO: im not sure about the whole "stopping" thing. why just 1 frame? it probably could be even more than 1 in some situations, or 0. not changing it now, but keep an eye on it.
	bool               m_bForceFinishAsTooNear; // force finish when the target is too near. This avoids a problem with rapidly moving target position that is updated on every frame, when a fixed time is specified for the movement
	bool               m_bUsePhysicUpdate;
	static const float EASE_MARGIN_FACTOR;

public:
	CMoveEntityTo(SActivationInfo* pActInfo)
		: m_position(ZERO),
		m_destination(ZERO),
		m_startPos(ZERO),
		m_lastFrameTime(0.0f),
		m_topSpeed(0.0f),
		m_easeOutDistance(0.0f),
		m_easeInDistance(0.0f),
		m_coorSys(CS_WORLD),
		m_valueType(VT_SPEED),
		m_startTime(0.f),
		m_bActive(false),
		m_stopping(false),
		m_bForceFinishAsTooNear(false),
		m_bUsePhysicUpdate(false)
	{
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CMoveEntityTo(pActInfo);
	}

	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
		ser.BeginGroup("Local");
		ser.Value("Active", m_bActive);
		if (m_bActive)
		{
			ser.Value("position", m_position);
			ser.Value("destination", m_destination);
			ser.Value("startPos", m_startPos);
			ser.Value("lastFrameTime", m_lastFrameTime);
			ser.Value("topSpeed", m_topSpeed);
			ser.Value("easeInDistance", m_easeInDistance);
			ser.Value("easeOutDistance", m_easeOutDistance);
			ser.Value("stopping", m_stopping);
			ser.Value("startTime", m_startTime);
			ser.Value("bForceFinishAsTooNear", m_bForceFinishAsTooNear);
		}
		ser.EndGroup();

		if (ser.IsReading())
		{
			m_coorSys = (ECoordSys)GetPortInt(pActInfo, IN_COORDSYS);
			m_valueType = (EValueType)GetPortInt(pActInfo, IN_VALUETYPE);
		}
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] =
		{
			InputPortConfig<Vec3>("Destination",      _HELP("Destination")),
			InputPortConfig<bool>("DynamicUpdate",    true,                                             _HELP("If dynamic update of Destination [follow-during-movement] is allowed or not"),                                  _HELP("DynamicUpdate")),
			InputPortConfig<int>("ValueType",         0,                                                _HELP("Defines if the 'Value' input will be interpreted as speed or as time (duration)."),                             _HELP("ValueType"),      _UICONFIG("enum_int:speed=0,time=1")),
			InputPortConfig<float>("Speed",           0.f,                                              _HELP("Speed (m/sec) or time (duration in secs), depending on 'ValueType' input."),                                    _HELP("Value")),
			InputPortConfig<float>("EaseDistance",    0.f,                                              _HELP("Distance from destination at which the moving entity starts slowing down (0=no slowing down)"),                 _HELP("EaseInDistance")),
			InputPortConfig<float>("EaseOutDistance", 0.f,                                              _HELP("Distance from destination at which the moving entity starts slowing down (0=no slowing down)")),
			InputPortConfig<int>("CoordSys",          0,                                                _HELP("Destination in world, local or parent space (warning : parent space doesn't work if your parent is a brush)"),  _HELP("CoordSys"),       _UICONFIG("enum_int:Parent=0,World=1,Local=2")),
			InputPortConfig_Void("Start",             _HELP("Trigger this port to start the movement")),
			InputPortConfig_Void("Stop",              _HELP("Trigger this port to stop the movement")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] =
		{
			OutputPortConfig<Vec3>("Current",    _HELP("Current position")),
			OutputPortConfig_Void("Start",       _HELP("Triggered when start input is triggered")),
			OutputPortConfig_Void("Stop",        _HELP("Triggered when stop input is triggered")),
			OutputPortConfig_Void("Finish",      _HELP("Triggered when destination is reached")),
			OutputPortConfig_Void("DoneTrigger", _HELP("Triggered when destination is reached or stop input is triggered"),_HELP("Done")),
			{ 0 }
		};
		config.sDescription = _HELP("Move an entity to a destination position at a defined speed or in a defined interval of time");
		config.nFlags |= EFLN_TARGET_ENTITY | EFLN_AISEQUENCE_SUPPORTED;
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	//////////////////////////////////////////////////////////////////////////
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			{
				OnActivate(pActInfo);
				break;
			}
		case eFE_Initialize:
			{
				OnInitialize(pActInfo);
				break;
			}

		case eFE_Update:
			{
				OnUpdate(pActInfo);
				break;
			}
		}
		;
	};

	//////////////////////////////////////////////////////////////////////////
	void OnActivate(SActivationInfo* pActInfo)
	{
		// update destination only if dynamic update is enabled. otherwise destination is read on Start/Reset only
		if (m_bActive && IsPortActive(pActInfo, IN_DEST) && GetPortBool(pActInfo, IN_DYN_DEST) == true)
		{
			ReadDestinationPosFromInput(pActInfo);
			if (m_valueType == VT_TIME)
				CalcSpeedFromTimeInput(pActInfo);
		}
		if (m_bActive && IsPortActive(pActInfo, IN_VALUE))
		{
			ReadSpeedFromInput(pActInfo);
		}
		if (IsPortActive(pActInfo, IN_START))
		{
			Start(pActInfo);
		}
		if (IsPortActive(pActInfo, IN_STOP))
		{
			Stop(pActInfo);
		}

		// we dont support dynamic change of those inputs
		assert(!IsPortActive(pActInfo, IN_COORDSYS));
		assert(!IsPortActive(pActInfo, IN_VALUETYPE));
	}

	//////////////////////////////////////////////////////////////////////////
	void OnInitialize(SActivationInfo* pActInfo)
	{
		m_bActive = false;
		m_position = ZERO;
		m_coorSys = (ECoordSys)GetPortInt(pActInfo, IN_COORDSYS);
		m_valueType = (EValueType)GetPortInt(pActInfo, IN_VALUETYPE);
		IEntity* pEnt = pActInfo->pEntity;
		if (pEnt)
			m_position = pEnt->GetWorldPos();
		ActivateOutput(pActInfo, OUT_CURRENT, m_position);  // i dont see a sense for this, but lets keep it for now
		pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
	}

	//////////////////////////////////////////////////////////////////////////
	void OnUpdate(SActivationInfo* pActInfo)
	{
		IEntity* pEnt = pActInfo->pEntity;
		if (!pEnt)
		{
			pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
			return;
		}
		IPhysicalEntity* pPhysEnt = m_bUsePhysicUpdate ? pEnt->GetPhysics() : NULL;

		if (m_stopping)
		{
			m_stopping = false;
			pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);

			if (pPhysEnt == NULL)
			{
				SetPos(pActInfo, m_destination);
			}

			m_bActive = false;
			return;
		}

		if (!m_bActive)
		{
			return;
		}
		float time = gEnv->pTimer->GetFrameStartTime().GetSeconds();
		float timeDifference = time - m_lastFrameTime;
		m_lastFrameTime = time;

		// note - if there's no physics then this will be the same, but if the platform is moved through physics, then
		//        we have to get back the actual movement - this maybe framerate dependent.
		m_position = pActInfo->pEntity->GetWorldPos();

		// let's compute the movement vector now
		Vec3 oldPosition = m_position;

		const float valuesMagnitude = m_destination.GetLength();
		const float movementEpsilon = valuesMagnitude * 0.000001f; // use an epsilon with a size relevant to the magnitude of distances we handle
		if (m_bForceFinishAsTooNear || m_position.IsEquivalent(m_destination, movementEpsilon))
		{
			m_position = m_destination;
			oldPosition = m_destination;
			ActivateOutput(pActInfo, OUT_DONE, true);
			ActivateOutput(pActInfo, OUT_FINISH, true);
			SetPos(pActInfo, m_position); // for finishing we have to make a manual setpos even if there is physics involved, to make sure the entity will be where it should.
			if (pPhysEnt)
			{
				pe_action_set_velocity setVel;
				setVel.v = ZERO;
				pPhysEnt->Action(&setVel);

				m_stopping = true;
			}
			else
			{
				pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
				m_bActive = false;
			}
		}
		else
		{
			Vec3 direction = m_destination - m_position;
			float distance = direction.GetLength();
			Vec3 directionAndSpeed = direction.normalized();

			// ease-area calcs
			float distanceForEaseOutCalc = distance + m_easeOutDistance * EASE_MARGIN_FACTOR;
			if (distanceForEaseOutCalc < m_easeOutDistance) // takes care of m_easeOutDistance being 0
			{
				directionAndSpeed *= distanceForEaseOutCalc / m_easeOutDistance;
			}
			else  // init code makes sure both eases dont overlap, when the movement is time defined. when it is speed defined, only ease out is applied if they overlap.
			{
				if (m_easeInDistance > 0.f)
				{
					Vec3 vectorFromStart = m_position - m_startPos;
					float distanceFromStart = vectorFromStart.GetLength();
					float distanceForEaseInCalc = distanceFromStart + m_easeInDistance * EASE_MARGIN_FACTOR;
					if (distanceForEaseInCalc < m_easeInDistance)
					{
						directionAndSpeed *= distanceForEaseInCalc / m_easeInDistance;
					}
				}
			}

			directionAndSpeed *= (m_topSpeed * timeDifference);

			if (direction.GetLength() < directionAndSpeed.GetLength())
			{
				m_position = m_destination;
				m_bForceFinishAsTooNear = true;
			}
			else
				m_position += directionAndSpeed;
		}
		ActivateOutput(pActInfo, OUT_CURRENT, m_position);
		if (pPhysEnt == NULL)
		{
			SetPos(pActInfo, m_position);
		}
		else
		{
			pe_action_set_velocity setVel;
			float rTimeStep = timeDifference > 0.000001f ? 1.f / timeDifference : 0.0f;
			setVel.v = (m_position - oldPosition) * rTimeStep;
			pPhysEnt->Action(&setVel);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	bool CanUsePhysicsUpdate(IEntity* pEntity)
	{
		// Do not use physics when
		// a) there is no physics geometry
		// b) the entity is a child
		// c) the entity is not rigid

		if (IStatObj* pStatObj = pEntity->GetStatObj(ENTITY_SLOT_ACTUAL))
		{
			phys_geometry* pPhysGeom = pStatObj->GetPhysGeom();
			if (!pPhysGeom)
			{
				return false;
			}
		}
		IPhysicalEntity* pPhysEnt = pEntity->GetPhysics();

		if (!pPhysEnt)
		{
			return false;
		}
		else
		{
			if (pEntity->GetParent() || pPhysEnt->GetType() == PE_STATIC)
			{
				return false;
			}
		}
		return true;
	}

	//////////////////////////////////////////////////////////////////////////
	void SetPos(SActivationInfo* pActInfo, const Vec3& vPos)
	{
		if (pActInfo->pEntity)
		{
			if (pActInfo->pEntity->GetParent())
			{
				Matrix34 tm = pActInfo->pEntity->GetWorldTM();
				tm.SetTranslation(vPos);
				pActInfo->pEntity->SetWorldTM(tm);
			}
			else
				pActInfo->pEntity->SetPos(vPos);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void ReadDestinationPosFromInput(SActivationInfo* pActInfo)
	{
		m_destination = GetPortVec3(pActInfo, IN_DEST);
		if (pActInfo->pEntity)
		{
			switch (m_coorSys)
			{
			case CS_PARENT:
				{
					IEntity* pParent = pActInfo->pEntity->GetParent();
					if (pParent)
					{
						const Matrix34& mat = pParent->GetWorldTM();
						m_destination = mat.TransformPoint(m_destination);
					}
				}
				break;
			case CS_LOCAL:
				{
					const Matrix34& mat = pActInfo->pEntity->GetWorldTM();
					m_destination = mat.TransformPoint(m_destination);
				}
				break;
			case CS_WORLD:
			default:
				break;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void ReadSpeedFromInput(SActivationInfo* pActInfo)
	{
		if (m_valueType == VT_TIME)
		{
			CalcSpeedFromTimeInput(pActInfo);
		}
		else
		{
			m_topSpeed = GetPortFloat(pActInfo, IN_VALUE);
			if (m_topSpeed < 0.0f) m_topSpeed = 0.0f;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// when the inputs are set to work with "time" instead of speed, we just calculate speed from that time value
	void CalcSpeedFromTimeInput(SActivationInfo* pActInfo)
	{
		assert(GetPortInt(pActInfo, IN_VALUETYPE) == VT_TIME);
		float movementDuration = GetPortFloat(pActInfo, IN_VALUE);
		float timePassed = gEnv->pTimer->GetFrameStartTime().GetSeconds() - m_startTime;
		float movementDurationLeft = movementDuration - timePassed;
		if (movementDurationLeft < 0.0001f) movementDurationLeft = 0.0001f;
		Vec3 movement = m_destination - m_position;
		float distance = movement.len();

		if (m_easeInDistance + m_easeOutDistance > distance)    // make sure they dont overlap
			m_easeInDistance = distance - m_easeOutDistance;
		float distanceNoEase = distance - (m_easeInDistance + m_easeOutDistance);

		const float factorAverageSpeedEase = 0.5f; // not real because is not lineal, but is good enough for this
		float realEaseInDistance = max(m_easeInDistance - m_easeInDistance * EASE_MARGIN_FACTOR, 0.f);
		float realEaseOutDistance = max(m_easeOutDistance - m_easeOutDistance * EASE_MARGIN_FACTOR, 0.f);

		m_topSpeed = ((realEaseInDistance / factorAverageSpeedEase) + distanceNoEase + (realEaseOutDistance / factorAverageSpeedEase)) / movementDurationLeft;          // this comes just from V = S / T;
	}

	//////////////////////////////////////////////////////////////////////////
	void Start(SActivationInfo* pActInfo)
	{
		IEntity* pEnt = pActInfo->pEntity;
		if (!pEnt)
			return;

		m_bForceFinishAsTooNear = false;
		m_position = m_coorSys == CS_WORLD ? pEnt->GetWorldPos() : pEnt->GetPos();
		m_startPos = m_position;
		m_lastFrameTime = gEnv->pTimer->GetFrameStartTime().GetSeconds();
		m_startTime = m_lastFrameTime;
		ReadDestinationPosFromInput(pActInfo);
		m_easeOutDistance = GetPortFloat(pActInfo, IN_EASEOUT);
		m_easeInDistance = GetPortFloat(pActInfo, IN_EASEIN);
		ReadSpeedFromInput(pActInfo);
		if (m_easeOutDistance < 0.0f) m_easeOutDistance = 0.0f;
		m_stopping = false;
		m_bActive = true;
		m_bUsePhysicUpdate = CanUsePhysicsUpdate(pEnt);

		pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);

		ActivateOutput(pActInfo, OUT_START, true);
	}

	//////////////////////////////////////////////////////////////////////////
	void Stop(SActivationInfo* pActInfo)
	{
		pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);

		if (m_bActive)
		{
			m_bActive = false;
			ActivateOutput(pActInfo, OUT_DONE, true);

			if (m_bUsePhysicUpdate)
			{
				if (IEntity* pEntity = pActInfo->pEntity)
				{
					if (IPhysicalEntity* pPhysEntity = pEntity->GetPhysics())
					{
						pe_action_set_velocity setVel;
						setVel.v = ZERO;
						pPhysEntity->Action(&setVel);
					}
				}
			}
		}
		ActivateOutput(pActInfo, OUT_STOP, true);
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

const float CMoveEntityTo::EASE_MARGIN_FACTOR = 0.2f;  // the eases dont go to/from speed 0, they go to/from this fraction of the max speed.

REGISTER_FLOW_NODE("Movement:MoveEntityTo", CMoveEntityTo);
