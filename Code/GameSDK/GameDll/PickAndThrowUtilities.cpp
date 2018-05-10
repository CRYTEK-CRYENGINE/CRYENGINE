// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PickAndThrowUtilities.h"
#include <IActorSystem.h>
#include <IMovementController.h>
#include "GameRules.h"


namespace PickAndThrow
{

	CObstructionCheck::CObstructionCheck()
		: m_queuedPrimitiveId(0)
		, m_obstructed(false)
	{

	}

	CObstructionCheck::~CObstructionCheck()
	{
		Reset();
	}

	void CObstructionCheck::Reset()
	{
		if (m_queuedPrimitiveId != 0)
		{
			g_pGame->GetIntersectionTester().Cancel(m_queuedPrimitiveId);
			m_queuedPrimitiveId = 0;
		}

		m_obstructed = false;
	}

	void CObstructionCheck::DoCheck( IActor* pOwnerActor, EntityId objectId )
	{
		//Still waiting for last one, skip new one
		if (m_queuedPrimitiveId != 0)
			return;

		IMovementController* pMovementController = pOwnerActor ? pOwnerActor->GetMovementController() : NULL;

		if (pMovementController)
		{
			SMovementState movementState;
			pMovementController->GetMovementState(movementState);

			primitives::cylinder cylPrimitive;
			cylPrimitive.axis = movementState.eyeDirection;
			cylPrimitive.center = movementState.eyePosition - Vec3(0.0f, 0.0f, 0.15f) + (movementState.eyeDirection * 0.4f);
			cylPrimitive.hh = 0.6f;
			cylPrimitive.r = 0.25f;

			IEntity* pObjectEntity = gEnv->pEntitySystem->GetEntity(objectId);
			IPhysicalEntity* pObjectPhysics = pObjectEntity ? pObjectEntity->GetPhysics() : NULL;

			const int collisionEntityTypes = ent_static | ent_terrain | ent_rigid | ent_sleeping_rigid;

			int count = 0;
			IPhysicalEntity* skipList[3];
			if(pOwnerActor)
			{
				skipList[count++] = pOwnerActor->GetEntity()->GetPhysics();
				ICharacterInstance* pCharInstance = pOwnerActor->GetEntity()->GetCharacter(0);
				ISkeletonPose* pSkeletonPose(NULL);
				if(pCharInstance && (pSkeletonPose = pCharInstance->GetISkeletonPose()))
				{
					PREFAST_SUPPRESS_WARNING(6386); // fits in skiplist
					skipList[count++] = pSkeletonPose->GetCharacterPhysics();
				}
			}
			if(pObjectPhysics)
			{
				skipList[count++] = pObjectPhysics;
			}

			m_queuedPrimitiveId = g_pGame->GetIntersectionTester().Queue(IntersectionTestRequest::HighestPriority,
				IntersectionTestRequest(cylPrimitive.type, cylPrimitive, ZERO, collisionEntityTypes, 0, geom_colltype0|geom_colltype_player, &skipList[0], count),
				functor(*this, &PickAndThrow::CObstructionCheck::IntersectionTestComplete));
		}
	}

	void CObstructionCheck::IntersectionTestComplete(const QueuedIntersectionID& intersectionID, const IntersectionTestResult& result)
	{
		CRY_ASSERT(intersectionID == m_queuedPrimitiveId);

		m_queuedPrimitiveId = 0;

		m_obstructed = (result.distance > 0.0f);
	}

	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////

	// Looks just in the given slot and only for the given helper-name
	IStatObj::SSubObject* FindHelperObject( const char* pHelperName, const EntityId objectId, const int slot ) 
	{
		IStatObj::SSubObject* pSubObject = FindHelperObject_Basic( pHelperName, objectId, slot );
		if (pSubObject == NULL)
		{
			pSubObject = FindHelperObject_Extended( pHelperName, objectId, slot );
		}
		return pSubObject;
	}


	IStatObj::SSubObject* FindHelperObject_Basic( const char* pHelperName, const EntityId objectId, const int slot ) 
	{
		IStatObj::SSubObject* pSObjHelper = NULL;
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity( objectId );
		SEntitySlotInfo info;
		if ((pEntity != NULL) && pEntity->GetSlotInfo( slot, info ))
		{
			if (info.pStatObj)  // TODO: this code is getting too much complicated, we should restrict how and where to place the helpers. But too late now.
			{
				IStatObj* pStatObj = info.pStatObj->GetCloneSourceObject(); // we use the clone source in case it exists. Because when it is cloned, only geometries are cloned. the helpers are NOT cloned. 
				if (!pStatObj)
					pStatObj = info.pStatObj;
				pSObjHelper = pStatObj->FindSubObject( pHelperName ); // first try an easy look in the current object

				// if not success, look recursively.
				if (!pSObjHelper) 
				{
					pSObjHelper = FindHelperObject_RecursivePart( pStatObj, pHelperName );
				}
			}
		}
		return pSObjHelper;
	}

