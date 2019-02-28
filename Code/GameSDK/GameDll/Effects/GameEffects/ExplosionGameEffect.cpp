// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// Explosion game effect - handles screen filters like radial blur etc.

// Includes
#include "StdAfx.h"
#include "ExplosionGameEffect.h"
#include "IGameRulesSystem.h"
#include "IMovementController.h"
#include "Player.h"
#include "GameCVars.h"
#include "ScreenEffects.h"
#include "GameRules.h"
#include "GameCodeCoverage/GameCodeCoverageTracker.h"
#include "RecordingSystem.h"
#include "Battlechatter.h"
#include "PersistantStats.h"

#include <IForceFeedbackSystem.h>
#include "ActorManager.h"

REGISTER_EFFECT_DEBUG_DATA(CExplosionGameEffect::DebugOnInputEvent,CExplosionGameEffect::DebugDisplay,Explosion);
REGISTER_DATA_CALLBACKS(CExplosionGameEffect::LoadStaticData,CExplosionGameEffect::ReleaseStaticData,CExplosionGameEffect::ReloadStaticData,ExplosionData);

//--------------------------------------------------------------------------------------------------
// Name: SExplosionGameEffectData
// Desc: Data loaded from xml to control game effect
//--------------------------------------------------------------------------------------------------
struct SExplosionGameEffectData
{
	SExplosionGameEffectData()
	: isInitialised(false)
	, fCloakHighlightStrengthModifier(0.0f)
	{
	}

	float											fCloakHighlightStrengthModifier;

	bool											isInitialised;
};
static SExplosionGameEffectData s_explosionGEData;

CDeferredExplosionEffect::~CDeferredExplosionEffect()
{
	for (uint32 i = 0; i < m_queuedRays.size(); ++i)
	{
		g_pGame->GetRayCaster().Cancel(m_queuedRays[i].rayID);
	}
	m_queuedRays.clear();
}

void CDeferredExplosionEffect::OnRayCastDataReceived( const QueuedRayID& rayID, const RayCastResult& result )
{
	const int queuedRayIdx = FindRequestedRay(rayID);

	CRY_ASSERT(queuedRayIdx != -1);

	if (queuedRayIdx != -1)
	{
		const SQueuedRayInfo& info = m_queuedRays[queuedRayIdx];
		if ((info.effectType == CDeferredExplosionEffect::eDET_RadialBlur) && (result.hitCount == 0))
		{
			TriggerRadialBlur( info.explosionPos, info.effectMaxDistance, info.distance);
		}
		m_queuedRays.removeAt(queuedRayIdx);
	}
}

void CDeferredExplosionEffect::RequestRayCast( CDeferredExplosionEffect::EDeferredEffectType effectType, const Vec3 &startPos, const Vec3 &dir, float distance, float effectMaxDistance, int objTypes, int flags, IPhysicalEntity **pSkipEnts, int nSkipEnts )
{
	FreeOldestRequestIfNeeded();

	SQueuedRayInfo requestedRay;
	requestedRay.effectType = effectType;
	requestedRay.effectMaxDistance = effectMaxDistance;
	requestedRay.distance = distance;
	requestedRay.explosionPos = startPos;
	requestedRay.request = ++m_requestCounter;

	requestedRay.rayID = g_pGame->GetRayCaster().Queue(
		RayCastRequest::HighPriority,
		RayCastRequest(startPos, dir * distance,
		objTypes,
		flags,
		pSkipEnts,
		nSkipEnts),
		functor(*this, &CDeferredExplosionEffect::OnRayCastDataReceived));

	m_queuedRays.push_back(requestedRay);
}

int CDeferredExplosionEffect::FindRequestedRay( const QueuedRayID& rayID ) const
{
	const int queuedCount = m_queuedRays.size();
	for (int i = 0; i < queuedCount; ++i)
	{
		if (m_queuedRays[i].rayID == rayID)
			return i;
	}

	return -1;
}

