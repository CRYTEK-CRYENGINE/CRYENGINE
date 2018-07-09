// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   -------------------------------------------------------------------------
   File name:   GoalOp_G02.cpp
   Description: Game 02 goalops
             These should move into GameDLL when interfaces allow!
   -------------------------------------------------------------------------
   History:
   - 25:02:2008 - Created by Matthew Jack
 *********************************************************************/

#include "StdAfx.h"
#include "GoalOp_G02.h"
#include "GoalOpTrace.h"

#include "PipeUser.h"
#include "Puppet.h"
#include "DebugDrawContext.h"

IGoalOp* CGoalOpFactoryG02::GetGoalOp(const char* sGoalOpName, IFunctionHandler* pH, int nFirstParam, GoalParameters& params) const
{
	// For now, we look up the central enum - but this should become totally localised
	EGoalOperations op = CGoalPipe::GetGoalOpEnum(sGoalOpName);

	switch (op)
	{
	case eGO_CHARGE:
		{
			// Charge distance front
			pH->GetParam(nFirstParam, params.fValue);

			// Charge distance back
			params.fValueAux = 0;
			if (pH->GetParamCount() > nFirstParam)
				pH->GetParam(nFirstParam + 1, params.fValueAux);

			// Lastop result usage flags, see EAILastOpResFlags.
			params.nValue = 0;
			if (pH->GetParamCount() > nFirstParam + 1)
				pH->GetParam(nFirstParam + 2, params.nValue);
		}
		break;
	case eGO_SEEKCOVER:
		{
			pH->GetParam(nFirstParam, params.bValue);     // hide/unhide
			pH->GetParam(nFirstParam + 1, params.fValue); // radius
			params.nValue = 3;
			if (pH->GetParamCount() > nFirstParam + 1)
				pH->GetParam(nFirstParam + 2, params.nValue); // iterations
			int useLastOpResultAsBackup = 0;
			if (pH->GetParamCount() > nFirstParam + 2)
				pH->GetParam(nFirstParam + 3, useLastOpResultAsBackup); // use lastopresult if attention target does not exists
			if (pH->GetParamCount() > nFirstParam + 3)
				pH->GetParam(nFirstParam + 4, params.fValueAux);  // minimum radius to move
			params.nValueAux = useLastOpResultAsBackup;
		}
		break;
	default:
		// We don't know this goalop, but another factory might
		return NULL;
	}

	return GetGoalOp(op, params);
}

IGoalOp* CGoalOpFactoryG02::GetGoalOp(EGoalOperations op, GoalParameters& params) const
{
	IGoalOp* pResult = NULL;

	switch (op)
	{
	case eGO_CHARGE:
		{
			pResult = new COPCharge(params.fValue, params.fValueAux,
			                        (params.nValue & AILASTOPRES_USE) != 0, (params.nValue & AILASTOPRES_LOOKAT) != 0);
		}
		break;
	case eGO_SEEKCOVER:
		{
			pResult = new COPSeekCover(params.bValue, params.fValue, params.fValueAux, params.nValue, (params.nValueAux & 1) != 0, (params.nValueAux & 2) != 0);
		}
		break;
	default:
		// We don't know this goalop, but another factory might
		return NULL;
	}

	return pResult;
}

//====================================================================
// COPCharge
//====================================================================
COPCharge::COPCharge(float distanceFront, float distanceBack, bool useLastOp, bool lookatLastOp) :
	m_moveTarget(0),
	m_sightTarget(0),
	m_distanceFront(distanceFront),
	m_distanceBack(distanceBack),
	m_useLastOpResult(useLastOp),
	m_lookAtLastOp(lookatLastOp),
	m_initialized(false),
	m_state(STATE_APPROACH),
	m_pOperand(0),
	m_stuckTime(0),
	m_debugHitRes(false),
	m_debugHitRad(2.0f),
	m_bailOut(false),
	m_looseAttentionId(0),
	m_lastOpPos(0, 0, 0)
{
	for (unsigned i = 0; i < 5; i++)
		m_debugAntenna[i].zero();
	m_debugRange[0].zero();
	m_debugRange[1].zero();
	m_debugHit[0].zero();
	m_debugHit[1].zero();
}

//
//----------------------------------------------------------------------------------------------------------
COPCharge::~COPCharge()
{
}

//
//----------------------------------------------------------------------------------------------------------
EGoalOpResult COPCharge::Execute(CPipeUser* pOperand)
{
	CCCPOINT(COPCharge_Execute);
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	m_pOperand = pOperand;

	// Do not mind the target direction when approaching.
	m_pOperand->m_bPathfinderConsidersPathTargetDirection = false;

	CPuppet* pPuppet = pOperand->CastToCPuppet();

	if (!m_moveTarget)// first time = lets stick to target
	{
		m_moveTarget = (CAIObject*)m_pOperand->GetAttentionTarget();

		CAIObject* pLastOpResult = pOperand->m_refLastOpResult.GetAIObject();

		if (!m_moveTarget || m_useLastOpResult)
		{
			if (pLastOpResult)
				m_moveTarget = pLastOpResult;
			else
			{
				// no target, nothing to approach to
				Reset(m_pOperand);
				return eGOR_FAILED;
			}
		}

		// keep last op. result as sight target
		if (!m_sightTarget && m_lookAtLastOp && pLastOpResult)
			m_sightTarget = pLastOpResult;

		// TODO: This is hack to prevent the Alien Scouts not to use the speed control.
		// Use better test to use the speed control _only_ when it is really needed (human formations).
		if (pPuppet)
		{
			if (!m_pOperand->m_movementAbility.b3DMove && !m_initialized)
			{
				pPuppet->ResetSpeedControl();
				m_initialized = true;
			}
		}

		// Start approach animation.
		if (m_pOperand->GetProxy())
			m_pOperand->GetProxy()->SetAGInput(AIAG_ACTION, "meleeMoveBender");

		m_approachStartTime = GetAISystem()->GetFrameStartTime();
	}

	//make sure the guy looks in correct direction
	if (m_lookAtLastOp && m_sightTarget)
	{
		CWeakRef<CAIObject> refSightTarget = GetWeakRef(m_sightTarget);
		m_looseAttentionId = pOperand->SetLooseAttentionTarget(refSightTarget);
	}

	if (m_moveTarget && !m_moveTarget->IsEnabled())
	{
		Reset(m_pOperand);
		return eGOR_FAILED;
	}

	ExecuteDry(m_pOperand);

	if (m_state == STATE_APPROACH)
	{
		// Make sure we do not sped too much time in the approach
		CTimeValue currentTime = GetAISystem()->GetFrameStartTime();
		float elapsed = (currentTime - m_approachStartTime).GetSeconds();
		if (elapsed > 7.0f)
		{
			Reset(m_pOperand);
			return eGOR_FAILED;
		}
	}

	if (m_state == STATE_CHARGE || m_state == STATE_FOLLOW_TROUGH)
	{
		// If the entity is past the plane at the end of the charge range, we're done!
		Vec3 normal = m_chargeEnd - m_chargeStart;
		float d = normal.NormalizeSafe();
		float d2 = normal.Dot(m_pOperand->GetPos() - m_chargeStart);
		if (d2 > (d - m_pOperand->m_Parameters.m_fPassRadius))
		{
			Reset(m_pOperand);
			return eGOR_SUCCEEDED;
		}
	}

	// Check if got stuck while charging, and cancel the attack.
	if (m_stuckTime > 0.3f)
	{
		Reset(m_pOperand);
		return eGOR_FAILED;
	}

	return eGOR_IN_PROGRESS;
}

