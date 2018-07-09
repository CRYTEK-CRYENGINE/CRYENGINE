// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description:
Part of Player that controls interaction with large objects

-------------------------------------------------------------------------
History:

*************************************************************************/
#include "StdAfx.h"
#include "Player.h"
#include "Game.h"
#include "IVehicleSystem.h"
#include "VehicleMovementBase.h"
#include "GameCVars.h"
#include "PersistantStats.h"
#include <CryMath/Cry_GeoOverlap.h>

CLargeObjectInteraction::TXmlParams CLargeObjectInteraction::s_XmlParams;
bool CLargeObjectInteraction::s_XmlDataLoaded = false;

//////////////////////////////////////////////////////////////////////////

void CLargeObjectInteraction::Init(CPlayer* pPlayer)
{
	CRY_ASSERT(m_pPlayer == NULL);
	if (m_pPlayer != NULL)
		return;

	m_pPlayer = pPlayer;
	m_lastObjectTime.SetValue(0);
}


void CLargeObjectInteraction::ReadXmlData( const IItemParamsNode* pRootNode, bool reloading )
{
	if (!reloading && s_XmlDataLoaded)
		return;

	const IItemParamsNode* pParams = pRootNode->GetChild("LargeObjectInteraction");
	if (!pParams)
		return;

	s_XmlDataLoaded = true;
	
	s_XmlParams.ReadXmlData( pParams );
}



//////////////////////////////////////////////////////////////////////////

void CLargeObjectInteraction::TXmlParams::ReadXmlData( const IItemParamsNode* pNode )
{
	const IItemParamsNode* pGlobalParams = pNode->GetChild( "GlobalParams" );
	if (pGlobalParams)
	{
		pGlobalParams->GetAttribute( "ImpulseDelay", impulseDelay );
		pGlobalParams->GetAttribute( "SideImpulseAngle", cosSideImpulseAngle );
		pGlobalParams->GetAttribute( "ImpulseFactorNoPower", impulseFactorNoPower );
		pGlobalParams->GetAttribute( "ImpulseFactorMaximumPower", impulseFactorMaximumPower );
		pGlobalParams->GetAttribute( "InteractionDistanceSP", interactionDistanceSP);
		pGlobalParams->GetAttribute( "InteractionDistanceMP", interactionDistanceMP);
		
		frontImpulseParams.ReadXmlData( pNode, "FrontImpulse" );
		sideImpulseParams.ReadXmlData( pNode, "SideImpulse" );
		assistanceParams.ReadXmlData( pNode, "Assistance" );

		if (const IItemParamsNode* pMPNode = pNode->GetChild("MPKickableCar"))
		{
			mpKickableFrontParams.ReadXmlData( pMPNode, "FrontImpulse" );
			mpKickableSideParams.ReadXmlData( pMPNode, "SideImpulse" );
			mpKickableDiagonalParams.ReadXmlData( pMPNode, "DiagonalImpulse" );
			mpParams.ReadXmlData( pMPNode, "MPParams" );
		}

		// Convert to cosine just once
		cosSideImpulseAngle = cosf(DEG2RAD(cosSideImpulseAngle));
	}
}


void CLargeObjectInteraction::TXmlParams::TImpulseParams::ReadXmlData( const IItemParamsNode* pNode, const char* pChildNodeName )
{
	const IItemParamsNode* pLocalNode = pNode->GetChild( pChildNodeName );
	if (pLocalNode)
	{
		pLocalNode->GetAttribute( "VerticalAngle", tanVerticalAngle );
		pLocalNode->GetAttribute( "Speed", speed );
		pLocalNode->GetAttribute( "RotationFactor", rotationFactor );
		pLocalNode->GetAttribute( "SwingRotationFactor", swingRotationFactor );

		// Convert to tan just once
		tanVerticalAngle = tanf( DEG2RAD( tanVerticalAngle ) );
	}
}

void CLargeObjectInteraction::TXmlParams::TAssistanceParams::ReadXmlData( const IItemParamsNode* pNode, const char* pChildNodeName )
{
	const IItemParamsNode* pLocalNode = pNode->GetChild( pChildNodeName );
	if (pLocalNode)
	{
		pLocalNode->GetAttribute( "AngleThreshold2D", cos2DAngleThreshold );
		pLocalNode->GetAttribute( "DistanceThreshold", distanceThreshold );

		// Convert to cos just once
		cos2DAngleThreshold = cosf( DEG2RAD( cos2DAngleThreshold ) );
	}
}

