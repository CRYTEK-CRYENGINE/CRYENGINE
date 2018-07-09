// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "NetPlayerInput.h"
#include "Player.h"
#include "Game.h"
#include "GameCVars.h"
#include "Weapon.h"
#include <CryAISystem/IAIActor.h>
#include "PlayerStateEvents.h"
#if SNAP_ERROR_LOGGING
#include <CrySystem/ISystem.h>
#include <CrySystem/Profilers/IStatoscope.h>
#endif // SNAP_ERROR_LOGGING

#include "Utility/CryWatch.h"
#include <CryMath/Cry_GeoDistance.h>


/*
=====================================================================================
	CNetLerper::SSettings
=====================================================================================
*/
CNetLerper::SSettings::SSettings()
{
	maxInterSpeed = 12.f;
	lookAhead = 0.1f;
	maxLookAhead = 0.05f;
	snapDistXY = 2.5f;
	snapDistZ = 3.f;
	snapDistZInAir = 6.f;
	allowableLerpErrorXY = 1.5f;
	minLerpSpeed = 0.5f;
	minLerpDistance = 0.02f;
	minTimeOnGround = 0.5f;
	snapDistMarkedMajor = 1.4f;
}

/*
=====================================================================================
	CNetLerper::Debug
=====================================================================================
*/

#if !defined(_RELEASE)

struct CNetLerper::Debug
{
	enum { k_maxNum = 10000 };

	struct Desired
	{
		Vec3 pos;
		Vec3 vel;
		float time;
	};

	struct Actual
	{
		Vec3 pos;
		Vec3 vel;
		Vec3 lerpError;
		bool snapped;
		float time;
	};

	std::vector<Desired> m_desired;
	std::vector<Actual> m_actual;
	float m_time0;
	float m_timer;

	Debug()
	{
		m_time0 = 0.f;
		m_timer = 0.f;
	}

	void Reset()
	{
		m_desired.clear();
		m_actual.clear();
		m_time0 = 0.f;
	}

	bool CanAdd()
	{
		return (m_desired.size()<k_maxNum && m_actual.size()<k_maxNum);
	}

	void AddDesired(Vec3 p, Vec3 v)
	{
		if (!CanAdd()) return;
		if (m_time0==0.f)
			m_time0 = gEnv->pTimer->GetCurrTime();
		Desired d;
		d.pos = p;
		d.vel = v;
		d.time = gEnv->pTimer->GetCurrTime() - m_time0;
		m_desired.push_back(d);
	}

	void AddCurrent(Vec3 p, Vec3 v, Vec3 lerpError, bool snap)
	{
		if (!CanAdd()) return;
		if (m_time0==0.f)
			m_time0 = gEnv->pTimer->GetCurrTime();
		Actual a;
		a.pos = p;
		a.vel = v;
		a.lerpError = lerpError;
		a.snapped = snap;
		a.time = gEnv->pTimer->GetCurrTime() - m_time0;
		m_actual.push_back(a);
	}

