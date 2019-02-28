// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Crysis2 interactive object, for playing co-operative animations with player

-------------------------------------------------------------------------
History:
- 10:12:2009: Created by Benito G.R.

*************************************************************************/

#include "StdAfx.h"
#include "InteractiveObject.h"
#include "InteractiveObjectRegistry.h"
#include "ScriptBind_InteractiveObject.h"

#include "../Game.h"
#include "../Actor.h"
#include "../Player.h"

#include "EntityUtility/EntityScriptCalls.h"
#include "StatsRecordingMgr.h"

CInteractiveObjectEx::CInteractiveObjectEx()
: m_state(eState_NotUsed)
, m_physicalizationAfterAnimation(PE_NONE)
, m_activeInteractionIndex(0)
, m_currentLoadedCharacterCrc(0)
, m_bHighlighted(false)
, m_bRemoveDecalsOnUse(false)
, m_bStartInteractionOnExplosion(false)
, m_interactionTagID(TAG_ID_INVALID)
, m_stateTagID(TAG_ID_INVALID)
{
}


CInteractiveObjectEx::~CInteractiveObjectEx()
{
	if (g_pGame)
	{
		g_pGame->GetInteractiveObjectsRegistry().OnInteractiveObjectSutdown(GetEntityId());
		g_pGame->GetInteractiveObjectScriptBind()->Detach(GetEntityId());
	}

	HighlightObject(false);
}

bool CInteractiveObjectEx::Init( IGameObject *pGameObject )
{
	SetGameObject(pGameObject);

	if (!Reset())
		return false;
	
	if(!GetGameObject()->BindToNetwork())
		return false;

	g_pGame->GetInteractiveObjectScriptBind()->AttachTo(this);

	//record that interactive object exists in stats log
	if( CStatsRecordingMgr* sr = g_pGame->GetStatsRecorder() )
	{
		sr->InteractiveObjectEntry( pGameObject->GetEntityId(), 0 );
	}

	return true;
}

void CInteractiveObjectEx::InitClient( int channelId )
{

}

void CInteractiveObjectEx::PostInit( IGameObject *pGameObject )
{
#ifndef _RELEASE
	if(pGameObject)
	pGameObject->EnableUpdateSlot(this,0);
#endif //#ifndef _RELEASE
}

void CInteractiveObjectEx::PostInitClient( int channelId )
{

}

bool CInteractiveObjectEx::ReloadExtension( IGameObject * pGameObject, const SEntitySpawnParams &params )
{
	ResetGameObject();

	CRY_ASSERT_MESSAGE(false, "CInteractiveObjectEx::ReloadExtension not implemented");
	
	return false;
}

bool CInteractiveObjectEx::GetEntityPoolSignature( TSerialize signature )
{
	CRY_ASSERT_MESSAGE(false, "CInteractiveObjectEx::GetEntityPoolSignature not implemented");
	
	return true;
}

void CInteractiveObjectEx::Release()
{
	delete this;
}

void CInteractiveObjectEx::FullSerialize( TSerialize ser )
{
	if (ser.IsReading())
	{
		Reset();
	}

	ser.EnumValue("m_state", m_state, eState_NotUsed, eState_Done);

	ICharacterInstance* pCharacterInstance = GetEntity()->GetCharacter(0);
	if (ser.BeginOptionalGroup("character", pCharacterInstance != NULL))
	{
		CRY_ASSERT(pCharacterInstance);
		if (pCharacterInstance)
		{
			pCharacterInstance->Serialize(ser);
		}
		ser.EndGroup();
	}
}

bool CInteractiveObjectEx::NetSerialize( TSerialize ser, EEntityAspects aspect, uint8 profile, int flags )
{
	if (aspect == eEA_GameServerStatic)
	{
		EState newState = m_state;
		ser.EnumValue("state", newState, eState_NotUsed, eState_Done);
		ser.Value("interactId", m_activeInteractionIndex, 'ui4');
	
		if (ser.IsReading())
		{
			if ( (newState == eState_Done || newState == eState_Used) && m_state != eState_Done )
			{
				const bool wasAlreadyInUse = m_state == eState_InUse;
				m_state = newState;
				if(!wasAlreadyInUse)
				{
					PostSerialize(); //Late joiners
				}
				else if(newState == eState_Done)
				{
					HighlightObject(false);
				}
			}
		}
	}
	return true;
}