void COPCharge::ValidateRange()
{
	AIAssert(m_moveTarget);
	if (!m_pOperand)
		return;
	ray_hit hit;

	Vec3 delta = m_chargeEnd - m_chargeStart;

	Vec3 hitPos;
	float hitDist;

	if (IntersectSweptSphere(&hitPos, hitDist, Lineseg(m_chargeStart, m_chargeEnd), m_pOperand->m_Parameters.m_fPassRadius * 1.1f, AICE_ALL))
	{
		m_chargeEnd = m_chargeStart + (m_chargeEnd - m_chargeStart).GetNormalizedSafe() * max(0.0f, hitDist - 0.3f);
	}
}

void COPCharge::SetChargeParams()
{
	if (!m_pOperand)
		return;
	const Vec3 physPos = m_pOperand->GetPhysicsPos();
	// Set charge based on the current charge pos.
	Vec3 dirToChargePos = m_chargePos - physPos;
	m_chargeStart = physPos;
	m_chargeEnd = physPos + dirToChargePos * ((m_distanceFront + m_distanceBack) / dirToChargePos.GetLength());
	ValidateRange();
}

void COPCharge::UpdateChargePos()
{
	if (!m_pOperand)
		return;

	m_chargePos = m_moveTarget->GetPos();

	m_debugRange[0] = m_moveTarget->GetPos();
	m_debugRange[1] = m_chargePos;
}

