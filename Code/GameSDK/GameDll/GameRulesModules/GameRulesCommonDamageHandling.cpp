// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
	-------------------------------------------------------------------------
	$Id$
	$DateTime$
	Description: 
	
	-------------------------------------------------------------------------
	History:
	- 01:08:2010  : Created by Ben Parbury

*************************************************************************/

#include "StdAfx.h"
#include "GameRulesCommonDamageHandling.h"
#include <CrySystem/XML/IXml.h>
#include "GameRules.h"
#include "Actor.h"
#include "Player.h"
#include "PersistantDebug.h"
#include "IVehicleSystem.h"
#include "GameRulesModules/IGameRulesAssistScoringModule.h"
#include "GameRulesModules/IGameRulesPlayerStatsModule.h"
#include "BodyDamage.h"


//------------------------------------------------------------------------
CGameRulesCommonDamageHandling::CGameRulesCommonDamageHandling()
: m_pGameRules(NULL)
{
	CryLog("GameRulesCommonDamageHandling::GameRulesCommonDamageHandling()");
	
	IEntityClassRegistry * pClassReg = gEnv->pEntitySystem->GetClassRegistry();
	m_pEnvironmentalWeaponClass = pClassReg->FindClass("EnvironmentalWeapon");
}

//------------------------------------------------------------------------
CGameRulesCommonDamageHandling::~CGameRulesCommonDamageHandling()
{
	CryLog("CGameRulesCommonDamageHandling::~GameRulesCommonDamageHandling()");
}

