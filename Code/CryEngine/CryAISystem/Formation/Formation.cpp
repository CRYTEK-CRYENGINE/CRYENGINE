// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   -------------------------------------------------------------------------
   File name:   Formation.cpp
   $Id$
   Description:

   -------------------------------------------------------------------------
   History:

 *********************************************************************/
#include "StdAfx.h"
#include "Formation.h"
#include "PipeUser.h"
#include "Formation/AIFormationDescriptor.h"

#include <CryEntitySystem/IEntity.h>
#include "AIObject.h"
#include "CAISystem.h"
#include <CryMath/Cry_Math.h>
#include <CrySystem/ITimer.h>
#include <CrySystem/ISystem.h>
#include <CryNetwork/ISerialize.h>

#include <CrySystem/IConsole.h>
#include "DebugDrawContext.h"
#include <Cry3DEngine/I3DEngine.h>

const float CFormation::m_fDISTANCE_UPDATE_THR = 0.3f;
const float CFormation::m_fSIGHT_WIDTH = 1.f;

CFormation::TFormationID CFormation::s_NextFormation = 0;

CFormation::CFormation()
	: m_formationID(s_NextFormation++)
	, m_bReservationAllowed(true)
	, m_vLastUpdatePos(ZERO)
	, m_vLastPos(ZERO)
	, m_vLastTargetPos(ZERO)
	, m_vLastMoveDir(ZERO)
	, m_vInitialDir(ZERO)
	, m_vBodyDir(ZERO)
	, m_vMoveDir(ZERO)
	, m_bInitMovement(true)
	, m_bFirstUpdate(false)
	, m_bUpdate(true)
	, m_bForceReachable(false)
	, m_bFollowingUnits(false)
	, m_bFollowPointInitialized(false)
	, m_fUpdateThreshold(m_fDISTANCE_UPDATE_THR)
	, m_fDynamicUpdateThreshold(0.0f)
	, m_fStaticUpdateThreshold(0.0f)
	, m_fScaleFactor(1.0f)
	, m_pPathMarker(nullptr)
	, m_iSpecialPointIndex(-1)
	, m_fMaxFollowDistance(0.0f)
	, m_maxUpdateSightTimeMs(0)
	, m_minUpdateSightTimeMs(0)
	, m_fSightRotationRange(0.0f)
	, m_orientationType(OT_MOVE)
	, m_dbgVector1(ZERO)
	, m_dbgVector2(ZERO)
	, m_fRepositionSpeedWrefTarget(0.0f)
	, m_vDesiredTargetPos(ZERO)
{
	SetUpdateThresholds(m_fDISTANCE_UPDATE_THR);
}

CFormation::~CFormation()
{
	ReleasePoints();
	SAFE_DELETE(m_pPathMarker);
}

void CFormation::ReleasePoints(void)
{
	for (TFormationPoints::iterator itr(m_FormationPoints.begin()); itr != m_FormationPoints.end(); ++itr)
	{
		CFormationPoint& curPoint(*itr);
		curPoint.m_refDummyTarget.Release();
		curPoint.m_refWorldPoint.Release();
	}
}

void CFormation::OnObjectRemoved(CAIObject* pAIObject)
{
	// (MATT) Although this goes through a couple of levels, I think much of this could be removed {2009/03/18}
	CCCPOINT(CFormation_OnObjectRemoved);

	FreeFormationPoint(GetWeakRef(pAIObject));
}

void CFormation::AddPoint(FormationNode& desc)
{
	CFormationPoint curPoint;
	Vec3 pos = desc.vOffset;
	char name[100];
	int i = m_FormationPoints.size();

	{
		cry_sprintf(name, "FORMATION_%d", i);
		CStrongRef<CAIObject> refFormationDummy;
		gAIEnv.pAIObjectManager->CreateDummyObject(refFormationDummy, name);
		refFormationDummy.GetAIObject()->SetSubType(CAIObject::STP_FORMATION);
		curPoint.m_refWorldPoint = refFormationDummy;
	}

	curPoint.m_vSight = desc.vSightDirection;
	int iClass = desc.eClass;
	if (iClass == SPECIAL_FORMATION_POINT)
		m_iSpecialPointIndex = i;
	curPoint.m_Class = iClass;

	curPoint.m_FollowHeightOffset = desc.fFollowHeightOffset;
	curPoint.m_FollowDistance = desc.fFollowDistance;
	curPoint.m_FollowOffset = desc.fFollowOffset;
	curPoint.m_FollowDistanceAlternate = desc.fFollowDistanceAlternate;
	curPoint.m_FollowOffsetAlternate = desc.fFollowOffsetAlternate;

	if (curPoint.m_FollowDistance > 0)
	{
		if (m_fMaxFollowDistance < curPoint.m_FollowDistance)
			m_fMaxFollowDistance = curPoint.m_FollowDistance;

		m_bFollowingUnits = true;
		pos = Vec3(curPoint.m_FollowOffset, curPoint.m_FollowDistance, curPoint.m_FollowHeightOffset);
	}
	curPoint.m_vPoint = pos;

	{
		cry_sprintf(name, "FORMATION_DUMMY_TARGET_%d", i);
		CStrongRef<CAIObject> refNewDummyTarget;
		gAIEnv.pAIObjectManager->CreateDummyObject(refNewDummyTarget, name);
		curPoint.m_refDummyTarget = refNewDummyTarget;
	}

	m_FormationPoints.push_back(curPoint);
}

// fills the formation class with all necessary information
void CFormation::Create(CFormationDescriptor& desc, CWeakRef<CAIObject> refOwner, Vec3 vTargetPos)
{
	CAIObject* const pOwner = refOwner.GetAIObject();

	CFormationDescriptor::TVectorOfNodes::iterator vi;
	m_fMaxFollowDistance = 0;
	//float m_fMaxYOffset = -1000000;
	//m_iForemostNodeIndex = -1;
	m_szDescriptor = desc.m_sName;

	for (vi = desc.m_Nodes.begin(); vi != desc.m_Nodes.end(); ++vi)
	{
		FormationNode& nodeDesc = *vi;
		AddPoint(nodeDesc);
	}

	// clamp max distance to a minimum decent value
	if (m_fMaxFollowDistance < 1)
		m_fMaxFollowDistance = 1;

	/*if(m_iForemostNodeIndex<0)
	   m_bFollowingUnits = false; // if it was true but no foremost node, it means that all the nodes
	                // were set to follow
	 */
	m_bInitMovement = true;
	m_bFirstUpdate = true;
	m_bFollowPointInitialized = false;
	m_vInitialDir = vTargetPos.IsZero() ? Vec3Constants<float>::fVec3_Zero : (vTargetPos - pOwner->GetPos()).GetNormalizedSafe();
	m_vMoveDir = m_vInitialDir;

	m_dbgVector1.zero();
	m_dbgVector2.zero();

	m_vLastUpdatePos = pOwner->GetPos();
	m_vLastPos = pOwner->GetPos();
	m_vLastTargetPos.zero();
	m_vLastMoveDir = pOwner->GetMoveDir();
	m_fLastUpdateTime = 0.f;//GetAISystem()->GetCurrentTime();
	m_vDesiredTargetPos.zero();

	m_refOwner = refOwner;
	m_FormationPoints[0].m_refReservation = StaticCast<CAIActor>(refOwner);

	if (m_bFollowingUnits)
	{
		if (m_pPathMarker)
		{
			delete m_pPathMarker;
			m_pPathMarker = 0;
		}
		m_pPathMarker = new CPathMarker(m_fMaxFollowDistance + 10, m_fDISTANCE_UPDATE_THR / 2);
		m_vInitialDir.z = 0; // only in 2D
		m_vInitialDir.NormalizeSafe();
		m_pPathMarker->Init(pOwner->GetPos(), pOwner->GetPos() - pOwner->GetEntityDir() * 0.5f);
	}

	m_fSightRotationRange = 0;

	InitWorldPosition(m_vInitialDir);
}

//-----------------------------------------------------------------------------------------------------------------
//

float CFormation::GetMaxWidth()
{
	int s = m_FormationPoints.size();
	float maxDist = 0;
	for (int i = 0; i < s; i++)
		for (int j = i + 1; j < s; j++)
		{
			float dist = Distance::Point_PointSq(m_FormationPoints[i].m_vPoint, m_FormationPoints[j].m_vPoint);
			if (dist > maxDist)
				maxDist = dist;
		}
	return sqrtf(maxDist);
}

//-----------------------------------------------------------------------------------------------------------------
//