	void Draw()
	{
		IRenderAuxGeom* pRender = gEnv->pRenderer->GetIRenderAuxGeom();
		SAuxGeomRenderFlags flags = pRender->GetRenderFlags();
		SAuxGeomRenderFlags oldFlags = pRender->GetRenderFlags();
		flags.SetDepthWriteFlag(e_DepthWriteOff);
		flags.SetDepthTestFlag(e_DepthTestOff);
		pRender->SetRenderFlags(flags);
	
		m_timer += gEnv->pTimer->GetFrameTime();
		if (m_timer>30.f) m_timer = 0.f;
		float time = gEnv->pTimer->GetCurrTime();
		float dt = (1.f/50.f);
		Vec3 offset = Vec3(0.f, 0.f, 0.025f + 0.003f*sinf(8.f*m_timer));
		Vec3 offset2 = Vec3(0.f, 0.f, 0.035f + 0.003f*sinf(5.f*m_timer));

		ColorB desiredColour = ColorB(255,0,0,255);  // Red
		ColorB desiredVelColour = ColorB(255,(int)(128.f+127.f*sinf(8.f*m_timer)),0,255);  // Yellow/Red
		ColorB actualPosColour = ColorB(0,255,0,255);  // Green
		ColorB actualVelColour = ColorB(0,0,(int)(128.f+127.f*sinf(5.f*m_timer)),255);  // blue/black
		ColorB snapPosColour = ColorB(255,255,255,255); // White
		ColorB lerpErrorColour = ColorB(255,0,0,255); // Red
		
		// Draw the desired positions
		for (unsigned int i=0; i<m_desired.size(); i++)
		{
			Desired &d = m_desired[i];
			pRender->DrawSphere(d.pos + offset, 0.025f, desiredColour);
			pRender->DrawLine(d.pos + offset, desiredVelColour, d.pos + offset + d.vel*dt, desiredVelColour);
		}

		if(g_pGameCVars->pl_debugInterpolation == 1) // Show entity position + velocity
		{
			for (unsigned int i=0; i<m_actual.size(); i++)
			{
				Actual &a = m_actual[i];
				pRender->DrawSphere(a.pos + offset2, 0.025f, a.snapped ? snapPosColour : actualPosColour);
				pRender->DrawLine(a.pos + offset2, actualVelColour, a.pos + offset2 + a.vel*dt, actualVelColour);
			}
		}

		if(g_pGameCVars->pl_debugInterpolation == 2) // Show entity position + lerpError
		{
			for (unsigned int i=0; i<m_actual.size(); i++)
			{
				Actual &a = m_actual[i];
				pRender->DrawSphere(a.pos + offset2, 0.025f, a.snapped ? snapPosColour : actualPosColour);
				pRender->DrawLine(a.pos + offset2, lerpErrorColour, a.pos + offset2 + a.lerpError, lerpErrorColour);
			}
		}
		
		pRender->SetRenderFlags(oldFlags);
	}
};

#endif // !defined(_RELEASE), CNetLerper::Debug

/*
=====================================================================================
	CNetLerper
=====================================================================================
*/
CNetLerper::CNetLerper()
{
	m_settings = NULL;
#if !defined(_RELEASE)
	m_debug = NULL;
#endif
	Reset(); 
}

CNetLerper::~CNetLerper()
{
#if !defined(_RELEASE)
	SAFE_DELETE(m_debug);
#endif
}

void CNetLerper::Reset(CPlayer* player)
{
	m_pPlayer = player;
	m_clock = -1.f;
	m_desired.pos.zero();
	m_desired.worldPos.zero();
	m_desired.vel.zero();
	m_lerpedPos.zero();
	m_lerpedError.zero();
	m_standingOn = 0;
	m_snapType = eSnap_None;
	m_enabled = false;

#if SNAP_ERROR_LOGGING
	m_snapErrorInProgress = false;
#endif // SNAP_ERROR_LOGGING

#if !defined(_RELEASE)
	SAFE_DELETE(m_debug);
#endif
}
	
void CNetLerper::ApplySettings(SSettings* settings)
{
	m_settings = settings;
}

void CNetLerper::AddNewPoint(const Vec3& inPos, const Vec3& inVel, const Vec3& entityPos, EntityId standingOn)
{
	CRY_ASSERT(m_settings);

	m_desired.pos = inPos;
	m_desired.worldPos = inPos;
	m_standingOn = standingOn;
	if (IEntity* pGroundEntity = gEnv->pEntitySystem->GetEntity(standingOn))
	{
		m_desired.worldPos = pGroundEntity->GetWorldTM() * inPos;
	}
	
	// Run the desired velocity at slightly below the real velocity
	// helps to smooth the lerping since there is large granularity in
	// the network quantisation of the velocity
	m_desired.vel = inVel * 0.9f;

#if SNAP_ERROR_LOGGING
	if (m_snapErrorInProgress)
	{
		if ((m_pPlayer != NULL) && (!m_pPlayer->IsDead()))
		{
			CryFixedStringT<128> buffer;
			buffer.Format("[netlerper] prediction error ended for [%s] after %fs", m_pPlayer->GetEntity()->GetName(), m_clock);
#if ENABLE_STATOSCOPE
			if(gEnv->pStatoscope)
			{
				gEnv->pStatoscope->AddUserMarker("NetLerper", buffer.c_str());
			}
#endif // ENABLE_STATOSCOPE
			CryLog("%s", buffer.c_str());
			m_snapErrorInProgress = false;
		}
	}
#endif // SNAP_ERROR_LOGGING

	// Zero clock and re calibrate the lerp error
	m_clock = 0.f;
	m_lerpedError = entityPos - m_desired.worldPos;
	m_lerpedPos = m_desired.worldPos;

	m_enabled = true;
}

