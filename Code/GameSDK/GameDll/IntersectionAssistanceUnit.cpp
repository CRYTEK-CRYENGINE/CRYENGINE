// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "IntersectionAssistanceUnit.h"
#include "Player.h"
#include <CryMath/Cry_GeoOverlap.h>

#ifndef _RELEASE
#include "Utility/CryWatch.h"
#endif //#ifndef _RELEASE

//  (Will move to CVars if really have to.. trying to avoid adding yet more longwinded spam). 
static const float kPhysBoxScaleFactor									= 0.95f; 
static const uint8 kMaxNumOOBBIntersectionTestsPerFrame	= 2; // Timeslicing
static const float kFloorAdjustConstant									= 1.1f;
static const float kDistanceTolerance										= 3.0f; 
static const float kInverseDistanceTolerance						= 1.0f / kDistanceTolerance; 
static const float kDistanceWeight											= 1.25f; 
static const float kOrientationWeight										= 1.0f;

CIntersectionAssistanceUnit::TRotationContainer CIntersectionAssistanceUnit::s_embedPreventRotations = TRotationContainer(); 

CIntersectionAssistanceUnit::CIntersectionAssistanceUnit(): 
m_embedTimer(0.0f), 
m_subjectEntityId(0), 
m_entityStartingWPos(0.0f,0.0f,0.0f), 
m_collisionCount(0),
m_embedState(eES_None),
m_resolutionPolicy(eRP_None),
m_bClientHasAuthority(true),
m_focalEntityId(0),
m_currentTestIndex(0),
m_bEnabled(false)
#ifndef _RELEASE
,m_currentBestIndex(0)
#endif //#ifndef _RELEASE
{
	 
}

CIntersectionAssistanceUnit::~CIntersectionAssistanceUnit()
{
	m_intersectionTester.Reset();
}

void CIntersectionAssistanceUnit::UpdateCheckingForObjectEmbed(const float dt)
{
	if(IsClientResponsibleForSubjectEntity())
	{
		if(m_embedTimer > 0.0f)
		{
			m_embedTimer -= dt;
			if(m_embedTimer < 0.0f)
			{
				IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_subjectEntityId);
				if(pEntity)
				{

					// Make our decision, is this object stuck?
					Vec3 translation = m_entityStartingWPos - pEntity->GetWorldPos(); 
					const float transPerSec = translation.GetLength() / g_pGameCVars->pl_pickAndThrow.intersectionAssistTimePeriod; 
					const float countsPerSec  = m_collisionCount / g_pGameCVars->pl_pickAndThrow.intersectionAssistTimePeriod;

					if( countsPerSec >= g_pGameCVars->pl_pickAndThrow.intersectionAssistCollisionsPerSecond && 
							transPerSec <= g_pGameCVars->pl_pickAndThrow.intersectionAssistTranslationThreshold )
					{
						OnDetectObjectEmbedded(); 
					}
					else
					{
						// Are we still suspicious?
						if(countsPerSec >= g_pGameCVars->pl_pickAndThrow.intersectionAssistCollisionsPerSecond)
						{
							// The object moved a fair distance, but its getting an awful lot of high penetration collision events - Test a while longer
							BeginCheckingForObjectEmbed();
							m_embedState = eES_ReEvaluating;
						}
						else
						{
							m_embedState = eES_NotEmbedded; 
						}
					}
				}
			}
		}
	}

#ifndef _RELEASE
	DebugUpdate(); 
#endif //#ifndef _RELEASE
}

void CIntersectionAssistanceUnit::BeginCheckingForObjectEmbed()
{
	// Assess behaviour of entity physics over a predefined period. 
	m_embedTimer			= g_pGameCVars->pl_pickAndThrow.intersectionAssistTimePeriod; 
	m_collisionCount	= 0; 

	// Record starting position
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_subjectEntityId);
	if(pEntity)
	{
		m_entityStartingWPos = pEntity->GetWorldPos(); 
	}

	m_embedState			= eES_Evaluating; 
}