	// TODO: this whole function should be removed and FindHelperObject_Basic integrated back into FindHelperObject.
	//       It manages some undefined cases that appeared in C2, but it should not be needed with a strict definition of how the grabAndThrow helpers have to be defined in the objects
	IStatObj::SSubObject* FindHelperObject_Extended( const char* pHelperName, EntityId objectId, int slot ) 
	{
		IStatObj::SSubObject* pSObjHelper = NULL;
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity( objectId );
		SEntitySlotInfo info;
		if (pEntity && pEntity->GetSlotInfo( slot, info ))
		{
			if (info.pStatObj)  
			{
				IStatObj* pStatObj = info.pStatObj->GetCloneSourceObject(); // we use the clone source in case it exists. Because when it is cloned, only geometries are cloned. the helpers are NOT cloned. 
				if (!pStatObj)
					pStatObj = info.pStatObj;

				// special case: when the pStatObj is the root, we look for the first helper that does not have a hidden parent, whatever is its name as long as it includes pHelperName
				// because: it can be child of a visible geometry (usually "main"...) even when that geometry is not the root
				if (!pSObjHelper && !pStatObj->GetParentObject())
				{
					const int subObjectCount = pStatObj->GetSubObjectCount();
					for (int sid = 0; sid < subObjectCount; ++sid)
					{
						IStatObj::SSubObject* pLocSObjHelper = pStatObj->GetSubObject( sid );
						if ( pLocSObjHelper && (pLocSObjHelper->nType==STATIC_SUB_OBJECT_DUMMY) && strstr( pLocSObjHelper->name.c_str(), pHelperName ))
						{
							pLocSObjHelper = pStatObj->GetSubObject( pLocSObjHelper->nParent );
							if (pLocSObjHelper && (pLocSObjHelper->nType==STATIC_SUB_OBJECT_MESH) && !pLocSObjHelper->bHidden)
							{
								pSObjHelper = pStatObj->GetSubObject( sid );
								break;
							}
						}
					}
				}


				// if all failed, we look from the parent, but by id
				// because: helpers are not necesarily a subobject of their geometry, but just a child 
				if (!pSObjHelper && pStatObj->GetParentObject())
				{
					IStatObj* pParent = pStatObj->GetParentObject();
					IStatObj::SSubObject* pMeSubObject = pParent->FindSubObject( pStatObj->GetGeoName() );
					if (pMeSubObject)
					{
						const int subObjectCount = pParent->GetSubObjectCount();
						for (int sid=0; sid < subObjectCount; ++sid)
						{
							IStatObj::SSubObject* pLocSObjHelper = pParent->GetSubObject( sid );
							if ( pLocSObjHelper && (pLocSObjHelper->nType==STATIC_SUB_OBJECT_DUMMY) && (pLocSObjHelper->name==pHelperName) && (pParent->GetSubObject( pLocSObjHelper->nParent )==pMeSubObject) )
							{
								pSObjHelper = pLocSObjHelper;
								break;
							}
						}
					}
				}

				//If STILL we don't find the object, try with composed name based on geometry name (for destroyed pieces), and look on the whole hierarchy
				if (!pSObjHelper)
				{
					CryFixedStringT<128> helperNameBuffer;
					helperNameBuffer.Format("%s_%s", pStatObj->GetGeoName(), pHelperName);

					pSObjHelper = pStatObj->FindSubObject( helperNameBuffer.c_str() ); 
					if (!pSObjHelper) 
					{
						IStatObj* pObj = pStatObj;
						while (pObj->GetParentObject())   
							pObj = pObj->GetParentObject();
						pSObjHelper = FindHelperObject_RecursivePart( pObj, helperNameBuffer.c_str() );
					}
				}
			}
		}
		return pSObjHelper;
	}

	IStatObj::SSubObject* FindHelperObject_RecursivePart( IStatObj* pObj, const char* pHelperName ) 
	{
		IStatObj::SSubObject* pSObjHelper = pObj->FindSubObject( pHelperName );
		if (!pSObjHelper)
		{
			uint32 numSubObjects = pObj->GetSubObjectCount();
			for (uint32 i=0; i<numSubObjects && !pSObjHelper; i++)
			{
				if (IStatObj* pNextSubObject = pObj->GetSubObject( i )->pStatObj)
				{
					pSObjHelper = FindHelperObject_RecursivePart( pNextSubObject, pHelperName );
				}
			}
		}

		return pSObjHelper;
	}

	int FindActiveSlot( const EntityId objectId )
	{
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity( objectId );
		if (pEntity != NULL)
		{
			const int slotCount = pEntity->GetSlotCount();
			for (int slot = 0; slot < slotCount; ++slot)
			{
				if ( (pEntity->GetSlotFlags( slot ) & ENTITY_SLOT_RENDER) !=0 )
				{
					return slot;
				}
			}
		}

		return 0;
	}

	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////

	bool TargetEntityWithinFrontalCone(const Vec3& attackerLocation,const Vec3& victimLocation,const Vec3& attackerFacingdir, const float targetConeRads, float& theta) 
	{
		// Convert
		const Vec3 vecAttackerToVictim	= victimLocation - attackerLocation;
		theta = acos(attackerFacingdir.dot(vecAttackerToVictim.GetNormalized()));

		return ( theta < (0.5f * targetConeRads) );
	}

	bool AllowedToTargetPlayer(const EntityId attackerId, const EntityId victimEntityId)
	{
		CGameRules *pGameRules = g_pGame->GetGameRules();
		if(pGameRules && pGameRules->IsTeamGame())
		{
			const int clientTeamId = pGameRules->GetTeam(attackerId);
			const int victimTeamId = pGameRules->GetTeam(victimEntityId);

			return (clientTeamId != victimTeamId) || (g_pGameCVars->g_friendlyfireratio > 0.0f);
		}

		return true; 
	}

};