void CInteractiveObjectEx::PostSerialize()
{
	if(m_state == eState_Done || m_state == eState_Used)
	{
		g_pGame->GetInteractiveObjectsRegistry().ApplyInteractionToObject(GetEntity(), m_interactionTagID, m_activeInteractionIndex);

		if(m_state == eState_Done)
		{
			HighlightObject(false);
		}
	}
	else if(m_state == eState_NotUsed)
	{
		if(ICharacterInstance* pObjectCharacter = GetEntity()->GetCharacter(0))
		{
			//Reset animation
			ISkeletonPose* pSkeletonPose = pObjectCharacter->GetISkeletonPose();
			assert(pSkeletonPose);
			if (pSkeletonPose)
			{
				pSkeletonPose->SetDefaultPose();	

				// force skeleton processing
				QuatTS AnimatedCharacter = QuatTS( GetEntity()->GetWorldTM() );

				float fDistance = (GetISystem()->GetViewCamera().GetPosition() - AnimatedCharacter.t).GetLength();

				SAnimationProcessParams params;
				params.locationAnimation = AnimatedCharacter;
				params.bOnRender = 0;
				params.zoomAdjustedDistanceFromCamera = fDistance;
				pObjectCharacter->StartAnimationProcessing(params);
			}
		}

		Physicalize(PE_STATIC, true);
	}
}

void CInteractiveObjectEx::SerializeSpawnInfo( TSerialize ser )
{

}

ISerializableInfoPtr CInteractiveObjectEx::GetSpawnInfo()
{
	return 0;
}

void CInteractiveObjectEx::Update( SEntityUpdateContext &ctx, int updateSlot )
{

#ifndef _RELEASE
	if(g_pGameCVars->g_interactiveObjectDebugRender)
	{
		DebugRender(); 
	}
#endif //#ifndef _RELEASE

}

#ifndef _RELEASE
void CInteractiveObjectEx::DebugRender() const
{
	IRenderAuxGeom* pAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();
	const static ColorB red(255,0,0);
	const static ColorB blue(0,0,255);
	const static ColorB green(0,255,0);

	SAuxGeomRenderFlags originalFlags = pAuxGeom->GetRenderFlags();
	SAuxGeomRenderFlags newFlags	  = originalFlags; 
	newFlags.SetFillMode(e_FillModeWireframe); 

	pAuxGeom->SetRenderFlags(newFlags);

	const IEntity* pUserEntity = gEnv->pEntitySystem->GetEntity(g_pGame->GetIGameFramework()->GetClientActorId());

	// For each interaction this object has
	const uint numInteractions = m_interactionDataSets.size(); 
	for(uint i = 0; i < numInteractions; ++i )
	{
		const SInteractionDataSet& interactionDataSet = m_interactionDataSets[i];
		ColorB renderCol = blue; 
		if(pUserEntity)
		{
			renderCol = InteractionConstraintsSatisfied(pUserEntity,interactionDataSet) ? green: red; 
		}

		// Render align helpers + interaction cone/radius
		QuatT helperOrientation = interactionDataSet.m_alignmentHelperLocation; 
		const float radius = interactionDataSet.m_interactionRadius; 
		pAuxGeom->DrawSphere(helperOrientation.t,0.01f,renderCol,true);
		pAuxGeom->DrawSphere(helperOrientation.t,radius,renderCol,true);
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(helperOrientation.t, renderCol,helperOrientation.t + (helperOrientation.GetColumn1().GetNormalized() * radius),renderCol);
	}

	pAuxGeom->SetRenderFlags(originalFlags);
}
#endif //#ifndef _RELEASE

void CInteractiveObjectEx::PostUpdate( float frameTime )
{

}

void CInteractiveObjectEx::PostRemoteSpawn()
{

}

void CInteractiveObjectEx::HandleEvent( const SGameObjectEvent &goEvent )
{

}