//
//----------------------------------------------------------------------------------------------------------
void COPCharge::ExecuteDry(CPipeUser* pOperand)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);
	m_pOperand = pOperand;

	// Move towards the current target.
	if (!m_moveTarget)
		return;

	float maxMoveSpeed = 1.0f;
	const Vec3& opPos = m_pOperand->GetPos();

	// Do not move during the anticipate time
	if (m_state == STATE_ANTICIPATE)
	{
		// Keep updating the jump direction.
		//		UpdateChargePos(pOperand);
		//		SetChargeParams(pOperand->GetPos());

		maxMoveSpeed = 0.01f;
	}
	else if (m_state == STATE_APPROACH)
	{
		UpdateChargePos();

		Vec3 dirToTarget = (m_moveTarget->GetPos() - opPos);
		if (!pOperand->m_movementAbility.b3DMove)
			dirToTarget.z = 0.0f;
		dirToTarget.Normalize();

		m_chargeStart = m_chargePos - dirToTarget * m_distanceFront;
		m_chargeEnd = m_chargePos + dirToTarget * m_distanceBack;

		ValidateRange();

		maxMoveSpeed = 0.6f;
	}

	Vec3 targetPos;

	if (m_state == STATE_APPROACH)
		targetPos = m_moveTarget->GetPos();
	else
		targetPos = m_chargeEnd;

	Vec3 delta = targetPos - opPos;

	if (m_state == STATE_APPROACH)
	{
		Vec3 dirToTarget = (m_moveTarget->GetPos() - opPos);
		if (!m_pOperand->m_movementAbility.b3DMove)
			dirToTarget.z = 0.0f;
		float curDist = dirToTarget.GetLength();
		if (curDist > 0.0f)
			dirToTarget /= curDist;

		// validate the charge range
		ValidateRange();

		// Check if the charge should be executed or if the charge fails.
		float t;
		float distToLine = Distance::Point_Lineseg(opPos, Lineseg(m_chargeStart, m_chargeEnd), t);

		//		else if (curDist <= m_distanceFront && Distance::Point_Point(m_chargePos, m_moveTarget->GetPos()) < m_distanceFront * 0.3f)
		if (t > 0.0f && distToLine < m_distanceFront * 0.3f)
		{
			// Succeed -> Charge sequence.
			SetChargeParams();
			m_state = STATE_CHARGE; //STATE_ANTICIPATE;
			m_anticipateTime = GetAISystem()->GetFrameStartTime();
			m_pOperand->SetSignal(0, "OnChargeStart", pOperand->GetEntity(), 0, gAIEnv.SignalCRCs.m_nOnChargeStart);

			// Start jump approach animation.
			if (m_pOperand->GetProxy())
				m_pOperand->GetProxy()->SetAGInput(AIAG_ACTION, "meleeAttackBender");
		}
		else if (t > 0.2f) // && distToLine < m_distanceFront * 0.75f)
		{
			// Fail -> charge anyway.
			SetChargeParams();
			m_state = STATE_CHARGE; //STATE_ANTICIPATE;
			m_anticipateTime = GetAISystem()->GetFrameStartTime();
			m_pOperand->SetSignal(0, "OnChargeStart", pOperand->GetEntity(), 0, gAIEnv.SignalCRCs.m_nOnChargeStart);

			// Start jump approach animation.
			if (m_pOperand->GetProxy())
				m_pOperand->GetProxy()->SetAGInput(AIAG_ACTION, "meleeAttackBender");
		}
	}
	else if (m_state == STATE_ANTICIPATE)
	{
		CTimeValue currentTime = GetAISystem()->GetFrameStartTime();
		float elapsed = (currentTime - m_anticipateTime).GetSeconds();

		if (elapsed > 0.3f)
			m_state = STATE_CHARGE;
	}
	else if (m_state == STATE_CHARGE || m_state == STATE_FOLLOW_TROUGH)
	{
		Lineseg charge(m_chargeStart, m_chargeEnd);
		float tOp = 0;
		float tTarget = 0;
		Distance::Point_LinesegSq(opPos, charge, tOp);
		Distance::Point_LinesegSq(m_moveTarget->GetPos(), charge, tTarget);

		if (m_state == STATE_CHARGE)
		{
			CAIActor* pActor = m_moveTarget->CastToCAIActor();
			const float rad = pOperand->m_Parameters.m_fPassRadius + (pActor ? pActor->m_Parameters.m_fPassRadius : 0.f);
			if (Distance::Point_Point(opPos, m_moveTarget->GetPos()) < rad * 1.5f)
			{
				// Send the target that was hit along with the signal.
				AISignalExtraData* pData = new AISignalExtraData;
				pData->nID = m_moveTarget->GetEntityID();
				m_pOperand->SetSignal(0, "OnChargeHit", m_pOperand->GetEntity(), pData, gAIEnv.SignalCRCs.m_nOnChargeHit);

				m_state = STATE_FOLLOW_TROUGH;
			}
			else if (tOp > tTarget)
			{
				if (!m_bailOut)
				{
					m_pOperand->SetSignal(0, "OnChargeMiss", m_pOperand->GetEntity(), 0, gAIEnv.SignalCRCs.m_nOnChargeMiss);
					m_state = STATE_FOLLOW_TROUGH;

					if (m_pOperand->GetProxy())
					{
						m_pOperand->m_State.coverRequest.ClearCoverAction();
						m_pOperand->GetProxy()->ResetAGInput(AIAG_ACTION);
						m_pOperand->GetProxy()->SetAGInput(AIAG_SIGNAL, "meleeRollOutBender");
					}
					m_bailOut = true;
				}
			}
		}
		else if (m_state == STATE_FOLLOW_TROUGH)
		{
			if (tOp > 0.5f)
			{
				// Start jump approach animation.
				//				SetAGInput("Persistent", "none");
				//				SetAGInput("Signal", "meleeRollOutBender");

				if (!m_bailOut)
				{
					m_pOperand->SetSignal(0, "OnChargeBailOut", m_pOperand->GetEntity(), 0, gAIEnv.SignalCRCs.m_nOnChargeBailOut);
					m_bailOut = true;
				}
			}
		}
	}

	//	if( dist > m_distance )
	{
		Vec3 checkDelta(delta.GetNormalizedSafe());
		checkDelta *= m_pOperand->m_Parameters.m_fPassRadius;

		Vec3 dir = delta.GetNormalizedSafe();

		/*		{
		   ray_hit hit;
		   IPhysicalWorld*		pWorld = GetAISystem()->GetPhysicalWorld();

		   Matrix33	dirMat;
		   dirMat.SetRotationVDir(dir, gf_PI/4);

		   Vec3	antenna[4];
		   Vec3	steerOffset[4];
		   const float	outer = DEG2RAD(45.0f);
		   const float	inner = DEG2RAD(10.0f);
		   const	float	r = 1.0f * 4.0f;

		   const float osn = sinf(outer) * r;
		   const float ocs = cosf(outer) * r;

		   antenna[0].Set(osn, ocs, 0);	steerOffset[0].Set(1, 0, 0);
		   antenna[1].Set(-osn, ocs, 0);	steerOffset[1].Set(-1, 0, 0);
		   antenna[2].Set(0, ocs, osn);	steerOffset[2].Set(0, 0, 1);
		   antenna[3].Set(0, ocs, -osn);	steerOffset[3].Set(0, 0, -1);

		   Vec3	newSteer(dir);
		   Vec3	lastPass(dir);

		   Vec3	physPos = m_pOperand->GetPhysicsPos();

		   m_debugAntenna[0] = physPos;

		   // Steer away from the hit surface.
		   int	nhits = 0;
		   for (unsigned i = 0; i < 4; i++)
		   {
		   Vec3	ant = dirMat * antenna[i];
		   Vec3	off = dirMat * steerOffset[i];

		   m_debugAntenna[i+1].zero();

		   if(!pWorld->RayWorldIntersection(physPos, ant, ent_terrain|ent_static|ent_rigid|ent_sleeping_rigid, (geom_colltype_ray << rwi_colltype_bit) | rwi_colltype_any, &hit, 1))
		   {
		   m_debugAntenna[i+1] = ant;
		   lastPass = off;
		   newSteer += off;
		   }
		   else
		   {
		   float	a = hit.dist / r;
		   newSteer += off * a;
		   nhits++;
		   }
		   }

		   // Test center
		   if(pWorld->RayWorldIntersection(physPos, dir * r, ent_terrain|ent_static|ent_rigid|ent_sleeping_rigid, (geom_colltype_ray << rwi_colltype_bit) | rwi_colltype_any, &hit, 1))
		   {
		   float	a = 1.0f - (hit.dist / r);
		   newSteer += lastPass * a * 4.0f;
		   }
		   newSteer.NormalizeSafe();

		   dir = newSteer;

		   // Try to figure out if the alien is stuck.
		   // If most of the antennae are hit, increase a counter and after a while, signal failure in the main handler.
		   float	dt = GetAISystem()->GetFrameDeltaTime();

		   Vec3	hitPos;
		   float	hitDist;
		   m_debugHit[0] = physPos;
		   m_debugHit[1] = physPos + dir * 0.9f;
		   m_debugHitRes = false;
		   m_debugHitRad = 0.3f;
		   if(IntersectSweptSphere(&hitPos, hitDist, Lineseg(physPos, physPos + dir * 0.9f), 0.3f, AICE_ALL))
		   {
		   m_stuckTime += dt;
		   m_debugHitRes = true;
		   }
		   }*/

		float normalSpeed, minSpeed, maxSpeed;
		pOperand->GetMovementSpeedRange(pOperand->m_State.fMovementUrgency, pOperand->m_State.allowStrafing, normalSpeed, minSpeed, maxSpeed);

		//    float normalSpeed = pOperand->GetNormalMovementSpeed(pOperand->m_State.fMovementUrgency, true);

		m_pOperand->m_State.fDesiredSpeed = normalSpeed * maxMoveSpeed; //0.05f + speed * 0.95f;
		m_pOperand->m_State.vMoveDir = dir;
		//		pOperand->m_State.bExactPositioning = false;
		m_pOperand->m_State.fDistanceToPathEnd = 0; //dist;

#ifdef _DEBUG
		m_pOperand->m_DEBUGmovementReason = CPipeUser::AIMORE_MOVE;
#endif
	}

	if (m_state == STATE_APPROACH || m_state == STATE_CHARGE || m_state == STATE_FOLLOW_TROUGH)
	{
		Vec3 deltaMove = pOperand->GetPhysicsPos() - m_lastOpPos;
	}

	m_lastOpPos = pOperand->GetPhysicsPos();
}

