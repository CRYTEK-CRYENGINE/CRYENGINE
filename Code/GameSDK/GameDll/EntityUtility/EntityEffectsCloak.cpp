// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "EntityEffectsCloak.h"

#include <CryEntitySystem/IEntitySystem.h>
#include <CryEntitySystem/IEntityComponent.h>
#include "GameRules.h"
#include "Actor.h"

#if !defined(_RELEASE)
	#define ENTITYEFFECTSCLOAK_LOG(...)    GameWarning("[EntityEffects::Cloak] " __VA_ARGS__)
#else
	#define ENTITYEFFECTSCLOAK_LOG(...)    (void)(0)
#endif

//////////////////////////////////////////////////////////////////////////
void EntityEffects::Cloak::CloakEntity( IEntity* pEntity, bool bEnable, bool bFade, float blendSpeedScale, bool bCloakFadeByDistance, uint8 cloakPaletteColorChannel, bool bIgnoreCloakRefractionColor)
{
	if (pEntity)
	{
		IEntityRender *pIEntityRender =  pEntity->GetRenderInterface();
		
		{
			// If the render-node for this entity currently isn't rendering, then force the cloaking transition
			// not to fade in/out. Currently the engine only updates the blending when a render node is visible, so
			// if they cloak around a corner and the come into view you will suddenly see the transition which should of
			// happened a few seconds ago - this also reveals their position. Ideally a proper update fix should go into
			// the engine, but this would involve multiple code changes, at this point in time this is a much safer fix
			if(bFade && gEnv->bMultiplayer)
			{
				IRenderNode* pRenderNode = pIEntityRender->GetRenderNode();
				if(pRenderNode != NULL && gEnv->pRenderer != NULL)
				{
					// If the renderer frame ID is 2 ahead of the renderNode frame ID then we can assume its not currently
					// visible. This same test is made in many places in the engine and is a safe test. Unfortunately there
					// are no functions in the renderproxy, rendernode or entity which will actually give me the visibility status
					if((gEnv->pRenderer->GetFrameID() - pRenderNode->GetDrawFrame()) > 2 )
					{
						bFade = false;
					}
				}
			}

			uint8 currentMask = 0;//pIEntityRender->GetMaterialLayersMask();
			uint8 newMask = currentMask;
			uint32 blend = 0;//pIEntityRender->GetMaterialLayersBlend();

			if (!bFade)
			{
				blend = (blend & ~MTL_LAYER_BLEND_CLOAK) | (bEnable ? MTL_LAYER_BLEND_CLOAK : 0);
			}
			else
			{
				blend = (blend & ~MTL_LAYER_BLEND_CLOAK) | (bEnable ? 0 : MTL_LAYER_BLEND_CLOAK);
			}

			if (bEnable)
			{
				newMask = currentMask|MTL_LAYER_CLOAK;
			}
			else
			{
				newMask = currentMask&~MTL_LAYER_CLOAK;
			}

			if (((currentMask ^ newMask) & MTL_LAYER_CLOAK) != 0)
			{
				// Set cloak
				//pIEntityRender->SetCloakBlendTimeScale(blendSpeedScale);
				//pIEntityRender->SetMaterialLayersMask(newMask);
				//pIEntityRender->SetMaterialLayersBlend(blend);
				//pIEntityRender->SetCloakColorChannel(cloakPaletteColorChannel);
				//pIEntityRender->SetCloakFadeByDistance(bCloakFadeByDistance);
				//pIEntityRender->SetIgnoreCloakRefractionColor(bIgnoreCloakRefractionColor);
			}
		}
	}
}

void EntityEffects::Cloak::CloakEntity( EntityId entityId, bool bEnable, bool bFade, float blendSpeedScale, bool bCloakFadeByDistance, uint8 cloakPaletteColorChannel, bool bIgnoreCloakRefractionColor)
{
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity( entityId );

	CloakEntity(pEntity, bEnable, bFade, blendSpeedScale, bCloakFadeByDistance, cloakPaletteColorChannel, bIgnoreCloakRefractionColor);
}