#ifndef _RELEASE
void CIntersectionAssistanceUnit::DebugUpdate() const
{
	if(g_pGameCVars->pl_pickAndThrow.intersectionAssistDebugEnabled)
	{
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_subjectEntityId);
		if(pEntity)
		{
			IPhysicalEntity *pPhysical = pEntity->GetPhysics();
			if(pPhysical)
			{
				const float fFontSize = 1.2f;
				float drawColor[4] = {1.0f, 1.0f, 1.0f, 1.0f};

				string sMsg(string().Format(" Entity ID: [%d]", m_subjectEntityId));
				sMsg += string().Format("\n Entity Name: [%s]", pEntity->GetName());

				sMsg += string().Format("\n EmbedTimer: [%.3f]", m_embedTimer);
				sMsg += string().Format("\n EmbedState: [%s]",(m_embedState == eES_None) ? "NONE" : (m_embedState == eES_Evaluating) ? "EVALUATING" : (m_embedState == eES_ReEvaluating) ? "REEVALUATING" : (m_embedState == eES_NotEmbedded) ? "NOT EMBEDDED" : (m_embedState == eES_Embedded) ? "EMBEDDED" : "UNKNOWN");

				Vec3 vCurrTrans = m_entityStartingWPos - pEntity->GetWorldPos(); 
				sMsg += string().Format("\n Translation: < %.3f, %.3f, %.3f >", vCurrTrans.x, vCurrTrans.y, vCurrTrans.z );
				sMsg += string().Format("\n Trans magnitude: < %.3f >", vCurrTrans.GetLength() );
				sMsg += string().Format("\n Trans per sec: < %.3f >", vCurrTrans.GetLength() / g_pGameCVars->pl_pickAndThrow.intersectionAssistTimePeriod );

				sMsg += string().Format("\n Collision count: %u", m_collisionCount );

				// RENDER
				Vec3 vDrawPos = pEntity->GetWorldPos() + Vec3(0.0f,0.0f,0.6f);
				IRenderAuxText::DrawLabelEx(vDrawPos, fFontSize, drawColor, true, true, sMsg.c_str());

				// Box
				pe_params_bbox bbox;
				if(pPhysical->GetParams(&bbox))
				{
					ColorB colDefault = ColorB( 127,127,127 );
					ColorB embedded = ColorB(255, 0, 0);
					ColorB notEmbedded = ColorB(0, 255, 0);

					gEnv->pRenderer->GetIRenderAuxGeom()->DrawAABB( AABB(bbox.BBox[0],bbox.BBox[1]), Matrix34(IDENTITY), false, (m_embedState == eES_Embedded) ? embedded : (m_embedState == eES_NotEmbedded) ? notEmbedded : colDefault, eBBD_Faceted);
				}
			}
		}
		
	}
}
#endif // #ifndef _RELEASE

void CIntersectionAssistanceUnit::OnDetectObjectEmbedded()
{
	m_embedState = eES_Embedded; 

	// How do we react to the bad news?
	if(m_resolutionPolicy == eRP_DeleteObject)
	{
		CGameRules *pGameRules=g_pGame->GetGameRules();
		assert(pGameRules);
		if (pGameRules)
		{
			pGameRules->ScheduleEntityRemoval(m_subjectEntityId, 0.0f, true);
		}
	}
	else
	{
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_subjectEntityId);
		if(pEntity)
		{
			IPhysicalEntity* pPhysical = pEntity->GetPhysics();
			if(pPhysical)
			{
				// Force entity to sleep (for now)
				pe_action_awake pa;
				pa.bAwake = 0;
				pPhysical->Action( &pa );
			}
		}
	}
}