void CDeferredExplosionEffect::FreeOldestRequestIfNeeded()
{
	const int queuedCount = m_queuedRays.size();

	if (queuedCount == m_queuedRays.max_size())
	{
		int oldestIdx = 0;
		uint32 oldestRequest = 0xFFFFFFFF;

		for (int i = 0; i < queuedCount; ++i)
		{
			if (m_queuedRays[i].request < oldestRequest)
			{
				oldestIdx = i;
				oldestRequest = m_queuedRays[i].request;
			}
		}

		g_pGame->GetRayCaster().Cancel(m_queuedRays[oldestIdx].rayID);

		m_queuedRays.removeAt(oldestIdx);
	}
}

void CDeferredExplosionEffect::TriggerRadialBlur( const Vec3& radialBlurCenter, float maxBlurDistance, float distance )
{
	CRY_ASSERT(maxBlurDistance > 0.0f);

	const float maxBlurDistanceInv = __fres(maxBlurDistance);

	if(CScreenEffects* pScreenFX = g_pGame->GetScreenEffects())
	{
		const float blurRadius = (-maxBlurDistanceInv * distance) + 1.0f;
		pScreenFX->ProcessExplosionEffect(blurRadius, radialBlurCenter);
	}

	const float distAmp = 1.0f - (distance * maxBlurDistanceInv);

	IForceFeedbackSystem* pForceFeedback = g_pGame->GetIGameFramework()->GetIForceFeedbackSystem();
	const ForceFeedbackFxId effectId = pForceFeedback->GetEffectIdByName("explosion");
	SForceFeedbackRuntimeParams ffParams(distAmp * 3.0f, 0.0f);
	pForceFeedback->PlayForceFeedbackEffect(effectId, ffParams);
	
}