void CInteractiveObjectEx::ProcessEvent( const SEntityEvent &entityEvent )
{
	switch (entityEvent.event)
	{
	case ENTITY_EVENT_RESET:
		{
			Reset();
		}
		break;

	case ENTITY_EVENT_ATTACH_THIS:
	case ENTITY_EVENT_XFORM:
		{
			// Recalculate the helper location
			const uint numInteractions = m_interactionDataSets.size(); 
			for(uint i = 0; i < numInteractions; ++i)
			{
				CalculateHelperLocation(m_interactionDataSets[i].m_helperName.c_str(), m_interactionDataSets[i].m_alignmentHelperLocation);
			}
		}
		break;
	case ENTITY_EVENT_START_LEVEL:
		{
			if(gEnv->bMultiplayer)
			{
				HighlightObject(true);
			}
		}
		break;
	}
}

uint64 CInteractiveObjectEx::GetEventMask() const
{
	return ENTITY_EVENT_BIT(ENTITY_EVENT_RESET) | ENTITY_EVENT_BIT(ENTITY_EVENT_ATTACH_THIS) | ENTITY_EVENT_BIT(ENTITY_EVENT_XFORM) | ENTITY_EVENT_BIT(ENTITY_EVENT_START_LEVEL);
}

void CInteractiveObjectEx::SetChannelId( uint16 id )
{

}

void CInteractiveObjectEx::GetMemoryUsage(ICrySizer *pSizer) const
{
	pSizer->AddObject(this, sizeof(*this));
}

bool CInteractiveObjectEx::Reset()
{
	m_state = eState_NotUsed;
	m_stateTagID = TAG_ID_INVALID;

	SmartScriptTable entityProperties;
	IScriptTable* pScriptTable = GetEntity()->GetScriptTable();
	if(!pScriptTable || !pScriptTable->GetValue("Properties", entityProperties))
		return false;

	//Physics properties
	SmartScriptTable physicProperties;
	if (entityProperties->GetValue("Physics", physicProperties))
	{
		int physicsValue = 0;
		if (physicProperties->GetValue("bRigidBody", physicsValue) && (physicsValue != 0))
		{
			m_physicalizationAfterAnimation = PE_RIGID;
		}
	}

	//Model and interaction
	SmartScriptTable interactionProperties;
	std::vector<char*> interactionNames;  // Store these locally and discard after processing and storing CRCs 
	m_interactionDataSets.clear(); 
	if (entityProperties->GetValue("Interaction", interactionProperties))
	{
		ParseAllInteractions(interactionProperties, interactionNames); 
		CRY_ASSERT_MESSAGE(interactionNames.size() == m_interactionDataSets.size(), "bool CInteractiveObjectEx::Reset() < Error - differing number of animation names than anim sets");

		const char* objectModel = NULL;
		interactionProperties->GetValue("object_Model", objectModel);
		assert(objectModel);

		uint32 crcForModel = GetCrcForName(objectModel);
		if (crcForModel != m_currentLoadedCharacterCrc)
		{
			GetEntity()->LoadCharacter(0, objectModel);

			m_currentLoadedCharacterCrc = crcForModel;
		}
		else if(gEnv->IsEditor())
		{
			if(ICharacterInstance* pCharacter = GetEntity()->GetCharacter(0))
			{
				ISkeletonPose* pSkeletonPose = pCharacter->GetISkeletonPose();
				assert(pSkeletonPose);
				if (pSkeletonPose)
				{
					pSkeletonPose->SetDefaultPose();	//Reset anims
				}
			}
		}

		Physicalize(PE_STATIC);

		interactionProperties->GetValue("bRemoveDecalsOnUse", m_bRemoveDecalsOnUse);
		interactionProperties->GetValue("bStartInteractionOnExplosion", m_bStartInteractionOnExplosion);
	}

	if (interactionNames.empty())
	{
		m_interactionTagID = TAG_ID_INVALID;
	}
	else
	{
		m_interactionTagID = g_pGame->GetInteractiveObjectsRegistry().GetInteractionTag(interactionNames.front());
		m_loadedInteractionData = m_interactionDataSets[0];
	}

	InitAllInteractionsFromMannequin();

	return true;
}