void CIntersectionAssistanceUnit::Init( EntityId subjectEntityId, eResolutionPolicy resolutionPolicy )
{
	m_subjectEntityId  = subjectEntityId; 
	m_resolutionPolicy = resolutionPolicy; 
	m_bEnabled = true; 
	m_intersectionTester.Init(this);

	// Static array. Init only performed once. 
	if(s_embedPreventRotations.empty())
	{
		s_embedPreventRotations.push_back(Quat::CreateRotationX(gf_PI * 0.25f));
		s_embedPreventRotations.push_back(Quat::CreateRotationXYZ(Ang3_tpl<f32>(gf_PI * 0.25f,0.0f,gf_PI * 0.5f)));
		s_embedPreventRotations.push_back(Quat::CreateRotationXYZ(Ang3_tpl<f32>(gf_PI * 0.25f,0.0f,gf_PI * 1.0f)));
		s_embedPreventRotations.push_back(Quat::CreateRotationXYZ(Ang3_tpl<f32>(gf_PI* 0.25f,0.0f,gf_PI* 1.5f)));
		s_embedPreventRotations.push_back(Quat::CreateRotationX(gf_PI * 0.5f));
		s_embedPreventRotations.push_back(Quat::CreateRotationXYZ(Ang3_tpl<f32>(gf_PI * 0.5f,0.0f,gf_PI * 0.25f)));
		s_embedPreventRotations.push_back(Quat::CreateRotationXYZ(Ang3_tpl<f32>(gf_PI * 0.5f,0.0f,gf_PI * 0.5f)));
		s_embedPreventRotations.push_back(Quat::CreateRotationXYZ(Ang3_tpl<f32>(gf_PI * 0.5f,0.0f,gf_PI * 0.75f)));
	}
	
	m_lastKnownGoodPositions.clear();
	const int num = s_embedPreventRotations.size();
	m_lastKnownGoodPositions.resize(num);
	for(int i=0; i<num; i++)
	{
		m_lastKnownGoodPositions[i].SetIdentity();
	}
	m_intersectionTester.Reset(); 
	m_intersectionTester.SetNumTestOffsets(s_embedPreventRotations.size());
}

void CIntersectionAssistanceUnit::OnCollision( const EventPhysCollision& collision )
{
	if(IsClientResponsibleForSubjectEntity())
	{
		if(collision.idCollider != -2 &&  // -2 == water... :( 
			m_embedTimer > 0.0f && 
			collision.penetration > g_pGameCVars->pl_pickAndThrow.intersectionAssistPenetrationThreshold)
		{
			// We want to work out some 'collision per second' type value, that we can use as a threshold to work out if something is inside geom and jittering
			++m_collisionCount;
		}
		else if (m_embedState == eES_Embedded) // If disabled and have now been collided with again (and deletion on embed isnt enabled)
		{
			IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_subjectEntityId);
			if(pEntity)
			{
				IPhysicalEntity* pPhysical = pEntity->GetPhysics();
				if(pPhysical)
				{
					pe_status_awake psa;
					const bool bIsSleeping = pPhysical->GetStatus( &psa ) ? false : true;
					if(!bIsSleeping) // Have we just woken up?
					{
						BeginCheckingForObjectEmbed();
					}
				}
			}
		}
	}
}

void CIntersectionAssistanceUnit::AbortCheckingForObjectEmbed()
{
	m_embedState  = eES_None;

	m_embedTimer  = -1.0f;
	m_collisionCount = 0;
	m_entityStartingWPos = Vec3(0.0f,0.0f,0.0f);
}

bool CIntersectionAssistanceUnit::IsClientResponsibleForSubjectEntity() const
{
	// If deleting an object upon detection, only the server can kick off the process
	if(gEnv->bServer && m_resolutionPolicy == eRP_DeleteObject)
	{
		return true;
	}
	else
	{
		return m_bClientHasAuthority; 
	}
}