//------------------------------------------------------------------------
void CGameRulesCommonDamageHandling::Init( XmlNodeRef xml )
{
	m_pGameRules = g_pGame->GetGameRules();

	// Reserved hit types - in sync with RESERVED_HIT_TYPES
	/*00*/ //m_pGameRules->RegisterHitType("invalid",							CGameRules::EHitTypeFlag::None);
	/*01*/ m_pGameRules->RegisterHitType("melee",								CGameRules::EHitTypeFlag::IsMeleeAttack | CGameRules::EHitTypeFlag::IgnoreHeadshots);
	/*02*/ m_pGameRules->RegisterHitType("collision",						CGameRules::EHitTypeFlag::Server);
	/*03*/ m_pGameRules->RegisterHitType("frag",								CGameRules::EHitTypeFlag::Server | CGameRules::EHitTypeFlag::AllowPostDeathDamage);
	/*04*/ m_pGameRules->RegisterHitType("explosion",						CGameRules::EHitTypeFlag::Server | CGameRules::EHitTypeFlag::AllowPostDeathDamage);
	/*05*/ m_pGameRules->RegisterHitType("stealthKill",					CGameRules::EHitTypeFlag::Server | CGameRules::EHitTypeFlag::AllowPostDeathDamage);
	/*06*/ m_pGameRules->RegisterHitType("silentMelee",					CGameRules::EHitTypeFlag::IsMeleeAttack | CGameRules::EHitTypeFlag::SinglePlayerOnly | CGameRules::EHitTypeFlag::IgnoreHeadshots);
	/*07*/ m_pGameRules->RegisterHitType("punish",							CGameRules::EHitTypeFlag::ClientSelfHarm);
	/*08*/ m_pGameRules->RegisterHitType("punishFall",					CGameRules::EHitTypeFlag::ClientSelfHarm);
	/*09*/ m_pGameRules->RegisterHitType("mike_burn",                     CGameRules::EHitTypeFlag::ValidationRequired);		
	/*10*/ m_pGameRules->RegisterHitType("fall",								CGameRules::EHitTypeFlag::ClientSelfHarm);
	/*11*/ m_pGameRules->RegisterHitType("normal",							CGameRules::EHitTypeFlag::Server);	//Used for killing players so they can switch teams
	/*12*/ m_pGameRules->RegisterHitType("fire",								CGameRules::EHitTypeFlag::Server); // used by PressurizedObject.lua
	/*13*/ m_pGameRules->RegisterHitType("bullet",							CGameRules::EHitTypeFlag::ValidationRequired);
	/*14*/ m_pGameRules->RegisterHitType("stamp",						CGameRules::EHitTypeFlag::Server | CGameRules::EHitTypeFlag::CustomValidationRequired);
	/*15*/ m_pGameRules->RegisterHitType("environmentalThrow",	CGameRules::EHitTypeFlag::CustomValidationRequired | CGameRules::EHitTypeFlag::AllowPostDeathDamage | CGameRules::EHitTypeFlag::IgnoreHeadshots);
	/*16*/ m_pGameRules->RegisterHitType("meleeLeft",						CGameRules::EHitTypeFlag::IsMeleeAttack | CGameRules::EHitTypeFlag::SinglePlayerOnly | CGameRules::EHitTypeFlag::IgnoreHeadshots);
	/*17*/ m_pGameRules->RegisterHitType("meleeRight",					CGameRules::EHitTypeFlag::IsMeleeAttack | CGameRules::EHitTypeFlag::SinglePlayerOnly | CGameRules::EHitTypeFlag::IgnoreHeadshots);
	/*18*/ m_pGameRules->RegisterHitType("meleeKick",						CGameRules::EHitTypeFlag::IsMeleeAttack | CGameRules::EHitTypeFlag::SinglePlayerOnly | CGameRules::EHitTypeFlag::IgnoreHeadshots);
	/*19*/ m_pGameRules->RegisterHitType("meleeUppercut",				CGameRules::EHitTypeFlag::IsMeleeAttack | CGameRules::EHitTypeFlag::SinglePlayerOnly | CGameRules::EHitTypeFlag::IgnoreHeadshots);
	/*20*/ m_pGameRules->RegisterHitType("vehicleDestruction",	CGameRules::EHitTypeFlag::Server | CGameRules::EHitTypeFlag::AllowPostDeathDamage);
	/*21*/ m_pGameRules->RegisterHitType("electricity",				CGameRules::EHitTypeFlag::SinglePlayerOnly);
	/*22*/ m_pGameRules->RegisterHitType("stealthKill_Maximum",		CGameRules::EHitTypeFlag::SinglePlayerOnly);
	/*23*/ m_pGameRules->RegisterHitType("eventDamage",					CGameRules::EHitTypeFlag::ClientSelfHarm);
	/*24*/ m_pGameRules->RegisterHitType("VTOLExplosion",				CGameRules::EHitTypeFlag::Server);
	/*25*/ m_pGameRules->RegisterHitType("environmentalMelee",	CGameRules::EHitTypeFlag::IsMeleeAttack | CGameRules::EHitTypeFlag::CustomValidationRequired | CGameRules::EHitTypeFlag::AllowPostDeathDamage | CGameRules::EHitTypeFlag::IgnoreHeadshots);
	
	CRY_ASSERT(m_pGameRules->GetHitTypesCount() == CGameRules::EHitType::Unreserved);

	// Read any non-native hit_types from the HitTypes.xml file!
	XmlNodeRef xmlNode = gEnv->pSystem->LoadXmlFromFile( "Scripts/Entities/Items/HitTypes.xml" );

	if( xmlNode )
	{
		const int numEntries = xmlNode->getChildCount();
		for (int i = 0; i < numEntries; ++i)
		{
			XmlNodeRef hitTypeXML = xmlNode->getChild(i);

			if (strcmp(hitTypeXML->getTag(), "hit_type") != 0)
				continue;

			if( const char* pHitType = hitTypeXML->getAttr("name") )
			{
				TBitfield flags = CGameRules::EHitTypeFlag::None;

				if( const char * pHitTypeFlags = hitTypeXML->getAttr("flags"))
				{
					flags = AutoEnum_GetBitfieldFromString(pHitTypeFlags, CGameRules::s_hitTypeFlags, CGameRules::EHitTypeFlag::HIT_TYPES_FLAGS_numBits);
				}

				m_pGameRules->RegisterHitType( pHitType, flags );
			}
		}
	}
	m_scriptHitInfo.Create(gEnv->pScriptSystem);
}

//------------------------------------------------------------------------
void CGameRulesCommonDamageHandling::PostInit()
{
	CryLog("CGameRulesCommonDamageHandling::PostInit()");
}

//------------------------------------------------------------------------
void CGameRulesCommonDamageHandling::Update(float frameTime)
{
	CRY_ASSERT(0);
}