//
//----------------------------------------------------------------------------------------------------------
void COPCharge::DebugDraw(CPipeUser* pOperand) const
{
	// Draw antennae
	CDebugDrawContext dc;
	const ColorB color(255, 255, 255);
	for (unsigned i = 0; i < 4; i++)
	{
		dc->DrawLine(m_debugAntenna[0], color, m_debugAntenna[0] + m_debugAntenna[i + 1], color);
	}

	dc->DrawLine(m_debugRange[0], color, m_debugRange[1], color);

	// Draw range
	ColorB color2(255, 0, 0);
	ColorB color3(255, 255, 0);

	if (m_state == STATE_ANTICIPATE)
	{
		color2.set(64, 0, 0, 255);
		color3.set(64, 64, 0, 255);
	}

	dc->DrawLine(m_chargeStart, color2, m_chargeEnd, color3);

	if (m_state == STATE_APPROACH)
	{
		dc->DrawSphere(m_chargeStart, 0.2f, color2);
		dc->DrawSphere(m_chargeEnd, 0.2f, color3);

		Lineseg charge(m_chargeStart, m_chargeEnd);
		float t;
		Distance::Point_LinesegSq(pOperand->GetPos(), charge, t);

		dc->DrawSphere(charge.GetPoint(0.2f), 0.1f, color2);

		if (t > 0.0f && t < 1.0f)
		{
			dc->DrawSphere(charge.GetPoint(t), 0.1f, color);
			dc->DrawLine(pOperand->GetPos(), color, charge.GetPoint(t), color);
		}
	}

	/*	if(m_stuckTime > 0.6f)
	   {
	   dc->DrawSphere(m_pOperand->GetPos(), 2.5f, ColorB(255, 0, 0));
	   }*/

	ColorB hitColor(0, 0, 0, 128);
	if (m_debugHitRes)
		hitColor.set(255, 0, 0, 128);
	dc->DrawSphere(m_debugHit[0], m_debugHitRad, hitColor);
	dc->DrawSphere((m_debugHit[0] + m_debugHit[1]) / 2, m_debugHitRad, hitColor);
	dc->DrawSphere(m_debugHit[1], m_debugHitRad, hitColor);
}

//
//----------------------------------------------------------------------------------------------------------
void COPCharge::Reset(CPipeUser* pOperand)
{
	m_moveTarget = NULL;
	m_sightTarget = NULL;
	m_state = STATE_APPROACH;
	m_stuckTime = 0;
	m_bailOut = false;
	m_lastOpPos.zero();

	if (pOperand)
	{
		pOperand->m_State.coverRequest.ClearCoverAction();

		if (pOperand->GetProxy())
			pOperand->GetProxy()->ResetAGInput(AIAG_ACTION);

		if (m_lookAtLastOp && m_sightTarget)
		{
			pOperand->SetLooseAttentionTarget(NILREF, m_looseAttentionId);
			m_looseAttentionId = 0;
		}
	}
}

//
//----------------------------------------------------------------------------------------------------------
void COPCharge::OnObjectRemoved(CAIObject* pObject)
{
	//if (pObject == m_pLooseAttentionTarget)
	//m_pLooseAttentionTarget = 0;
	if (pObject == m_moveTarget || pObject == m_sightTarget)
		Reset(m_pOperand);
}

//====================================================================
// COPSeekCover
//====================================================================
COPSeekCover::COPSeekCover(bool uncover, float radius, float minRadius, int iterations, bool useLastOpAsBackup, bool towardsLastOpResult) :
	m_uncover(uncover),
	m_radius(radius),
	m_minRadius(minRadius),
	m_iterations(iterations),
	m_useLastOpAsBackup(useLastOpAsBackup),
	m_towardsLastOpResult(towardsLastOpResult),
	m_endAccuracy(0.0f),
	m_notMovingTimeMs(0),
	m_pRoot(0),
	m_state(0),
	m_center(0, 0, 0),
	m_forward(0, 0, 0),
	m_right(0, 0, 0),
	m_target(0, 0, 0)
{
	Limit(m_iterations, 1, 5);
	m_pTraceDirective = 0;
}

COPSeekCover::COPSeekCover(const XmlNodeRef& node)
	: m_uncover(!s_xml.GetBool(node, "hide", true)),
	m_radius(0.0f),
	m_minRadius(0.0f),
	m_iterations(3),
	m_useLastOpAsBackup(s_xml.GetBool(node, "useLastOpAsBackup", true)),
	m_towardsLastOpResult(s_xml.GetBool(node, "towardsLastOpResult", true)),
	m_endAccuracy(0.f),
	m_notMovingTimeMs(0),
	m_pTraceDirective(0),
	m_pRoot(0),
	m_state(0),
	m_center(ZERO),
	m_forward(ZERO),
	m_right(ZERO),
	m_target(ZERO)
{
	s_xml.GetMandatory(node, "radius", m_radius);
	node->getAttr("iterations", m_iterations);
	node->getAttr("minRadius", m_minRadius);

	Limit(m_iterations, 1, 5);
}

//====================================================================
// ~COPSeekCover
//====================================================================
COPSeekCover::~COPSeekCover()
{
	SAFE_DELETE(m_pRoot);

	Reset(0);
}

//====================================================================
// Reset
//====================================================================
void COPSeekCover::Reset(CPipeUser* pOperand)
{
	SAFE_DELETE(m_pTraceDirective);

	m_notMovingTimeMs = 0;
	m_state = 0;
	if (pOperand)
	{
		pOperand->ClearPath("COPSeekCover::Reset m_Path");
		pOperand->SetMovingToCover(false);
	}
}