bool CIntersectionAssistanceUnit::TestForIntersectionAtLocation(const eTestMethod testMethod, const Matrix34& wMat, EntityId testEntityId, EntityId ignoreEnt, QuatT& outAdjustedResult, const bool bCentreOnFocalEnt /* = false */, bool bRenderOnFail /* = true */, const int index /* = -1*/) 
{
	// Build an OOBB that surrounds this entity, test for intersection between that and world
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(testEntityId);
	if(pEntity)
	{
		IPhysicalEntity* pPhysical = pEntity->GetPhysics();
		if(pPhysical)
		{
			OBB entOBB;
			AABB entAABB;
			pEntity->GetLocalBounds(entAABB);
			entOBB.SetOBBfromAABB(Quat(IDENTITY), entAABB);

			// Do Primitive world intersection
			primitives::box physBox; 
			physBox.bOriented = 1;

			// LSpace
			physBox.center = entOBB.c;
			physBox.Basis = entOBB.m33;
			physBox.size.x = entOBB.h.x; 
			physBox.size.y = entOBB.h.y; 
			physBox.size.z = entOBB.h.z; 

			// WSpace
			physBox.center					= wMat.TransformPoint(physBox.center); 
			physBox.Basis					  *= Matrix33(wMat).GetInverted();

			// Optional tweak - We can get away with a little bit of scaling down (if edges are slightly embedded the physics pushes them out easily)
			physBox.size = physBox.size.scale(kPhysBoxScaleFactor);

			// adjust
			Vec3 vAdjustments(0.0f,0.0f,0.0f); 
			if(bCentreOnFocalEnt && m_focalEntityId)
			{
				Vec3 vDesiredPos = CalculateTargetAdjustPoint(pEntity, wMat, physBox.center);
				vAdjustments = (vDesiredPos - physBox.center);
				physBox.center += vAdjustments;
			}
			
			IEntity* pIgnoreEnt = gEnv->pEntitySystem->GetEntity(ignoreEnt);
			IPhysicalEntity* pIgnorePhys = pIgnoreEnt ? pIgnoreEnt->GetPhysics() : NULL;

			// Test
			if(testMethod == eTM_Immediate 
#ifndef _RELEASE
				|| g_pGameCVars->pl_pickAndThrow.intersectionAssistDebugEnabled >= 1 
#endif // #ifndef _RELEASE
				)
			{
				geom_contact *contacts;
				intersection_params params;
				float numHits = gEnv->pPhysicalWorld->PrimitiveWorldIntersection(primitives::box::type, &physBox, Vec3(ZERO),
					ent_static|ent_terrain, &contacts, 0,
					3, &params, 0, 0, &pIgnorePhys, pIgnorePhys ? 1 : 0);

				// Debug
#ifndef _RELEASE
				if(g_pGameCVars->pl_pickAndThrow.intersectionAssistDebugEnabled)
				{
				
					const bool bIntersect = numHits <= 0.0f ? false : true;
					if(bRenderOnFail || !bIntersect)
					{
						const ColorB colorPositive = ColorB(16, 96, 16);
						const ColorB colorNegative = ColorB(128, 0, 0);
						const ColorB colorSelected = ColorB(0,255,0);
						
						if(numHits > 0.0f)
						{
							gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(contacts->pt, 0.1f, colorPositive);
						}

						OBB finalOBB;
						finalOBB.SetOBB(Matrix33(IDENTITY), physBox.size, Vec3(0.0f,0.0f,0.0f));
						Matrix34 drawMat = wMat;
						drawMat.AddTranslation(physBox.center - wMat.GetTranslation());
						if(index != -1 && index == m_currentBestIndex)
						{
							gEnv->pRenderer->GetIRenderAuxGeom()->DrawOBB(finalOBB, drawMat, false, colorSelected, eBBD_Faceted);
						}
						else
						{
							gEnv->pRenderer->GetIRenderAuxGeom()->DrawOBB(finalOBB, drawMat, false, bIntersect ? colorNegative : colorPositive, eBBD_Faceted);
						}
		
					}
				}
#endif //#ifndef RELEASE

				// If we performed an adjust, make sure we pass out the QuatT representing the FINAL ENTITY POSITION that passed/failed (not the phys box etc)
				outAdjustedResult.t = wMat.GetTranslation() + vAdjustments;
				outAdjustedResult.q = Quat(wMat);

#ifndef _RELEASE
				// allow optional debug drawing of last known good positions by retaining non adjusted position
				if(g_pGameCVars->pl_pickAndThrow.intersectionAssistDebugEnabled == 1)
				{
					outAdjustedResult.t = wMat.GetTranslation();
				}
#endif // #ifndef _RELEASE
			
				return (numHits > 0.0f); 
			}
			else
			{
				// QUEUE primitive intersection check
				outAdjustedResult.t = wMat.GetTranslation() + vAdjustments;
				outAdjustedResult.q = Quat(wMat);
				CRY_ASSERT(index >= 0);
				m_intersectionTester.DoCheck(index,physBox,outAdjustedResult,pIgnorePhys);
				return false;
			}
		}
	}

	return false; 
}