void CLargeObjectInteraction::TXmlParams::TMPParams::ReadXmlData( const IItemParamsNode* pNode, const char* pChildNodeName )
{
	const IItemParamsNode* pLocalNode = pNode->GetChild( pChildNodeName );
	if (pLocalNode)
	{
		pLocalNode->GetAttribute("FrontImpulseAngle", cosFrontImpulseAngle );
		pLocalNode->GetAttribute("SideImpulseAngle", cosSideImpulseAngle );
		pLocalNode->GetAttribute( "Timer", timer );
		// Convert to cosine just once
		cosFrontImpulseAngle = cosf(DEG2RAD(cosFrontImpulseAngle));
		cosSideImpulseAngle = cosf(DEG2RAD(cosSideImpulseAngle));
	}
}

//////////////////////////////////////////////////////////////////////////
bool CLargeObjectInteraction::IsLargeObject( IEntity* pEntity, const SmartScriptTable& propertiesTable ) const
{
	if (pEntity->GetPhysics() && propertiesTable)
	{
		int interactLargeObject = 0;
		return (propertiesTable->GetValue("bInteractLargeObject", interactLargeObject) && interactLargeObject);
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CLargeObjectInteraction::CanExecuteOn( IEntity* pEntity, bool objectWithinProximity ) const
{
	CRY_ASSERT(pEntity);

	if (m_pPlayer->IsInCinematicMode())
		return false;

	const bool isMultiplayer = gEnv->bMultiplayer;

	const bool bailOutIfNotInProximity = (isMultiplayer && !objectWithinProximity);
	if(bailOutIfNotInProximity)
		return false;

	if (m_pPlayer->IsOnGround() && (pEntity->GetId() != m_pPlayer->GetActorPhysics().groundColliderId))
	{
		AABB objectLocalBBox;
		pEntity->GetLocalBounds(objectLocalBBox);
		if (!objectLocalBBox.IsEmpty())
		{
			const Matrix34& objectTM = pEntity->GetWorldTM();

			const bool doOverlapTest = isMultiplayer || !objectWithinProximity;
			if(doOverlapTest)
			{
				const float interactionDistance = isMultiplayer ?s_XmlParams.interactionDistanceMP : s_XmlParams.interactionDistanceSP;

				const Vec3 refPosition = m_pPlayer->GetEntity()->GetWorldTM().TransformPoint(m_pPlayer->GetEyeOffset());
				const Vec3 viewDirection = m_pPlayer->GetViewQuatFinal().GetColumn1();
				const Lineseg viewLineSeg(refPosition, refPosition + (viewDirection * interactionDistance));

				OBB localOBB;
				localOBB.SetOBBfromAABB(Matrix33(objectTM), objectLocalBBox);
				if(!Overlap::Lineseg_OBB(viewLineSeg, objectTM.GetTranslation(), localOBB))
					return false;
			}

			const Vec3 playerWPos = m_pPlayer->GetEntity()->GetWorldPos();
			const float playerMin = playerWPos.z;
			const float playerMax = playerMin + m_pPlayer->GetEyeOffset().z;
			const float playerCentre = 0.5f*(playerMin + playerMax);
			AABB objectBBox;
			objectBBox.SetTransformedAABB( objectTM, objectLocalBBox );

			if((playerMin < (objectBBox.max.z - 0.25f)) && (playerCentre > objectBBox.min.z))
			{				
				pe_player_dimensions dimensions;
				IPhysicalEntity *pPhysics = m_pPlayer->GetEntity()->GetPhysics();
				if (pPhysics && pPhysics->GetParams(&dimensions))
				{
					if (dimensions.bUseCapsule && !gEnv->bMultiplayer)
					{
						// Perform a basic capsule intersection test, similar to that used by stealth kill
						primitives::capsule physPrimitive;
						physPrimitive.center = playerWPos;
						physPrimitive.center.z += 0.6f * dimensions.heightCollider;
						physPrimitive.axis.Set(0.f, 0.f, 1.0f);
						physPrimitive.r = dimensions.sizeCollider.x * 0.4f; // Shrink slightly so as not too restrictive
						physPrimitive.hh = dimensions.sizeCollider.z;

						// Skip list
						IPhysicalEntity* pSkipEntities[3];
						int numSkipEntities = 0;

						pSkipEntities[0] = pPhysics; // Player physics
						++numSkipEntities; 
						
						ICharacterInstance *pChar = m_pPlayer->GetEntity()->GetCharacter(0);
						if (pChar)
						{
							ISkeletonPose *pPose = pChar->GetISkeletonPose();
							if (pPose)
							{
								// Player character phys
								pSkipEntities[numSkipEntities] = pPose->GetCharacterPhysics();
								++numSkipEntities;
							}
						}
						
						// large object phys
						pSkipEntities[numSkipEntities] = pEntity->GetPhysics(); 
						if(pSkipEntities[numSkipEntities])
						{
							++numSkipEntities; 
						}

						Vec3 vPlayerToTarget = playerWPos - pEntity->GetWorldPos();
						float distance = gEnv->pPhysicalWorld->PrimitiveWorldIntersection(physPrimitive.type, &physPrimitive, -vPlayerToTarget, ent_static|ent_terrain, NULL, 0, geom_colltype0|geom_colltype_player, NULL, NULL, 0, pSkipEntities, numSkipEntities);
						if (distance == 0.f)
						{
							return true;
						}
						else
						{
							return false;
						}
					}
					
					return true; 
				}
			}
		}
	}
	
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CLargeObjectInteraction::Update( float frameTime )
{
	switch (m_state)
	{
		case ST_IDLE: 
			return; 
			break;
		
		case ST_WAITING_FOR_PUNCH:
		{
			m_timer += frameTime;
			if ( m_timer>=s_XmlParams.impulseDelay ||
				   m_bSkipAnim)
			{
				ThrowObject();
				m_state = ST_WAITING_FOR_END_ANIM;
				m_timer = 0;
			}
			break;
		}
		
		case ST_WAITING_FOR_END_ANIM:
		{
			//This only will have an effect on SP at the moment (see ThrowObject)
			AdjustObjectTrajectory(frameTime);

			if( m_pPlayer->IsInteractiveActionDone() || m_bSkipAnim)
			{
				Leave();
			}
			break;
		}
		
		default:
			assert( false );
			break;
	}
}



//////////////////////////////////////////////////////////////////////////
void CLargeObjectInteraction::Leave()
{
	m_state = ST_IDLE;
	m_hitTargetId = 0;
	if (gEnv->bMultiplayer && m_pPlayer->IsClient())
		CHANGED_NETWORK_STATE(m_pPlayer, CPlayer::ASPECT_INTERACTIVE_OBJECT);
}

	
void CLargeObjectInteraction::NetSerialize(TSerialize ser)
{
	ser.Value("boosted", m_boostLevel, 'ui2');
	ser.Value("largeObjEnt", m_lastObjectId, 'eid');
	ser.Value("skipAnim", m_bSkipAnim, 'bool'); 
	uint8 prevInteractionCount = m_interactionCount;
	ser.Value("interactionCount", m_interactionCount, 'ui2');

	if(ser.IsReading() && prevInteractionCount != m_interactionCount)
	{
		NetEnterInteraction(m_lastObjectId, m_bSkipAnim);
	}
}

void CLargeObjectInteraction::NetEnterInteraction(EntityId entityId, const bool bSkipAnim /*= false*/)
{
	if(entityId)
	{
		Enter( entityId, bSkipAnim );
	}
}


//////////////////////////////////////////////////////////////////////////
void CLargeObjectInteraction::Enter( EntityId objectEntityId, const bool bSkipKickAnim /*= false*/)
{
	m_bSkipAnim = bSkipKickAnim; 
	m_lastObjectId = objectEntityId;
	m_lastObjectTime = gEnv->pTimer->GetAsyncTime();
	m_state = ST_WAITING_FOR_PUNCH;

	if (gEnv->bMultiplayer)
	{
		m_boostLevel = eBoost_Normal;
	}
	else
	{
		m_boostLevel = eBoost_None;
	}

	IEntity* pObject = gEnv->pEntitySystem->GetEntity( m_lastObjectId );

	//we should kick the object from a point a certain amount away from the closest point on the box to us

	Vec3 playerPos = m_pPlayer->GetEntity()->GetWorldPos();

	const Matrix34& objectWorldTM = pObject->GetWorldTM();
	Vec3 targetPos = objectWorldTM.GetTranslation();
	Matrix33 targetTransform( objectWorldTM );

	AABB targetBox;
	pObject->GetLocalBounds( targetBox );

	Vec3 targetSpaceDisplacement = ( playerPos - targetPos ) * targetTransform;

	Vec3 closest;
	float dist = Distance::Point_AABBSq( targetSpaceDisplacement, targetBox, closest );

	//transform back to world space
	closest = (targetTransform * closest) + targetPos;

	// create an offset of where the player needs to be to look right kicking the object
	// TODO: get anims re-authored with origin moved, and remove this offset
	Vec3 desiredPos;
	if( !iszero( dist ) )
	{
		//player outside box, displace from closest towards/past player
		desiredPos = playerPos - closest;
	}
	else
	{
		//player inside box, displace closest away from player
		desiredPos = closest - playerPos;
	}
	desiredPos.normalize();
	desiredPos = closest + desiredPos * 1.2f;

	if(!m_bSkipAnim)
	{
		m_pPlayer->StartInteractiveActionByName( "KickLargeObject", true );

		if (m_pPlayer->IsClient() && (m_boostLevel != eBoost_None))
		{
			CAudioSignalPlayer::JustPlay( "Perk_BruteForce_KickCharge_FP", m_pPlayer->GetEntityId() );
		}
	}
	
	if (gEnv->bMultiplayer && m_pPlayer->IsClient())
	{
		m_interactionCount = (m_interactionCount + 1) % kInteractionCountMax;
		CHANGED_NETWORK_STATE(m_pPlayer, CPlayer::ASPECT_INTERACTIVE_OBJECT);
	}
}


//////////////////////////////////////////////////////////////////////////
void CLargeObjectInteraction::ThrowObject()
{
	IEntity *pEntity = gEnv->pEntitySystem->GetEntity( m_lastObjectId );
	if (!pEntity)
		return;

	if(pEntity->GetId() == m_pPlayer->GetActorPhysics().groundColliderId)
		return;

	IPhysicalEntity *pPhysicalEntity = pEntity->GetPhysics();
	if (!pPhysicalEntity)
		return;
		
	if ( gEnv->bMultiplayer == false)
	{
		g_pGame->GetPersistantStats()->IncrementClientStats( EIPS_Kicks, 1 );
	}

	const Quat& viewQuat = m_pPlayer->GetViewQuatFinal();
	Vec3 playerDir2D = viewQuat.GetColumn1();
	playerDir2D.z = 0.0f;
	playerDir2D.Normalize();
	Vec3 objectDir2D = pEntity->GetForwardDir();
	objectDir2D.z = 0.0f;
	objectDir2D.Normalize();
	
	IVehicle* pVehicle = gEnv->pGameFramework->GetIVehicleSystem()->GetVehicle( m_lastObjectId );

	CRY_ASSERT(m_boostLevel < 3);

	const float impulseFactorTable[3] = { s_XmlParams.impulseFactorNoPower, 1.0f, s_XmlParams.impulseFactorMaximumPower };
	const float boostFactor = impulseFactorTable[m_boostLevel];

	if (g_pGameCVars->g_mpKickableCars && pVehicle)
	{
		// MP - Kickable car
		IMovementController* pMovementController = m_pPlayer->GetMovementController();
		if (pMovementController)
		{
			if( g_pGameCVars->g_kickCarDetachesEntities == 1 )
			{
				//detach all entity parts on kick
				SVehicleEventParams	vehicleEventParams;
				vehicleEventParams.fParam = g_pGameCVars->g_kickCarDetachStartTime;
				vehicleEventParams.fParam2 = g_pGameCVars->g_kickCarDetachEndTime;
				pVehicle->BroadcastVehicleEvent(eVE_RequestDelayedDetachAllPartEntities, vehicleEventParams);
			}
			
			SMovementState moveState;
			pMovementController->GetMovementState(moveState);

			// Choose kick type - front, side, or diagonal
			int kickType;
			TXmlParams::TImpulseParams* impulseParams;
			const float dotVal = fabsf( playerDir2D.dot( objectDir2D ) );
			if (dotVal < s_XmlParams.mpParams.cosSideImpulseAngle)
			{
				kickType = SVehicleMovementEventLargeObject::k_KickSide;
				impulseParams = &s_XmlParams.mpKickableSideParams;
			}
			else if (dotVal > s_XmlParams.mpParams.cosFrontImpulseAngle)
			{
				kickType = SVehicleMovementEventLargeObject::k_KickFront;
				impulseParams = &s_XmlParams.mpKickableFrontParams;
			}
			else
			{
				kickType = SVehicleMovementEventLargeObject::k_KickDiagonal;
				impulseParams = &s_XmlParams.mpKickableDiagonalParams;
			}

			Vec3 impulseDirection = playerDir2D;
			impulseDirection.z = impulseParams->tanVerticalAngle;
			impulseDirection.Normalize();
		
			// Let the vehicle deal with the throw
			SVehicleMovementEventParams	vehicleMovementEventParams;
			SVehicleMovementEventLargeObject largeObjectInfo;
			vehicleMovementEventParams.sParam = (const char*)&largeObjectInfo;
			largeObjectInfo.impulseDir = impulseDirection;
			largeObjectInfo.speed = impulseParams->speed;
			largeObjectInfo.rotationFactor = impulseParams->rotationFactor;
			largeObjectInfo.swingRotationFactor = impulseParams->swingRotationFactor;
			largeObjectInfo.boostFactor = boostFactor;
			largeObjectInfo.eyePos = moveState.eyePosition;
			largeObjectInfo.viewQuat = viewQuat;
			largeObjectInfo.kicker = m_pPlayer->GetEntityId();
			largeObjectInfo.kickType = kickType;
			largeObjectInfo.timer = s_XmlParams.mpParams.timer;
			pVehicle->GetMovement()->OnEvent((IVehicleMovement::EVehicleMovementEvent)eVME_StartLargeObject, vehicleMovementEventParams);
		}
	}
	else if (gEnv->bServer)
	{
		//if the angle between the player and the entity is less than 'sideImpulseAngle', the impulse will be front, if greater then it will be side.
		const float dotVal = fabsf( playerDir2D.dot( objectDir2D ) );
		const bool side = dotVal< s_XmlParams.cosSideImpulseAngle;
		
		const TXmlParams::TImpulseParams& impulseParams = side ? s_XmlParams.sideImpulseParams : s_XmlParams.frontImpulseParams;

		if (!side && pVehicle)
		{
			// SF: I don't think this works because the vehicle will force the handbrake
			// back on every frame, unless told otherwise
			SVehicleMovementEventParams	vehicleMovementEventParams;
			vehicleMovementEventParams.bValue = false;
			pVehicle->GetMovement()->OnEvent(IVehicleMovement::eVME_EnableHandbrake, vehicleMovementEventParams);
		}

		Vec3 impulseDirection;
		m_hitTargetId = GetBestTargetAndDirection( playerDir2D, impulseDirection);
		if (m_hitTargetId != 0)
		{
			impulseDirection.z += impulseParams.tanVerticalAngle;
			impulseDirection.Normalize();
		}
		else
		{
			impulseDirection = playerDir2D;
			impulseDirection.z = impulseParams.tanVerticalAngle;
			impulseDirection.Normalize();
		} 


		float speed = impulseParams.speed;
		float wRotationIntensityFact = impulseParams.rotationFactor;
			
		speed *= boostFactor;
		wRotationIntensityFact *= boostFactor;

		pe_action_set_velocity asv;
		asv.v = (impulseDirection*speed);
		asv.w = -m_pPlayer->GetViewQuat().GetColumn0()*wRotationIntensityFact;
		pPhysicalEntity->Action(&asv);
	}
}

EntityId CLargeObjectInteraction::GetBestTargetAndDirection( const Vec3& playerDirection2D, Vec3& outputTargetDirection ) const
{
	//For now don't use this in MP,
	//Also probably won't work because this happens in the server,and the automaim manager data might not be the one we need for the client which triggered the action
	if (gEnv->bMultiplayer)
		return 0;

	CRY_ASSERT(m_pPlayer->IsClient());

	const EntityId playerId = m_pPlayer->GetEntityId();

	IMovementController* pMovementController = m_pPlayer->GetMovementController();
	if (!pMovementController)
		return 0;

	SMovementState moveState;
	pMovementController->GetMovementState(moveState);

	const Vec3 scanDirection = playerDirection2D;
	const Vec3 scanPosition = moveState.eyePosition;
	const float range = s_XmlParams.assistanceParams.distanceThreshold;
	const float angleLimitCos = s_XmlParams.assistanceParams.cos2DAngleThreshold;

	const TAutoaimTargets& aaTargets = g_pGame->GetAutoAimManager().GetAutoAimTargets();
	const int targetCount = aaTargets.size();

	Vec3 bestTargetSpot(0.0f, 0.0f, 0.0f);
	EntityId bestTargetId = 0;
	float nearestEnemyDst = range + 0.1f;

	for (int i = 0; i < targetCount; ++i)
	{
		const SAutoaimTarget& target = aaTargets[i];

		CRY_ASSERT(target.entityId != playerId);

		//Skip friendly ai
		if (target.HasFlagSet(eAATF_AIHostile) == false)
			continue;

		Vec3 targetDir = (target.primaryAimPosition - scanPosition);
		const float distance = targetDir.NormalizeSafe();

		// Reject if the enemy is not near enough or too far away
		if ((distance >= nearestEnemyDst) || (distance > range))
			continue;

		// Reject if not inside the view cone
		Vec3 targetDirection2D(targetDir.x, targetDir.y, 0.0f); 
		targetDirection2D.Normalize();
		const float alignment = scanDirection.Dot(targetDirection2D);
		if (alignment < angleLimitCos)
			continue;

		// We have a new candidate
		nearestEnemyDst = distance;
		bestTargetSpot = target.primaryAimPosition;
		bestTargetId = target.entityId;
	}

	outputTargetDirection = (bestTargetSpot - scanPosition).GetNormalized();
	
	return bestTargetId;
}

void CLargeObjectInteraction::AdjustObjectTrajectory( float frameTime )
{
	if (m_hitTargetId == 0)
		return;

	CRY_ASSERT(gEnv->bServer);

	IEntity* pObjectEntity = gEnv->pEntitySystem->GetEntity(m_lastObjectId);
	IPhysicalEntity* pObjectPhysics = pObjectEntity ? pObjectEntity->GetPhysics() : NULL;

	pe_status_dynamics objectDynamicStatus;
	if (pObjectPhysics && pObjectPhysics->GetStatus(&objectDynamicStatus))
	{
		const SAutoaimTarget* pTargetInfo = g_pGame->GetAutoAimManager().GetTargetInfo(m_hitTargetId);
		if (pTargetInfo)
		{
			//Correct trajectory slightly towards our target (should help if it's moving)
			Vec3 currentDirection = objectDynamicStatus.v;
			const float currentSpeed = currentDirection.NormalizeSafe(FORWARD_DIRECTION);
			Vec3 goalDirection = (pTargetInfo->primaryAimPosition - objectDynamicStatus.centerOfMass).GetNormalized();

			const float totalDeviationAngle = RAD2DEG(acos_tpl(currentDirection.Dot(goalDirection)));
			CRY_ASSERT(totalDeviationAngle >= 0.0f);

			const float frameDeviationAngle = clamp_tpl(20.0f * frameTime, 0.0f, totalDeviationAngle);
			if (totalDeviationAngle > 0.1f)
			{
				const float t = (frameDeviationAngle/totalDeviationAngle) * 0.4f;

				CRY_ASSERT(t>=0.0 && t<=1.0);

				goalDirection = Vec3::CreateSlerp(currentDirection, goalDirection, t);
				goalDirection.Normalize();
			}

			pe_action_set_velocity objectVelocity;
			objectVelocity.v = goalDirection * currentSpeed;
			pObjectPhysics->Action(&objectVelocity);
		}
	}
}