//====================================================================
// Serialize
//====================================================================
void COPSeekCover::Serialize(TSerialize ser)
{
	ser.BeginGroup("COPSeekCover");
	{
		ser.Value("m_radius", m_radius);
		ser.Value("m_iterations", m_iterations);
		ser.Value("m_endAccuracy", m_endAccuracy);
		ser.Value("m_useLastOpAsBackup", m_useLastOpAsBackup);
		ser.Value("m_towardsLastOpResult", m_towardsLastOpResult);
		ser.Value("m_lastTime", m_lastTime);
		ser.Value("m_notMovingTimeMs", m_notMovingTimeMs);
		ser.Value("m_state", m_state);

		if (ser.IsWriting())
		{
			if (ser.BeginOptionalGroup("TraceDirective", m_pTraceDirective != NULL))
			{
				assert(m_pTraceDirective);
				m_pTraceDirective->Serialize(ser);
				ser.EndGroup();
			}
		}
		else
		{
			SAFE_DELETE(m_pTraceDirective);
			if (ser.BeginOptionalGroup("TraceDirective", true))
			{
				m_pTraceDirective = new COPTrace(false, m_endAccuracy);
				m_pTraceDirective->Serialize(ser);
				ser.EndGroup();
			}
			// Restart calculating
			if (m_state >= 0)
			{
				m_state = 0;
				delete m_pTraceDirective;
				m_pTraceDirective = 0;
			}
		}
		ser.EndGroup();
	}
}

//====================================================================
// IsSegmentValid
//====================================================================
bool COPSeekCover::IsSegmentValid(IAISystem::tNavCapMask navCap, float rad, const Vec3& posFrom, Vec3& posTo, IAISystem::ENavigationType& navTypeFrom)
{
	if (IsInDeepWater(posTo))
		return false;

	Vec3 initPos(posTo);
	if (!GetFloorPos(posTo, initPos, WalkabilityFloorUpDist, 1.0f, WalkabilityDownRadius, AICE_ALL))
		return false;

	return true;
}

inline float Ease(float a)
{
	return (a + sqr(a)) / 2.0f;
}

//====================================================================
// IsTargetVisibleFrom
//====================================================================
bool COPSeekCover::IsTargetVisibleFrom(const Vec3& from, const Vec3& targetPos)
{
	ray_hit hit;
	Vec3 waistPos = from + Vec3(0, 0, m_uncover ? 1.2f : 0.6f);
	Vec3 delta = targetPos - waistPos;
	if (gEnv->pPhysicalWorld->RayWorldIntersection(
	      waistPos, delta, COVER_OBJECT_TYPES,
	      AI_VISION_RAY_CAST_FLAG_BLOCKED_BY_SOLID_COVER | AI_VISION_RAY_CAST_FLAG_BLOCKED_BY_SOFT_COVER,
	      &hit, 1))
	{
		if (hit.dist < delta.GetLength() * 0.95f)
			return false;
	}
	return true;
}

//====================================================================
// CheckSegmentAgainstAvoidPos
//====================================================================
bool COPSeekCover::OverlapSegmentAgainstAvoidPos(const Vec3& from, const Vec3& to, float rad, const std::vector<Vec3FloatPair>& avoidPos)
{
	Lineseg movement(from, to);
	float t;
	for (unsigned i = 0, ni = avoidPos.size(); i < ni; ++i)
	{
		const float radSq = sqr(avoidPos[i].second + rad);
		if (Distance::Point_Lineseg2DSq(avoidPos[i].first, movement, t) < radSq)
			return true;
	}
	return false;
}

//====================================================================
// OverlapCircleAgaintsAvoidPos
//====================================================================
bool COPSeekCover::OverlapCircleAgaintsAvoidPos(const Vec3& pos, float rad, const std::vector<Vec3FloatPair>& avoidPos)
{
	for (unsigned i = 0, ni = avoidPos.size(); i < ni; ++i)
	{
		const float radSq = sqr(avoidPos[i].second + rad);
		if (Distance::Point_Point2DSq(avoidPos[i].first, pos) < radSq)
			return true;
	}
	return false;
}

//====================================================================
// GetNearestPuppets
//====================================================================
void COPSeekCover::GetNearestPuppets(CPuppet* pSelf, const Vec3& pos, float radius, std::vector<Vec3FloatPair>& positions)
{
	CAIActor* self = static_cast<CAIActor*>(pSelf);

	const float radiusSq = sqr(radius);

	ActorLookUp& lookUp = *gAIEnv.pActorLookUp;
	size_t activeActorCount = lookUp.GetActiveCount();

	for (size_t actorIndex = 0; actorIndex < activeActorCount; ++actorIndex)
	{
		CAIActor* pAIActor = lookUp.GetActor<CAIActor>(actorIndex);
		if (pAIActor == self)
			continue;

		Vec3 delta = pAIActor->GetPhysicsPos() - pos;
		if (delta.GetLengthSquared2D() < radiusSq && delta.z >= -1.0f && delta.z < 2.0f)
		{
			const float passRadius = pAIActor->GetParameters().m_fPassRadius;
			positions.push_back(std::make_pair(lookUp.GetPosition(actorIndex), passRadius));

			if (CPipeUser* pPipeUser = pAIActor->CastToCPipeUser())
			{
				const Vec3& vPipeUserLastPathPos = pPipeUser->m_Path.GetLastPathPos();
				if (Distance::Point_Point2DSq(vPipeUserLastPathPos, pos) < radiusSq)
					positions.push_back(std::make_pair(vPipeUserLastPathPos, passRadius));
			}
		}
	}
}

//====================================================================
// CalcStrafeCoverTree
//====================================================================
bool COPSeekCover::IsInDeepWater(const Vec3& refPos)
{
	float terrainLevel = gEnv->p3DEngine->GetTerrainElevation(refPos.x, refPos.y);
	if (refPos.z + 2.0f < terrainLevel)
	{
		// Completely under terrain, skip water level checks.
		// This is not consistent with the player code, but for performance reasons do not do the ray check here.
		return false;
	}
	float waterLevel = gEnv->p3DEngine->GetWaterLevel(&refPos);
	return (waterLevel - terrainLevel) > 0.8f;
}