void CIntersectionAssistanceUnit::UpdateEmbeddingPrevention()
{

#ifndef _RELEASE
	// allow optional debug drawing of last known good positions by retaining non adjusted position
	if(g_pGameCVars->pl_pickAndThrow.intersectionAssistDebugEnabled == 2)
	{
		QuatT qSafe;
		GetSafeLocationForSubjectEntity(qSafe);
		const ColorB colorPositive = ColorB(128, 255, 128);
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(qSafe.t, 0.1f, colorPositive);
		Vec3 dirVec = qSafe.q.GetColumn2().GetNormalized(); 
		Vec3 endPos = Vec3(qSafe.t);
		endPos += dirVec;
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(qSafe.t, colorPositive,  endPos, colorPositive);
	}
#endif // #ifndef _RELEASE

	// Lets queue as many intersection tests as allowed.
	IEntity* pFocalEnt = gEnv->pEntitySystem->GetEntity(m_focalEntityId);
	if(pFocalEnt)
	{
		if(!s_embedPreventRotations.empty())
		{
			const Matrix34& focalMat = pFocalEnt->GetWorldTM();
			const QuatT qFocalQuatT(focalMat);

			int count = 0; 
			const uint8 numTestRotations = s_embedPreventRotations.size(); 
			while(true)
			{
				CRY_ASSERT(m_currentTestIndex < m_lastKnownGoodPositions.size());

				// request a primitive world intersection on the physics thread + process the results on callback
				eTestMethod testMethod = eTM_Deferred; 
#ifndef _RELEASE
				if(g_pGameCVars->pl_pickAndThrow.intersectionAssistDebugEnabled)
				{
					testMethod = eTM_Immediate;
				}
#endif // #ifndef _RELEASE
				TestForIntersection(testMethod, qFocalQuatT,s_embedPreventRotations[m_currentTestIndex], true, m_currentTestIndex);
				
				++m_currentTestIndex; 

				// WRAP
				if(m_currentTestIndex >= numTestRotations)
				{
					m_currentTestIndex = 0; 
				}

				++count;
				if(count >= kMaxNumOOBBIntersectionTestsPerFrame ||
						count >= numTestRotations)
				{
					return;
				}
			}
		}
	}
}

bool CIntersectionAssistanceUnit::TestForIntersection(const eTestMethod testMethod, const QuatT& qWOrient, const Quat& qRotOffset, const bool bCentreOnFocalEnt, int index )
{
	QuatT temp = qWOrient; 
	temp.q *= qRotOffset;
	Matrix34 finalMatrix = Matrix34(temp);
	QuatT outAdjustedResult; 
	if(!TestForIntersectionAtLocation(testMethod, finalMatrix, m_subjectEntityId, m_focalEntityId, outAdjustedResult,bCentreOnFocalEnt, false, index))
	{
		if(testMethod == eTM_Immediate)
		{
			m_lastKnownGoodPositions[index] = outAdjustedResult;
		}
		return false;
	}
	#ifndef _RELEASE
	else if(g_pGameCVars->pl_pickAndThrow.intersectionAssistDebugEnabled == 1)
	{	
		const QuatT& qLastKnownGood =m_lastKnownGoodPositions[index]; 
		if(IsPositionWithinAcceptedLimits(qLastKnownGood.t, finalMatrix.GetTranslation(), 1.5f))
		{
			QuatT dummyTest;
			TestForIntersectionAtLocation(eTM_Immediate, Matrix34(qLastKnownGood), m_subjectEntityId, m_focalEntityId, dummyTest, bCentreOnFocalEnt, true, index);
		}
	}
	#endif // #ifndef _RELEASE

	return true;
	
}

void CIntersectionAssistanceUnit::SetFocalEntityId( EntityId id )
{
	m_focalEntityId = id;

	// Whenever this changes, clear out any saved data.
	m_lastKnownGoodPositions.clear();
	if(id)
	{
		const int num = s_embedPreventRotations.size();
		m_lastKnownGoodPositions.resize(num);
		for(int i=0; i<num; i++)
		{
			m_lastKnownGoodPositions[i].SetIdentity();
		}
	}
	else
	{
		m_lastKnownGoodPositions.resize(0);  
	}
	m_intersectionTester.Reset(); 
}