void CInteractiveObjectEx::ParseAllInteractions(const SmartScriptTable& interactionProperties, std::vector<char*>& interactionNames)
{
	// older objects have a single specified interaction name, and 2 float params. Read those if present. 
	// if *not* present, look for new interaction Group. 

	// Setup
	char* pTemp = NULL; 
 	if(interactionProperties->GetValue("Interaction", pTemp) && IsValidName(pTemp))
	{
		m_interactionDataSets.push_back(SInteractionDataSet()); 
		SInteractionDataSet& interactionDataSet = m_interactionDataSets.back(); 
		
		// Old style specification for single interaction. 
		interactionNames.push_back(pTemp);
		interactionProperties->GetValue("InteractionRadius", interactionDataSet.m_interactionRadius);
		interactionProperties->GetValue("InteractionAngle",  interactionDataSet.m_interactionAngle);

		interactionDataSet.m_interactionAngle = DEG2RAD(interactionDataSet.m_interactionAngle); 
	}
}

void CInteractiveObjectEx::InitAllInteractionsFromMannequin() 
{
	EntityId entId = GetEntityId(); 

	std::vector<CInteractiveObjectRegistry::SInteraction> interactions;  // Store these locally and discard after processing and storing CRCs 

	g_pGame->GetInteractiveObjectsRegistry().QueryInteractiveActionOptions(entId, m_interactionTagID, m_stateTagID, interactions);

	m_interactionDataSets.clear(); 

	const uint32 numHelpers = interactions.size();
	m_interactionDataSets.resize(numHelpers);

	for (uint32 i=0; i<numHelpers; i++)
	{
		SInteractionDataSet &dataSet = m_interactionDataSets[i];
		dataSet = m_loadedInteractionData;
		dataSet.m_helperName  = interactions[i].helper;
		dataSet.m_targetTagID = interactions[i].targetStateTagID;

		CalculateHelperLocation(dataSet.m_helperName.c_str(), dataSet.m_alignmentHelperLocation);
	}
}

int CInteractiveObjectEx::CanUse( EntityId entityId ) const
{
	//Only client can use these objects (for now at least)
	const bool userIsClient = (g_pGame->GetIGameFramework()->GetClientActorId() == entityId);
	//If we have multiple states we want to continue to check for usage even after the first use
	const bool canUse = (userIsClient) && (m_state == eState_NotUsed || m_state == eState_Used);
	if(canUse)
	{
		// This CanUse() func called by scriptbinds, so send interaction index to lua in range [1->N]
		return (CanUserPerformAnyInteraction(entityId) + 1);
	}
	else
	{
		return 0; 
	}
		
}

void CInteractiveObjectEx::Use( EntityId entityId )
{
	if (m_state == eState_NotUsed || m_state == eState_Used)
	{
		StartUse(entityId);
	}
}

void CInteractiveObjectEx::StartUse( EntityId entityId )
{
	//force animation on this object to update even if we can't see it
	ForceSkeletonUpdate( true );

	//Notify the user, interaction starts
	CActor* pActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(entityId));

	if (pActor)
	{
		m_state = eState_InUse;
		m_activeInteractionIndex = CalculateSelectedInteractionIndex(entityId); 
		m_stateTagID = m_interactionDataSets[m_activeInteractionIndex].m_targetTagID;
		pActor->StartInteractiveAction(GetEntityId(),m_activeInteractionIndex);

		//Remove any decals from this object
		if(m_bRemoveDecalsOnUse)
		{
			const Vec3 pos = GetEntity()->GetWorldPos();
			AABB aabb;
			GetEntity()->GetLocalBounds(aabb);
			const float radius = aabb.GetRadius();
			const Vec3 vRadius(radius,radius,radius);
			AABB worldBox(pos-vRadius, pos+vRadius);

			if (IEntityRender *pIEntityRender = GetEntity()->GetRenderInterface())
			{
				gEnv->p3DEngine->DeleteDecalsInRange(&worldBox, pIEntityRender->GetRenderNode());
			}
		}

		if (m_stateTagID == TAG_ID_INVALID)
		{
			m_interactionDataSets.clear();
		}
		else
		{
			InitAllInteractionsFromMannequin();
		}
	}
	else
	{
		//No actor, simply play the animation on the object
		ForceInstantStandaloneUse( 0 );
	}

	CStatsRecordingMgr::TryTrackEvent( pActor, eGSE_InteractiveEvent, GetEntityId() );
}