//====================================================================
// CalcStrafeCoverTree
//====================================================================
void COPSeekCover::CalcStrafeCoverTree(SAIStrafeCoverPos& pos, int i, int j,
                                       const Vec3& center, const Vec3& forw, const Vec3& right, float radius,
                                       const Vec3& target, IAISystem::tNavCapMask navCap, float passRadius, bool insideSoftCover,
                                       const std::vector<Vec3FloatPair>& avoidPos, const std::vector<Vec3FloatPair>& softCoverPos)
{
	if (i >= m_iterations)
		return;

	unsigned n = 1 << (i + 1);
	unsigned b = j * 2;
	for (unsigned k = 0; k < 2; ++k)
	{
		float u = Ease((float)(i + 1) / (float)m_iterations) * radius * cry_random(0.9f, 1.0f);
		float v = ((float)(b + k) - ((n - 1) / 2.0f));
		float a = v / (float)n * gf_PI * cry_random(0.75f, 0.80f);
		float x = cosf(a) * u;
		float y = sinf(a) * u;
		Vec3 p = center + x * right + y * forw;
		p.z = pos.pos.z + 0.5f;

		IAISystem::ENavigationType navType;
		bool overlapSoftCover = false;
		if (!insideSoftCover)
			overlapSoftCover = OverlapSegmentAgainstAvoidPos(pos.pos, p, passRadius, softCoverPos);

		if (!OverlapSegmentAgainstAvoidPos(pos.pos, p, passRadius, avoidPos) && !overlapSoftCover &&
		    IsSegmentValid(navCap, passRadius, pos.pos, p, navType))
		{
			insideSoftCover = OverlapCircleAgaintsAvoidPos(p, passRadius / 2, softCoverPos);
			bool valid = !insideSoftCover && !OverlapSegmentAgainstAvoidPos(target, p, 0.0f, avoidPos);
			bool vis = IsTargetVisibleFrom(p, target);

			if (m_uncover)
			{
				pos.child.push_back(SAIStrafeCoverPos(p, !vis, valid, navType));
				if (!vis || !valid)
					CalcStrafeCoverTree(pos.child.back(), i + 1, b + k, center, forw, right, radius, target, navCap, passRadius, insideSoftCover, avoidPos, softCoverPos);
			}
			else
			{
				pos.child.push_back(SAIStrafeCoverPos(p, vis, valid, navType));
				if (vis || !valid)
					CalcStrafeCoverTree(pos.child.back(), i + 1, b + k, center, forw, right, radius, target, navCap, passRadius, insideSoftCover, avoidPos, softCoverPos);
			}

		}
		else
		{
			Vec3 p2 = (p + pos.pos) * 0.5f;

			overlapSoftCover = false;
			if (!insideSoftCover)
				overlapSoftCover = OverlapSegmentAgainstAvoidPos(pos.pos, p2, passRadius, softCoverPos);

			if (!OverlapSegmentAgainstAvoidPos(pos.pos, p2, passRadius, avoidPos) && !overlapSoftCover &&
			    IsSegmentValid(navCap, passRadius, pos.pos, p2, navType))
			{
				insideSoftCover = OverlapCircleAgaintsAvoidPos(p, passRadius / 2, softCoverPos);
				bool valid = !insideSoftCover && !OverlapSegmentAgainstAvoidPos(target, p2, 0.0f, avoidPos);
				bool vis = IsTargetVisibleFrom(p2, target);

				if (m_uncover)
				{
					pos.child.push_back(SAIStrafeCoverPos(p2, !vis, valid, navType));
				}
				else
				{
					pos.child.push_back(SAIStrafeCoverPos(p2, vis, valid, navType));
				}
			}
		}
	}

	return;
}

//====================================================================
// GetNearestHidden
//====================================================================
COPSeekCover::SAIStrafeCoverPos* COPSeekCover::GetNearestHidden(SAIStrafeCoverPos& pos, const Vec3& from, float& minDist)
{
	SAIStrafeCoverPos* minPos = 0;
	for (unsigned i = 0, ni = pos.child.size(); i < ni; ++i)
	{
		float d = minDist;
		SAIStrafeCoverPos* res = GetNearestHidden(pos.child[i], from, d);
		if (res && d < minDist)
		{
			minPos = res;
			minDist = d;
		}

		if (pos.child[i].valid && !pos.child[i].vis)
		{
			d = Distance::Point_PointSq(from, pos.child[i].pos);
			if (d < minDist)
			{
				minPos = &pos.child[i];
				minDist = d;
			}
		}
	}
	return minPos;
}

//====================================================================
// GetFurthestFromTarget
//====================================================================
COPSeekCover::SAIStrafeCoverPos* COPSeekCover::GetFurthestFromTarget(SAIStrafeCoverPos& pos, const Vec3& center, const Vec3& forw, const Vec3& right, float& maxDist)
{
	SAIStrafeCoverPos* maxPos = 0;
	for (unsigned i = 0, ni = pos.child.size(); i < ni; ++i)
	{
		float d = maxDist;
		SAIStrafeCoverPos* res = GetFurthestFromTarget(pos.child[i], center, forw, right, d);
		if (res && d > maxDist)
		{
			maxPos = res;
			maxDist = d;
		}

		if (pos.child[i].valid)
		{
			//		d = Distance::Point_PointSq(target, pos.child[i].pos);
			d = fabsf(right.Dot(pos.child[i].pos - center)) + max(0.0f, -forw.Dot(pos.child[i].pos - center));
			if (d > maxDist)
			{
				maxPos = &pos.child[i];
				maxDist = d;
			}
		}
	}
	return maxPos;
}

//====================================================================
// GetNearestToTarget
//====================================================================
COPSeekCover::SAIStrafeCoverPos* COPSeekCover::GetNearestToTarget(SAIStrafeCoverPos& pos, const Vec3& center, const Vec3& forw, const Vec3& right, float& minDist)
{
	SAIStrafeCoverPos* minPos = 0;
	for (unsigned i = 0, ni = pos.child.size(); i < ni; ++i)
	{
		float d = minDist;
		SAIStrafeCoverPos* res = GetNearestToTarget(pos.child[i], center, forw, right, d);
		if (res && d < minDist)
		{
			minPos = res;
			minDist = d;
		}

		if (pos.child[i].valid)
		{
			d = Distance::Point_PointSq(center, pos.child[i].pos);
			if (d < minDist)
			{
				minPos = &pos.child[i];
				minDist = d;
			}
		}
	}
	return minPos;
}