// Calculates the desired position for the physics box, so that its center will be superimposed with AABB center of provided entity. 
// Also adjusts upwards to avoid any obvious floor clipping.  Returns desired Position for entity.
Vec3 CIntersectionAssistanceUnit::CalculateTargetAdjustPoint(const IEntity* pEntity, const Matrix34 &wMat, const Vec3& vStartingPos) const
{
	// (if present + desired) adjust physbox center to that of owner Ent - to make sure centers of PhysBox + focal ent superimposed
	// at the desired position
	const IEntity* pFocalEnt = gEnv->pEntitySystem->GetEntity(m_focalEntityId);
	if(pFocalEnt)
	{
		OBB focalOBB;
		AABB focalAABB;

		// Compensate for actor/non actor entities that require different paths :(
		if(CPlayer* pPlayer = static_cast<CPlayer*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(m_focalEntityId)))
		{
			EStance playerStance = pPlayer->GetStance();
			focalAABB = pPlayer->GetStanceInfo(playerStance)->GetStanceBounds();
		}
		else
		{
			pFocalEnt->GetLocalBounds(focalAABB);
		}
		focalOBB.SetOBBfromAABB(Quat(IDENTITY), focalAABB);

		// shift to match focus ent Center (taking into account crouch etc if player). 
		float fVerticalAdjust = focalOBB.h.z; 

		// Additionally.. if the new test pos *would* immediately penetrate the floor (assumption based on any part of the volume being < player AABB z min value)
		// shift it up. 
		float fFloorPenetrationAdjust = 0.0f; 
		AABB wEntABB;
		pEntity->GetLocalBounds(wEntABB);
		wEntABB.SetTransformedAABB(wMat,wEntABB);
		float fFloorClearance =  focalOBB.h.z - (wEntABB.GetSize().z * 0.5f); 
		
		fFloorPenetrationAdjust += (0.0f - min(fFloorClearance, 0.0f));

		// Apply floor clearance + Vertical adjust
		Vec3 desiredPos = wMat.GetTranslation() + Vec3(0.0f,0.0f,(fFloorPenetrationAdjust) * kFloorAdjustConstant); 
		desiredPos += (fVerticalAdjust * pFocalEnt->GetWorldTM().GetColumn2() * kFloorAdjustConstant);  
		return desiredPos;
	}

	return wMat.GetTranslation();
}

// Returns true if object requires adjusting from current location
bool CIntersectionAssistanceUnit::GetSafeLocationForSubjectEntity(QuatT& safePos)
{
	safePos = QuatT(IDENTITY);
	const IEntity* pSubjectEntity = gEnv->pEntitySystem->GetEntity(m_subjectEntityId);
	if(pSubjectEntity)
	{
		// 1) If current *real/actual* orientation safe, return that
		const Matrix34& wMatActual = pSubjectEntity->GetWorldTM(); 
		if(TestForIntersectionAtLocation(eTM_Immediate, wMatActual, m_subjectEntityId, m_focalEntityId,safePos)
#ifndef _RELEASE
			|| g_pGameCVars->pl_pickAndThrow.intersectionAssistDebugEnabled >= 1
#endif //#ifndef _RELEASE
			) 
		{
			// 2) return "best" last known good pos ( < distance +  > orientation match scored higher)
			if(!GetHighestScoringLastKnownGoodPosition(QuatT(wMatActual), safePos))
			{
				// 2) If still no candidates, fall back on upright default orientation on focal entity (e.g. in player capsule) 
				const IEntity* pFocalEntity = gEnv->pEntitySystem->GetEntity(m_focalEntityId);
				if(pFocalEntity)
				{
					const Matrix34& focalMat = pFocalEntity->GetWorldTM();
					Vec3 vDesiredPos = CalculateTargetAdjustPoint(pFocalEntity, focalMat, focalMat.GetTranslation()); // adjusting to prevent floor penetration etc
					safePos.t = vDesiredPos;
					safePos.q = Quat(focalMat);
				}
			}		
		}
		else
		{
			// Current position already good
			return false;
		}
	}

	// object requires adjust
	return true;
}