bool CFormation::GetPointOffset(int i, Vec3& ret)
{
	if (i >= GetSize())
		return false;
	CFormationPoint& point = m_FormationPoints[i];
	if (point.m_FollowDistance != 0)
	{
		ret.x = point.m_FollowOffset;
		ret.y = point.m_FollowDistance;
		ret.z = point.m_FollowHeightOffset;
	}
	else
	{
		ret = point.m_vPoint;
	}
	ret *= m_fScaleFactor;
	return true;

}

//-----------------------------------------------------------------------------------------------------------------
//
CAIObject* CFormation::GetOwner()
{
	return m_refOwner.GetAIObject();
}

int CFormation::GetPointIndex(CWeakRef<const CAIObject> refRequester) const
{
	int pointIndex = 0;
	for (TFormationPoints::const_iterator itrPoint(m_FormationPoints.begin()); itrPoint != m_FormationPoints.end(); ++itrPoint, ++pointIndex)
		if ((*itrPoint).m_refReservation == refRequester)
			return pointIndex;
	return -1;
}

//-----------------------------------------------------------------------------------------------------------------
//
Vec3 CFormation::GetPointSmooth(const Vec3& headPos, float followDist, float sideDist, float heightValue, int smoothValue, Vec3& smoothPosAlongCurve, CAIObject* pUser)
{
	Vec3 thePos(0, 0, 0), smoothOffset(0, 0, 0);//,smoothPosAlongCurve(0,0,0);
	float smoothD = .5f;
	float weight = (float)smoothValue;
	Vec3 peppe = m_pPathMarker->GetPointAtDistance(headPos, followDist);
	followDist -= smoothD * smoothValue / 2;
	if (followDist < 0)
		followDist = 0;
	smoothPosAlongCurve.zero();
	for (int cnt = 0; cnt < smoothValue; ++cnt)
	{
		const float d = followDist + smoothD * cnt;
		Vec3 vFollowDir;
		Vec3 vFollowPos;
		vFollowPos = m_pPathMarker->GetPointAtDistance(headPos, d);
		vFollowDir = m_pPathMarker->GetDirectionAtDistance(headPos, d);
		if (vFollowDir.IsZero())
			vFollowDir = m_vMoveDir;
		Vec3 vHOffset;
		vHOffset.x = -vFollowDir.y * sideDist;
		vHOffset.y = vFollowDir.x * sideDist;
		vHOffset.z = heightValue;
		smoothPosAlongCurve += vFollowPos;
		smoothOffset += vHOffset;
		//weight += 1.0f;
	}
	//	if(weight > 0)
	{
		smoothPosAlongCurve /= weight;
		smoothOffset /= weight;
	}

	//	float dist = Distance::Point_Point(smoothPosAlongCurve,peppe);
	thePos = smoothPosAlongCurve + smoothOffset;

	return thePos;
}

//====================================================================
// MoveFormationPointOutOfVehicles
//====================================================================
/*static void MoveFormationPointOutOfVehicles(Vec3 &pos)
   {
   IAIObject *obstVehicle = GetAISystem()->GetNearestObjectOfTypeInRange( pos, AIOBJECT_VEHICLE, 0, 10, NULL, AIFAF_INCLUDE_DISABLED );
   if (!obstVehicle)	// nothing to steer away from
    return;

   // get bounding rectangle of the vehicle width length
   Vec3 FL, FR, BL, BR;
   obstVehicle->GetProxy()->GetWorldBoundingRect(FL, FR, BL, BR, 1.0f);

   // zero heights
   FL.z = FR.z = BL.z = BR.z = 0.0f;

   Vec3 fwd = (FL - BL);
   fwd.NormalizeSafe();
   Vec3 right = (FR - FL);
   right.NormalizeSafe();

   // +ve dist means we're inside
   float distToFront =  (FL - pos) | fwd;
   float distToBack  = -(BL - pos) | fwd;
   float distToRight =  (FR - pos) | right;
   float distToLeft  = -(FL - pos) | right;

   if (distToFront < 0.0f || distToBack < 0.0f ||
      distToRight < 0.0f || distToLeft < 0.0f)
      return;

   // inside - choose the smallest distance - not the most elegant way
   float bestDist = distToFront;
   Vec3 newPos = pos + fwd * distToFront;
   if (distToBack < bestDist)
   {
    bestDist = distToBack;
    newPos = pos - fwd * distToBack;
   }
   if (distToRight < bestDist)
   {
    bestDist = distToRight;
    newPos = pos + right * distToRight;
   }
   if (distToLeft < bestDist)
   {
    bestDist = distToLeft;
    newPos = pos - right * distToLeft;
   }
   pos = newPos;
   }
 */