//------------------------------------------------------------------------
bool CGameRulesCommonDamageHandling::SvOnHit( const HitInfo &hitInfo )
{
	CRY_ASSERT(0);
	return true;
}

//------------------------------------------------------------------------
bool CGameRulesCommonDamageHandling::SvOnHitScaled( const HitInfo &hitInfo )
{
	CRY_ASSERT(0);
	return true;
}

//------------------------------------------------------------------------
void CGameRulesCommonDamageHandling::SvOnExplosion(const ExplosionInfo &explosionInfo, const CGameRules::TExplosionAffectedEntities& affectedEntities)
{
	CRY_ASSERT(0);
}

//------------------------------------------------------------------------
void CGameRulesCommonDamageHandling::SvOnCollision(const IEntity *pEntity, const CGameRules::SCollisionHitInfo& colHitInfo)
{
	CRY_ASSERT(0);
}

//------------------------------------------------------------------------
bool CGameRulesCommonDamageHandling::AllowHitIndicatorForType(int hitTypeId)
{
	return ((hitTypeId != CGameRules::EHitType::StealthKill) && (hitTypeId != CGameRules::EHitType::StealthKill_Maximum));
}

//------------------------------------------------------------------------
void CGameRulesCommonDamageHandling::MakeMovementVisibleToAIForEntityClass(const IEntityClass* pEntityClass)
{
	
}

//------------------------------------------------------------------------
void CGameRulesCommonDamageHandling::OnGameEvent(const IGameRulesDamageHandlingModule::EGameEvents& gameEvent)
{

}

//------------------------------------------------------------------------
void CGameRulesCommonDamageHandling::ClProcessHit(Vec3 dir, EntityId shooterId, EntityId weaponId, float damage, uint16 projectileClassId, uint8 hitTypeId)
{
	const char* hit = g_pGame->GetGameRules()->GetHitType(hitTypeId);
	if(hit)
	{
		IActor *pClientActor = gEnv->pGameFramework->GetClientActor();
		string hitSound;
		hitSound.Format("ClientDamage%s", hit);
		float maxHealth = pClientActor->GetMaxHealth();
		float normalizedDamage = SATURATE(maxHealth > 0.0f ? damage / maxHealth : 0.0f);
		CAudioSignalPlayer::JustPlay(hitSound.c_str(), pClientActor->GetEntityId(), "damage", normalizedDamage);
	}
}

