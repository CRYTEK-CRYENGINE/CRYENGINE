// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 08:12:2005   14:14 : Created by MichaelR (port from Marcios HomingMissile.lua)

*************************************************************************/
#include "StdAfx.h"
#include "HomingMissile.h"
#include "Actor.h"
#include "Game.h"
#include "GameCVars.h"
#include "WeaponSystem.h"
#include "Single.h"
#include "NetInputChainDebug.h"
#include "Chaff.h"
#include <IVehicleSystem.h>
#include <IViewSystem.h>

#if !defined(_RELEASE)
	#define DEBUG_HOMINGMISSILE 1
#else
	#define DEBUG_HOMINGMISSILE 0
#endif

#define HM_TIME_TO_UPDATE 0.0f

//------------------------------------------------------------------------
CHomingMissile::CHomingMissile()
	: m_homingGuidePosition(ZERO)
	, m_homingGuideDirection(ZERO)
	, m_destination(ZERO)
	, m_targetId(0)
	, m_lockedTimer(0.0f)
	, m_controlLostTimer(0.0f)
	, m_destinationRayId(0)
	, m_isCruising(false)
	, m_isDescending(false)
	, m_trailEnabled(false)
{
}

//------------------------------------------------------------------------
CHomingMissile::~CHomingMissile()
{
	if ((m_destinationRayId != 0) && g_pGame)
	{
		g_pGame->GetRayCaster().Cancel(m_destinationRayId);
	}
}

//------------------------------------------------------------------------
bool CHomingMissile::Init(IGameObject *pGameObject)
{
	if (CRocket::Init(pGameObject))
	{
		if(m_pAmmoParams->pHomingParams != NULL)
		{
			m_lockedTimer = m_pAmmoParams->pHomingParams->m_lockedTimer;
			pGameObject->EnableDelegatableAspect(eEA_Physics, false);
			return true;
		}
	}

	return false;
}

//------------------------------------------------------------------------
void CHomingMissile::Launch(const Vec3 &pos, const Vec3 &dir, const Vec3 &velocity, float speedScale)
{
	CRocket::Launch(pos, dir, velocity, speedScale);

	OnLaunch();

	if (m_pAmmoParams->pHomingParams->m_controlled)
	{
		Vec3 dest(pos+dir*1000.0f);

		SetDestination(dest);
	}

	SetViewMode(CItem::eIVM_FirstPerson);

	DisableTrail();

	UpdateHomingGuide();

	if(gEnv->bServer)
	{
		if (m_pAmmoParams->pHomingParams != NULL)
		{
			if (m_pAmmoParams->pHomingParams->m_cruise || m_pAmmoParams->pHomingParams->m_controlled)
			{
				IGameFramework *pFramework = g_pGame->GetIGameFramework();
				IActor* pActor = pFramework->GetIActorSystem()->GetActor(m_ownerId);

				// give authority of the missile entity to whoever just fired it
				if (pActor && pActor->IsPlayer())
				{
					INetContext *pNetContext = pFramework->GetNetContext();
					INetChannel *pNetChannel = pFramework->GetNetChannel(pActor->GetChannelId());
					if (pNetContext && pNetChannel)
					{
						pNetContext->DelegateAuthority(GetEntityId(), pNetChannel);
					}
				}
			}
		}
	}
}

//------------------------------------------------------------------------
void CHomingMissile::Update(SEntityUpdateContext &ctx, int updateSlot)
{

	CRY_PROFILE_FUNCTION(PROFILE_GAME);

	CRocket::Update(ctx, updateSlot);

	if (m_controlLostTimer > 0.0f)
	{
		m_controlLostTimer -= ctx.fFrameTime;
		if (m_controlLostTimer < 0.0f)
			m_controlLostTimer = 0.0f;
	}

	bool cruiseActivated = m_totalLifetime > m_pAmmoParams->pHomingParams->m_cruiseActivationTime;

	// update destination if required
	if (!m_detonatorFired && cruiseActivated)
	{

		if (!m_trailEnabled)
		{
			EnableTrail();
			m_trailEnabled = true;
		}

		CWeapon* pOwnerWeapon = static_cast<CWeapon*>(GetWeapon());
		if (pOwnerWeapon && pOwnerWeapon->IsZoomed())
			UpdateHomingGuide();

		if (!m_pAmmoParams->pHomingParams->m_cruise)
			UpdateControlledMissile(ctx.fFrameTime);
		else
			UpdateCruiseMissile(ctx.fFrameTime);
	}

	if (cruiseActivated)
	{
		SetViewMode(CItem::eIVM_ThirdPerson);
	}
}