// Update of the formation (refreshes position of formation points)
void CFormation::Update()
{
	// (MATT) Here's one place we might trigger a delete {2009/03/18}
	CAIObject* const pOwner = m_refOwner.GetAIObject();
	if (!pOwner)
		return;

	if (!m_bReservationAllowed)
		return;

	CCCPOINT(CFormation_Update);

	Vec3 vUpdateMoveDir;
	Vec3 vUpdateTargetDir;
	Vec3 vMoveDir;

	TFormationPoints::iterator itFormEnd = m_FormationPoints.end();
	if (m_bFirstUpdate)
	{
		m_vLastMoveDir = pOwner->GetMoveDir();
		m_bFirstUpdate = false;
		m_fLastUpdateTime = 0.0f;//GetAISystem()->GetFrameStartTime();
		for (TFormationPoints::iterator itrPoint(m_FormationPoints.begin()); itrPoint != itFormEnd; ++itrPoint)
		{
			CAIObject* const pWorldPoint = itrPoint->m_refWorldPoint.GetAIObject();
			if (pWorldPoint)
			{
				pWorldPoint->SetAssociation(m_refOwner);
				pWorldPoint->Event(AIEVENT_DISABLE, NULL);
			}
		}
	}

	// if it's in init movement, we don't consider the update threshold, but any movement!=0
	CTimeValue currentTime = GetAISystem()->GetFrameStartTime();
	int64 updateTimeMs = (currentTime - m_fLastUpdateTime).GetMilliSecondsAsInt64();
	if (updateTimeMs <= 10)
		return;

	float updateTimeFloat = (float)(updateTimeMs);
	float intervalRangeFloat = (float)(m_maxUpdateSightTimeMs - m_minUpdateSightTimeMs);

	Vec3 vOwnerPos = pOwner->GetPhysicsPos();

	vUpdateMoveDir = vOwnerPos - m_vLastUpdatePos;
	float fMovement = vUpdateMoveDir.GetLength();

	vMoveDir = vUpdateMoveDir;

	if (m_bUpdate)
	{
		m_vLastPos = vOwnerPos;

		if (m_refReferenceTarget.ValidateOrReset())
		{
			const CAIObject* pReferenceTarget = m_refReferenceTarget.GetAIObject();
			const Vec3& vRefPos = pReferenceTarget->GetPos();
			vUpdateTargetDir = vRefPos - m_vLastTargetPos;

			CCCPOINT(CFormation_OnObjectRemoved_A);

			float fTargetMovement = vUpdateTargetDir.GetLength();
			if (fTargetMovement > fMovement)
				fMovement = fTargetMovement;
			if (m_fRepositionSpeedWrefTarget == 0 || m_vDesiredTargetPos.IsZero())
				m_vDesiredTargetPos = vRefPos;
			else
			{
				Vec3 moveDir = (vRefPos - m_vDesiredTargetPos);
				float length = moveDir.GetLength();
				if (length < 0.1f)
					m_vDesiredTargetPos = vRefPos;
				else
				{
					moveDir.NormalizeSafe();
					float dist = (vRefPos - vOwnerPos).GetLength();
					m_vDesiredTargetPos += moveDir * min(length, m_fRepositionSpeedWrefTarget / 10 * dist);
				}
			}
			m_vMoveDir = m_vDesiredTargetPos - vOwnerPos;
			m_vMoveDir.NormalizeSafe();
			if (m_vMoveDir.x == 0 && m_vMoveDir.y == 0)//2D only - .IsZero())
			{
				m_vMoveDir = pReferenceTarget->GetMoveDir();
				m_vMoveDir.z = 0;
				m_vMoveDir.NormalizeSafe();
			}
		}
	}

	if (!m_bUpdate || fMovement < m_fUpdateThreshold && !m_bInitMovement && m_fRepositionSpeedWrefTarget == 0)
	{
		// always update special formation points
		Vec3 formDirXAxis(m_vMoveDir.y, -m_vMoveDir.x, 0);
		if (m_iSpecialPointIndex >= 0)
		{
			CFormationPoint& frmPoint = m_FormationPoints[m_iSpecialPointIndex];
			Vec3 pos = frmPoint.m_vPoint;
			Vec3 posRot;
			Vec3 vY(pOwner->GetMoveDir());
			Vec3 vX(vY.y, -vY.x, 0);
			vX.NormalizeSafe();

			posRot = pos.x * vX + pos.y * vY;
			pos = posRot + vOwnerPos;

			frmPoint.SetPos(pos, vOwnerPos, m_bForceReachable);

			CAIObject* const pDummyTarget = frmPoint.m_refDummyTarget.GetAIObject();
			if (pDummyTarget)
			{
				CCCPOINT(CFormation_Update_B);

				Vec3 vY2(posRot);
				Vec3 vX2(vY.y, -vY.x, 0);
				vX2.NormalizeSafe();
				vY2.NormalizeSafe();
				Vec3 vDummyPosRelativeToOwner = frmPoint.m_vPoint + frmPoint.m_vSight; // in owner's space
				Vec3 posRot2 = vDummyPosRelativeToOwner.x * vX2 + vDummyPosRelativeToOwner.y * vY2;
				Vec3 vDummyWorldPos = posRot2 + vOwnerPos;
				// TO DO: set the sight dummy target position relative to the point's user if it's close enough
				vDummyWorldPos.z = pos.z;
				pDummyTarget->SetPos(vDummyWorldPos);
			}

		}

		// randomly rotate the look-at dummy targets when formation is not moving
		if (m_fSightRotationRange > 0)
		{
			float intervalRange = intervalRangeFloat;

			for (TFormationPoints::iterator itrPoint(m_FormationPoints.begin()); itrPoint != itFormEnd; ++itrPoint)
			{
				CFormationPoint& frmPoint = (*itrPoint);
				int randomIntervalMs = m_minUpdateSightTimeMs + cry_random(0U, (uint32)intervalRange);
				int pointUpdateTimeMs = (int)(currentTime - frmPoint.m_fLastUpdateSightTime).GetMilliSecondsAsInt64();

				if (pointUpdateTimeMs > randomIntervalMs)
				{
					frmPoint.m_fLastUpdateSightTime = currentTime;
					Vec3 pos = frmPoint.m_vPoint;// m_vPoints[i];

					CAIObject* const pDummyTarget = frmPoint.m_refDummyTarget.GetAIObject();
					const CAIObject* const pReferenceTarget = m_refReferenceTarget.GetAIObject();

					if (pDummyTarget)
					{
						if (pReferenceTarget && frmPoint.m_Class != SPECIAL_FORMATION_POINT)
						{
							pDummyTarget->SetPos(pReferenceTarget->GetPos());
						}
						else
						{
							CCCPOINT(CFormation_Update_C);

							float angle = cry_random(-0.5f, 0.5f) * m_fSightRotationRange;
							Vec3 vSight = frmPoint.m_vSight.GetRotated(Vec3Constants<float>::fVec3_OneZ, angle);
							Vec3 vDummyPosRelativeToOwner = frmPoint.m_vPoint + vSight; // in owner's space
							Vec3 posRot = vDummyPosRelativeToOwner.x * formDirXAxis + vDummyPosRelativeToOwner.y * m_vMoveDir;
							Vec3 vDummyWorldPos = posRot + vOwnerPos;
							// TO DO: set the sight dummy target position relative to the point's user if it's close enough
							vDummyWorldPos.z = pos.z;
							pDummyTarget->SetPos(vDummyWorldPos);
						}
					}
				}
			}
		}

		m_fLastUpdateTime = currentTime;
		return;
	}

	float invUpdateTime = updateTimeFloat * 0.001f;

	// formation's owner has moved

	if (!m_refReferenceTarget.IsValid())
	{
		CAIObject* const pReservation0 = m_FormationPoints[0].m_refReservation.GetAIObject();
		CAIObject* const pEntityOwner = pOwner->CastToCAIActor() || !pReservation0 ? pOwner : pReservation0;
		m_vMoveDir = m_orientationType == OT_MOVE ? pEntityOwner->GetMoveDir() : pEntityOwner->GetViewDir();
		m_vMoveDir.z = 0;
		m_vMoveDir.NormalizeSafe();
		if (m_vMoveDir.IsEquivalent(Vec3Constants<float>::fVec3_Zero))
			m_vMoveDir = (vOwnerPos - m_vLastUpdatePos).GetNormalizedSafe();
		if (m_vMoveDir.IsEquivalent(Vec3Constants<float>::fVec3_Zero))
			m_vMoveDir = pOwner->GetViewDir();

		if (vUpdateMoveDir.GetLengthSquared() > sqr(0.01f))
		{
			Vec3 vUpdateMoveDirN = vUpdateMoveDir.GetNormalizedSafe();
			if (vUpdateMoveDirN.Dot(m_vLastMoveDir.GetNormalizedSafe()) < -0.1f)
			{
				//formation is inverting direction, flip it
				if (m_pPathMarker)
					m_pPathMarker->Init(vOwnerPos, vOwnerPos - m_fMaxFollowDistance * vUpdateMoveDirN);
				for (TFormationPoints::iterator itrPoint(m_FormationPoints.begin()); itrPoint != itFormEnd; ++itrPoint)
				{
					CCCPOINT(CFormation_Update_D);

					CFormationPoint& frmPoint = (*itrPoint);
					frmPoint.m_FollowOffset = -frmPoint.m_FollowOffset;
					frmPoint.m_vPoint.x = -frmPoint.m_vPoint.x;
					frmPoint.m_vSight.x = -frmPoint.m_vSight.x;
				}
			}
		}
	}
	Vec3 formDirXAxis(m_vMoveDir.y, -m_vMoveDir.x, 0);
	m_fUpdateThreshold = m_fDynamicUpdateThreshold;
	m_fLastUpdateTime = currentTime;

	if (!vUpdateMoveDir.IsEquivalent(Vec3Constants<float>::fVec3_Zero))
		m_vLastMoveDir = vUpdateMoveDir;
	m_vLastUpdatePos = vOwnerPos;

	if (m_refReferenceTarget.IsValid())
		m_vLastTargetPos = m_refReferenceTarget.GetAIObject()->GetPos();

	m_bInitMovement = false;

	static float speedUpdateFrac = 0.1f;
	bool bFormationOwnerPoint = true;
	//first update the fixed offset points
	for (TFormationPoints::iterator itrPoint(m_FormationPoints.begin()); itrPoint != itFormEnd; ++itrPoint)
	{
		CFormationPoint& frmPoint = (*itrPoint);
		Vec3 pos = frmPoint.m_vPoint;// m_vPoints[i];
		CAIObject* const pFormationDummy = frmPoint.m_refWorldPoint.GetAIObject(); // m_vWorldPoints[i];
		frmPoint.m_fLastUpdateSightTime = currentTime;

		//Update nodes
		if (!(m_bFollowingUnits && frmPoint.m_FollowDistance > 0))// m_vFollowDistances[i]>0 ))
		{
			// fixed offset node
			Vec3 posRot;
			if (bFormationOwnerPoint)
			{
				//force the formation owner point to be where the owner is - skip all the checks
				frmPoint.m_refWorldPoint.GetAIObject()->SetPos(vOwnerPos);
				bFormationOwnerPoint = false;
				continue;
			}

			posRot = pos.x * formDirXAxis + pos.y * m_vMoveDir;
			pos = posRot + vOwnerPos;
			frmPoint.m_LastPosition = pFormationDummy->GetPos();
			frmPoint.m_Dir = (pos - frmPoint.m_LastPosition) * invUpdateTime;
			frmPoint.SetPos(pos, vOwnerPos, m_bForceReachable);

			if (!pFormationDummy->IsEnabled())
				pFormationDummy->Event(AIEVENT_ENABLE, NULL);

			// blend the speed estimation to smooth it since it's very noisy
			float newSpeed = (frmPoint.m_LastPosition - pos).GetLength() * invUpdateTime;
			frmPoint.m_Speed = speedUpdateFrac * newSpeed + (1.0f - speedUpdateFrac) * frmPoint.m_Speed;

			//Update dummy targets

			//rotate sighting
			CAIObject* const pDummyTarget = frmPoint.m_refDummyTarget.GetAIObject();
			const CAIObject* const pReferenceTarget = m_refReferenceTarget.GetAIObject();
			if (pDummyTarget)
			{
				if (pReferenceTarget && frmPoint.m_Class != SPECIAL_FORMATION_POINT)
				{
					pDummyTarget->SetPos(pReferenceTarget->GetPos());
				}
				else
				{
					CCCPOINT(CFormation_Update_E);

					Vec3 vSight = frmPoint.m_vSight;
					Vec3 vDummyPosRelativeToOwner = frmPoint.m_vPoint + vSight; // in owner's space
					Vec3 posRot2 = vDummyPosRelativeToOwner.x * formDirXAxis + vDummyPosRelativeToOwner.y * m_vMoveDir;
					Vec3 vDummyWorldPos = posRot2 + vOwnerPos;
					// TO DO: set the sight dummy target position relative to the point's user if it's close enough
					vDummyWorldPos.z = pos.z;
					pDummyTarget->SetPos(vDummyWorldPos);
				}
			}
		}
	}

	if (m_bFollowingUnits)
	{
		TFormationPoints::iterator itrPoint(m_FormationPoints.begin());

		m_pPathMarker->Update(vOwnerPos, true);// true if 2D movement

		bool bPlayerLeader = (pOwner && pOwner->GetType() == AIOBJECT_PLAYER);

		// then update following-type nodes and sights
		for (; itrPoint != itFormEnd; ++itrPoint)
		{
			CFormationPoint& frmPoint = (*itrPoint);
			CAIObject* const pFormationDummy = frmPoint.m_refWorldPoint.GetAIObject();

			if (frmPoint.m_FollowDistance > 0)
			{
				float fDistance = frmPoint.m_FollowDistance;

				float fTotalDistance = m_pPathMarker->GetTotalDistanceRun();

				Vec3 vFollowDir;
				if (fTotalDistance < 1.0f || fTotalDistance < fDistance)
				{
					// Keep the point disabled until it is possible to update it properly.
					if (pFormationDummy->IsEnabled())
						pFormationDummy->Event(AIEVENT_DISABLE, NULL);

					if (frmPoint.m_refReservation.IsValid())
					{
						// Only update the point after it has been reserved so that the formation point
						// assignment can find the nearest points.
						frmPoint.SetPos(vOwnerPos, vOwnerPos, true, bPlayerLeader);
					}

					frmPoint.m_Speed = 0;

					vFollowDir = m_vMoveDir;
				}
				else
				{
					// There is enough path markers to update the point.
					if (!pFormationDummy->IsEnabled())
						pFormationDummy->Event(AIEVENT_ENABLE, NULL);

					// follow-type node
					vFollowDir = m_pPathMarker->GetDirectionAtDistance(vOwnerPos, fDistance);
					if (vFollowDir.IsZero())
						vFollowDir = m_vMoveDir;

					int numSmoothPasses = 5;

					//don't smooth formation for squad-mates
					if (pOwner->CastToCAIPlayer())
						numSmoothPasses = 1;

					if (vMoveDir.GetLengthSquared() > .01f && (vOwnerPos - pFormationDummy->GetPos()).Dot(vMoveDir) > 0.f)
					{
						Vec3 pointAlongLeaderPath;
						Vec3 smoothedPos = GetPointSmooth(vOwnerPos, fDistance, frmPoint.m_FollowOffset, frmPoint.m_FollowHeightOffset, numSmoothPasses, pointAlongLeaderPath, frmPoint.m_refReservation.GetAIObject());
						frmPoint.SetPos(smoothedPos, pointAlongLeaderPath, m_bForceReachable, bPlayerLeader);
					}

					float newSpeed = (frmPoint.m_LastPosition - pFormationDummy->GetPos()).GetLength() * invUpdateTime;
					frmPoint.m_Speed = speedUpdateFrac * newSpeed + (1.0f - speedUpdateFrac) * frmPoint.m_Speed;
				}

				CCCPOINT(CFormation_Update_F);

				frmPoint.m_Dir = (pFormationDummy->GetPos() - frmPoint.m_LastPosition) * invUpdateTime;
				frmPoint.m_LastPosition = pFormationDummy->GetPos();

				//rotate 90 degrees
				formDirXAxis.x = vFollowDir.y;
				formDirXAxis.y = -vFollowDir.x;
				formDirXAxis.z = 0;
				//Update dummy targets
				Vec3 vSight = frmPoint.m_vSight;
				//rotate sighting

				Vec3 pos2 = pFormationDummy->GetPos();

				CAIObject* pDummyTarget = frmPoint.m_refDummyTarget.GetAIObject();
				if (pDummyTarget)
				{
					Vec3 posRot = vSight.x * formDirXAxis + vSight.y * vFollowDir;
					Vec3 vDummyWorldPos = posRot + pFormationDummy->GetPos();
					// TO DO: set the sight dummy target position relative to the point's owner if it's close enough
					vDummyWorldPos.z = pos2.z;
					pDummyTarget->SetPos(vDummyWorldPos, vFollowDir);
				}

			}
		}
	}
}