//------------------------------------------------------------------------
float CGameRulesCommonDamageHandling::GetCollisionEnergy( const IEntity *pVictim, const CGameRules::SCollisionHitInfo& colHitInfo ) const
{
	float m1 = colHitInfo.target_mass;
	float m0 = 0.f;

	IPhysicalEntity *phys = pVictim->GetPhysics();
	if(phys)
	{
		pe_status_dynamics	dyn;
		phys->GetStatus(&dyn);
		m0 = dyn.mass;
	}

	IEntity *pOffender = gEnv->pEntitySystem->GetEntity(colHitInfo.targetId);
	bool bCollider = (pOffender || m1 > 0.001f);

	const bool debugColl = DebugCollisions();
	if (debugColl)
	{
		CryLog("GetCollisionEnergy %s (%.1f) <-> %s (%.1f)", pVictim?pVictim->GetName():"[no entity]", m0, pOffender?pOffender->GetName():"[no entity]", m1);
	}

	float v0Sq = 0.f, v1Sq = 0.f;

	if (bCollider)	// non-static
	{
		m0 = min(m0, m1);

		Vec3 v0normal, v1normal, vrel;
		Vec3 tempNormal = colHitInfo.normal;

		float v0dotN = colHitInfo.velocity.dot(colHitInfo.normal);
		v0normal = tempNormal.scale(v0dotN);

		float v1dotN = colHitInfo.target_velocity.dot(colHitInfo.normal);;  // "target" is the offender
		v1normal = tempNormal.scale(v1dotN);

		vrel = v0normal.sub(v1normal);
		float vrelSq = vrel.len2();

		v0Sq = min( sqr(v0dotN), vrelSq );
		v1Sq = min( sqr(v1dotN), vrelSq );

		if (debugColl)
		{
			IPersistantDebug* pPD = g_pGame->GetIGameFramework()->GetIPersistantDebug();

			pPD->Begin("CollDamage", false);
			pPD->AddSphere(colHitInfo.pos, 0.15f, Col_Red, 5.f);
			pPD->AddDirection(colHitInfo.pos, 1.5f, tempNormal.scale(sgn(v0dotN)), Col_Green, 5.f);
			pPD->AddDirection(colHitInfo.pos, 1.5f, tempNormal.scale(sgn(v1dotN)), Col_Red, 5.f);

			if ((v0Sq > 2*2) || (v1Sq > 2*2))
			{
				CryLog("normal velocities: rel %.1f, <%s> %.1f / <%s> %.1f", sqrt(vrelSq), pVictim?pVictim->GetName():"none", v0dotN, pOffender?pOffender->GetName():"none", v1dotN); 
				CryLog("target_type: %i, target_velocity: %.2f %.2f %.2f", colHitInfo.target_type, colHitInfo.target_velocity.x, colHitInfo.target_velocity.y, colHitInfo.target_velocity.z);
			}
		}
	}
	else
	{
		v0Sq = sqr(colHitInfo.velocity.dot(colHitInfo.normal));

		if (debugColl && v0Sq>5*5)
		{
			IPersistantDebug* pPD = g_pGame->GetIGameFramework()->GetIPersistantDebug();

			pPD->Begin("CollDamage", false);
			pPD->AddDirection(colHitInfo.pos, 1.5f, colHitInfo.normal, Col_Green, 5.f);
			string debugText;
			debugText.Format("z: %f", colHitInfo.velocity.z);
			pPD->Add2DText(debugText.c_str(), 1.5f, Col_White, 5.f);
		}
	}

	float colliderEnergyScale = 1.f;
	if (pVictim != NULL && pOffender != NULL)
	{
		if(IActor* pVictimActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(pVictim->GetId()))
		{
			colliderEnergyScale = !pVictimActor->IsPlayer() ?	GetAIPlayerAgainstColliderEnergyScale(*pOffender) :
																GetPlayerAgainstColliderEnergyScale(*pOffender);
			
			if (debugColl)
			{
				CryLog("colliderEnergyScale: %.1f", colliderEnergyScale);
			}
		}
	}

	const float energy0 = 0.5f * m0 * v0Sq;
	const float energy1 = 0.5f * m1 * v1Sq * colliderEnergyScale;

	return energy0 + energy1;
}

//------------------------------------------------------------------------
float CGameRulesCommonDamageHandling::GetCollisionDamageMult( const IEntity *pEntity, const IEntity *pCollider, const CGameRules::SCollisionHitInfo& colHitInfo ) const
{
	float result = 1.0f;

	// EnvironmentalWeapons deal their own damage!
	if (pEntity->GetClass()==m_pEnvironmentalWeaponClass || (pCollider && pCollider->GetClass()==m_pEnvironmentalWeaponClass))
		return 0.f;

	const SCollisionEntityInfo entityInfo(pEntity);
	const SCollisionEntityInfo colliderInfo(pCollider);

	if (colliderInfo.pEntity)
	{
		float damageMultiplier = 1.0f;
		if (colliderInfo.pEntityActor)
		{
			damageMultiplier = colliderInfo.pEntityActor->IsPlayer() ? 0.1f : 1.0f;
		}
		else if (colliderInfo.pEntityVehicle)
		{
			damageMultiplier = GetVehicleForeignCollisionMultiplier(*colliderInfo.pEntityVehicle, entityInfo, colHitInfo);
		}
		result *= damageMultiplier;

		if (DebugCollisions() && damageMultiplier!=1.f)
		{
			CryLog("<%s>: collider <%s> has ForeignCollisionMult %.2f", pEntity ? pEntity->GetName() : "NULL", pCollider ? pCollider->GetName() : "NULL", damageMultiplier);
		}
	}

	if (entityInfo.pEntity)
	{
		float damageMultiplier = 1.0f;
		if (entityInfo.pEntityActor)
		{
			damageMultiplier = !entityInfo.pEntityActor->IsPlayer() ?	GetAIPlayerSelfCollisionMultiplier(colliderInfo) : 
																GetPlayerSelfCollisionMultiplier(colliderInfo);
		}
		else if (entityInfo.pEntityVehicle)
		{
			damageMultiplier = entityInfo.pEntityVehicle->GetSelfCollisionMult(colHitInfo.velocity, colHitInfo.normal, colHitInfo.partId, colliderInfo.entityId);
		}
		result *= damageMultiplier;

		if (DebugCollisions() && (damageMultiplier != 1.0f))
		{
			CryLog("<%s>: returned SelfCollisionMult %.2f", pEntity ? pEntity->GetName() : "NULL", damageMultiplier);
		}
	}

	return result;
}

