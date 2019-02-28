// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include <CryMath/Cry_Math.h>
#include "ExactPositioningTrigger.h"
#include "AnimationGraph/AnimationGraphCVars.h"

CExactPositioningTrigger::CExactPositioningTrigger()
: m_state(eS_Invalid)
, m_pos(ZERO)
, m_width(1.0f)
, m_posSize(1.0f)
, m_orient(Quat::CreateIdentity())
, m_cosOrientTolerance(1.0f)
, m_sideTime(0)
, m_distanceErrorFactor(1.0f)
, m_animMovementLength(0)
, m_distanceError(1000.0f)
, m_orientError(1000.0f)
, m_oldFwdDir(0.0f)
, m_userPos(ZERO)
, m_userOrient(Quat::CreateIdentity())
{

}

CExactPositioningTrigger::CExactPositioningTrigger( const Vec3& pos, float width, const Vec3& triggerSize, const Quat& orient, float orientTolerance, float animMovementLength )
	: m_pos(pos)
	, m_width(width)
	, m_posSize(triggerSize)
	, m_orient(orient)
	, m_cosOrientTolerance(cos(orientTolerance))
	, m_sideTime(0)
	, m_distanceErrorFactor(1.0f)
	, m_animMovementLength(0)
	, m_distanceError(1000.0f)
	, m_orientError(1000.0f)
	, m_oldFwdDir(0.0f)
	, m_state(eS_Initializing)
	, m_userPos(pos)
	, m_userOrient(orient)
{
	CRY_ASSERT(m_pos.IsValid());
	CRY_ASSERT(m_userPos.IsValid());
	CRY_ASSERT(m_orient.IsValid());
	CRY_ASSERT(m_userOrient.IsValid());
	CRY_ASSERT(m_posSize.IsValid());
	CRY_ASSERT(NumberValid(m_cosOrientTolerance));
	CRY_ASSERT(NumberValid(m_width));

	// Make sure we have bigger than zero length, 
	// so that the error measurement is not zero no matter what direction.
	m_animMovementLength = max(animMovementLength, 2.0f);

	// Make sure the trigger is not rotated, only Z-axis rotations are allowed
	m_orient.v.x = m_orient.v.y = 0.0f; m_orient.Normalize();
	m_userOrient = m_orient;
}