void CInteractiveObjectEx::StartUseSpecific( EntityId entityId, int interactionIndex )
{
	//force animation on this object to update even if we can't see it
	ForceSkeletonUpdate( true );

	//Notify the user, interaction starts
	CActor* pActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(entityId));

	if (pActor)
	{
		m_state = eState_InUse;
		m_activeInteractionIndex = interactionIndex; 
		pActor->StartInteractiveAction(GetEntityId(),interactionIndex);

		if (m_stateTagID == TAG_ID_INVALID)
		{
			m_interactionDataSets.clear();
		}
		else
		{
			InitAllInteractionsFromMannequin();
		}
	}
	else
	{
		//No actor, simply play the animation on the object
		ForceInstantStandaloneUse(interactionIndex);
	}

	CStatsRecordingMgr::TryTrackEvent( pActor, eGSE_InteractiveEvent, GetEntityId() );
}


int CInteractiveObjectEx::CalculateSelectedInteractionIndex( const EntityId entityId ) const
{
	// Pick the first interaction in our interaction set the user satisfies the constraints for.
	// If satisfies constraints for none, pick first
	if(m_interactionDataSets.size() > 1)
	{
		CryFixedArray<int, 8> interactionCandidates; // Should be more than enough
		
		IEntity* pUserEntity = gEnv->pEntitySystem->GetEntity(entityId);
		if (pUserEntity)
		{
			uint numInteractions = m_interactionDataSets.size(); 
			for(uint i = 0; (i < numInteractions) && (interactionCandidates.size() < interactionCandidates.max_size()); ++i )
			{
				const SInteractionDataSet& interactionDataSet = m_interactionDataSets[i];

				if(InteractionConstraintsSatisfied(pUserEntity, interactionDataSet))
				{
					interactionCandidates.push_back(i);
				}
			}
		}

		if (!interactionCandidates.empty())
		{
			return interactionCandidates[cry_random(0U, interactionCandidates.size() - 1)];
		}
	}

	return 0; 
}

bool CInteractiveObjectEx::InteractionConstraintsSatisfied(const IEntity* pUserEntity, const SInteractionDataSet& interactionDataSet) const
{
	const float angleLimitDot = (float)__fsel(-interactionDataSet.m_interactionAngle, -1.0f, cos_tpl(interactionDataSet.m_interactionAngle));
	const float distanceLimitSqrd = interactionDataSet.m_interactionRadius * interactionDataSet.m_interactionRadius; 
	const Matrix34& wMat = pUserEntity->GetWorldTM(); 

	const float actualAngleDot = interactionDataSet.m_alignmentHelperLocation.q.GetColumn1().Dot(wMat.GetColumn1());
	const float actualDistanceSqrd = (interactionDataSet.m_alignmentHelperLocation.t - wMat.GetTranslation()).GetLengthSquared();

	if ((actualDistanceSqrd < distanceLimitSqrd) && (actualAngleDot > angleLimitDot))
	{
		return true;
	}
	else
	{
		return false; 
	}
}

void CInteractiveObjectEx::ForceInstantStandaloneUse(const int interactionIndex)
{
	m_activeInteractionIndex = interactionIndex;

	// Force immediate animation playback and stop usage
	g_pGame->GetInteractiveObjectsRegistry().ApplyInteractionToObject( GetEntity(), m_interactionTagID, interactionIndex );

	EntityScripts::CallScriptFunction(GetEntity(), GetEntity()->GetScriptTable(), "StopUse", ScriptHandle(0));
}

void CInteractiveObjectEx::Done( EntityId entityId )
{
	//done with animation
	ForceSkeletonUpdate( false );

	if (m_physicalizationAfterAnimation == PE_RIGID)
	{
		Physicalize(PE_RIGID);
	}


	if(CanStillBeUsed())
	{
		m_state = eState_Used;
	}
	else
	{
		m_state = eState_Done;

		HighlightObject(false);
	}

	CHANGED_NETWORK_STATE(this, eEA_GameServerStatic);
}

void CInteractiveObjectEx::StopUse( EntityId entityId )
{
	Done(entityId);
}

void CInteractiveObjectEx::AbortUse()
{
	if (m_physicalizationAfterAnimation == PE_RIGID)
	{
		Physicalize(PE_RIGID);
	}

	m_state = eState_NotUsed;

	CHANGED_NETWORK_STATE(this, eEA_GameServerStatic);
}