void CNetLerper::LogSnapError()
{
#if SNAP_ERROR_LOGGING
	if (!m_snapErrorInProgress)
	{
		if ((m_pPlayer != NULL) && (!m_pPlayer->IsDead()))
		{
			CryFixedStringT<128> buffer;
			buffer.Format("[netlerper] prediction error started for [%s]", m_pPlayer->GetEntity()->GetName());
#if ENABLE_STATOSCOPE
			if(gEnv->pStatoscope)
			{
				gEnv->pStatoscope->AddUserMarker("NetLerper", buffer.c_str());
			}
#endif // ENABLE_STATOSCOPE
			CryLog("%s", buffer.c_str());
			m_snapErrorInProgress = true;
		}
	}
#endif // SNAP_ERROR_LOGGING
}

int CNetLerper::GetLerpError(const Vec3& errorToDesiredPos, const Vec3& lerpError) const
{
	int error = k_noError;
	float snapDistZ = m_settings->snapDistZ;

	if (m_pPlayer)
	{
		const float timeOnGround = m_pPlayer->GetActorStats()->onGround - m_settings->minTimeOnGround;
		snapDistZ = (float)__fsel(timeOnGround, m_settings->snapDistZInAir, snapDistZ);
	}

	if ((errorToDesiredPos.GetLengthSquared2D() >= sqr(m_settings->snapDistXY)) || (fabsf(errorToDesiredPos.z) >= m_settings->snapDistZ))
		error |= k_desiredError;


	if (lerpError.GetLengthSquared2D() > sqr(m_settings->allowableLerpErrorXY))
		error |= k_lerpError;

	return error;
}
	
void CNetLerper::Update(float dt, const Vec3& entityPos, SPrediction& predictionOut, const Vec3& velGround, bool bInAirOrJumping)
{
	if(!m_enabled)
	{
		predictionOut.predictedPos = entityPos;
		predictionOut.lerpVel.zero();
		predictionOut.shouldSnap = false;
		return;
	}

	CRY_ASSERT(m_settings);
	dt = max(dt, 0.001f);
	
	IEntity* pGroundEntity = gEnv->pEntitySystem->GetEntity(m_standingOn);
	m_desired.worldPos = m_desired.pos;
	if (pGroundEntity)
	{
		if (IPhysicalEntity* pGroundPhys = pGroundEntity->GetPhysics())
		{
			pe_status_pos psp;
			pGroundPhys->GetStatus(&psp);
			m_desired.worldPos = psp.q * m_desired.pos + psp.pos;
		}
	}

	// Prediction is done a "long" time ahead
	const float predictTime = min(m_clock + m_settings->lookAhead, m_settings->maxLookAhead);
	const Vec3 predictedPos = m_desired.worldPos + (m_desired.vel * predictTime);
	const Vec3 predOffset = predictedPos - entityPos;
	const float predDist = predOffset.GetLength();

	// Errors:
	m_lerpedError = entityPos - predictedPos;
	// Error between desired pos (nb: not predicted pos)
	const Vec3 errorToDesiredPos = entityPos - m_desired.worldPos;
	const int snapError = GetLerpError(errorToDesiredPos, m_lerpedError);

	m_clock += dt;

	const float lerpDist = predDist + (dt*velGround).GetLength();

	if (lerpDist<m_settings->minLerpDistance && m_desired.vel.GetLengthSquared() < sqr(m_settings->minLerpSpeed) && !bInAirOrJumping)  // Stop lerping
	{
		// This block should be entered as few times as possible while on a moving platform.
		predictionOut.predictedPos = predictedPos;
		predictionOut.lerpVel.zero();
		predictionOut.shouldSnap = false;
		m_lerpedPos = m_desired.worldPos;
		m_lerpedError.zero();

		if (m_snapType== eSnap_None)
		{
			predictionOut.shouldSnap = true;
			m_snapType = eSnap_Minor;
			LogSnapError();
		}
	}
	else if (snapError & k_desiredError)  // k_lerpError is ignored because it works improperly during collisions with living entities
	{
		predictionOut.predictedPos = m_desired.worldPos;
		predictionOut.lerpVel.zero();
		predictionOut.shouldSnap = true;
		m_lerpedPos = m_desired.worldPos;
		m_lerpedError.zero();
		if(errorToDesiredPos.GetLengthSquared() > sqr(m_settings->snapDistMarkedMajor))
		{
			m_snapType = eSnap_Major;
		}
		else
		{
			m_snapType = eSnap_Normal;
		}
		LogSnapError();
	}
	else 
	{
		// Calculate simple lerp velocity
		Vec3 lerpVel = predOffset * (float)__fres(m_settings->lookAhead);
			
		// Clamp it
		const float maxPredictionDistance = m_settings->maxInterSpeed * m_settings->lookAhead;
		if (predDist > maxPredictionDistance)
			lerpVel *= maxPredictionDistance * (float)__fres(predDist);

		// Output
		predictionOut.predictedPos = predictedPos;
		predictionOut.lerpVel = lerpVel;
		predictionOut.shouldSnap = false;
		m_snapType = eSnap_None;
	}

	// Keep this in local space
	m_lerpedPos += dt * (predictionOut.lerpVel + velGround);
	m_lerpedPos += (m_desired.worldPos - m_lerpedPos) * 0.05f;	// Keep on top of any drift
}