bool CIntersectionAssistanceUnit::GetHighestScoringLastKnownGoodPosition( const QuatT& baseOrientation, QuatT& outQuat ) const
{
	bool bFlippedIsBest = false;

	if(!m_lastKnownGoodPositions.empty())
	{
		// Higher is better
		float fBestScore = 0.0f; 
		int bestIndex = -1; 
		Vec3 vBaseUpDir = baseOrientation.q.GetColumn2().GetNormalized(); 
		for(uint8 i = 0; i < m_lastKnownGoodPositions.size(); ++i)
		{
			const QuatT& qLastKnownGood = m_lastKnownGoodPositions[i]; 
			if(IsPositionWithinAcceptedLimits(qLastKnownGood.t, baseOrientation.t, kDistanceTolerance))
			{
				// Generate [0.0f,1.0f] score for distance
				const Vec3 distVec = (qLastKnownGood.t - baseOrientation.t); 

				const float length = max(distVec.GetLengthFast(),0.0001f);
				const float distanceScore = max(1.0f - (length * kInverseDistanceTolerance) * kDistanceWeight, 0.0f);

				Vec3 vUpDir		 = qLastKnownGood.q.GetColumn2();

				const float regularOrientationScore = vBaseUpDir.Dot(vUpDir);
				const float flippedOrientationScore = vBaseUpDir.Dot(-vUpDir);

				float orientationScore = max(regularOrientationScore, flippedOrientationScore);
				orientationScore *= kOrientationWeight;

				const float fCandidateScore = distanceScore + orientationScore;

#ifndef _RELEASE
				if(g_pGameCVars->pl_pickAndThrow.intersectionAssistDebugEnabled == 2)
				{
					CryWatch("[INDEX(%d)] : D[%.3f] O[%.3f] T[%.3f] (%s)", i, distanceScore, orientationScore, fCandidateScore, flippedOrientationScore > regularOrientationScore ? "*F*" : "R");
				}
#endif //#ifndef _RELEASE

				if(fCandidateScore > fBestScore)
				{
					bestIndex	 = i; 
					fBestScore = fCandidateScore;
					bFlippedIsBest = (flippedOrientationScore > regularOrientationScore);
				}
			}
		}

		if(bestIndex >= 0)
		{
			outQuat = m_lastKnownGoodPositions[bestIndex];
			if(bFlippedIsBest)
			{
				Matrix34 wMat(outQuat); 
				Vec3 vFlippedUpDir = -outQuat.q.GetColumn2().GetNormalized();
				Vec3 vForwardDir	 = outQuat.q.GetColumn1().GetNormalized(); 
				Vec3 vSideDir			 = -outQuat.q.GetColumn0().GetNormalized();
				Matrix34 wFlippedMat;
				wFlippedMat = Matrix34::CreateFromVectors(vSideDir, vForwardDir, vFlippedUpDir, wMat.GetTranslation());
				outQuat = QuatT(wFlippedMat);

				// Adjust pos (rotating around OOBB centre effectively)
				const IEntity* pSubjectEntity = gEnv->pEntitySystem->GetEntity(m_subjectEntityId);
				if(pSubjectEntity)
				{
					AABB entAABB;
					OBB  entOBB;
					pSubjectEntity->GetLocalBounds(entAABB);
					entOBB.SetOBBfromAABB(Quat(IDENTITY), entAABB);

					Vec3 Centre = wMat.TransformPoint(entOBB.c);
					Vec3 toCentre = Centre - outQuat.t;
					outQuat.t += (toCentre * 2.0f);
				}
			}

#ifndef _RELEASE
			if(g_pGameCVars->pl_pickAndThrow.intersectionAssistDebugEnabled == 2)
			{
				m_currentBestIndex = bestIndex;
				CryWatch("[BEST INDEX] : %d", bestIndex);
			}
#endif // ifndef _RELEASE

			return true;
		}
	}

#ifndef _RELEASE
	m_currentBestIndex = -1;
#endif // ifndef _RELEASE

	return false;
}