void CFormationPoint::SetPos(Vec3 pos, const Vec3& startPos, bool force, bool bPlayerLeader)
{
	if (!m_refReservation.IsValid() && !force)
	{
		m_refWorldPoint.GetAIObject()->SetPos(pos);
		return;
	}

	if (!bPlayerLeader && m_FollowDistance > 0 && m_FollowOffset < 0.2f)
	{
		//optimization: point is on a walked path (offset too low)
		m_refWorldPoint.GetAIObject()->SetPos(pos);
		return;
	}

	// this line would replace all the code below
	//m_pWorldPoint->SetReachablePos(pos,m_pReservation,startPos);

	CCCPOINT(CFormation_SetPos);

	IAISystem::tNavCapMask navMask;
	float fPassRadius;
	CAIActor* const pReservation = m_refReservation.GetAIObject();
	if (pReservation)
	{
		navMask = pReservation->GetMovementAbility().pathfindingProperties.navCapMask;
		fPassRadius = pReservation->GetParameters().m_fPassRadius;
	}
	else
	{
		navMask = 0xff;
		fPassRadius = 0.6f;
	}

	m_refWorldPoint.GetAIObject()->SetPos(pos);
}

// returns an available formation point, if that exists right now by proximity
CAIObject* CFormation::GetNewFormationPoint(CWeakRef<CAIActor> refRequester, int index)
{
	CAIObject* pPoint = NULL;

	int size = m_FormationPoints.size();
	if (index >= size)
		return NULL;

	CCCPOINT(CFormation_GetNewFormationPoint);

	if (index < 0)
	{
		float mindist = FLT_MAX;
		const Vec3& requesterPos = refRequester.GetAIObject()->GetPos();
		for (int i = 0; i < size; i++)
		{
			CFormationPoint& curFormationPoint(m_FormationPoints[i]);
			CAIObject* pThisPoint = curFormationPoint.m_refWorldPoint.GetAIObject();
			if (refRequester == curFormationPoint.m_refReservation)
				return pThisPoint;
			if (curFormationPoint.m_Class == SPECIAL_FORMATION_POINT)
				continue;
			if (!curFormationPoint.m_refReservation.IsValid())
			{
				float dist = Distance::Point_PointSq(pThisPoint->GetPos(), requesterPos);
				if (dist < mindist)
				{
					index = i;
					mindist = dist;
					pPoint = pThisPoint;
				}
			}
		}
	}
	else if (m_FormationPoints[index].m_refReservation.IsNil() || refRequester == m_FormationPoints[index].m_refReservation)
	{
		pPoint = m_FormationPoints[index].m_refWorldPoint.GetAIObject();
	}
	if (index >= 0)
	{
		CCCPOINT(CFormation_GetNewFormationPoint_A);
		m_FormationPoints[index].m_refReservation = refRequester;
		pPoint = m_FormationPoints[index].m_refWorldPoint.GetAIObject();
		SetUpdate(true);
	}
	return pPoint;
}

int CFormation::GetClosestPointIndex(CWeakRef<CAIActor> refRequester, bool bReserve, int maxSize, int iClass, bool bCLosestToOwner)
{
	CCCPOINT(CFormation_GetClosestPointIndex);

	int size = (maxSize == 0 ? m_FormationPoints.size() : maxSize);
	int index = -1;
	float mindist = 2000000.f;
	Vec3 basePos = bCLosestToOwner ? m_FormationPoints[0].m_refWorldPoint.GetAIObject()->GetPos() : refRequester.GetAIObject()->GetPos();
	for (int i = 1; i < size; i++)
		if ((iClass < 0 && m_FormationPoints[i].m_Class != SPECIAL_FORMATION_POINT) || (m_FormationPoints[i].m_Class & iClass))
		{
			if (!m_FormationPoints[i].m_refReservation.IsValid() || refRequester == m_FormationPoints[i].m_refReservation)
			{
				CAIObject* pThisPoint = m_FormationPoints[i].m_refWorldPoint.GetAIObject();
				float dist = Distance::Point_PointSq(pThisPoint->GetPos(), basePos);
				if (dist < mindist)
				{
					index = i;
					mindist = dist;
				}
			}
		}
	if (index >= 0 && bReserve)
	{
		CCCPOINT(CFormation_GetClosestPointIndex_A);
		FreeFormationPoint(refRequester);
		m_FormationPoints[index].m_refReservation = refRequester;
	}
	return index;
}

