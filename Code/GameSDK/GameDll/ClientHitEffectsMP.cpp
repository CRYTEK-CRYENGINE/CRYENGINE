// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ClientHitEffectsMP.h"

#include "Player.h"
#include "GameRules.h"

#define HIT_EFFECT_XML "Scripts/GameRules/ClientHitEffectsMP.xml"


//------------------------------------------------------------------------
CClientHitEffectsMP::CClientHitEffectsMP()
{
	m_hitTargetSignal.SetSignal("HitTarget");
	m_hitTargetHeadshotSignal.SetSignal("HitTargetHeadshot");
	m_hitTargetMelee = g_pGame->GetGameAudio()->GetSignalID("ClientHitMelee");
}


//------------------------------------------------------------------------
CClientHitEffectsMP::~CClientHitEffectsMP()
{
	
}

//------------------------------------------------------------------------
void CClientHitEffectsMP::Initialise()
{
	XmlNodeRef rootNode = gEnv->pSystem->LoadXmlFromFile(HIT_EFFECT_XML);

	if (!rootNode || strcmp(rootNode->getTag(), "Effects"))
	{
		GameWarning("Could not load Hit Effect data. Invalid XML file '%s'! ", HIT_EFFECT_XML);
		return;
	}

	if(const char* libraryName = rootNode->getAttr("library"))
	{
		XmlNodeRef normalEffects = rootNode->findChild("Normal");
	
		if(normalEffects)
		{
			ProcessEffectInfo(m_normalEffects, normalEffects, libraryName);
		}
	}
	else
	{
		GameWarning("Hit Effect data missing 'library' attribute");
	}
}

void CClientHitEffectsMP::ProcessEffectInfo(SHitEffectInfoSet& hitEffectSet, XmlNodeRef xmlNode, const char* libraryName)
{
	bool foundDefault = false;
	bool foundMelee = false;
	const uint numEffects = xmlNode->getChildCount();
	IMaterialEffects* pMaterialEffects = gEnv->pMaterialEffects;
	IEntityClassRegistry* pClassRegistry = gEnv->pEntitySystem->GetClassRegistry();

	hitEffectSet.m_effectInfos.reserve(numEffects);

	for (uint i = 0; i < numEffects; i++)
	{
		if(XmlNodeRef childNode = xmlNode->getChild(i))
		{
			if(const char* nameTag = childNode->getTag())
			{
				if(!foundDefault && !strcmp("default", nameTag))
				{
					const char* effectName = childNode->getAttr("effect");

					if(effectName)
					{
						hitEffectSet.m_default = pMaterialEffects->GetEffectIdByName(libraryName, effectName);				
					}

					foundDefault = true;
				}
				else if(!foundMelee && !strcmp("melee", nameTag))
				{
					const char* effectName = childNode->getAttr("effect");

					if(effectName)
					{
						hitEffectSet.m_melee = pMaterialEffects->GetEffectIdByName(libraryName, effectName);				
					}

					foundMelee = true;
				}
				else
				{
					SHitEffectInfo newInfo;

					newInfo.pAmmoClass = pClassRegistry->FindClass(nameTag);
					
					const char* effectName = childNode->getAttr("effect");

					if(effectName)
					{
						newInfo.effectId = pMaterialEffects->GetEffectIdByName(libraryName, effectName);	
					}

					if(newInfo.pAmmoClass && newInfo.effectId)
					{
						hitEffectSet.m_effectInfos.push_back(newInfo);
					}
					else
					{
						if(!newInfo.pAmmoClass)
						{
							GameWarning("Class type %s does not exist", nameTag);
						}
						
						if(!newInfo.effectId)
						{
							GameWarning("Material Effect %s does not exist", effectName ? effectName : "");
						}
					}
				}
			}
		}
	}

	if(!hitEffectSet.m_melee)
	{
		hitEffectSet.m_melee = hitEffectSet.m_default;
	}
}

//------------------------------------------------------------------------
void CClientHitEffectsMP::Feedback(const CGameRules* pGameRules, const CPlayer* pTargetPlayer, const HitInfo &hitInfo)
{
	if(hitInfo.type == CGameRules::EHitType::Melee)
	{
		CAudioSignalPlayer::JustPlay(m_hitTargetMelee, pTargetPlayer->GetEntityId());
	}
	
	SpawnMaterialEffect(pGameRules, pTargetPlayer, hitInfo, false);
}

//------------------------------------------------------------------------
void CClientHitEffectsMP::KillFeedback(const CActor* pTarget, const HitInfo &hitInfo)
{
}

//------------------------------------------------------------------------
void CClientHitEffectsMP::SpawnMaterialEffect(const CGameRules* pGameRules, const CPlayer* pTargetPlayer, const HitInfo &hitInfo, bool inArmour)
{
	TMFXEffectId spawnEffectId = InvalidEffectId;
	SHitEffectInfoSet* effectInfos = &m_normalEffects;

	SMFXRunTimeEffectParams newParams;

	newParams.dir[0] = hitInfo.dir;
	newParams.pos = hitInfo.pos;
	newParams.normal = hitInfo.normal;
	newParams.partID = hitInfo.partId;
	newParams.src = hitInfo.projectileId;
	newParams.trg = hitInfo.targetId;

	if(inArmour)
	{
		const CGameRules::eThreatRating threat = pGameRules->GetThreatRating(gEnv->pGameFramework->GetClientActorId(), pTargetPlayer->GetEntityId());
		const int effect = CLAMP(threat==CGameRules::eHostile ? 1 : 0, 0, static_cast<int>(m_armourTeamEffects.size()) - 1);
		effectInfos = &m_armourTeamEffects[effect];
	}

	if(hitInfo.type == CGameRules::EHitType::Melee)
	{
		spawnEffectId = effectInfos->m_melee;
	}
	else if(IEntity* pEntity = gEnv->pEntitySystem->GetEntity(hitInfo.projectileId))
	{
		spawnEffectId = FindEffectIdForClass(pEntity->GetClass(), *effectInfos);
	}

	if(spawnEffectId)
	{
		gEnv->pMaterialEffects->ExecuteEffect(spawnEffectId, newParams);
	}
}

TMFXEffectId CClientHitEffectsMP::FindEffectIdForClass(IEntityClass* pEntityClass, SHitEffectInfoSet& hitEffectInfos)
{
	const uint numEffects = hitEffectInfos.m_effectInfos.size();

	for (uint i = 0; i < numEffects; i++)
	{
		SHitEffectInfo& effectInfo = hitEffectInfos.m_effectInfos[i];

		if(effectInfo.pAmmoClass == pEntityClass)
		{
			return effectInfo.effectId;
		}
	}

	return hitEffectInfos.m_default;
}