//------------------------------------------------------------------------
float CGameRulesCommonDamageHandling::GetAIPlayerAgainstColliderEnergyScale(const IEntity& collider) const
{
	const float energyScaleAgainstNonActor = 10.0f; 
	const float energyScaleAgainstRagdoll = 50.0f;

	CActor* pColliderActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(collider.GetId()));

	if (pColliderActor)
	{
		uint8 physicsAspectProfile = pColliderActor->GetGameObject()->GetAspectProfile(eEA_Physics) ;
		if ((physicsAspectProfile == eAP_Ragdoll) || (physicsAspectProfile == eAP_Sleep))
		{
			return energyScaleAgainstRagdoll;
		}

		if (pColliderActor->GetActorClass() == CPlayer::GetActorClassType())
		{
			SPlayerStats* pColliderStats = static_cast<SPlayerStats*>(pColliderActor->GetActorStats());
			if (pColliderStats != NULL && pColliderStats->followCharacterHead)
			{
				return 0.0f;
			}
		}
	}

	return energyScaleAgainstNonActor;
}

//------------------------------------------------------------------------
float CGameRulesCommonDamageHandling::GetPlayerAgainstColliderEnergyScale(const IEntity& collider) const
{
	CActor* pColliderActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(collider.GetId()));

	if (pColliderActor)
	{
		if (pColliderActor->GetActorClass() == CPlayer::GetActorClassType())
		{
			SPlayerStats* pColliderStats = static_cast<SPlayerStats*>(pColliderActor->GetActorStats());
			if (pColliderStats != NULL && pColliderStats->followCharacterHead)
			{
				return 0.0f;
			}
		}
	}

	return 1.0f;
}

//------------------------------------------------------------------------
float CGameRulesCommonDamageHandling::GetPlayerSelfCollisionMultiplier(const CGameRulesCommonDamageHandling::SCollisionEntityInfo& colliderInfo) const
{
	const float collisionAgainstVehicleMultiplier = 7.5f;

	if (colliderInfo.pEntityVehicle)
	{
		return collisionAgainstVehicleMultiplier;
	}

	return 1.0f;
}

//------------------------------------------------------------------------
// when something collides with an AI
float CGameRulesCommonDamageHandling::GetAIPlayerSelfCollisionMultiplier(const CGameRulesCommonDamageHandling::SCollisionEntityInfo& colliderInfo) const
{
	const float collisionAgainstVehicleMultiplier = 7.5f;
	const float collisionAgainstPlayerSliding = 0.0f;
	const float collisionAgainstPlayerSprinting = 0.0f;
	const float collisionAgainstPlayerAirborne = 5.0f;

	if (colliderInfo.pEntityVehicle)
	{
		return collisionAgainstVehicleMultiplier;
	}
	else if (colliderInfo.pEntityActor)
	{
		if (colliderInfo.pEntityActor->IsPlayer())
		{
			const CPlayer* pColliderPlayer = static_cast<const CPlayer*>(colliderInfo.pEntityActor);
			if (pColliderPlayer->IsSprinting())
				return collisionAgainstPlayerSprinting;
			else if (pColliderPlayer->IsSliding())
				return collisionAgainstPlayerSliding;
			else if(!pColliderPlayer->GetActorStats()->onGround)
				return collisionAgainstPlayerAirborne;
		}
		else
		{
			return 0.0f; // no collision damage between AIs
		}
	}
	else if (colliderInfo.pEntity) 
	{
		float multiplier = g_pGameCVars->AICollisions.dmgFactorWhenCollidedByObject;
		if (IScriptTable *pEntityScript = colliderInfo.pEntity->GetScriptTable())
		{
			SmartScriptTable propertiesTable;
			if (pEntityScript->GetValue("Properties", propertiesTable))
			{
				float dmgFactor = 1.f;
				propertiesTable->GetValue("DmgFactorWhenCollidingAI", dmgFactor);
				multiplier *= dmgFactor;
			}
		}
		return multiplier;
	}

	return 1.0f;
}