CAIObject* CFormation::GetFormationDummyTarget(CWeakRef<const CAIObject> refRequester) const
{
	CCCPOINT(CFormation_GetFormationDummyTarget);

	for (uint32 i = 0; i < m_FormationPoints.size(); i++)
		if (m_FormationPoints[i].m_refReservation == refRequester)
			return m_FormationPoints[i].m_refDummyTarget.GetAIObject();
	return NULL;
}

void CFormation::SetDummyTargetPosition(const Vec3& vPosition, CAIObject* pDummyTarget, const Vec3& vSight)
{
	Vec3 myDir;
	Vec3 otherDir;

	if (pDummyTarget)
	{

		Vec3 myangles = vSight;

		myangles.z = 0;

		float fi = m_fSIGHT_WIDTH * cry_random(-0.5f, 0.5f);

		Matrix33 m = Matrix33::CreateRotationXYZ(DEG2RAD(Ang3(0, 0, fi)));
		myangles = m * (Vec3)myangles;

		myangles *= 20.f;

		pDummyTarget->SetPos(vPosition + (Vec3)myangles);

	}
}

void CFormation::Draw()
{
	CDebugDrawContext dc;

	ColorB lineColPathOk(0, 255, 0, 179);
	ColorB lineColNoPath(255, 255, 255, 179);
	ColorB lineColDiffPath(255, 255, 0, 179);
	for (unsigned int i = 0; i < m_FormationPoints.size(); ++i)
	{
		CFormationPoint& fp = m_FormationPoints[i];
		CAIObject* const pReservoir = fp.m_refReservation.GetAIObject();
		uint8 alfaSud = pReservoir ? 255 : 102;
		CAIObject* const pWorldPoint = fp.m_refReservation.GetAIObject();

		if (!pWorldPoint)
			continue;

		Vec3 pos = fp.m_refWorldPoint->GetPos(); //  pWorldPoint->GetPos();
		ColorB color;
		if (!pWorldPoint->IsEnabled())
			color.set(255, 0, 0, alfaSud);
		else if (fp.m_Speed > 0)
			color.set(0, 255, 0, alfaSud);
		else
			color.set(255, 255, 0, alfaSud);
		dc->DrawSphere(pos, i == m_iSpecialPointIndex ? 0.2f : 0.3f, color);

		if (pReservoir)
		{
			ColorB lineColor;
			CPipeUser* pPiper = pReservoir->CastToCPipeUser();
			assert(pPiper);//Shut up SCA
			if (pPiper && pPiper->m_refPathFindTarget == pWorldPoint)
				lineColor = (pPiper->m_nPathDecision == PATHFINDER_NOPATH ? lineColNoPath : lineColPathOk);
			else
				lineColor = lineColDiffPath;
			dc->DrawLine(pos, lineColor, pReservoir->GetPos(), lineColor);
			pos.z += 1.0f;
			if (pPiper)
			{
				CAIObject* pPathFindTarget = pPiper->m_refPathFindTarget.GetAIObject();
				if ((pPathFindTarget != NULL))
					dc->Draw3dLabel(pos, 1, "%s (%s->%s)", pWorldPoint->GetName(), pReservoir->GetName(), pPathFindTarget->GetName());
				else
					dc->Draw3dLabel(pos, 1, "%s (%s)", pWorldPoint->GetName(), pReservoir->GetName());

				dc->DrawArrow(pos, m_vMoveDir, 0.1f, ColorB(255, 0, 0));
			}
		}
		else
		{
			pos.z += 1;
			dc->Draw3dLabel(pos, 1, "%s", pWorldPoint->GetName());
		}

		// Draw the initial position, this is to help the designers to match the initial character locations with formation points.
		dc->DrawLine(fp.m_DEBUG_vInitPos - Vec3(0, 0, 10), ColorB(255, 196, 0), fp.m_DEBUG_vInitPos + Vec3(0, 0, 10), ColorB(255, 196, 0));
		dc->DrawCone(fp.m_DEBUG_vInitPos + Vec3(0, 0, 0.5f), Vec3(0, 0, -1), 0.2f, 0.5f, ColorB(255, 196, 0));
		if (Distance::Point_PointSq(fp.m_DEBUG_vInitPos, pos) > 5.0f)
			dc->Draw3dLabel(fp.m_DEBUG_vInitPos + Vec3(0, 0, 0.5f), 1, "Init\n%s", pWorldPoint->GetName());
	}

	for (unsigned int i = 0; i < m_FormationPoints.size(); i++)
	{
		const CFormationPoint& fp = m_FormationPoints[i];
		float alfaSud = fp.m_refReservation.IsValid() ? 1 : 0.4f;
		ColorB colorSight(0, 255, 179, uint8(255 * alfaSud));
		Vec3 pos = fp.m_refWorldPoint.GetAIObject()->GetPos();
		Vec3 posd = fp.m_refDummyTarget.GetAIObject()->GetPos() - pos;
		posd.NormalizeSafe();
		Vec3 possight = pos + posd / 2;
		dc->DrawCone(possight, posd, 0.05f, 0.2f, colorSight);
		dc->DrawLine(pos, colorSight, possight, colorSight);
	}

	if (!m_FormationPoints.empty())
	{
		ColorB color(255, 128, 255);
		Vec3 pos = m_FormationPoints[0].m_refWorldPoint.GetAIObject()->GetPos();
		dc->DrawLine(pos, color, pos + 3 * m_dbgVector1, color);
		dc->DrawLine(pos, color, pos + 3 * m_dbgVector2, color);
	}

	if (m_pPathMarker)
		m_pPathMarker->DebugDraw();
}

// (MATT) Note that weak references can't be used in things triggered when objects are deleted - because the ref goes first {2009/03/18}
void CFormation::FreeFormationPoint(CWeakRef<const CAIObject> refCurrentHolder)
{
	for (uint32 i = 0; i < m_FormationPoints.size(); i++)
	{
		CFormationPoint& point = m_FormationPoints[i];
		if (point.m_refReservation == refCurrentHolder)
			point.m_refReservation.Reset();
		if (point.m_refWorldPoint.GetAIObject()->GetAssociation() == refCurrentHolder)
			point.m_refWorldPoint.GetAIObject()->SetAssociation(NILREF);
		if (point.m_refDummyTarget.GetAIObject()->GetAssociation() == refCurrentHolder)
			point.m_refDummyTarget.GetAIObject()->SetAssociation(NILREF);
	}
}

void CFormation::FreeFormationPoint(int i)
{
	CCCPOINT(CFormation_FreeFormationPoint_i);

	CFormationPoint& point = m_FormationPoints[i];
	point.m_refReservation.Reset();
	point.m_refWorldPoint.GetAIObject()->SetAssociation(NILREF);
	point.m_refDummyTarget.GetAIObject()->SetAssociation(NILREF);
}
//----------------------------------------------------------------------

void CFormation::Reset()
{
	for (uint32 i = 0; i < m_FormationPoints.size(); i++)
		m_FormationPoints[i].m_refReservation.Reset();
}

void CFormation::InitWorldPosition(Vec3 vDir)
{
	CCCPOINT(CFormation_InitWorldPosition);

	CAIObject* const pOwner = m_refOwner.GetAIObject();
	if (vDir.IsZero())
		vDir = pOwner->GetViewDir();
	vDir.NormalizeSafe();
	if (vDir.IsZero())
		vDir = pOwner->GetMoveDir();

	m_vInitialDir = vDir;

	SetOffsetPointsPos(vDir);
	InitFollowPoints(vDir);//,pOwner);
}