void CNetLerper::DebugDraw(const SPrediction& prediction, const Vec3& entityPos, bool hadNewData)
{
#if !defined(_RELEASE)
	if (m_debug == NULL)
		m_debug = new Debug;
	
	Vec3 desiredVelocity = m_desired.vel;
	Vec3 lerpVel = prediction.lerpVel;
	Vec3 desiredPosition = m_desired.worldPos;
	Vec3 predictedPosition = prediction.predictedPos;

	if(g_pGameCVars->pl_debugInterpolation > 1)
	{
		CryWatch("Cur: (%.2f, %.2f, %.2f) Des: (%.2f, %.2f, %.2f) Pred: (%.2f, %.2f, %.2f) ", entityPos.x, entityPos.y, entityPos.z, desiredPosition.x, desiredPosition.y, desiredPosition.z, predictedPosition.x, predictedPosition.y, predictedPosition.z);
		CryWatch("LerpErrorXY: (%.2f)", m_lerpedError.GetLength2D());
		CryWatch("InputSpeed: (%.2f, %.2f, %.2f) lerpVel: (%.2f, %.2f, %.2f) lerpSpeed: %.2f", desiredVelocity.x, desiredVelocity.y, desiredVelocity.z, lerpVel.x, lerpVel.y, lerpVel.z, lerpVel.GetLength());
	}	
	if (hadNewData)
	{
		m_debug->AddDesired(m_desired.worldPos, m_desired.vel);
	}
	m_debug->AddCurrent(prediction.shouldSnap ? prediction.predictedPos : entityPos, prediction.lerpVel, m_lerpedError, prediction.shouldSnap);
	m_debug->Draw();
#endif
}


// ==================== CNetPlayerInput ======================


CNetLerper::SSettings CNetPlayerInput::m_lerperSettings;


CNetPlayerInput::CNetPlayerInput( CPlayer * pPlayer ) 
: 
	m_pPlayer(pPlayer),
	m_newInterpolation(false)
{
	m_desiredVelocity.Set(0.0f, 0.0f, 0.0f);
	m_lookDir(0.0f, 1.0f, 0.0f);
	m_lerpVel.zero();

	m_lerper.ApplySettings(&m_lerperSettings);
	m_lerper.Reset(pPlayer);
}

void CNetPlayerInput::UpdateInterpolation()
{
	Vec3 desiredPosition = m_curInput.position;
	Vec3 desiredVelocity = m_curInput.deltaMovement * g_pGameCVars->pl_netSerialiseMaxSpeed;

	// Use the physics pos as the entity position is a frame behind at this point
	IPhysicalEntity * pent = m_pPlayer->GetEntity()->GetPhysics();
	pe_status_pos psp;
	pent->GetStatus(&psp);
	Vec3 entPos = psp.pos;

	pe_status_living psl;
	psl.velGround.zero();
	pent->GetStatus(&psl);
	
	float dt = gEnv->pTimer->GetFrameTime();

	// New data?
	if (m_newInterpolation)
		m_lerper.AddNewPoint(m_curInput.position, desiredVelocity, entPos, m_curInput.standingOn);

	bool bInAirOrJumping = m_pPlayer->GetActorStats()->inAir > 0.01f || m_pPlayer->GetActorStats()->onGround < 0.01f;

	// Predict
	CNetLerper::SPrediction prediction;
	m_lerper.Update(gEnv->pTimer->GetFrameTime(), entPos, prediction, psl.velGround, bInAirOrJumping);

	// Update lerp velocity
	m_lerpVel = prediction.lerpVel;

	// Should Snap
	if (prediction.shouldSnap)
	{
		m_pPlayer->GetEntity()->SetPos(prediction.predictedPos);

		pe_action_set_velocity actionVel;
		actionVel.v = prediction.lerpVel;
		pent->Action(&actionVel);
	}

#if !defined(_RELEASE)
	// Debug Draw
	if (g_pGameCVars->pl_debugInterpolation)
		m_lerper.DebugDraw(prediction, entPos, m_newInterpolation);
	else
		SAFE_DELETE(m_lerper.m_debug);
#endif

	m_newInterpolation = false;
}