void CExactPositioningTrigger::Update( float frameTime, Vec3 userPos, Quat userOrient, bool allowTriggering )
{
	if (m_state == eS_Invalid)
		return;

	CRY_ASSERT(m_pos.IsValid());
	CRY_ASSERT(m_userPos.IsValid());
	CRY_ASSERT(m_orient.IsValid());
	CRY_ASSERT(m_userOrient.IsValid());
	CRY_ASSERT(m_posSize.IsValid());
	CRY_ASSERT(NumberValid(m_cosOrientTolerance));

	CRY_ASSERT(NumberValid(frameTime));
	CRY_ASSERT(userPos.IsValid());
	CRY_ASSERT(userOrient.IsValid());

	m_userPos = userPos;
	m_userOrient = userOrient;

	if (m_state == eS_Initializing)
		m_state = eS_Before;

	Plane threshold;
	threshold.SetPlane( m_orient.GetColumn1(), m_pos );
	if (threshold.DistFromPlane(userPos) >= 0.0f)
	{
		if (m_sideTime < 0.0f)
			m_sideTime = 0.0f;
		else
			m_sideTime += frameTime;
	}
	else
	{
		if (m_sideTime > 0.0f)
			m_sideTime = 0.0f;
		else
			m_sideTime -= frameTime;
	}

	Vec3 curDir = userOrient.GetColumn1();
	Vec3 wantDir = m_orient.GetColumn1();

	if (m_state == eS_Before)
	{
		OBB triggerBox;
		triggerBox.SetOBB( Matrix33(m_orient), m_posSize+Vec3(0.5f,0.5f,0), ZERO );
		if (Overlap::Point_OBB(m_userPos, m_pos, triggerBox))
			m_state = eS_Optimizing;
	}

	if ((m_state == eS_Optimizing) && allowTriggering)
	{
#ifdef INCLUDE_EXACTPOS_DEBUGGING
		bool debug = (CAnimationGraphCVars::Get().m_debugExactPos != 0);
		CPersistantDebug* pPD = CCryAction::GetCryAction()->GetPersistantDebug();
#endif

		Vec3 bump(0.0f, 0.0f, 0.1f);

		Vec3 posDistanceError = m_userPos - m_pos;
		if ( posDistanceError.z > -1.0f && posDistanceError.z < 1.0f )
			posDistanceError.z = 0;

		Vec3 orientFwd = m_orient.GetColumn1(); orientFwd.z = 0.0f; orientFwd.Normalize();
		Vec3 rotAnimMovementWanted = orientFwd * m_animMovementLength;

		Vec3 userFwd = m_userOrient.GetColumn1(); userFwd.z = 0.0f; userFwd.Normalize();
		Vec3 rotAnimMovementUser = userFwd * m_animMovementLength;

		float cosRotError = orientFwd.Dot( userFwd );
		float rotError = CLAMP(m_cosOrientTolerance - cosRotError, 0.0f, 1.0f);
		//Vec3 rotDistanceError = rotAnimMovementUser - rotAnimMovementWanted;

		float fwdDistance = fabsf(orientFwd.Dot( posDistanceError ));
		float sideDistance = max( 0.0f, sqrtf( std::max(0.f,posDistanceError.GetLengthSquared2D() - sqr(fwdDistance)) ) - m_width );

		float deltaFwd = m_oldFwdDir < fwdDistance ? fwdDistance - m_oldFwdDir : 0.0f;
		m_oldFwdDir = fwdDistance;
		fwdDistance += deltaFwd * 0.5f;
		deltaFwd = max(0.1f, deltaFwd);

		f32 distanceError = sqrtf(sqr(fwdDistance) + sqr(sideDistance)); // posDistanceError.len() * m_distanceErrorFactor;
		f32 temp = 1.0f-sqr(1.0f-rotError*rotError);
		temp = max(temp,0.0f); //never do a sqrtf with a negative value 
		f32 orientError = sqrtf(temp) * m_animMovementLength; // rotDistanceError.len();
		f32 totalDistanceError = distanceError + orientError;
		if (((m_distanceError * 1.05f) < distanceError) && ((m_orientError * 1.05f) < orientError) && (totalDistanceError < deltaFwd) ||
			(totalDistanceError < deltaFwd*0.5f))
		{ // found local minimum in distance error, force triggering.
			m_state = eS_Triggered;
			m_oldFwdDir = 0.0f;

#ifdef INCLUDE_EXACTPOS_DEBUGGING
			if (debug)
			{
				pPD->Begin("AnimationTrigger LocalMinima Triggered", false);
				pPD->AddPlanarDisc(m_pos + bump, 0.0f, m_distanceError, ColorF(0,1,0,0.5), 10.0f);
			}
#endif
		}
		else
		{
			m_distanceError = m_distanceError > distanceError ? distanceError : m_distanceError * 0.999f; // should timeout in ~2 secs. on 50 FPS
			m_orientError = m_orientError > orientError ? orientError : m_orientError - 0.0001f;

#ifdef INCLUDE_EXACTPOS_DEBUGGING
			if (debug)
			{
				pPD->Begin("AnimationTrigger LocalMinima Optimizing", true);
				pPD->AddPlanarDisc(m_pos + bump, 0.0f, m_distanceError, ColorF(1,1,0,0.5), 10.0f);
			}
#endif
		}

#ifdef INCLUDE_EXACTPOS_DEBUGGING
		if (debug)
		{
			pPD->AddLine(m_userPos + bump, m_pos + bump, ColorF(1,0,0,1), 10.0f);
			pPD->AddLine(m_userPos + rotAnimMovementUser + bump, m_pos + rotAnimMovementWanted + bump, ColorF(1,0,0,1), 10.0f);
			pPD->AddLine(m_pos + bump, m_pos + rotAnimMovementWanted + bump, ColorF(1,0.5,0,1), 10.0f);
			pPD->AddLine(m_userPos + bump, m_pos + rotAnimMovementUser + bump, ColorF(1,0.5,0,1), 10.0f);
		}
#endif
	}

	CRY_ASSERT(m_pos.IsValid());
	CRY_ASSERT(m_userPos.IsValid());
	CRY_ASSERT(m_orient.IsValid());
	CRY_ASSERT(m_userOrient.IsValid());
	CRY_ASSERT(m_posSize.IsValid());
	CRY_ASSERT(NumberValid(m_cosOrientTolerance));
}