bool CIntersectionAssistanceUnit::IsPositionWithinAcceptedLimits(const Vec3& vTestPos, const Vec3& vOrigin, const float fTolerance) const
{
	const Vec3 vDiff = vTestPos - vOrigin; 
	return (vDiff.GetLengthSquared() < (fTolerance * fTolerance)); 
}

void CIntersectionAssistanceUnit::SetEmbedPreventionResult( uint8 index, const QuatT& result )
{
	CRY_ASSERT(index < m_lastKnownGoodPositions.size()); 
	m_lastKnownGoodPositions[index] = result; 
}

void CIntersectionAssistanceUnit::Update( const float dt )
{
	// For now, focal entity is always the owner, so we can make these checks here. If we ever want to make it more generic, 
	// the seperate upate functions can be called

	// Once dropped make sure we don't leave objects stuck in walls. 
	if(!m_focalEntityId)
	{
		// Internally this func will check if the owner was responsible for this entity (so not all clients run this code)
		UpdateCheckingForObjectEmbed(dt); 
	}
	// whilst holding, we need to try our best not to ever drop in walls in the first place
	else if(g_pGame->GetIGameFramework()->GetClientActorId() == m_focalEntityId)
	{
		UpdateEmbeddingPrevention();
	}
}


CIntersectionAssistanceUnit::CPrimitiveIntersectionTester::CPrimitiveIntersectionTester() : m_pResultsOwner(nullptr)
{

} 

CIntersectionAssistanceUnit::CPrimitiveIntersectionTester::~CPrimitiveIntersectionTester()
{
	Reset();
}

void CIntersectionAssistanceUnit::CPrimitiveIntersectionTester::Reset()
{
	for(uint8 i = 0; i < m_testContainer.size(); ++i)
	{
		STest& test = m_testContainer[i];
		if (test.m_queuedPrimitiveId != 0)
		{
			g_pGame->GetIntersectionTester().Cancel(test.m_queuedPrimitiveId);
		}
		test.Clear();
	}
}

void CIntersectionAssistanceUnit::CPrimitiveIntersectionTester::DoCheck( uint8 index, const primitives::box& testBox, QuatT& resultIfSuccess, IPhysicalEntity* pIgnoreEntity /*= NULL*/ )
{
	if(index < m_testContainer.size())
	{
		STest& test = m_testContainer[index];

		//Still waiting for last one, skip new one
		if (test.m_queuedPrimitiveId != 0)
			return;

		test.m_queuedPrimitiveId = g_pGame->GetIntersectionTester().Queue(IntersectionTestRequest::HighestPriority,
			IntersectionTestRequest(testBox.type, testBox, ZERO, ent_static|ent_terrain, 0, 3, &pIgnoreEntity, pIgnoreEntity ? 1 : 0),
			functor(*this, &CIntersectionAssistanceUnit::CPrimitiveIntersectionTester::IntersectionTestComplete));
		test.m_resultIfSuccess = resultIfSuccess; 
	}
}

void CIntersectionAssistanceUnit::CPrimitiveIntersectionTester::IntersectionTestComplete(const QueuedIntersectionID& intersectionID, const IntersectionTestResult& result)
{
	for(uint8 i = 0; i < m_testContainer.size(); ++i)
	{
		if(m_testContainer[i].m_queuedPrimitiveId == intersectionID)
		{
			m_testContainer[i].m_queuedPrimitiveId = 0; 

			if(m_pResultsOwner && (result.distance <= 0.0f))
			{
				m_pResultsOwner->SetEmbedPreventionResult(i,m_testContainer[i].m_resultIfSuccess); 
			}
		}
	}
}

void CIntersectionAssistanceUnit::CPrimitiveIntersectionTester::Init( CIntersectionAssistanceUnit* pResultsOwner )
{
	m_pResultsOwner = pResultsOwner; 
}

void CIntersectionAssistanceUnit::CPrimitiveIntersectionTester::SetNumTestOffsets( uint8 numTestOffsets )
{
	if(m_testContainer.size() != numTestOffsets)
	{
		m_testContainer.resize(numTestOffsets);
	}
}