void CNetPlayerInput::UpdateMoveRequest()
{
	CMovementRequest moveRequest;
	SMovementState moveState;
	m_pPlayer->GetMovementController()->GetMovementState(moveState);
	Quat worldRot = m_pPlayer->GetBaseQuat(); // m_pPlayer->GetEntity()->GetWorldRotation();
	Vec3 deltaMovement = worldRot.GetInverted().GetNormalized() * m_curInput.deltaMovement;
	// absolutely ensure length is correct
	deltaMovement = deltaMovement.GetNormalizedSafe(ZERO) * m_curInput.deltaMovement.GetLength();
	moveRequest.AddDeltaMovement( deltaMovement );
	if( IsDemoPlayback() )
	{
		Vec3 localVDir(m_pPlayer->GetViewQuatFinal().GetInverted() * m_curInput.lookDirection);
		Ang3 deltaAngles(asinf(localVDir.z),0,atan2_tpl(-localVDir.x,localVDir.y));
		moveRequest.AddDeltaRotation(deltaAngles*gEnv->pTimer->GetFrameTime());
	}
	
	const float fNetAimLerpFactor = g_pGameCVars->pl_netAimLerpFactor;

	//Vector slerp produces artifacts here, using a per-component lerp instead
	Vec3 vCurrentRight			= m_lookDir.cross(Vec3Constants<float>::fVec3_OneZ);
	Vec3 vCurrentProjected = -(vCurrentRight.cross(Vec3Constants<float>::fVec3_OneZ));
	vCurrentRight.Normalize();
	vCurrentProjected.Normalize();

	Vec3 vNewRight					= m_curInput.lookDirection.cross(Vec3Constants<float>::fVec3_OneZ);
	Vec3 vNewProjected			= -(vNewRight.cross(Vec3Constants<float>::fVec3_OneZ));
	vNewProjected.Normalize();

	float fRotZDirDot	= vNewProjected.dot(vCurrentRight);
	float fRotZDot		= vNewProjected.dot(vCurrentProjected);
	float fRotZ			= crymath::acos(crymath::clamp(fRotZDot, -1.0f, 1.0f));

	fRotZ = AngleWrap_PI(fRotZ);

	float fRotZFinal	= -fsgnf(fRotZDirDot) * fRotZ * fNetAimLerpFactor;

	float fCurrentAngle = crymath::acos(crymath::clamp(
		Vec3Constants<float>::fVec3_OneZ.dot(m_lookDir), -1.0f, 1.0f));
	float fNewAngle		= crymath::acos(crymath::clamp(
		Vec3Constants<float>::fVec3_OneZ.dot(m_curInput.lookDirection), -1.0f, 1.0f));

	float fRotXFinal = (fNewAngle - fCurrentAngle) * -fNetAimLerpFactor;

	//Rotate around X first, as we have already generated the right vector
	Vec3 vNewLookDir = m_lookDir.GetRotated(vCurrentRight, fRotXFinal);

	m_lookDir = vNewLookDir.GetRotated(Vec3Constants<float>::fVec3_OneZ, fRotZFinal);

	Vec3 distantTarget = moveState.eyePosition + 1000.0f * m_lookDir;
	Vec3 lookTarget = distantTarget;

	if (m_curInput.usinglookik)
		moveRequest.SetLookTarget( lookTarget );
	else
		moveRequest.ClearLookTarget();

	if (m_curInput.aiming)
		moveRequest.SetAimTarget( lookTarget );
	else
		moveRequest.ClearAimTarget();

	if (m_curInput.deltaMovement.GetLengthSquared() > sqr(0.02f)) // 0.2f is almost stopped
		moveRequest.SetBodyTarget( distantTarget );
	else
		moveRequest.ClearBodyTarget();

	moveRequest.SetAllowStrafing(m_curInput.allowStrafing);

	moveRequest.SetPseudoSpeed(CalculatePseudoSpeed());

	moveRequest.SetStance( (EStance)m_curInput.stance );

	m_pPlayer->GetMovementController()->RequestMovement(moveRequest);

	if (m_curInput.sprint)
		m_pPlayer->m_actions |= ACTION_SPRINT;
	else
		m_pPlayer->m_actions &= ~ACTION_SPRINT;

#if 0
	if (m_pPlayer->m_netSetPosition)
	{
		SPredictedCharacterStates charStates;
		charStates.states[0].position		 = m_pPlayer->GetEntity()->GetPos();
		charStates.states[0].orientation = m_pPlayer->GetEntity()->GetRotation();
		charStates.states[0].deltatime	 = 0.0f;

		charStates.states[1].position		 = m_pPlayer->m_netCurrentLocation;
		charStates.states[1].orientation = m_pPlayer->GetEntity()->GetRotation();
		charStates.states[1].deltatime	 = gEnv->pTimer->GetFrameTime();

		charStates.states[2].position		 = m_pPlayer->m_netDesiredLocation;
		charStates.states[2].orientation = m_pPlayer->GetEntity()->GetRotation();
		charStates.states[2].deltatime	 = m_pPlayer->m_netLerpTime;
		charStates.nStates = 3;

		moveRequest.SetPrediction(charStates);
	}
#endif //0

#if !defined(_RELEASE)
	// debug..
	if (g_pGameCVars->g_debugNetPlayerInput & 2)
	{
		IPersistantDebug * pPD = gEnv->pGameFramework->GetIPersistantDebug();
		pPD->Begin( string("update_player_input_") + m_pPlayer->GetEntity()->GetName(), true );
		Vec3 wp = m_pPlayer->GetEntity()->GetWorldPos();
		wp.z += 2.0f;
		pPD->AddSphere( moveRequest.GetLookTarget(), 0.5f, ColorF(1,0,1,0.3f), 1.0f );
		//		pPD->AddSphere( moveRequest.GetMoveTarget(), 0.5f, ColorF(1,1,0,0.3f), 1.0f );
		pPD->AddDirection( m_pPlayer->GetEntity()->GetWorldPos() + Vec3(0,0,2), 1, m_curInput.deltaMovement, ColorF(1,0,0,0.3f), 1.0f );
	}
#endif

	//m_curInput.deltaMovement.zero();
} 