//-----------------------------------------------------------------------------
void CHomingMissile::UpdateControlledMissile(float frameTime)
{
	CActor* pOwnerActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(m_ownerId));
	bool isOwnerActor = pOwnerActor && pOwnerActor->IsClient();
	bool shouldFollowHomingGuide = isOwnerActor;
	

	float color[4] = {1,1,1,1};
	const static float step = 15.f;  
	float y = 20.f;    

	const SHomingMissileParams& homingParams = *(m_pAmmoParams->pHomingParams);

#if DEBUG_HOMINGMISSILE
	bool bDebug = g_pGameCVars->i_debug_projectiles > 0;
#endif

	if(isOwnerActor)
	{
		//If there's a target, follow the target
		if (m_targetId)
		{
			if (m_lockedTimer>0.0f)
			{
				m_lockedTimer = m_lockedTimer-frameTime;
			}
			else
			{
				// If we are here, there's a target
				IEntity* pTarget = gEnv->pEntitySystem->GetEntity(m_targetId);
				if (pTarget)
				{
					AABB box;
					pTarget->GetWorldBounds(box);
					Vec3 finalDes = box.GetCenter();
					SetDestination(finalDes);
					//SetDestination( box.GetCenter() );

#if DEBUG_HOMINGMISSILE
					if (bDebug)
					{
						IRenderAuxText::Draw2dLabel(5.0f, y+=step, 1.5f, color, false, "Target Entity: %s", pTarget->GetName());
					}
#endif
				}

				m_lockedTimer += 0.05f;
			}
		}
		else if(homingParams.m_autoControlled)
		{
			return;
		}

		if (homingParams.m_controlled && !homingParams.m_autoControlled && shouldFollowHomingGuide && !m_targetId)
		{
			//Check if the weapon is still selected
			CWeapon *pWeapon = GetWeapon();

			if(pWeapon && pWeapon->IsSelected())
			{
				if (m_destinationRayId == 0)
				{
					static const int objTypes = ent_all;
					static const int flags = (geom_colltype_ray << rwi_colltype_bit) | rwi_colltype_any | (7 & rwi_pierceability_mask) | (geom_colltype14 << rwi_colltype_bit);

					static PhysSkipList skipList;
					skipList.clear();
					CSingle::GetSkipEntities(pWeapon, skipList);

					m_destinationRayId = g_pGame->GetRayCaster().Queue(
						RayCastRequest::HighPriority,
						RayCastRequest(m_homingGuidePosition + 1.5f * m_homingGuideDirection,
						m_homingGuideDirection*homingParams.m_maxTargetDistance,
						objTypes,
						flags,
						!skipList.empty() ? &skipList[0] : NULL,
						skipList.size()),
						functor(*this, &CHomingMissile::OnRayCastDataReceived));
				}
				
#if DEBUG_HOMINGMISSILE
				if (bDebug)
				{
					IRenderAuxText::Draw2dLabel(5.0f, y+=step, 1.5f, color, false, "PlayerView eye direction: %.3f %.3f %.3f", m_homingGuideDirection.x, m_homingGuideDirection.y, m_homingGuideDirection.z);
					IRenderAuxText::Draw2dLabel(5.0f, y+=step, 1.5f, color, false, "PlayerView Target: %.3f %.3f %.3f", m_destination.x, m_destination.y, m_destination.z);
					gEnv->pRenderer->GetIRenderAuxGeom()->DrawCone(m_destination, Vec3(0,0,-1), 2.5f, 7.f, ColorB(255,0,0,255));
				}
#endif
			}
		}
	}



	//This code is shared by both modes above (auto and controlled)
	if(HasTarget())
	{
		IPhysicalEntity* pMissilePhysics = GetEntity()->GetPhysics();
		
		pe_status_dynamics status;
		pe_status_pos pos;
		if (pMissilePhysics)
		{
			if (!pMissilePhysics->GetStatus(&status))
			{
				CryLogAlways("couldn't get physics status!");
				return;
			}
			
			if (!pMissilePhysics->GetStatus(&pos))
			{
				CryLogAlways("couldn't get physics pos!");
				return;
			}
		}
		else
		{
			CryLogAlways("Homing missile: Missile is not physicalized!");
			return;
		}


		float currentSpeed = status.v.len();

		if (currentSpeed>0.001f)
		{
			Vec3 currentVel = status.v;
			Vec3 currentPos = pos.pos;
			Vec3 goalDir(ZERO);

			assert(!_isnan(currentSpeed));
			assert(!_isnan(currentVel.x) && !_isnan(currentVel.y) && !_isnan(currentVel.z));

			//Just a security check
			if((currentPos-m_destination).len2()<(homingParams.m_detonationRadius*homingParams.m_detonationRadius))
			{
				CProjectile::SExplodeDesc explodeDesc(true);
				explodeDesc.impact = true;
				explodeDesc.pos = m_destination;
				explodeDesc.normal = -currentVel.normalized();
				explodeDesc.vel = currentVel;
				explodeDesc.targetId = m_targetId;
				Explode(explodeDesc);

				return;
			}

			//Turn more slowly...
			currentVel.Normalize();

			if (m_controlLostTimer <= 0.0001f)
			{
				goalDir = m_destination - currentPos;
				goalDir.Normalize();
			}
			else
				goalDir = currentVel;

#if DEBUG_HOMINGMISSILE
			if(bDebug)
			{
				IRenderAuxText::Draw2dLabel(50,55,2.0f,color,false, "  Destination: %.3f, %.3f, %.3f",m_destination.x,m_destination.y,m_destination.z);
				IRenderAuxText::Draw2dLabel(50,80,2.0f,color,false, "  Current Dir: %.3f, %.3f, %.3f",currentVel.x,currentVel.y,currentVel.z);
				IRenderAuxText::Draw2dLabel(50,105,2.0f,color,false,"  Goal    Dir: %.3f, %.3f, %.3f",goalDir.x,goalDir.y,goalDir.z);
			}
#endif

			if (homingParams.m_bTracksChaff)
			{
				const float k_trackFov=cosf(DEG2RAD(30.0f));
				Vec3 trackDir;
				float dot;
				trackDir=currentVel*0.001f*(1.0f-k_trackFov);

				dot=max(goalDir.Dot(currentVel)-k_trackFov,0.0f);
				trackDir+=dot*goalDir;
	 
				for (size_t i=0; i<CChaff::s_chaffs.size(); ++i)
				{
					CChaff *c=CChaff::s_chaffs[i];
					Vec3 chaffPos=c->GetPosition();
					Vec3 chaffDir=chaffPos-currentPos;
					if (chaffDir.len2()>0.01f)
					{
						chaffDir.Normalize();
						dot=max(chaffDir.Dot(currentVel)-k_trackFov,0.0f);
						trackDir=dot*chaffDir;
					}
				}
				goalDir=trackDir.normalized();
			}

			float cosine = currentVel.Dot(goalDir);
			float totalAngle = RAD2DEG(acos_tpl(cosine));

			assert(totalAngle>=0);

			if (cosine<0.99)
			{
				float maxAngle = homingParams.m_turnSpeed*frameTime;
				if (maxAngle>totalAngle)
					maxAngle=totalAngle;
				float t=(maxAngle/totalAngle)*homingParams.m_lazyness;

				assert(t>=0.0 && t<=1.0);

				goalDir = Vec3::CreateSlerp(currentVel, goalDir, t);
				goalDir.Normalize();
			}

			currentSpeed = min(homingParams.m_maxSpeed, currentSpeed);

#if DEBUG_HOMINGMISSILE
			if(bDebug)
				IRenderAuxText::Draw2dLabel(50,180,2.0f,color,false,"Corrected Dir: %.3f, %.3f, %.3f",goalDir.x,goalDir.y,goalDir.z);
#endif

			pe_action_set_velocity action;
			action.v = goalDir * currentSpeed;
			pMissilePhysics->Action(&action);
		}
	}
}