//====================================================================
// GetPathTo
//====================================================================
bool COPSeekCover::GetPathTo(SAIStrafeCoverPos& pos, SAIStrafeCoverPos* pTarget, CNavPath& path, const Vec3& dirToTarget, IAISystem::tNavCapMask navCap, float passRadius)
{
	for (unsigned i = 0, ni = pos.child.size(); i < ni; ++i)
	{
		if (&pos.child[i] == pTarget)
		{
			// Try to assure that the point at the end of the path is a point where the character can aim.
			Vec3 adjPos = pos.child[i].pos;
			GetAISystem()->AdjustDirectionalCoverPosition(adjPos, dirToTarget, 0.5f, 0.7f);

			IAISystem::ENavigationType navType;
			if (!IsSegmentValid(navCap, passRadius, pos.pos, adjPos, navType))
			{
				adjPos = pos.child[i].pos;
				navType = pos.child[i].navType;
			}

			path.PushFront(PathPointDescriptor(navType, adjPos));

			return true;
		}
		if (GetPathTo(pos.child[i], pTarget, path, dirToTarget, navCap, passRadius))
		{
			path.PushFront(PathPointDescriptor(pos.child[i].navType, pos.child[i].pos));
			return true;
		}
	}
	return false;
}

//====================================================================
// UsePath
//====================================================================
void COPSeekCover::UsePath(CPipeUser* pOperand, SAIStrafeCoverPos* pPos, IAISystem::ENavigationType navType)
{
	pOperand->CancelRequestedPath(false);
	pOperand->ClearPath("COPSeekCover::Execute generate path");

	m_target = pPos->pos;

	Vec3 opPos = pOperand->GetPhysicsPos();

	SNavPathParams params(opPos, pPos->pos);
	params.precalculatedPath = true;
	pOperand->m_Path.SetParams(params);

	IAISystem::tNavCapMask navCap = pOperand->m_movementAbility.pathfindingProperties.navCapMask;
	const float passRadius = pOperand->GetParameters().m_fPassRadius;

	GetPathTo(*m_pRoot, pPos, pOperand->m_Path, m_forward, navCap, passRadius);

	pOperand->m_nPathDecision = PATHFINDER_PATHFOUND;
	pOperand->m_Path.PushFront(PathPointDescriptor(navType, opPos));

	pOperand->m_OrigPath = pOperand->m_Path;

	if (CPuppet* pPuppet = pOperand->CastToCPuppet())
	{
		// Update adaptive urgency control before the path gets processed further
		pPuppet->AdjustMovementUrgency(pPuppet->m_State.fMovementUrgency,
		                               pPuppet->m_Path.GetPathLength(!pPuppet->IsUsing3DNavigation()));
	}
}

//====================================================================
// Execute
//====================================================================
EGoalOpResult COPSeekCover::Execute(CPipeUser* pOperand)
{
	CCCPOINT(COPSeekCover_Execute);

	if (!m_pTraceDirective)
	{
		CRY_PROFILE_REGION(PROFILE_AI, "SeekCover/CalculatePathTree");

		if (m_state >= 0)
		{
			IAIObject* pTarget = pOperand->GetAttentionTarget();
			if (!pTarget && m_useLastOpAsBackup)
				pTarget = pOperand->m_refLastOpResult.GetAIObject();

			if (!pTarget)
			{
				Reset(pOperand);

				return eGOR_FAILED;
			}

			if (m_state == 0)
				gAIEnv.pActorLookUp->Prepare(ActorLookUp::Position);  // prefetch for GetNearestPuppets call below

			Vec3 center = pOperand->GetPhysicsPos();
			Vec3 target = pTarget->GetPos();

			Vec3 forw, right;
			float radiusLeft = m_radius;
			float radiusRight = m_radius;

			forw = target - center;
			forw.z = 0;
			forw.Normalize();
			right.Set(forw.y, -forw.x, 0);

			const Vec3& refPointPos = pOperand->GetRefPoint()->GetPos();

			if (m_towardsLastOpResult)
			{
				// Make the side which is towards the refpoint more dominant.
				if (right.Dot(refPointPos - center) > 0.0f)
					radiusLeft *= 0.5f;
				else
					radiusRight *= 0.5f;
			}

			// Choose the side first which is away from the target
			const Vec3& targetView = pTarget->GetViewDir();
			if (right.Dot(targetView) > 0.0f)
				right = -right;

			m_center = center;
			m_forward = forw;
			m_right = right;

			IAISystem::tNavCapMask navCap = pOperand->m_movementAbility.pathfindingProperties.navCapMask;
			const float passRadius = pOperand->GetParameters().m_fPassRadius;

			int nBuildingID;
			IAISystem::ENavigationType navType = gAIEnv.pNavigation->CheckNavigationType(center, nBuildingID, navCap);

			if (m_state == 0)
			{
				m_avoidPos.clear();
				GetNearestPuppets(pOperand->CastToCPuppet(), pOperand->GetPhysicsPos(), m_radius + 2.0f, m_avoidPos);

				if (m_minRadius > 0.0f)
					m_avoidPos.push_back(std::make_pair(pOperand->GetPos(), m_minRadius));

				// Get soft vegetation from navigation
				if (navType == IAISystem::NAV_TRIANGULAR)
				{
					// NAV_TRIANGULAR is replaced by MNM
					assert(false);
				}

				// Iteration 0
				SAFE_DELETE(m_pRoot);
				m_pRoot = new SAIStrafeCoverPos(center, false, true, navType);

				bool insideSoftCover = OverlapCircleAgaintsAvoidPos(center, passRadius / 2, m_softCoverPos);
				CalcStrafeCoverTree(*m_pRoot, 0, 0, center, forw, right, radiusRight, target, navCap, passRadius,
				                    insideSoftCover, m_avoidPos, m_softCoverPos); // right

				// Check to see if we can use the path already
				// Try to get to hidden point first
				float dist = FLT_MAX;
				SAIStrafeCoverPos* nearestHidden = GetNearestHidden(*m_pRoot, center, dist);
				if (nearestHidden)
				{
					UsePath(pOperand, nearestHidden, navType);
					m_pTraceDirective = new COPTrace(false, m_endAccuracy);
					m_lastTime = GetAISystem()->GetFrameStartTime();
					// Done calculating
					m_state = -1;
				}
				else
				{
					m_state++;
					return eGOR_IN_PROGRESS;
				}
			}
			else if (m_state == 1)
			{
				// Iteration 1
				bool insideSoftCover = OverlapCircleAgaintsAvoidPos(center, passRadius / 2, m_softCoverPos);
				CalcStrafeCoverTree(*m_pRoot, 0, 0, center, forw, -right, radiusLeft, target, navCap, passRadius,
				                    insideSoftCover, m_avoidPos, m_softCoverPos); // left

				// Try to get to hidden point first
				float dist = FLT_MAX;
				SAIStrafeCoverPos* nearestHidden = GetNearestHidden(*m_pRoot, center, dist);
				if (nearestHidden)
				{
					UsePath(pOperand, nearestHidden, navType);
				}
				else
				{
					// If no hidden points available, try to get as far from the target as possible.
					SAIStrafeCoverPos* furthest = 0;
					if (m_uncover)
					{
						dist = FLT_MAX;
						furthest = GetNearestToTarget(*m_pRoot, center, forw, right, dist);
					}
					else
					{
						dist = -FLT_MAX;
						furthest = GetFurthestFromTarget(*m_pRoot, center, forw, right, dist);
					}

					if (furthest)
					{
						UsePath(pOperand, furthest, navType);
					}
					else
					{
						if (gAIEnv.CVars.DebugPathFinding)
							AIWarning("COPSeekCover::Entity %s could not find path.", pOperand->GetName());

						Reset(pOperand);

						return eGOR_FAILED;
					}
				}

				m_pTraceDirective = new COPTrace(false, m_endAccuracy);
				m_lastTime = GetAISystem()->GetFrameStartTime();
				// Done calculating
				m_state = -1;
			}
		}
	}
	AIAssert(m_pTraceDirective);
	PREFAST_ASSUME(m_pTraceDirective);
	EGoalOpResult done = m_pTraceDirective->Execute(pOperand);
	if (m_pTraceDirective == NULL)
		return eGOR_IN_PROGRESS;

	AIAssert(m_pTraceDirective);

	// HACK: The following code tries to put a lost or stuck agent back on track.
	// It works together with a piece of in ExecuteDry which tracks the speed relative
	// to the requested speed and if it drops dramatically for certain time, this code
	// will trigger and try to move the agent back on the path. [Mikko]

	int timeout = 1500;
	if (pOperand->GetType() == AIOBJECT_VEHICLE)
		timeout = 7000;

	if (m_notMovingTimeMs > timeout)
	{
		// Stuck or lost, move to the nearest point on path.
		AIWarning("COPSeekCover::Entity %s has not been moving fast enough for %.1fs it might be stuck, abort.",
		          pOperand->GetName(), m_notMovingTimeMs * 0.001f);

		Reset(pOperand);

		return eGOR_FAILED;
	}

	if (done != eGOR_IN_PROGRESS)
	{
		Reset(pOperand);

		return done;
	}
	if (!m_pTraceDirective)
	{
		Reset(pOperand);

		return eGOR_FAILED;
	}

	return eGOR_IN_PROGRESS;
}