void CFormation::SetOffsetPointsPos(const Vec3& vMoveDir)
{
	Vec3 vBasePos = m_refOwner.GetAIObject()->GetPos();
	Vec3 moveDirXAxis(-vMoveDir.y, vMoveDir.x, 0);

	// update the fixed offset points
	for (TFormationPoints::iterator itrPoint(m_FormationPoints.begin()); itrPoint != m_FormationPoints.end(); ++itrPoint)
	{
		bool bFirst = true;
		CFormationPoint& formPoint = (*itrPoint);
		Vec3 pos = formPoint.m_vPoint;
		CAIObject* const pFormationDummy = formPoint.m_refWorldPoint.GetAIObject();

		//Update nodes
		if (!(m_bFollowingUnits && formPoint.m_FollowDistance > 0))
		{
			// fixed offset node
			Vec3 posRot;
			posRot = pos.x * moveDirXAxis + pos.y * vMoveDir;
			pos = posRot + vBasePos;
			pos.z = gEnv->p3DEngine->GetTerrainElevation(pos.x, pos.y) + 1.75f;

			if (bFirst)
				pFormationDummy->SetPos(pos);
			else
				formPoint.SetPos(pos, vBasePos);

			formPoint.m_DEBUG_vInitPos = pos;

			formPoint.m_LastPosition = pos;
			//Update dummy targets
			Vec3 vSight = formPoint.m_vSight;

			CAIObject* pDummyTarget = formPoint.m_refDummyTarget.GetAIObject();
			const CAIObject* const pReferenceTarget = m_refReferenceTarget.GetAIObject();
			if (pDummyTarget)
			{
				if ((pReferenceTarget != NULL) && (formPoint.m_Class != SPECIAL_FORMATION_POINT))
				{
					pDummyTarget->SetPos(pReferenceTarget->GetPos());
				}
				else
				{
					CCCPOINT(CFormation_SetOffsetPointsPos);

					Vec3 vDummyPosRelativeToOwner = formPoint.m_vPoint + vSight; // in owner's space
					Vec3 posRot2 = vDummyPosRelativeToOwner.x * moveDirXAxis + vDummyPosRelativeToOwner.y * vMoveDir;
					Vec3 vDummyWorldPos = posRot2 + vBasePos;
					vDummyWorldPos.z = pos.z;
					pDummyTarget->SetPos(vDummyWorldPos);
				}
			}
		}
	}
}

//----------------------------------------------------------------------
void CFormation::InitFollowPoints(const Vec3& vDirection)
{
	CCCPOINT(CFormation_InitFollowPoints);

	CAIObject* const pOwner = m_refOwner.GetAIObject();

	// update the follow points
	Vec3 vInitPos = pOwner->GetPos();
	Vec3 vDir(vDirection.IsZero() ? (m_vLastMoveDir.IsZero() ? m_vMoveDir : m_vLastMoveDir) : vDirection);
	vDir.z = 0; //2d only
	vDir.NormalizeSafe();

	Vec3 vX(-vDir.y, vDir.x, 0);

	// The initial follow points are used for assigning the formation positions.
	// For this reason the formation points are slightly contracted, to allow a bit more setup in the level.

	// Find the nearest follow distance.
	float nearestFollowDistance = FLT_MAX;
	for (TFormationPoints::iterator itrPoint = m_FormationPoints.begin(), end = m_FormationPoints.end(); itrPoint != end; ++itrPoint)
	{
		CFormationPoint& formPoint = (*itrPoint);
		if (formPoint.m_FollowDistance > 0)
			nearestFollowDistance = min(nearestFollowDistance, formPoint.m_FollowDistance);
	}

	bool bPlayerLeader = (pOwner && pOwner->GetType() == AIOBJECT_PLAYER);
	for (TFormationPoints::iterator itrPoint(m_FormationPoints.begin()); itrPoint != m_FormationPoints.end(); ++itrPoint)
	{
		CFormationPoint& formPoint = (*itrPoint);
		if (formPoint.m_FollowDistance > 0)
		{
			float totalFollowDist = formPoint.m_FollowDistance - nearestFollowDistance; // Contracted formation.
			Vec3 requestedPosAlongCurve(vInitPos - vDir * totalFollowDist);
			Vec3 offsetPos = vX * formPoint.m_FollowOffset + Vec3(0, 0, formPoint.m_FollowHeightOffset);
			Vec3 requestedPos(requestedPosAlongCurve + offsetPos);
			formPoint.SetPos(requestedPos, vInitPos, true, bPlayerLeader);
			formPoint.m_LastPosition = requestedPos;
			formPoint.m_DEBUG_vInitPos = requestedPos;
			Vec3 vSight = formPoint.m_vSight;

			CAIObject* pDummyTarget = formPoint.m_refDummyTarget.GetAIObject();
			if (pDummyTarget)
			{
				Vec3 vDummyPosRelativeToOwner = formPoint.m_vPoint + vSight; // in owner's space
				Vec3 posRot = -vDummyPosRelativeToOwner.x * vX + vDummyPosRelativeToOwner.y * vDir;
				Vec3 vDummyWorldPos = posRot + vInitPos;
				vDummyWorldPos.z = requestedPos.z;
				pDummyTarget->SetPos(vDummyWorldPos);

				CCCPOINT(CFormation_InitFollowPoints_A);
			}

		}
	}
}

//----------------------------------------------------------------------
CAIObject* CFormation::GetFormationPoint(CWeakRef<const CAIObject> refRequester) const
{
	for (TFormationPoints::const_iterator itrPoint(m_FormationPoints.begin()); itrPoint != m_FormationPoints.end(); ++itrPoint)
		if ((*itrPoint).m_refReservation == refRequester)
			return (*itrPoint).m_refWorldPoint.GetAIObject();
	return NULL;
}

//----------------------------------------------------------------------
float CFormation::GetFormationPointSpeed(CWeakRef<const CAIObject> refRequester) const
{
	for (TFormationPoints::const_iterator itrPoint(m_FormationPoints.begin()); itrPoint != m_FormationPoints.end(); ++itrPoint)
		if ((*itrPoint).m_refReservation == refRequester)
			return (*itrPoint).m_Speed;
	return 0;
}

//----------------------------------------------------------------------
float CFormation::GetDistanceToOwner(int i) const
{

	if ((unsigned)i >= m_FormationPoints.size())
		return 0;
	if (m_FormationPoints[i].m_FollowDistance)
		return sqrtf(m_FormationPoints[i].m_FollowDistance * m_FormationPoints[i].m_FollowDistance +
		             m_FormationPoints[i].m_FollowOffset * m_FormationPoints[i].m_FollowOffset +
		             m_FormationPoints[i].m_FollowHeightOffset * m_FormationPoints[i].m_FollowHeightOffset);
	else
		return (m_FormationPoints[i].m_vPoint - m_FormationPoints[0].m_vPoint).GetLength();

}

//----------------------------------------------------------------------
int CFormation::CountFreePoints() const
{
	int iSize = GetSize();
	int iFree = 0;
	for (int i = 0; i < iSize; i++)
		if (m_FormationPoints[i].m_refReservation.IsNil())
			iFree++;
	return iFree;
}

//----------------------------------------------------------------------
float CFormationDescriptor::GetNodeDistanceToOwner(const FormationNode& nodeDescriptor) const
{
	if (m_Nodes.size())
	{
		if (nodeDescriptor.fFollowDistance)
			return sqrtf(nodeDescriptor.fFollowDistance * nodeDescriptor.fFollowDistance +
			             nodeDescriptor.fFollowOffset * nodeDescriptor.fFollowOffset);
		else
			return nodeDescriptor.vOffset.GetLength();
	}
	else
		return 0;
}

//----------------------------------------------------------------------
void CFormationDescriptor::AddNode(const FormationNode& nodeDescriptor)
{
	// insert new node descriptor in a distance-to-owner sorted vector
	if (m_Nodes.size() && nodeDescriptor.eClass != SPECIAL_FORMATION_POINT)
	{
		TVectorOfNodes::iterator itNext = m_Nodes.begin();
		float fDist = GetNodeDistanceToOwner(nodeDescriptor);
		for (TVectorOfNodes::iterator it = m_Nodes.begin(); it != m_Nodes.end(); ++it)
		{
			++itNext;
			if (itNext != m_Nodes.end())
			{
				float fDistNext = GetNodeDistanceToOwner(*itNext);
				if (fDist < fDistNext || itNext->eClass == SPECIAL_FORMATION_POINT)
				// leave the special formation points at the bottom
				{
					m_Nodes.insert(itNext, nodeDescriptor);
					break;
				}
			}
			else
			{
				m_Nodes.push_back(nodeDescriptor);
				break;
			}
		}
	}
	else
		m_Nodes.push_back(nodeDescriptor);
}