//----------------------------------------------------------------------------
void CHomingMissile::UpdateCruiseMissile(float frameTime)
{
	IRenderer* pRenderer = gEnv->pRenderer;
	IRenderAuxGeom* pGeom = pRenderer->GetIRenderAuxGeom();
	float color[4] = {1,1,1,1};
	const static float step = 15.f;  
	float y = 20.f;    

	const SHomingMissileParams& homingParams = *(m_pAmmoParams->pHomingParams);
#if DEBUG_HOMINGMISSILE
	bool bDebug = g_pGameCVars->i_debug_projectiles > 0;
#endif

	if (m_targetId)
	{
		IEntity* pTarget = gEnv->pEntitySystem->GetEntity(m_targetId);
		if (pTarget)
		{
			AABB box;
			pTarget->GetWorldBounds(box);
			SetDestination( box.GetCenter() );

#if DEBUG_HOMINGMISSILE
			//if (bDebug)
				//pRenderer->Draw2dLabel(5.0f, y+=step, 1.5f, color, false, "Target Entity: %s", pTarget->GetName());
#endif
		}    
	}
	else 
	{
		// update destination pos from weapon
		static IItemSystem* pItemSystem = g_pGame->GetIGameFramework()->GetIItemSystem();
		IItem* pItem = pItemSystem->GetItem(m_weaponId);
		if (pItem && pItem->GetIWeapon())
		{
			const Vec3& dest = pItem->GetIWeapon()->GetDestination();
			SetDestination( dest );

#if DEBUG_HOMINGMISSILE
			//if (bDebug)
				//pRenderer->Draw2dLabel(5.0f, y+=step, 1.5f, color, false, "Weapon Destination: (%.1f %.1f %.1f)", dest.x, dest.y, dest.z);
#endif
		}
	}

	pe_status_dynamics status;
	if (!GetEntity()->GetPhysics()->GetStatus(&status))
		return;

	float currentSpeed = status.v.len();
	Vec3 currentPos = GetEntity()->GetWorldPos();
	Vec3 goalDir(ZERO);

	if (HasTarget())
	{
#if DEBUG_HOMINGMISSILE
		if (bDebug)
			pGeom->DrawCone(m_destination, Vec3(0,0,-1), 2.5f, 7.f, ColorB(255,0,0,255));
#endif

		float heightDiff = (homingParams.m_cruiseAltitude-homingParams.m_alignAltitude) - currentPos.z;

		if (!m_isCruising && heightDiff * sgn(status.v.z) > 0.f)
		{
#if DEBUG_HOMINGMISSILE
			// if heading towards align altitude (but not yet reached) accelerate to max speed    
			if (bDebug)
				IRenderAuxText::Draw2dLabel(5.0f,  y+=step,   1.5f, color, false, "[HomingMissile] accelerating (%.1f / %.1f)", currentSpeed, homingParams.m_maxSpeed);
#endif
		}
		else if (!m_isCruising && heightDiff * sgnnz(status.v.z) < 0.f && (status.v.z<0 || status.v.z>0.25f))
		{
			// align to cruise
			if (currentSpeed != 0)
			{
				goalDir = status.v;
				goalDir.z = 0;
				goalDir.normalize();
			}    

#if DEBUG_HOMINGMISSILE
			if (bDebug)
				IRenderAuxText::Draw2dLabel(5.0f,  y+=step, 1.5f, color, false, "[HomingMissile] aligning");
#endif
		}
		else
		{
#if DEBUG_HOMINGMISSILE
			if (bDebug)
				IRenderAuxText::Draw2dLabel(5.0f,  y+=step, 1.5f, color, false, "[HomingMissile] cruising...");
#endif

			// cruise
			m_isCruising = true;

			if (HasTarget())
			{
				float groundDistSq = m_destination.GetSquaredDistance2D(currentPos);
				float distSq = m_destination.GetSquaredDistance(currentPos);
				float descendDistSq = sqr(homingParams.m_descendDistance);

				if (m_isDescending || groundDistSq <= descendDistSq)
				{
#if DEBUG_HOMINGMISSILE
					if (bDebug)
						IRenderAuxText::Draw2dLabel(5.0f,  y+=step, 1.5f, color, false, "[HomingMissile] descending!");
#endif

					if ((distSq != 0) && (m_controlLostTimer <= 0.0001f))
						goalDir = (m_destination - currentPos).normalized();
					else 
						goalDir.zero();

					m_isDescending = true;
				}              
				else
				{
					Vec3 airPos = m_destination;
					airPos.z = currentPos.z;          
					goalDir = airPos - currentPos;
					if (goalDir.len2() != 0)
						goalDir.Normalize();
				}
			}
		}
	}  

	float desiredSpeed = currentSpeed;
	if (currentSpeed < homingParams.m_maxSpeed-0.1f)
	{
		desiredSpeed = min(homingParams.m_maxSpeed, desiredSpeed + homingParams.m_accel*frameTime);
	}

	Vec3 currentDir = status.v.GetNormalizedSafe(FORWARD_DIRECTION);
	Vec3 dir = currentDir;

	if (!goalDir.IsZero())
	{ 
		float cosine = max(min(currentDir.Dot(goalDir), 0.999f), -0.999f);
		float goalAngle = RAD2DEG(acos_tpl(cosine));
		float maxAngle = homingParams.m_turnSpeed * frameTime;

#if DEBUG_HOMINGMISSILE
		if (bDebug)
		{ 
			pGeom->DrawCone( currentPos, goalDir, 0.4f, 12.f, ColorB(255,0,0,255) );
			IRenderAuxText::Draw2dLabel(5.0f,  y+=step, 1.5f, color, false, "[HomingMissile] goalAngle: %.2f", goalAngle);
		}
#endif

		if (goalAngle > maxAngle+0.05f)    
			dir = (Vec3::CreateSlerp(currentDir, goalDir, maxAngle/goalAngle)).normalize();
		else //if (goalAngle < 0.005f)
			dir = goalDir;
	}

	pe_action_set_velocity action;
	action.v = dir * desiredSpeed;
	GetEntity()->GetPhysics()->Action(&action);

#if DEBUG_HOMINGMISSILE
	if (bDebug)
	{
		pGeom->DrawCone( currentPos, dir, 0.4f, 12.f, ColorB(128,128,0,255) );  
		IRenderAuxText::Draw2dLabel(5.0f,  y+=step, 1.5f, color, false, "[HomingMissile] currentSpeed: %.1f (max: %.1f)", currentSpeed, homingParams.m_maxSpeed);
	}
#endif
}
//-------------------------------------------------------------------------------
void CHomingMissile::FullSerialize(TSerialize ser)
{
	CRocket::FullSerialize(ser);

	ser.Value("m_controlLostTimer", m_controlLostTimer);

	SerializeDestination(ser);
}