//====================================================================
// ExecuteDry
//====================================================================
void COPSeekCover::ExecuteDry(CPipeUser* pOperand)
{
	pOperand->SetMovingToCover(!m_uncover);

	CAISystem* pSystem = GetAISystem();

	if (m_pTraceDirective)
	{
		m_pTraceDirective->ExecuteTrace(pOperand, false);

		// HACK: The following code together with some logic in the execute tries to keep track
		// if the agent is not moving for some time (is stuck), and pathfinds back to the path. [Mikko]
		CTimeValue time(pSystem->GetFrameStartTime());
		int64 dt((time - m_lastTime).GetMilliSecondsAsInt64());

		float speed = pOperand->GetVelocity().GetLength();
		float desiredSpeed = pOperand->m_State.fDesiredSpeed;
		if (desiredSpeed > 0.0f)
		{
			float ratio = clamp_tpl(speed / desiredSpeed, 0.0f, 1.0f);
			if (ratio < 0.1f)
				m_notMovingTimeMs += (int)dt;
			else
				m_notMovingTimeMs -= (int)dt;

			if (m_notMovingTimeMs < 0)
				m_notMovingTimeMs = 0;
		}

		m_lastTime = time;
	}
}

//===================================================================
// DebugDraw
//===================================================================
void COPSeekCover::DebugDraw(CPipeUser* pOperand) const
{
	if (m_pTraceDirective)
		m_pTraceDirective->DebugDraw(pOperand);

	Vec3 target;
	if (pOperand->GetAttentionTarget())
		target = pOperand->GetAttentionTarget()->GetPos();
	else
		target = pOperand->GetPos() + pOperand->GetEntityDir() * 10.0f;

	CDebugDrawContext dc;

	if (m_pRoot)
	{
		dc->DrawLine(m_center, ColorB(255, 32, 32), m_center + m_right * 4.0f, ColorB(255, 32, 32), 4.0f);
		dc->DrawLine(m_center, ColorB(32, 255, 32), m_center + m_forward * 4.0f, ColorB(32, 255, 32), 4.0f);

		DrawStrafeCoverTree(*m_pRoot, target);
	}

	dc->DrawCone(m_target + Vec3(0, 0, 1), Vec3(0, 0, -1), 0.15f, 0.6f, ColorB(255, 255, 255));

	for (unsigned i = 0, ni = m_avoidPos.size(); i < ni; ++i)
		dc->DrawCircleOutline(m_avoidPos[i].first, m_avoidPos[i].second, ColorB(255, 0, 0));

	for (unsigned i = 0, ni = m_softCoverPos.size(); i < ni; ++i)
		dc->DrawCircleOutline(m_softCoverPos[i].first, m_softCoverPos[i].second, ColorB(196, 255, 0));
}

//===================================================================
// DrawStrafeCoverTree
//===================================================================
void COPSeekCover::DrawStrafeCoverTree(SAIStrafeCoverPos& pos, const Vec3& target) const
{
	CDebugDrawContext dc;

	for (unsigned i = 0, ni = pos.child.size(); i < ni; ++i)
	{
		unsigned char a = pos.child[i].valid ? 255 : 120;
		dc->DrawLine(pos.pos, ColorB(255, 255, 255), pos.child[i].pos, ColorB(255, 255, 255));
		dc->DrawSphere(pos.child[i].pos, 0.15f, ColorB(255, 255, 255, a));
		if (!pos.child[i].vis)
			dc->DrawSphere(pos.child[i].pos + Vec3(0, 0, 0.7f), 0.25f, ColorB(255, 0, 0));
		//		else
		//			dc->DrawLine(pos.child[i].pos + Vec3(0, 0, 0.7f), ColorB(255, 255, 255), target, ColorB(255, 255, 255));

		DrawStrafeCoverTree(pos.child[i], target);
	}
}