void CInteractiveObjectEx::CalculateHelperLocation( const char* helperName, QuatT& helper ) const
{
	helper.t = GetEntity()->GetWorldPos();
	helper.q = GetEntity()->GetWorldRotation();

	if (ICharacterInstance* pObjectCharacter = GetEntity()->GetCharacter(0))
	{
		int16 jointId = pObjectCharacter->GetIDefaultSkeleton().GetJointIDByName(helperName);
		if (jointId >= 0)
		{
			const QuatT& helperLocation = pObjectCharacter->GetISkeletonPose()->GetAbsJointByID(jointId);
			helper.t = helper.t + (helper.q * helperLocation.t);
			helper.q = helper.q * helperLocation.q;
		}
		else
		{
			GameWarning("Helper '%s' for object '%s' not found, default to object location", GetEntity()->GetName(), helperName);
		}
	}
}

// returns -1 if not interaction constraints satsified i.e. none can be peformed, else returns index of first interaction the user can perform. 
int CInteractiveObjectEx::CanUserPerformAnyInteraction( EntityId userId ) const
{
	IEntity* pUserEntity = gEnv->pEntitySystem->GetEntity(userId);
	if (pUserEntity)
	{
		uint numInteractions = m_interactionDataSets.size(); 
		for(uint i = 0; i < numInteractions; ++i )
		{
			const SInteractionDataSet& interactionDataSet = m_interactionDataSets[i];

			// For each interaction we test user with distance + cone test
			if(InteractionConstraintsSatisfied(pUserEntity, interactionDataSet))
			{
				return i; 
			}
		}
	}

	return -1;
}

bool CInteractiveObjectEx::IsValidName( const char* name ) const
{
	return (name && (name[0]!='\0'));
}

uint32 CInteractiveObjectEx::GetCrcForName( const char* name ) const
{
	assert(name);

	return CCrc32::Compute(name);
}

void CInteractiveObjectEx::Physicalize( pe_type physicsType, bool forcePhysicalization /*= false*/ )
{
	IPhysicalEntity* pPhysics = GetEntity()->GetPhysics();
	const pe_type currentPhysicsType = pPhysics ? pPhysics->GetType() : PE_NONE;

	if ((physicsType != currentPhysicsType) || (forcePhysicalization))
	{
		SEntityPhysicalizeParams physicParams;
		physicParams.type = physicsType;
		physicParams.nSlot = 0;
		GetEntity()->Physicalize(physicParams);
	}
}

bool CInteractiveObjectEx::CanStillBeUsed()
{
	return (m_interactionDataSets.empty() == false);
}

void CInteractiveObjectEx::ForceSkeletonUpdate( bool on )
{
	if( IEntity* pObjEnt = GetEntity() )
	{
		ICharacterInstance *pObjChar = pObjEnt->GetCharacter(0);
		if( pObjChar )
		{
			ISkeletonPose *pSkeletonPose = pObjChar->GetISkeletonPose();
			if (pSkeletonPose)
			{
				if( on )
				{
					pSkeletonPose->SetForceSkeletonUpdate(0x8000);
				}
				else
				{
					pSkeletonPose->SetForceSkeletonUpdate(0);
				}
			}
		}
	}
}

void CInteractiveObjectEx::HighlightObject( bool highlight )
{
	if(highlight != m_bHighlighted && (!highlight || m_state != eState_Done))
	{
		m_bHighlighted = highlight;
	}
}

void CInteractiveObjectEx::OnExploded( const Vec3& explosionSource )
{
	if(m_bStartInteractionOnExplosion && m_state == eState_NotUsed)
	{
		//Pick best fit
		const int interactions = m_interactionDataSets.size();
		if(interactions)
		{
			int bestFitIndex = 0;
			float bestFitDistance = FLT_MAX;
			for(int i = 0; i < interactions; ++i)
			{
				const float distSquared = m_interactionDataSets[i].m_alignmentHelperLocation.GetColumn3().GetSquaredDistance(explosionSource);
				if(distSquared < bestFitDistance)
				{
					bestFitIndex = i;
					bestFitDistance = distSquared;
				}
			}

			StartUseSpecific(0, bestFitIndex);

			m_state = eState_Done;
			HighlightObject(false);
			CHANGED_NETWORK_STATE(this, eEA_GameServerStatic);
		}
	}
}

CInteractiveObjectEx::SInteractionDataSet::SInteractionDataSet():
m_interactionRadius(1.5f)
, m_interactionAngle(70.0f)
{
	m_alignmentHelperLocation.SetIdentity();
}