void CNetPlayerInput::PreUpdate()
{
	IPhysicalEntity * pPhysEnt = m_pPlayer->GetEntity()->GetPhysics();

	if (pPhysEnt && !m_pPlayer->IsDead() && m_pPlayer->GetLinkedVehicle()==NULL && m_pPlayer->GetActorStats()->isAnimatedSlave==0)
	{
		if (m_pPlayer->AllowPhysicsUpdate(m_curInput.physCounter) && m_pPlayer->IsRemote() && HasReceivedUpdate())
		{
			UpdateInterpolation();
		}
		else
		{
			m_newInterpolation = false;
		}
		UpdateDesiredVelocity();
		UpdateMoveRequest();
	}
	else
	{
		m_lerper.Disable();
		m_lerpVel.zero();
	}
}

void CNetPlayerInput::Update()
{
	if (gEnv->bServer && (g_pGameCVars->sv_input_timeout>0) && ((gEnv->pTimer->GetFrameStartTime()-m_lastUpdate).GetMilliSeconds()>=g_pGameCVars->sv_input_timeout))
	{
		m_curInput.deltaMovement.zero();
		m_curInput.sprint=false;
		m_curInput.stance=(uint8)STANCE_NULL;

		CHANGED_NETWORK_STATE(m_pPlayer,  CPlayer::ASPECT_INPUT_CLIENT );
	}
}

void CNetPlayerInput::PostUpdate()
{
}

void CNetPlayerInput::SetState( const SSerializedPlayerInput& input )
{
	DoSetState(input);

	m_lastUpdate = gEnv->pTimer->GetCurrTime();
}