//////////////////////////////////////////////////////////////////////////
bool EntityEffects::Cloak::CloakSyncEntities( const CloakSyncParams& params )
{
	// Check validity of helper systems
	CRY_ASSERT(gEnv->pEntitySystem);

	// check if the owner is cloaked
	IEntity* pCloakMaster = gEnv->pEntitySystem->GetEntity(params.cloakMasterId);
	IEntity* pCloakSlave = gEnv->pEntitySystem->GetEntity(params.cloakSlaveId);

	if(!pCloakMaster || !pCloakSlave)
	{
		ENTITYEFFECTSCLOAK_LOG("CloakSync() - cloak Master/Slave ID invalid, aborting cloak sync");
		return false;
	}

	IEntityRender* pCloakMasterRP = (pCloakMaster->GetRenderInterface());
	IEntityRender* pCloakSlaveRP	 = (pCloakSlave->GetRenderInterface());
	if (!pCloakMasterRP || !pCloakSlaveRP)
	{
		ENTITYEFFECTSCLOAK_LOG("CloakSync() - cloak Master/Slave Render proxy ID invalid, aborting cloak sync");
		return false;
	}

	uint8 masterMask = 0;//pCloakMasterRP->GetMaterialLayersMask();
	bool shouldCloakSlave = (masterMask & MTL_LAYER_CLOAK) != 0;
	if(params.forceDecloakOfSlave) // Allow an override. 
	{
		shouldCloakSlave = false; 
	}
	uint8 slaveMask = 0;//pCloakSlaveRP->GetMaterialLayersMask();
	bool slaveCloaked = (slaveMask & MTL_LAYER_CLOAK) != 0;

	// May be able to early out, if the object we are holding is already in the correct cloak state
	if(slaveCloaked == shouldCloakSlave)
	{
		return slaveCloaked; // Done
	}
	else
	{
		//const float cloakBlendSpeedScale = pCloakMasterRP->GetCloakBlendTimeScale();
		//const bool bFadeByDistance = pCloakMasterRP->DoesCloakFadeByDistance();
		//const uint8 colorChannel = pCloakMasterRP->GetCloakColorChannel();
		//const bool bIgnoreCloakRefractionColor = pCloakMasterRP->DoesIgnoreCloakRefractionColor();
		
		//EntityEffects::Cloak::CloakEntity(params.cloakSlaveId, shouldCloakSlave, params.fadeToDesiredCloakTarget, cloakBlendSpeedScale, bFadeByDistance, colorChannel, bIgnoreCloakRefractionColor);
		
		return shouldCloakSlave; 
	}
}

//////////////////////////////////////////////////////////////////////////
bool EntityEffects::Cloak::DoesCloakFadeByDistance(EntityId ownerEntityId)
{
	bool bCloakFadeByDist = false;
	if(gEnv->bMultiplayer)
	{
		CActor* pLocalPlayer = static_cast<CActor*>(gEnv->pGameFramework->GetClientActor());
		if(pLocalPlayer)
		{
			bool isEnemy = (pLocalPlayer->IsFriendlyEntity(ownerEntityId) == 0);
			bool isLocalPlayer = (ownerEntityId == pLocalPlayer->GetEntityId());
			bCloakFadeByDist = (isEnemy || isLocalPlayer);
		}
	}
	return bCloakFadeByDist;
}

//////////////////////////////////////////////////////////////////////////
uint8 EntityEffects::Cloak::GetCloakColorChannel(EntityId ownerEntityId)
{
	uint8 cloakColorChannel = 0;
	CGameRules* pGameRules = g_pGame->GetGameRules();
	if(pGameRules)
	{
		const CGameRules::eThreatRating threatRating = pGameRules->GetThreatRating(gEnv->pGameFramework->GetClientActorId(), ownerEntityId);
		return GetCloakColorChannel(threatRating==CGameRules::eFriendly);
	}
	return cloakColorChannel;
}

//////////////////////////////////////////////////////////////////////////
bool EntityEffects::Cloak::IgnoreRefractionColor(EntityId ownerEntityId)
{
	bool bIgnoreCloakRefractionColor = false; // Leave default setting for sp
	if(gEnv->bMultiplayer)
	{
		EntityId pLocalPlayerId = gEnv->pGameFramework->GetClientActorId();
		bool isLocalPlayer = (ownerEntityId == pLocalPlayerId);
		// Local player has no refraction color in mp
		if(isLocalPlayer)
		{
			bIgnoreCloakRefractionColor = true;
		}
	}
	return bIgnoreCloakRefractionColor;
}