/*
void CExactPositioningTrigger::DebugDraw()
{
	if (m_state == eS_Invalid)
		return;

	CRY_ASSERT(m_pos.IsValid());
	CRY_ASSERT(m_userPos.IsValid());
	CRY_ASSERT(m_orient.IsValid());
	CRY_ASSERT(m_userOrient.IsValid());
	CRY_ASSERT(m_posSize.IsValid());
	CRY_ASSERT(NumberValid(m_cosOrientTolerance));

	OBB triggerBox;
	triggerBox.SetOBB( Matrix33(m_orient), m_state == eS_Before ? m_posSize+Vec3(0.5f,0.5f,0) : m_posSize, ZERO );

	ColorF clr(1,0,0,1);

	Vec3 curDir = m_userOrient.GetColumn1();
	Vec3 wantDir = m_orient.GetColumn1();
	float cosAngle = curDir.Dot(wantDir);

	bool inAngleRange = false;

	switch (m_state)
	{
	case eS_Initializing:
		clr = ColorF(1,0,0,0.3f);
		break;
	case eS_Before:
		inAngleRange = cosAngle < 2*m_cosOrientTolerance;
		clr = ColorF(1,0,1,0.3f);
		break;
	case eS_Optimizing:
		inAngleRange = cosAngle < m_cosOrientTolerance;
		clr = ColorF(1,1,0,1);
		break;
	case eS_Triggered:
		inAngleRange = true;
		clr = ColorF(1,0,1,0.1f);
		break;
	}

	gEnv->pRenderer->GetIRenderAuxGeom()->DrawOBB( triggerBox, Matrix34::CreateTranslationMat(m_pos), true, clr, eBBD_Faceted );

	clr = ColorF(0,1,0,1);
	if (inAngleRange)
		clr = ColorF(1,0,0,1);
	static const int NUM_STEPS = 20;
	for (int i=0; i<NUM_STEPS; i++)
	{
		float t = float(i)/float(NUM_STEPS-1);
		Vec3 dir = Quat::CreateSlerp( m_userOrient, m_orient, t ).GetColumn1();
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine( m_pos, clr, m_pos + 4.0f * dir, clr );
	}
}
*/

void CExactPositioningTrigger::ResetRadius( const Vec3& triggerSize, float orientTolerance )
{
	CRY_ASSERT(m_pos.IsValid());
	CRY_ASSERT(m_userPos.IsValid());
	CRY_ASSERT(m_orient.IsValid());
	CRY_ASSERT(m_userOrient.IsValid());
	CRY_ASSERT(m_posSize.IsValid());
	CRY_ASSERT(NumberValid(m_cosOrientTolerance));

	if (m_state == eS_Invalid)
		return;
	m_state = eS_Initializing;
	m_posSize = triggerSize;
	m_cosOrientTolerance = cos(orientTolerance);

	// TODO: Update should not be called here. Maybe it's only used to set some values or something, not really an update.
	Update( 0.0f, m_userPos, m_userOrient, false );
}