void CNetPlayerInput::GetState( SSerializedPlayerInput& input )
{
	input = m_curInput;
}

void CNetPlayerInput::Reset()
{
	CRY_TODO(24, 3, 2010, "Consider feeding into a timeout onto the inputs here");
	//SSerializedPlayerInput i(m_curInput);
	//i.leanl=i.leanr=i.sprint=false;
	//i.deltaMovement.zero();

	//DoSetState(i);

	//CHANGED_NETWORK_STATE(m_pPlayer, CPlayer::ASPECT_INPUT_CLIENT);
}

void CNetPlayerInput::DisableXI(bool disabled)
{
}

float CNetPlayerInput::CalculatePseudoSpeed() const
{
	const float desiredSpeed = m_desiredVelocity.len();
	const float pseudoSpeed = m_pPlayer->CalculatePseudoSpeed(m_curInput.sprint, desiredSpeed);

#if 0
	const float XPOS = 500.0f;
	const float YPOS = 100.0f;
	const float COLOUR[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	IRenderAuxText::Draw2dLabel(XPOS, YPOS, 2.0f, COLOUR, false, "CurSpeed: %.2f CurISpeed: %f PS: %.2f", actualSpeed, inputSpeed, pseudoSpeed);
#endif 

	return pseudoSpeed;
}

void CNetPlayerInput::DoSetState(const SSerializedPlayerInput& input )
{
	m_newInterpolation |= (input.position != m_curInput.position) || (input.deltaMovement != m_curInput.deltaMovement);

	const bool wasSprinting = m_curInput.sprint;

	m_curInput = input;
	CHANGED_NETWORK_STATE(m_pPlayer,  CPlayer::ASPECT_INPUT_CLIENT );

	if(wasSprinting != input.sprint)
	{
		SInputEventData inputEventData( SInputEventData::EInputEvent_Sprint, m_pPlayer->GetEntityId(), CCryName("sprint"), input.sprint ? eAAM_OnPress : eAAM_OnRelease, 0.f );
		m_pPlayer->StateMachineHandleEventMovement( SStateEventPlayerInput( &inputEventData ) );
	}

	// not having these set seems to stop a remote avatars rotation being reflected
	m_curInput.aiming = true;
	m_curInput.allowStrafing = true;
	m_curInput.usinglookik = true;

	IAIActor* pAIActor = CastToIAIActorSafe(m_pPlayer->GetEntity()->GetAI());
	if (pAIActor)
		pAIActor->GetState().bodystate=input.bodystate;

	CMovementRequest moveRequest;
	moveRequest.SetStance( (EStance)m_curInput.stance );

	if(IsDemoPlayback())
	{
		Vec3 localVDir(m_pPlayer->GetViewQuatFinal().GetInverted() * m_curInput.lookDirection);
		Ang3 deltaAngles(asinf(localVDir.z),0,atan2_tpl(-localVDir.x,localVDir.y));
		moveRequest.AddDeltaRotation(deltaAngles*gEnv->pTimer->GetFrameTime());
	}

	moveRequest.SetPseudoSpeed(CalculatePseudoSpeed());
	moveRequest.SetAllowStrafing(input.allowStrafing);

	m_pPlayer->GetMovementController()->RequestMovement(moveRequest);

#if !defined(_RELEASE)
	// debug..
	if (g_pGameCVars->g_debugNetPlayerInput & 1)
	{
		IPersistantDebug * pPD = gEnv->pGameFramework->GetIPersistantDebug();
		pPD->Begin( string("net_player_input_") + m_pPlayer->GetEntity()->GetName(), true );
		pPD->AddSphere( moveRequest.GetLookTarget(), 0.5f, ColorF(1,0,1,1), 1.0f );
		//			pPD->AddSphere( moveRequest.GetMoveTarget(), 0.5f, ColorF(1,1,0,1), 1.0f );

		Vec3 wp(m_pPlayer->GetEntity()->GetWorldPos() + Vec3(0,0,2));
		pPD->AddDirection( wp, 1.5f, m_curInput.deltaMovement, ColorF(1,0,0,1), 1.0f );
		pPD->AddDirection( wp, 1.5f, m_curInput.lookDirection, ColorF(0,1,0,1), 1.0f );
	}
#endif
}

void CNetPlayerInput::UpdateDesiredVelocity()
{
	m_desiredVelocity = m_lerpVel;
}