void CFormation::SetUpdate(bool bUpdate)
{
	m_bUpdate = bUpdate;
	if (bUpdate)
		m_fLastUpdateTime.SetValue(0); // force update the next time, when SetUpdate(true) is called
}

//----------------------------------------------------------------------
void CFormation::SetUpdateThresholds(float value)
{
	m_fDynamicUpdateThreshold = value;
	m_fStaticUpdateThreshold = 5 * value;
}

//----------------------------------------------------------------------
CAIObject* CFormation::GetClosestPoint(CAIActor* pRequester, bool bReserve, int iClass)
{
	int index = GetClosestPointIndex(GetWeakRef(pRequester), bReserve, 0, iClass);
	return (index < 0 ? NULL : m_FormationPoints[index].m_refWorldPoint.GetAIObject());
}

//
//--------------------------------------------------------------------------------------------------------------
void CFormation::Serialize(TSerialize ser)
{
	ser.BeginGroup("AIFormation");
	{
		ser.Value("m_Id", m_formationID);
		ser.Value("m_FormationPoints", m_FormationPoints);

		m_refOwner.Serialize(ser, "m_refOwner");

		ser.Value("m_bReservationAllowed", m_bReservationAllowed);

		ser.Value("m_vLastUpdatePos", m_vLastUpdatePos);
		ser.Value("m_vLastPos", m_vLastPos);
		ser.Value("m_vLastTargetPos", m_vLastTargetPos);
		ser.Value("m_vLastMoveDir", m_vLastMoveDir);
		ser.Value("m_vInitialDir", m_vInitialDir);
		ser.Value("m_vMoveDir", m_vMoveDir);
		ser.Value("m_bInitMovement", m_bInitMovement);
		ser.Value("m_bFirstUpdate", m_bFirstUpdate);
		ser.Value("m_bUpdate", m_bUpdate);
		ser.Value("m_bForceReachable", m_bForceReachable);
		ser.Value("m_bFollowingUnits", m_bFollowingUnits);
		ser.Value("m_bFollowingPointInitialized", m_bFollowPointInitialized);
		ser.Value("m_fUpdateThreshold", m_fUpdateThreshold);
		ser.Value("m_fDynamicUpdateThreshold", m_fDynamicUpdateThreshold);
		ser.Value("m_fStaticUpdateThreshold", m_fStaticUpdateThreshold);
		ser.Value("m_fScaleFactor", m_fScaleFactor);
		ser.Value("m_iSpecialPointIndex", m_iSpecialPointIndex);
		ser.Value("m_fMaxFollowDistance", m_fMaxFollowDistance);
		ser.Value("m_fLastUpdateTime", m_fLastUpdateTime);
		ser.Value("m_fMaxUpdateSightTime", m_maxUpdateSightTimeMs);
		ser.Value("m_fMinUpdateSightTime", m_minUpdateSightTimeMs);
		ser.Value("m_fSightRotationRange", m_fSightRotationRange);
		ser.Value("m_szDescriptor", m_szDescriptor);

		m_refReferenceTarget.Serialize(ser, "m_refReferenceTarget");

		ser.Value("m_fRepositionSpeedWrefTarget", m_fRepositionSpeedWrefTarget);
		ser.Value("m_vDesiredTargetPos", m_vDesiredTargetPos);
		ser.EnumValue("m_orientationType", m_orientationType, OT_MOVE, OT_VIEW);

		if (ser.IsWriting())
		{
			if (ser.BeginOptionalGroup("FormationPathMarker", m_pPathMarker != NULL))
			{
				if (m_pPathMarker)
					m_pPathMarker->Serialize(ser);
				ser.EndGroup();
			}
		}
		else
		{
			m_fLastUpdateTime = 0.0f;
			if (ser.BeginOptionalGroup("FormationPathMarker", true))
			{
				if (m_pPathMarker)
				{
					delete m_pPathMarker;
					m_pPathMarker = 0;
				}
				m_pPathMarker = new CPathMarker(m_fMaxFollowDistance + 10, m_fDISTANCE_UPDATE_THR / 2);
				m_pPathMarker->Serialize(ser);
				ser.EndGroup();
			}
		}
		ser.EndGroup();
	}

}

//
//--------------------------------------------------------------------------------------------------------------
CFormationPoint::CFormationPoint()
	: m_vPoint(ZERO)
	, m_vSight(ZERO)
	, m_FollowDistance(0.0f)
	, m_FollowOffset(0.0f)
	, m_FollowDistanceAlternate(0.0f)
	, m_FollowOffsetAlternate(0.0f)
	, m_FollowHeightOffset(0.0f)
	, m_Speed(0)
	, m_LastPosition(ZERO)
	, m_Dir(ZERO)
	, m_Class(0)
	, m_FollowFlag(false)
	, m_DEBUG_vInitPos(ZERO)
{

}

//
//--------------------------------------------------------------------------------------------------------------
CFormationPoint::~CFormationPoint()
{
}

//
//--------------------------------------------------------------------------------------------------------------
void CFormationPoint::Serialize(TSerialize ser)
{
	ser.BeginGroup("FormationPoint");
	ser.Value("m_vPoint", m_vPoint);
	ser.Value("m_vSight", m_vSight);
	ser.Value("m_FollowDistance", m_FollowDistance);
	ser.Value("m_FollowOffset", m_FollowOffset);
	ser.Value("m_FollowHeightOffset", m_FollowHeightOffset);
	ser.Value("m_FollowDistanceAlternate", m_FollowDistanceAlternate);
	ser.Value("m_FollowOffsetAlternate", m_FollowOffsetAlternate);
	ser.Value("m_Speed", m_Speed);
	ser.Value("m_LastPosition", m_LastPosition);
	ser.Value("m_Dir", m_Dir);
	ser.Value("m_Class", m_Class);

	m_refWorldPoint.Serialize(ser, "m_refWorldPoint");
	m_refReservation.Serialize(ser, "m_refReservation");
	m_refDummyTarget.Serialize(ser, "m_refDummyTarget");

	ser.Value("m_FollowFlag", m_FollowFlag);
	ser.EndGroup();
}