//--------------------------------------------------------------------------------------------------
// Name: CExplosionGameEffect
// Desc: Constructor
//--------------------------------------------------------------------------------------------------
CExplosionGameEffect::CExplosionGameEffect()
{
	m_cutSceneActive = false;
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: ~CExplosionGameEffect
// Desc: Destructor
//--------------------------------------------------------------------------------------------------
CExplosionGameEffect::~CExplosionGameEffect()
{
	
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: Initialise
// Desc: Initializes game effect
//--------------------------------------------------------------------------------------------------
void CExplosionGameEffect::Initialise(const SGameEffectParams* gameEffectParams)
{
	CGameEffect::Initialise(gameEffectParams);

	
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: Release
// Desc: Releases game effect
//--------------------------------------------------------------------------------------------------
void CExplosionGameEffect::Release()
{
	CGameEffect::Release();
	
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: Update
// Desc: Updates game effect
//--------------------------------------------------------------------------------------------------
void CExplosionGameEffect::Update(float frameTime)
{
	CGameEffect::Update(frameTime);

	
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: ResetRenderParameters
// Desc: Resets game effect
//--------------------------------------------------------------------------------------------------
void CExplosionGameEffect::ResetRenderParameters()
{
	gEnv->p3DEngine->SetPostEffectParam("Flashbang_Active", 0.0f);
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: Explode
// Desc: Spawns explosion
//--------------------------------------------------------------------------------------------------
void CExplosionGameEffect::Explode(SExplosionContainer &explosionContainer)
{
	if(IsFlagSet(GAME_EFFECT_ACTIVE))
	{
		SpawnParticleEffect(explosionContainer);
		SpawnCharacterEffects(explosionContainer);

		if(!m_cutSceneActive)
		{
			SpawnScreenExplosionEffect(explosionContainer);
			QueueMaterialEffect(explosionContainer);
		}
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: SpawnParticleEffect
// Desc: Spawns the explosion's particle effect
//--------------------------------------------------------------------------------------------------
void CExplosionGameEffect::SpawnParticleEffect(const SExplosionContainer &explosionContainer)
{
	if(gEnv->IsClient())
	{
		const ExplosionInfo& explosionInfo = explosionContainer.m_explosionInfo;
		if(explosionInfo.pParticleEffect)
		{
			const bool bIndependent = true;
			explosionInfo.pParticleEffect->Spawn(	bIndependent, IParticleEffect::ParticleLoc(explosionInfo.pos, explosionInfo.dir, explosionInfo.effect_scale) );
		}
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: SpawnScreenExplosionEffect
// Desc: Spawns screen explosion effect
//--------------------------------------------------------------------------------------------------
void CExplosionGameEffect::SpawnScreenExplosionEffect(const SExplosionContainer &explosionContainer)
{
	// Disclaimer: this code was originally from GameRulesClientServer::ProcessClientExplosionScreenFX()

	const ExplosionInfo& explosionInfo = explosionContainer.m_explosionInfo;

	if(explosionInfo.pressure < 1.0f)
		return;

	IActor *pClientActor = g_pGame->GetIGameFramework()->GetClientActor();
	if(pClientActor != NULL && !pClientActor->IsDead())
	{
		CPlayer* pPlayer = static_cast<CPlayer*>(pClientActor);
		bool hasFlashBangEffect = explosionInfo.blindAmount > 0.0f;

		// Flashbang friends and self?...
		if(hasFlashBangEffect)
		{
#ifndef _RELEASE
			const bool flashBangSelf = (g_pGameCVars->g_flashBangSelf != 0);
			const bool flashBangFriends = (g_pGameCVars->g_flashBangFriends != 0);
#else
			const bool flashBangSelf = true;
			const bool flashBangFriends = false;
#endif
			const bool ownFlashBang = (pPlayer->GetEntityId() == explosionInfo.shooterId);
			if((!flashBangSelf && ownFlashBang) || // FlashBang self?
				((g_pGame->GetGameRules()->GetFriendlyFireRatio()<=0.0f) && (!flashBangFriends) && (!ownFlashBang) && pPlayer->IsFriendlyEntity(explosionInfo.shooterId))) // FlashBang friends?
			{
				return;
			}
		}

		// Distance
		float dist = (pClientActor->GetEntity()->GetWorldPos() - explosionInfo.pos).len();

		// Is the explosion in Player's FOV (let's suppose the FOV a bit higher, like 80)
		SMovementState state;
		if(IMovementController *pMV = pClientActor->GetMovementController())
		{
			pMV->GetMovementState(state);
		}

		Vec3 eyeToExplosion = explosionInfo.pos - state.eyePosition;
		Vec3 eyeDir = pClientActor->GetLinkedVehicle() ? pPlayer->GetVehicleViewDir() : state.eyeDirection;
		eyeToExplosion.Normalize();
		float eyeDirectionDP = eyeDir.Dot(eyeToExplosion);
		bool inFOV = (eyeDirectionDP > 0.68f);

		// All explosions have radial blur (default 30m radius)
		const float maxBlurDistance = (explosionInfo.maxblurdistance >0.0f) ? explosionInfo.maxblurdistance : 30.0f;
		if((maxBlurDistance > 0.0f) && (g_pGameCVars->g_radialBlur > 0.0f) && (explosionInfo.radius > 0.5f))
		{		
			if (inFOV && (dist < maxBlurDistance))
			{
				const int intersectionObjTypes = ent_static | ent_terrain;
				const unsigned int intersectionFlags = rwi_stop_at_pierceable|rwi_colltype_any;

				m_deferredScreenEffects.RequestRayCast(CDeferredExplosionEffect::eDET_RadialBlur, 
														explosionInfo.pos, -eyeToExplosion, dist, maxBlurDistance, intersectionObjTypes, intersectionFlags, NULL, 0);
			}
		}

		// Flashbang effect 
		if(hasFlashBangEffect && ((dist < (explosionInfo.radius*g_pGameCVars->g_flashBangNotInFOVRadiusFraction)) 
			|| (inFOV && (dist < explosionInfo.radius))))
		{
			ray_hit hit;
			const int intersectionObjTypes = ent_static | ent_terrain;
			const unsigned int intersectionFlags = rwi_stop_at_pierceable|rwi_colltype_any;
			const int intersectionMaxHits = 1;

			int collision = gEnv->pPhysicalWorld->RayWorldIntersection(	explosionInfo.pos, 
																																	-eyeToExplosion*dist, 
																																	intersectionObjTypes, 
																																	intersectionFlags, 
																																	&hit, 
																																	intersectionMaxHits);

			// If there was no obstacle between flashbang grenade and player
			if(!collision)
			{
				bool enabled = true;
				if(enabled)
				{
					CCCPOINT (FlashBang_Explode_BlindLocalPlayer);
					float timeScale = max(0.0f, 1 - (dist/explosionInfo.radius));
					float lookingAt = max(g_pGameCVars->g_flashBangMinFOVMultiplier, (eyeDirectionDP + 1)*0.5f);

					float time = explosionInfo.flashbangScale * timeScale *lookingAt;	// time is determined by distance to explosion		

					CRY_ASSERT_MESSAGE(pClientActor->IsPlayer(),"Effect shouldn't be spawned if not a player");

					SPlayerStats* pStats = static_cast<SPlayerStats*>(pPlayer->GetActorStats());

					NET_BATTLECHATTER(BC_Blinded, pPlayer);

					if(pClientActor->GetEntityId() == explosionInfo.shooterId)
					{
						g_pGame->GetPersistantStats()->IncrementClientStats(EIPS_BlindSelf);
					}
					else
					{
						g_pGame->GetGameRules()->SuccessfulFlashBang(explosionInfo, time);
					}

					pPlayer->StartFlashbangEffects(time, explosionInfo.shooterId);

					gEnv->p3DEngine->SetPostEffectParam("Flashbang_Time", time);
					gEnv->p3DEngine->SetPostEffectParam("FlashBang_BlindAmount", explosionInfo.blindAmount);
					gEnv->p3DEngine->SetPostEffectParam("Flashbang_DifractionAmount", time);
					gEnv->p3DEngine->SetPostEffectParam("Flashbang_Active", 1.0f);

					CRecordingSystem *pRecordingSystem = g_pGame->GetRecordingSystem();
					if (pRecordingSystem)
					{
						pRecordingSystem->OnPlayerFlashed(time, explosionInfo.blindAmount);
					}
				}
			}
			else
			{
				CCCPOINT (FlashBang_Explode_NearbyButBlockedByGeometry);
			}
		}
		else if(inFOV && (dist < explosionInfo.radius))
		{
			if(explosionInfo.damage>10.0f || explosionInfo.pressure>100.0f)
			{
				// Add some angular impulse to the client actor depending on distance, direction...
				float dt = (1.0f - dist/explosionInfo.radius);
				dt = dt * dt;
				float angleZ = gf_PI*0.15f*dt;
				float angleX = gf_PI*0.15f*dt;

				if (pClientActor)
				{
					static_cast<CActor*>(pClientActor)->AddAngularImpulse(Ang3(cry_random(-angleX*0.5f,angleX),0.0f,cry_random(-angleZ,angleZ)),0.0f,dt*2.0f);
				}
			}
		}
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: QueueMaterialEffect
// Desc: Queues material effect and sets off a deferred linetest for later processing
//--------------------------------------------------------------------------------------------------
void CExplosionGameEffect::QueueMaterialEffect(SExplosionContainer &explosionContainer)
{
	ExplosionInfo explosionInfo = explosionContainer.m_explosionInfo;

	// If an effect was already specified, don't use MFX
	if(explosionInfo.pParticleEffect)
		return;

	const int intersectionObjTypes = ent_all|ent_water;    
	const unsigned int intersectionFlags = rwi_stop_at_pierceable|rwi_colltype_any;

	CRY_ASSERT(explosionContainer.m_mfxInfo.m_rayId == 0);

	if(explosionInfo.impact)
	{
		Vec3 explosionDir = explosionInfo.impact_velocity.normalized();
		
		explosionContainer.m_mfxInfo.m_rayId = g_pGame->GetRayCaster().Queue(
			RayCastRequest::HighPriority,
			RayCastRequest(explosionInfo.pos-explosionDir*0.1f, explosionDir,
			intersectionObjTypes,
			intersectionFlags),
			functor(explosionContainer.m_mfxInfo, &SDeferredMfxExplosion::OnRayCastDataReceived));
	}
	else
	{
		const Vec3 explosionDir(0.0f, 0.0f, -g_pGameCVars->g_explosion_materialFX_raycastLength);

		explosionContainer.m_mfxInfo.m_rayId = g_pGame->GetRayCaster().Queue(
			RayCastRequest::HighPriority,
			RayCastRequest(explosionInfo.pos, explosionDir,
			intersectionObjTypes,
			intersectionFlags),
			functor(explosionContainer.m_mfxInfo, &SDeferredMfxExplosion::OnRayCastDataReceived));
	}

	explosionContainer.m_mfxInfo.m_state = eDeferredMfxExplosionState_Dispatched;
}

//--------------------------------------------------------------------------------------------------
// Name: SpawnMaterialEffect
// Desc: Spawns material effect
//--------------------------------------------------------------------------------------------------
void CExplosionGameEffect::SpawnMaterialEffect(const SExplosionContainer &explosionContainer)
{
	// Disclaimer: this code was originally from GameRulesClientServer::ProcessExplosionMaterialFX()
	const ExplosionInfo& explosionInfo = explosionContainer.m_explosionInfo;

	// impact stuff here
	SMFXRunTimeEffectParams params;
	//params.soundSemantic = eSoundSemantic_Explosion;
	params.pos = params.decalPos = explosionInfo.pos;
	params.trg = 0;
	params.trgRenderNode = 0;
	params.trgSurfaceId = 0;

	if(explosionInfo.impact && (explosionInfo.impact_velocity.len2() > 0.000001f))
	{
		params.dir[0] = explosionInfo.impact_velocity.normalized();
		params.normal = explosionInfo.impact_normal;
	}
	else
	{
		const Vec3 gravityDir = Vec3(0.0f, 0.0f, -1.0f);

		params.dir[0] = gravityDir;
		params.normal = -gravityDir;
	}

	const SDeferredMfxExplosion& mfxInfo = explosionContainer.m_mfxInfo;
	if(mfxInfo.m_state == eDeferredMfxExplosionState_ResultImpact)
	{
		params.trgSurfaceId = mfxInfo.m_mfxTargetSurfaceId;

		if (mfxInfo.m_pMfxTargetPhysEnt.get())
		{
			if (mfxInfo.m_pMfxTargetPhysEnt->GetiForeignData() == PHYS_FOREIGN_ID_STATIC)
			{
				params.trgRenderNode = (IRenderNode*)mfxInfo.m_pMfxTargetPhysEnt->GetForeignData(PHYS_FOREIGN_ID_STATIC);
			}
		}
	}

	// Create query name
	stack_string effectClass = explosionInfo.effect_class;
	if(effectClass.empty())
		effectClass = "generic";

	const float waterLevel = gEnv->p3DEngine->GetWaterLevel(&params.pos); 

	stack_string query = effectClass + "_explode";
	if(waterLevel > explosionInfo.pos.z)
		query = query + "_underwater";

	// Get material effect id
	IMaterialEffects* pMaterialEffects = gEnv->pGameFramework->GetIMaterialEffects();
	TMFXEffectId effectId = pMaterialEffects->GetEffectId(query.c_str(), params.trgSurfaceId);

	if(effectId == InvalidEffectId)
	{
		// Get default surface id
		effectId = pMaterialEffects->GetEffectId(query.c_str(), pMaterialEffects->GetDefaultSurfaceIndex());
	}

	// Execute material effect
	if(effectId != InvalidEffectId)
	{
		pMaterialEffects->ExecuteEffect(effectId, params);

		bool hasFlashBangEffect = explosionInfo.blindAmount > 0.0f;
		if(hasFlashBangEffect)
		{
			// Calc screen pos
			Vec3 screenspace;
			gEnv->pRenderer->ProjectToScreen(explosionInfo.pos.x, explosionInfo.pos.y, explosionInfo.pos.z, &screenspace.x, &screenspace.y, &screenspace.z);

			// Pass screen pos to flow graph node
			SMFXCustomParamValue paramPosX;
			paramPosX.fValue = screenspace.x*0.01f;
			pMaterialEffects->SetCustomParameter(effectId,"Intensity",paramPosX); // Use intensity param to pass x pos

			SMFXCustomParamValue paramPosY;
			paramPosY.fValue = screenspace.y*0.01f;
			pMaterialEffects->SetCustomParameter(effectId,"BlendOutTime",paramPosY); // Use blendOutTime param to pass y pos
		}
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: SpawnCharacterEffects
// Desc: Spawns character effects
//--------------------------------------------------------------------------------------------------
void CExplosionGameEffect::SpawnCharacterEffects(const SExplosionContainer &explosionContainer)
{
}//-------------------------------------------------------------------------------------------------

#if DEBUG_GAME_FX_SYSTEM
//--------------------------------------------------------------------------------------------------
// Name: DebugOnInputEvent
// Desc: Called when input events happen in debug builds
//--------------------------------------------------------------------------------------------------
void CExplosionGameEffect::DebugOnInputEvent(int keyId)
{
	// Initialise static version of effect
	static CExplosionGameEffect explosionGameEffect;
	if(!explosionGameEffect.IsFlagSet(GAME_EFFECT_INITIALISED))
	{
		explosionGameEffect.Initialise();
		explosionGameEffect.SetFlag(GAME_EFFECT_DEBUG_EFFECT,true);
		explosionGameEffect.SetFlag(GAME_EFFECT_AUTO_RELEASE,true);
		explosionGameEffect.SetActive(true);
	}

	// Get player pos
	Vec3 playerDir(0.0f,0.0f,0.0f);
	Vec3 playerPos(0.0f,0.0f,0.0f);
	if(auto* pPlayerEntity = gEnv->pGameFramework->GetClientEntity())
	{
		playerDir = pPlayerEntity->GetForwardDir();
		playerPos = pPlayerEntity->GetPos();
	}

	// Distance from player controlled by keyboard
	static float distFromPlayer = 0.0f;
	static float distStep = 1.0f;

	switch(keyId)
	{
		case eKI_NP_Multiply:
		{
			distFromPlayer += distStep;
			break;
		}
		case eKI_NP_Divide:
		{
			distFromPlayer -= distStep;
			break;
		}
		case eKI_NP_0:
		{
			distFromPlayer = 0.0f;
			break;
		}
		case eKI_NP_1:
		{
			// Frag
			SExplosionContainer explosionContainer;
			ExplosionInfo& explosionInfo = explosionContainer.m_explosionInfo;
			explosionInfo.pParticleEffect = gEnv->pParticleManager->FindEffect("Crysis2_weapon_explosives.frag.concrete");
			explosionInfo.pos = playerPos + (playerDir * distFromPlayer);
			explosionInfo.dir.Set(0.0f,-1.0f,0.0f);
			explosionInfo.effect_scale = 1.0f;
			explosionInfo.pressure = 1000.0f;
			explosionInfo.maxblurdistance = 10.0;
			explosionInfo.radius = 15.0;
			explosionInfo.blindAmount = 0.0f;
			explosionInfo.flashbangScale = 8.0f;
			explosionInfo.damage = 5.0f;
			explosionInfo.hole_size = 0.0f;
			explosionGameEffect.Explode(explosionContainer);
			break;
		}
		case eKI_NP_2:
		{
			// Flashbang
			SExplosionContainer explosionContainer;
			ExplosionInfo& explosionInfo = explosionContainer.m_explosionInfo;
			explosionInfo.pParticleEffect = gEnv->pParticleManager->FindEffect("Crysis2_weapon_explosives.grenades.flash_explosion");
			explosionInfo.pos = playerPos + (playerDir * distFromPlayer);
			explosionInfo.dir.Set(0.0f,-1.0f,0.0f);
			explosionInfo.effect_scale = 1.0f;
			explosionInfo.pressure = 1000.0f;
			explosionInfo.maxblurdistance = 10.0;
			explosionInfo.radius = 15.0;
			explosionInfo.blindAmount = 0.66f;
			explosionInfo.flashbangScale = 8.0f;
			explosionInfo.damage = 5.0f;
			explosionInfo.hole_size = 0.0f;
			explosionGameEffect.Explode(explosionContainer);
			break;
		}
		case eKI_NP_3:
		{
			// L-Tag
			SExplosionContainer explosionContainer;
			ExplosionInfo& explosionInfo = explosionContainer.m_explosionInfo;
			explosionInfo.pParticleEffect = gEnv->pParticleManager->FindEffect("Crysis2_weapon_fx.l-tag.rico_explosion");
			explosionInfo.pos = playerPos + (playerDir * distFromPlayer);
			explosionInfo.dir.Set(0.0f,-1.0f,0.0f);
			explosionInfo.effect_scale = 1.0f;
			explosionInfo.pressure = 1000.0f;
			explosionInfo.maxblurdistance = 10.0;
			explosionInfo.radius = 15.0;
			explosionInfo.blindAmount = 0.0f;
			explosionInfo.flashbangScale = 8.0f;
			explosionInfo.damage = 5.0f;
			explosionInfo.hole_size = 0.0f;
			explosionGameEffect.Explode(explosionContainer);
			break;
		}
		case GAME_FX_INPUT_ReleaseDebugEffect:
		{
			explosionGameEffect.Release();
			break;
		}
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: DebugDisplay
// Desc: Display when this effect is selected to debug through the game effects system
//--------------------------------------------------------------------------------------------------
void CExplosionGameEffect::DebugDisplay(const Vec2& textStartPos,float textSize,float textYStep)
{
	ColorF textCol(1.0f,1.0f,0.0f,1.0f);
	Vec2 currentTextPos = textStartPos;
	IRenderAuxText::Draw2dLabel(currentTextPos.x,currentTextPos.y,textSize,&textCol.r,false,"Reset distance: NumPad 0");
	currentTextPos.y += textYStep;
	IRenderAuxText::Draw2dLabel(currentTextPos.x,currentTextPos.y,textSize,&textCol.r,false,"Add distance: NumPad *");
	currentTextPos.y += textYStep;
	IRenderAuxText::Draw2dLabel(currentTextPos.x,currentTextPos.y,textSize,&textCol.r,false,"Subtract distance: NumPad /");
	currentTextPos.y += textYStep;
	IRenderAuxText::Draw2dLabel(currentTextPos.x,currentTextPos.y,textSize,&textCol.r,false,"Frag: NumPad 1");
	currentTextPos.y += textYStep;
	IRenderAuxText::Draw2dLabel(currentTextPos.x,currentTextPos.y,textSize,&textCol.r,false,"Flashbang: NumPad 2");
	currentTextPos.y += textYStep;
	IRenderAuxText::Draw2dLabel(currentTextPos.x,currentTextPos.y,textSize,&textCol.r,false,"L-tag: NumPad 3");
}//-------------------------------------------------------------------------------------------------
#endif

//--------------------------------------------------------------------------------------------------
// Name: LoadStaticData
// Desc: Loads static data for effect
//--------------------------------------------------------------------------------------------------
void CExplosionGameEffect::LoadStaticData(IItemParamsNode* pRootNode)
{
	const IItemParamsNode* pParamNode = pRootNode->GetChild("Explosion");
	Vec3 tempData;

	if(pParamNode)
	{
		const IItemParamsNode* pActualParamsNode = pParamNode->GetChild("params");
		if(pActualParamsNode)
		{
			pActualParamsNode->GetAttribute("CloakHighlightStrengthModifier",s_explosionGEData.fCloakHighlightStrengthModifier);
		}

		s_explosionGEData.isInitialised = true;
	}
}

//--------------------------------------------------------------------------------------------------
// Name: ReloadStaticData
// Desc: Reloads static data
//--------------------------------------------------------------------------------------------------
void CExplosionGameEffect::ReloadStaticData(IItemParamsNode* pRootNode)
{
	ReleaseStaticData();
	LoadStaticData(pRootNode);

#if DEBUG_GAME_FX_SYSTEM
	// Data has been reloaded, so re-initialse debug effect with new data
	CExplosionGameEffect* pDebugEffect = (CExplosionGameEffect*)GAME_FX_SYSTEM.GetDebugEffect("Explosion");
	if(pDebugEffect && pDebugEffect->IsFlagSet(GAME_EFFECT_REGISTERED))
	{
		pDebugEffect->Initialise();
		// Re-activate effect if currently active, so it will register correctly
		if(pDebugEffect->IsFlagSet(GAME_EFFECT_ACTIVE))
		{
			pDebugEffect->SetActive(false);
			pDebugEffect->SetActive(true);
		}
	}
#endif
}

//--------------------------------------------------------------------------------------------------
// Name: ReleaseStaticData
// Desc: Releases static data
//--------------------------------------------------------------------------------------------------
void CExplosionGameEffect::ReleaseStaticData()
{
	if(s_explosionGEData.isInitialised)
	{
		s_explosionGEData.isInitialised = false;
	}
}