//------------------------------------------------------------------------
float CGameRulesCommonDamageHandling::GetVehicleForeignCollisionMultiplier( const IVehicle& vehicle, const SCollisionEntityInfo& colliderInfo, const CGameRules::SCollisionHitInfo& colHitInfo ) const
{
	float result = 1.0f;
	
	//Vehicle to vehicle collision
	if (colliderInfo.pEntityVehicle)
	{
		const float vehicleMass = vehicle.GetMass();
		const float vehicleColliderMass = colliderInfo.pEntityVehicle->GetMass();

		const float targetSpeedSqr = colHitInfo.target_velocity.len2();

		if ((vehicleMass > vehicleColliderMass * 1.5f) && (targetSpeedSqr > 0.01f))
		{
			//Reduce damage for collisions with large mass ratios, to avoid instant-killing
			const float ratio = 1.0f + (0.35f * min(10.0f, vehicleMass * __fres(vehicleColliderMass))) * min(1.0f, targetSpeedSqr * 0.31623f);
			result = __fres(ratio);

			if (DebugCollisions())
			{
				CryLog("Vehicle/Vehicle (%s <- %s), collision mult: %.2f", vehicle.GetEntity()->GetName(), colliderInfo.pEntity->GetName(), result);
			}
		}
	}

	return result;
}

//------------------------------------------------------------------------
void CGameRulesCommonDamageHandling::LogHit(const HitInfo& hit, bool extended, bool dead)
{
	const IEntity* shooter = gEnv->pEntitySystem->GetEntity(hit.shooterId);
	const IEntity* target = gEnv->pEntitySystem->GetEntity(hit.targetId);
	const IEntity* weapon = gEnv->pEntitySystem->GetEntity(hit.weaponId);

	CryLog("'%s' hit '%s' for %f with '%s'... %s", shooter?shooter->GetName():"null", target?target->GetName():"null", hit.damage, weapon?weapon->GetName():"null",dead?"*DEADLY*":"");

	if (extended)
	{
		CryLog("  shooterId..: %d", hit.shooterId);
		CryLog("  targetId...: %d", hit.targetId);
		CryLog("  weaponId...: %d", hit.weaponId);
		CryLog("  type.......: %d", hit.type);
		CryLog("  material...: %d", hit.material);
		CryLog("  damage.....: %f", hit.damage);
		CryLog("  partId.....: %d", hit.partId);
		CryLog("  pos........: %f %f %f", hit.pos.x, hit.pos.y, hit.pos.z);
		CryLog("  dir........: %f %f %f", hit.dir.x, hit.dir.y, hit.dir.z);
		CryLog("  radius.....: %.3f", hit.radius);
		CryLog("  remote.....: %d", hit.remote);

		if (IActor* targetActor = gEnv->pGameFramework->GetIActorSystem()->GetActor(hit.targetId))
			CryLog("  health.....: %f", targetActor->GetHealth());
		else
			CryLog("  health.....: N/A");
	}
}

#ifndef _RELEASE
bool CGameRulesCommonDamageHandling::DebugCollisions() const
{
	return (g_pGameCVars->g_debugCollisionDamage > 0);
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
CGameRulesCommonDamageHandling::SCollisionEntityInfo::SCollisionEntityInfo( const IEntity* _pEntity )
: pEntity(_pEntity)
, entityId(0)
, pEntityActor(NULL)
, pEntityVehicle(NULL)
{
	if (pEntity)
	{
		entityId = pEntity->GetId();

		IGameFramework* pGameFrameWork = g_pGame->GetIGameFramework();

		pEntityActor = pGameFrameWork->GetIActorSystem()->GetActor(entityId);
		pEntityVehicle = (pEntityActor == NULL) ? pGameFrameWork->GetIVehicleSystem()->GetVehicle(entityId) : NULL;
	}
}