void CHomingMissile::SerializeDestination( TSerialize ser )
{
	ser.Value("destination", m_destination, 'wrl3');
}

bool CHomingMissile::NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int flags)
{
	if (aspect == ASPECT_DESTINATION)
		SerializeDestination(ser);
	return CRocket::NetSerialize(ser, aspect, profile, flags);
}

NetworkAspectType CHomingMissile::GetNetSerializeAspects()
{
	return CRocket::GetNetSerializeAspects() | ASPECT_DESTINATION;
}


void CHomingMissile::SetViewMode(CItem::eViewMode viewMode)
{
	int renderFlags = GetEntity()->GetSlotFlags(0);
	if (viewMode == CItem::eIVM_FirstPerson)
	{
		renderFlags |= ENTITY_SLOT_RENDER;
		renderFlags |= ENTITY_SLOT_RENDER_NEAREST;
		GetEntity()->SetSlotFlags(0, renderFlags);
	}
	else
	{
		renderFlags &= ~ENTITY_SLOT_RENDER_NEAREST;
		GetEntity()->SetSlotFlags(0, renderFlags);
	}
}

void CHomingMissile::UpdateHomingGuide()
{
		CActor* pOwnerActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(m_ownerId));

		if (pOwnerActor && pOwnerActor->IsClient())
		{
			IMovementController* pMC = pOwnerActor->GetMovementController();
			if (!pMC)
				return;

			IVehicle* pVehicle = pOwnerActor->GetLinkedVehicle();
			if(!pVehicle)
			{
				SMovementState state;
				pMC->GetMovementState(state);

				m_homingGuidePosition = state.eyePosition;
				m_homingGuideDirection = state.eyeDirection;
			}
			else
			{	
				SViewParams viewParams;
				pVehicle->UpdateView(viewParams, pOwnerActor->GetEntityId());

				m_homingGuidePosition = viewParams.position;
				m_homingGuideDirection = viewParams.rotation * Vec3(0,1,0);
			}
		}
}

void CHomingMissile::Deflected(const Vec3& dir)
{
	m_controlLostTimer = 0.75f; // make parameter later
}

void CHomingMissile::OnRayCastDataReceived( const QueuedRayID& rayID, const RayCastResult& result )
{
	CRY_ASSERT(m_destinationRayId == rayID);
	CRY_ASSERT(m_pAmmoParams->pHomingParams);

	m_destinationRayId = 0;

	Vec3 destinationPoint(0.f, 0.f, 0.f);

	if(result.hitCount > 0)
	{
		destinationPoint = result.hits[0].pt;
	}
	else
	{
		destinationPoint = (m_homingGuidePosition + m_pAmmoParams->pHomingParams->m_maxTargetDistance*m_homingGuideDirection);	//Some point in the sky...
	}

	SetDestination(destinationPoint);
}

void CHomingMissile::SetDestination(const Vec3& pos)
{
	if (!m_destination.IsEquivalent(pos, 0.01f))
		CHANGED_NETWORK_STATE(this, ASPECT_DESTINATION);
	m_destination = pos;
}