//
//--------------------------------------------------------------------------------------------------------------
void CFormation::Change(CFormationDescriptor& desc, float fScale)
{
	if (desc.m_sName != m_szDescriptor)
	{

		if (desc.m_Nodes.empty() || m_FormationPoints.empty())
			return;

		m_szDescriptor = desc.m_sName;
		CFormationDescriptor::TVectorOfNodes::iterator vi;
		TFormationPoints::iterator fi;

		std::vector<bool> vDescChanged;
		std::vector<bool> vPointChanged;

		for (vi = desc.m_Nodes.begin(); vi != desc.m_Nodes.end(); ++vi)
			vDescChanged.push_back(false);

		for (fi = m_FormationPoints.begin(); fi != m_FormationPoints.end(); ++fi)
			vPointChanged.push_back(false);

		m_bFollowingUnits = false;

		m_fMaxFollowDistance = 0;
		//		float m_fMaxYOffset = -1000000;
		//		m_iForemostNodeIndex = -1;
		m_iSpecialPointIndex = -1;

		m_fScaleFactor = 1;

		float oldMaxFollowDistance = m_fMaxFollowDistance;
		m_fMaxFollowDistance = 0;
		//int i =0;
		for (int iteration = 0; iteration < 2; iteration++)
		{
			// iteration = 0 : consider only current points with a reservation (change them first)
			// iteration = 1 : consider all current points
			int i = 0;
			TFormationPoints::iterator itrPoint(m_FormationPoints.begin());

			for (++itrPoint; itrPoint != m_FormationPoints.end(); ++itrPoint)
			{
				CFormationPoint& frmPoint = (*itrPoint);

				if (iteration == 0 && !frmPoint.m_refReservation.IsValid() || iteration == 1 && vPointChanged[i])
					continue;

				int iClass = frmPoint.m_Class;

				bool bFound = false;
				for (int iTestType = 0; iTestType < 5 && !bFound; iTestType++)
				{
					int j = 1;
					vi = desc.m_Nodes.begin();
					for (++vi; !bFound && vi != desc.m_Nodes.end(); ++vi)
					{
						int iNewClass = (*vi).eClass;
						if (!vDescChanged[j] &&
						    (iTestType == 0 && iNewClass == iClass ||
						     iTestType == 1 && (iNewClass & iClass) == min(iNewClass, iClass) ||
						     iTestType == 2 && (iNewClass & iClass) != 0 ||
						     iTestType == 3 && (iNewClass == UNIT_CLASS_UNDEFINED) ||
						     iTestType == 4))
						{
							Vec3 pos = (*vi).vOffset;
							float fDistance = (*vi).fFollowDistance;
							frmPoint.m_FollowDistance = fDistance;

							if (frmPoint.m_FollowDistance > 0)
							{
								pos = Vec3((*vi).fFollowOffset, fDistance, 0);
								if (m_fMaxFollowDistance < frmPoint.m_FollowDistance)
									m_fMaxFollowDistance = frmPoint.m_FollowDistance;
							}
							/*else if(m_fMaxYOffset < pos.y)
							   { // find the foremost formation unit to be followed
							   m_fMaxYOffset = pos.y;
							   m_iForemostNodeIndex = i;
							   }*/
							if (iClass == SPECIAL_FORMATION_POINT)
								m_iSpecialPointIndex = i;
							frmPoint.m_vPoint = pos;
							frmPoint.m_FollowOffset = (*vi).fFollowOffset;

							frmPoint.m_FollowDistanceAlternate = (*vi).fFollowDistanceAlternate;
							frmPoint.m_FollowOffsetAlternate = (*vi).fFollowOffsetAlternate;
							frmPoint.m_FollowHeightOffset = (*vi).fFollowHeightOffset;

							vDescChanged[j] = true;
							vPointChanged[i] = true;
							bFound = true;
						}
						j++;
					} // vi

				} // testType

				++i;
			} // itrpoint
		}   // iteration
		//		if(m_iForemostNodeIndex<0)
		//			m_bFollowingUnits = false; // if it was true but no foremost node, it means that all the nodes
		// were set to follow
		int i = 0;

		// Delete old points not used and set the flag m_bFollowingUnits
		for (TFormationPoints::iterator itrPoint(m_FormationPoints.begin()); itrPoint != m_FormationPoints.end(); )
		{
			CFormationPoint& frmPoint = (*itrPoint);
			if (!vPointChanged[i] && !frmPoint.m_refReservation.IsValid())
			{
				frmPoint.m_refWorldPoint.Release();
				frmPoint.m_refDummyTarget.Release();
				itrPoint = m_FormationPoints.erase(itrPoint);
			}
			else
			{

				if (frmPoint.m_FollowDistance > 0)
					m_bFollowingUnits = true;
				++itrPoint;
			}
			i++;
		}

		// Add new points
		int j = 1;
		vi = desc.m_Nodes.begin();
		for (++vi; vi != desc.m_Nodes.end(); ++vi)
		{
			if (!vDescChanged[j])
			{
				FormationNode& descNode = *vi;
				AddPoint(descNode);
			}
			j++;
		}

		m_bInitMovement = true;
		m_bFirstUpdate = true;
		m_bFollowPointInitialized = false;

		CAIObject* const pOwner = m_refOwner.GetAIObject();
		if (pOwner)
		{
			if (m_bFollowingUnits && oldMaxFollowDistance < m_fMaxFollowDistance)
			{
				if (m_pPathMarker)
				{
					delete m_pPathMarker;
					m_pPathMarker = 0;
				}
				m_pPathMarker = new CPathMarker(m_fMaxFollowDistance + 10, m_fDISTANCE_UPDATE_THR / 2);
				m_pPathMarker->Init(pOwner->GetPos(), pOwner->GetPos() - m_fMaxFollowDistance * m_vMoveDir);
			}
			InitWorldPosition(m_vMoveDir);
		}
	}

	if (fScale != m_fScaleFactor)
	{
		// (MATT) The only call {2009/02/14}
		SetScale(fScale);
	}

}
//
//--------------------------------------------------------------------------------------------------------------
void CFormation::SetScale(float fScale)
{
	// to do: retrieve the original offsets/distances when the old scale factor is 0?
	if (fScale == 0)
		return;

	if (fScale == m_fScaleFactor)
		return;

	float fActualScale = fScale / m_fScaleFactor;
	m_fMaxFollowDistance *= fActualScale;
	m_fScaleFactor = fScale;

	if (!m_FormationPoints.empty())
	{
		// modify all points
		for (TFormationPoints::iterator itrPoint(m_FormationPoints.begin()); itrPoint != m_FormationPoints.end(); ++itrPoint)
		{
			itrPoint->m_vPoint *= fActualScale;
			itrPoint->m_FollowDistance *= fActualScale;
			itrPoint->m_FollowOffset *= fActualScale;
			itrPoint->m_FollowDistanceAlternate *= fActualScale;
			itrPoint->m_FollowOffsetAlternate *= fActualScale;
			itrPoint->m_FollowHeightOffset *= fActualScale;
		}

		CAIObject* const pOwner = m_FormationPoints[0].m_refReservation.GetAIObject();
		if (!pOwner)
			return;

		CCCPOINT(CFormation_SetScale);

		// find the formation orientation
		Vec3 vDir = m_vMoveDir;
		if (vDir.IsZero())
		{
			if (m_bFollowingUnits && m_pPathMarker)
			{
				vDir = m_pPathMarker->GetPointAtDistance(pOwner->GetPos(), 1);
				vDir -= pOwner->GetPos();
				vDir.z = 0; //2d only
				vDir.NormalizeSafe();
				if (vDir.IsZero())
					vDir = pOwner->GetMoveDir();
			}
			else
			{
				pe_status_dynamics dSt;
				pOwner->GetProxy()->GetPhysics()->GetStatus(&dSt);
				vDir = -dSt.v;
				if (!vDir.IsZero())
					vDir.Normalize();
				else
					vDir = pOwner->GetMoveDir();

			}
		}
		SetOffsetPointsPos(vDir);
		// modify path marker if the new formation is bigger
		if (m_bFollowingUnits && fActualScale > 1)
		{
			vDir.z = 0;
			vDir.NormalizeSafe();
			m_vInitialDir = vDir;

			if (m_pPathMarker)
			{
				delete m_pPathMarker;
				m_pPathMarker = 0;
			}

			// (MATT) Apart from serialisation, this is the only place the marker is set {2009/02/14}
			CCCPOINT(CFormation_SetScale_NewPathMarker);
			m_pPathMarker = new CPathMarker(m_fMaxFollowDistance + 10, m_fDISTANCE_UPDATE_THR / 2);
			m_pPathMarker->Init(pOwner->GetPos(), pOwner->GetPos() - m_fMaxFollowDistance * vDir);
			InitFollowPoints();//,vDir,pOwner);

		}
	}
}

Vec3 CFormation::GetPredictedPointPosition(CWeakRef<const CAIObject> refRequestor, const Vec3& ownerPos, const Vec3& ownerLookDir, Vec3 ownerMoveDir) const
{
	const CFormationPoint* pFormPoint = NULL;
	if (ownerMoveDir.IsZero())
		ownerMoveDir = ownerLookDir;

	for (TFormationPoints::const_iterator itrPoint(m_FormationPoints.begin()); itrPoint != m_FormationPoints.end(); ++itrPoint)
		if (itrPoint->m_refReservation == refRequestor)
		{
			pFormPoint = &(*itrPoint);
			break;
		}

	if (!pFormPoint)
		return ZERO;

	CCCPOINT(CFormation_GetPredictedPointPosition);

	Vec3 vPredictedPos;
	if (pFormPoint->m_FollowDistance > 0)
	{
		// approximate, assumes that the follow points will be aligned along ownerDir
		vPredictedPos.Set(pFormPoint->m_FollowOffset * (ownerMoveDir.x),
		                  pFormPoint->m_FollowDistance * (-ownerMoveDir.y),
		                  pFormPoint->m_FollowHeightOffset);
		vPredictedPos += ownerPos;
		return vPredictedPos;
	}
	else
	{
		vPredictedPos.Set(pFormPoint->m_vPoint.x * ownerLookDir.y, pFormPoint->m_vPoint.y * (-ownerLookDir.x), pFormPoint->m_vPoint.z);
		vPredictedPos += ownerPos;
		return vPredictedPos;
	}
}

void CFormation::SetReferenceTarget(const CAIObject* pTarget, float speed)
{
	m_refReferenceTarget = GetWeakRef(pTarget);
	m_vLastTargetPos.zero();
	m_fRepositionSpeedWrefTarget = speed;
	m_vDesiredTargetPos.zero();
	m_vLastUpdatePos.zero(); //to force update
	Update();
}

void CFormation::SetUpdateSight(float angleRange, float minTime /*=0*/, float maxTime /*=0*/)
{
	m_fSightRotationRange = angleRange;
	m_minUpdateSightTimeMs = (int)(minTime * 1000.0f);
	m_maxUpdateSightTimeMs = (int)(maxTime * 1000.0f);
}
