// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  Description: GameAIModule for assisting an AI using defense (armor) mode
  
 -------------------------------------------------------------------------
  History:
  - 26:04:2010: Created by Kevin Kirst

*************************************************************************/

#include "StdAfx.h"
#include "ArmorModule.h"
#include "GameAIEnv.h"
#include "Actor.h"
#include "Agent.h"
#include "GameRules.h"

namespace ArmorModule_Constants
{
	static const char* g_szArmorModule_ParamsXml		= "Libs\\AI\\ArmorModuleParams.xml";

	static const char* g_szArmorModule_ShieldStrengthLua	= "ArmorShieldStrength";
	static const char* g_szArmorModule_SpeedScaleLua	= "ArmorSpeedScale";
};

//////////////////////////////////////////////////////////////////////////
ArmorModule::SActiveParams::SActiveParams()
: bBlocked(false)
, bFailing(false)
, state(STATE_OFF)
, fShieldStrengthMax(0.0f)
, fShieldStrengthCurrent(0.0f)
, fSpeedScale(1.0f)
, fTransitionEnd(0.0f)
, bodyShieldAttachmentId(-1)
{
	
}

//////////////////////////////////////////////////////////////////////////
ArmorModule::ArmorModule()
: m_fFailingPercent(0.0f)
{
#ifndef _RELEASE
	REGISTER_COMMAND("armorModule_reload", ArmorModule::CmdArmorModuleReload, VF_CHEAT, "Reload the Armor Module Params Xml file.");
#endif //_RELEASE
}

//////////////////////////////////////////////////////////////////////////
#ifndef _RELEASE
void ArmorModule::CmdArmorModuleReload(IConsoleCmdArgs *pArgs)
{
	if (gGameAIEnv.armorModule)
		gGameAIEnv.armorModule->LoadParamsXml();
}
#endif //_RELEASE

//////////////////////////////////////////////////////////////////////////
void ArmorModule::LoadParamsXml()
{
	globalShaderParams.NormalMode.clear();
	globalShaderParams.FailingMode.clear();
	globalShaderParams.PowerUpModifiers.clear();
	globalShaderParams.HitReactionModifiers.clear();

	XmlNodeRef pRoot = gEnv->pSystem->LoadXmlFromFile(ArmorModule_Constants::g_szArmorModule_ParamsXml);
	if (pRoot)
	{
		XmlNodeRef pConstants = pRoot->findChild("Constants");
		if (pConstants)
		{
			m_sAttachmentName = pConstants->getAttr("AttachmentName");
			pConstants->getAttr("FailingPercent", m_fFailingPercent);
		}

		XmlNodeRef pWeaponInfo = pRoot->findChild("Weapons");
		if (pWeaponInfo)
		{
			LoadParamsXml_WeaponInfo(pWeaponInfo);
		}

		XmlNodeRef pShaderParams = pRoot->findChild("ShaderParams");
		if (pShaderParams)
		{
			// Normal mode
			XmlNodeRef pNormalMode = pShaderParams->findChild("Normal");
			if (pNormalMode)
			{
				LoadParamsXml_Mode(globalShaderParams.NormalMode, pNormalMode, "Normal");
			}

			// Failing mode
			XmlNodeRef pFailingMode = pShaderParams->findChild("Failing");
			if (pFailingMode)
			{
				LoadParamsXml_Mode(globalShaderParams.FailingMode, pFailingMode, "Failing");
			}

			// Modifiers
			XmlNodeRef pModifiers = pShaderParams->findChild("Modifiers");
			if (pModifiers)
			{
				// Power Up
				XmlNodeRef pPowerUpModifiers = pModifiers->findChild("PowerUp");
				if (pPowerUpModifiers)
				{
					LoadParamsXml_Modifiers(globalShaderParams.PowerUpModifiers, pPowerUpModifiers, "PowerUp Modifiers");
				}

				// Hit Reaction
				XmlNodeRef pHitReactionModifiers = pModifiers->findChild("HitReaction");
				if (pHitReactionModifiers)
				{
					LoadParamsXml_Modifiers(globalShaderParams.HitReactionModifiers, pHitReactionModifiers, "HidReaction Modifiers");
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void ArmorModule::LoadParamsXml_WeaponInfo(XmlNodeRef &pNode)
{
	IGameFramework *pGameFramework = g_pGame->GetIGameFramework();
	assert(pGameFramework);

	// Load damage multipliers
	XmlNodeRef pDamageMultipliers = pNode->findChild("DamageMultipliers");
	if (pDamageMultipliers)
	{
		const int childCount = pDamageMultipliers->getChildCount();
		for (int child = 0; child < childCount; ++child)
		{
			XmlNodeRef pWeapon = pDamageMultipliers->getChild(child);
			if (!pWeapon || !pWeapon->isTag("Weapon"))
				continue;

			float fMultiplier = 1.0f;
			if (pWeapon->getAttr("multiplier", fMultiplier) && fMultiplier >= 0.0f)
			{
				uint16 weaponClassId = 0;
				const char* className = pWeapon->getAttr("class");
				if (pGameFramework->GetNetworkSafeClassId(weaponClassId, className))
				{
					m_WeaponDamageMods.insert(std::make_pair(weaponClassId, fMultiplier));
				}
			}
		}
	}

	// Load instant damages
	XmlNodeRef pDamageInstant = pNode->findChild("DamageInstant");
	if (pDamageInstant)
	{
		const int childCount = pDamageInstant->getChildCount();
		for (int child = 0; child < childCount; ++child)
		{
			XmlNodeRef pWeapon = pDamageInstant->getChild(child);
			if (!pWeapon || !pWeapon->isTag("Weapon"))
				continue;

			uint16 weaponClassId = 0;
			const char* className = pWeapon->getAttr("class");
			if (pGameFramework->GetNetworkSafeClassId(weaponClassId, className))
			{
				m_WeaponDamageMods.insert(std::make_pair(weaponClassId, -1.0f));
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void ArmorModule::LoadParamsXml_Mode(TShaderParams &outShaderParams, XmlNodeRef &pNode, const char* szDebugName)
{
	Crc32Gen *pCrc32Gen = gEnv->pSystem->GetCrc32Gen();
	assert(pCrc32Gen);

	const int childCount = pNode->getChildCount();
	for (int child = 0; child < childCount; ++child)
	{
		XmlNodeRef pChild = pNode->getChild(child);
		if (!pChild)
			continue;

		SShaderParamEntry shaderParamEntry;
		shaderParamEntry.name = pChild->getAttr("name");

		const char* szTag = pChild->getTag();
		if (0 == stricmp(szTag, "Float"))
		{
			pChild->getAttr("value", shaderParamEntry.currValue.m_Float);
		}
		else if (0 == stricmp(szTag, "Color"))
		{
			Vec3 vColor;
			pChild->getAttr("value", vColor);
			shaderParamEntry.currValue.m_Color[0] = vColor.x;
			shaderParamEntry.currValue.m_Color[1] = vColor.y;
			shaderParamEntry.currValue.m_Color[2] = vColor.z;
			shaderParamEntry.currValue.m_Color[3] = 1.0f;
		}
		else
		{
			GameWarning("[ArmorModule::LoadParamsXml] Invalid child tag \'%s\' when parsing Shader Params - %s", szTag, szDebugName);
		}

		std::pair<TShaderParams::iterator, bool> result = outShaderParams.insert(std::make_pair(pCrc32Gen->GetCRC32(shaderParamEntry.name.c_str()), shaderParamEntry));
		if (result.second == false)
		{
			GameWarning("[ArmorModule::LoadParamsXml] Failed to insert child element \'%d\' when parsing Shader Params - %s (possibly a duplicate)", child+1, szDebugName);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void ArmorModule::LoadParamsXml_Modifiers(TShaderParams &outShaderParams, XmlNodeRef &pNode, const char* szDebugName)
{
	Crc32Gen *pCrc32Gen = gEnv->pSystem->GetCrc32Gen();
	assert(pCrc32Gen);

	const int childCount = pNode->getChildCount();
	for (int child = 0; child < childCount; ++child)
	{
		XmlNodeRef pChild = pNode->getChild(child);
		if (!pChild || !pChild->isTag("Param"))
		{
			const char* szTag = pChild ? pChild->getTag() : "Unknown";
			GameWarning("[ArmorModule::LoadParamsXml] Invalid child tag \'%s\' when parsing Shader Params - %s", szTag, szDebugName);
			continue;
		}

		SShaderParamEntry shaderParamEntry;
		shaderParamEntry.name = pChild->getAttr("name");
		pChild->getAttr("peak", shaderParamEntry.peak);
		pChild->getAttr("decay", shaderParamEntry.decay);

		std::pair<TShaderParams::iterator, bool> result = outShaderParams.insert(std::make_pair(pCrc32Gen->GetCRC32(shaderParamEntry.name.c_str()), shaderParamEntry));
		if (result.second == false)
		{
			GameWarning("[ArmorModule::LoadParamsXml] Failed to insert child element \'%d\' when parsing Shader Params - %s (possibly a duplicate)", child+1, szDebugName);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void ArmorModule::EntityEnter(EntityId entityID)
{
	TActiveIds::iterator itFind = m_ActiveIds.find(entityID);
	if (itFind == m_ActiveIds.end())
	{
		SActiveParams params;
		InitActiveParams(entityID, params);

		TActiveIds::value_type newEntry(entityID, params);
		m_ActiveIds.insert(newEntry);
	}
}

//////////////////////////////////////////////////////////////////////////
void ArmorModule::EntityLeave(EntityId entityID)
{
	TActiveIds::iterator itFind = m_ActiveIds.find(entityID);
	if (itFind != m_ActiveIds.end())
	{
		SActiveParams &params = itFind->second;
		KillEffect(entityID, params);

		m_ActiveIds.erase(itFind);
	}
}

//////////////////////////////////////////////////////////////////////////
void ArmorModule::EntityPause(EntityId entityID)
{
	TActiveIds::iterator itFind = m_ActiveIds.find(entityID);
	if (itFind != m_ActiveIds.end())
	{
		SActiveParams &params = itFind->second;
		KillEffect(entityID, params);
	}
}

//////////////////////////////////////////////////////////////////////////
void ArmorModule::EntityResume(EntityId entityID)
{
	TActiveIds::iterator itFind = m_ActiveIds.find(entityID);
	if (itFind != m_ActiveIds.end())
	{
		SActiveParams &params = itFind->second;
		StartEffect(entityID, params);
	}
}

//////////////////////////////////////////////////////////////////////////
void ArmorModule::SetBlockedMode(EntityId entityID, bool bBlocked)
{
	TActiveIds::iterator itFind = m_ActiveIds.find(entityID);
	if (itFind != m_ActiveIds.end())
	{
		SActiveParams &params = itFind->second;
		params.bBlocked = bBlocked;

		// Disable the effect if it's currently on
		if (bBlocked && SActiveParams::STATE_ON == params.state)
		{
			KillEffect(entityID, params);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void ArmorModule::ResumeDelayed(EntityId entityID, float fDelay)
{
	RestartTransitionTimer(entityID, fDelay);
}

//////////////////////////////////////////////////////////////////////////
void ArmorModule::RestartTransitionTimer(EntityId entityID, float fTimer)
{
	TActiveIds::iterator itFind = m_ActiveIds.find(entityID);
	if (itFind != m_ActiveIds.end())
	{
		SActiveParams &params = itFind->second;

		if (SActiveParams::STATE_OFF == params.state)
		{
			params.BeginTransition(gEnv->pTimer->GetCurrTime(), fTimer);
		}
		else if (SActiveParams::STATE_TRANSITION == params.state && fTimer > 0.0f)
		{
			params.BeginTransition(gEnv->pTimer->GetCurrTime(), fTimer);
		}
		else if (SActiveParams::STATE_BLOCKED == params.state && fTimer > 0.0f)
		{
			params.BeginTransition(gEnv->pTimer->GetCurrTime(), fTimer);
		}
		else
		{
			StartEffect(entityID, params);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void ArmorModule::Reset(bool bUnload)
{
	// Disable the effect for all of them
	TActiveIds::iterator itActive = m_ActiveIds.begin();
	TActiveIds::iterator itActiveEnd = m_ActiveIds.end();
	for (; itActive != itActiveEnd; ++itActive)
	{
		EntityId entityID = itActive->first;
		SActiveParams &params = itActive->second;
		KillEffect(entityID, params);

		// Re-read properties
		InitActiveParams(entityID, params);
	}
	m_ActiveIds.clear();

	if (gEnv->IsEditor())
		LoadParamsXml();
}

//////////////////////////////////////////////////////////////////////////
void ArmorModule::InitActiveParams(EntityId entityID, SActiveParams &params)
{
	// Read its properties
	IEntity *pEntity = gEnv->pEntitySystem->GetEntity(entityID);
	IScriptTable* pScriptTable = pEntity ? pEntity->GetScriptTable() : NULL;
	SmartScriptTable pProperties;
	if (pScriptTable != NULL && pScriptTable->GetValue("Properties", pProperties))
	{
		SmartScriptTable pArmorAbilityProperties;
		if (pProperties && pProperties->GetValue("ArmorAbility", pArmorAbilityProperties))
		{
			pArmorAbilityProperties->GetValue(ArmorModule_Constants::g_szArmorModule_ShieldStrengthLua, params.fShieldStrengthMax);
			pArmorAbilityProperties->GetValue(ArmorModule_Constants::g_szArmorModule_SpeedScaleLua, params.fSpeedScale);
		}
	}

	// Get the attachment index
	IAttachmentManager* pAttachmentManager = GetAttachmentManager(entityID);
	if (pAttachmentManager)
	{
		params.bodyShieldAttachmentId = pAttachmentManager->GetIndexByName(m_sAttachmentName.c_str());

		IAttachment* pAttachment = pAttachmentManager->GetInterfaceByIndex(params.bodyShieldAttachmentId);
		IAttachmentObject *pAttachmentObj = pAttachment ? pAttachment->GetIAttachmentObject() : NULL;
		if (pAttachmentObj)
		{
			IMaterialManager *pMaterialManager = gEnv->p3DEngine->GetMaterialManager();
			assert(pMaterialManager);

			// Clone the material to make a new instance for our needs
			IMaterial *pMaterial = pAttachmentObj->GetMaterial();
			IMaterial *pCloneMaterial = pMaterialManager->CloneMaterial(pMaterial);
			pAttachmentObj->SetMaterial(pCloneMaterial);
		}
	}
	else
	{
		params.bodyShieldAttachmentId = -1;
	}

	// Clone shader param
	params.shaderParams = globalShaderParams;
}

//////////////////////////////////////////////////////////////////////////
IAttachmentManager* ArmorModule::GetAttachmentManager(EntityId entityID)
{
	IEntitySystem *pEntitySystem = gEnv->pEntitySystem;
	assert(pEntitySystem);

	IEntity *pEntity = pEntitySystem->GetEntity(entityID);
	ICharacterInstance* pCharacter = pEntity ? pEntity->GetCharacter(0) : NULL;
	IAttachmentManager* pAttachmentManager = pCharacter ? pCharacter->GetIAttachmentManager() : NULL;

	return pAttachmentManager;
}

//////////////////////////////////////////////////////////////////////////
void ArmorModule::HideAttachment(EntityId entityID, const SActiveParams &params, bool bHide)
{
	if (params.bodyShieldAttachmentId > -1)
	{
		IAttachmentManager* pAttachmentManager = GetAttachmentManager(entityID);
		if (pAttachmentManager)
		{
			IAttachment* pAttachment = pAttachmentManager->GetInterfaceByIndex(params.bodyShieldAttachmentId);
			if (pAttachment)
				pAttachment->HideAttachment(bHide ? 1 : 0);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void ArmorModule::TriggerShaderParamModifiers(SActiveParams &params, TShaderParams &ApplyModifier)
{
	params.shaderParams.pModifierValues = &ApplyModifier;

	for (TShaderParams::iterator itParam = ApplyModifier.begin(), itParamEnd = ApplyModifier.end(); itParam != itParamEnd; ++itParam)
	{
		itParam->second.currValue.m_Float = itParam->second.peak;
	}
}

//////////////////////////////////////////////////////////////////////////
void ArmorModule::ApplyShaderParamModifiers(SActiveParams &params, TShaderParams &ModeParams, float dt)
{
	if (params.shaderParams.pModifierValues)
	{
		TShaderParams &ModifierParams = *(params.shaderParams.pModifierValues);

		for (TShaderParams::iterator itModifierParam = ModifierParams.begin(), itModifierParamEnd = ModifierParams.end(); 
			itModifierParam != itModifierParamEnd; ++itModifierParam)
		{
			SShaderParamEntry &modifierParam = itModifierParam->second;

			// Decay the current modifier value
			modifierParam.currValue.m_Float = modifierParam.currValue.m_Float - modifierParam.decay * dt;

			// Update the mode param with the modifier value if it's greater than the defined mode param
			TShaderParams::iterator itModeParam = ModeParams.find(itModifierParam->first);
			if (itModeParam != ModeParams.end())
			{
				SShaderParamEntry &modeParam = itModeParam->second;
				modeParam.currValue.m_Float = max(modeParam.currValue.m_Float, modifierParam.currValue.m_Float);
			}
			else
			{
				// Add entry to the mode params
				modifierParam.currValue.m_Float = (float)__fsel(modifierParam.currValue.m_Float, modifierParam.currValue.m_Float, 0.0f);
				ModeParams.insert(std::make_pair(itModifierParam->first, modifierParam));
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void ArmorModule::UpdateShaderParams(EntityId entityID, SActiveParams &params, float dt) const
{
	// Check if armor module is about to fail
	const bool bWasFailing = params.bFailing;
	const float fMaxFailStrength = params.fShieldStrengthMax * m_fFailingPercent;
	params.bFailing = params.fShieldStrengthCurrent <= fMaxFailStrength;

	// Apply the modifiers to the correct copy of our mode params
	TShaderParams &modeParams = params.appliedShaderParams;
	modeParams = params.bFailing ? params.shaderParams.FailingMode : params.shaderParams.NormalMode;
	ApplyShaderParamModifiers(params, modeParams, dt);

	// Update the shader param effects
	if (params.bodyShieldAttachmentId > -1)
	{
		IAttachmentManager* pAttachmentManager = GetAttachmentManager(entityID);
		IAttachment* pAttachment = pAttachmentManager->GetInterfaceByIndex(params.bodyShieldAttachmentId);
		IAttachmentObject *pAttachmentObj = pAttachment ? pAttachment->GetIAttachmentObject() : NULL;
		IMaterial *pMaterial = pAttachmentObj ? pAttachmentObj->GetMaterial() : NULL;

		if (pMaterial)
		{
			const SShaderItem& currShaderItem = pMaterial->GetShaderItem();  
			IShader* pShader = currShaderItem.m_pShader;
			if (pShader)
			{
				bool bUpdated = false;
				DynArrayRef<SShaderParam> &parameters = currShaderItem.m_pShaderResources->GetParameters();

				for (TShaderParams::iterator itParam = modeParams.begin(), itParamEnd = modeParams.end(); itParam != itParamEnd; ++itParam)
				{
					SShaderParamEntry& entry = itParam->second;

					bUpdated |= SShaderParam::SetParam(entry.name, &parameters, entry.currValue);
				}

				if (bUpdated)
				{
					currShaderItem.m_pShaderResources->UpdateConstants(pShader);
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void ArmorModule::StartEffect(EntityId entityID, SActiveParams &params, bool bForce)
{
	if (!params.bBlocked && (bForce || SActiveParams::STATE_OFF == params.state))
	{
		params.state = SActiveParams::STATE_ON;

		HideAttachment(entityID, params, false);

		CActor *pActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(entityID));
		if (pActor)
		{
			pActor->SetSpeedMultipler(SActorParams::eSMR_AIArmorModule, params.fSpeedScale);
		}

		// Refill strength
		params.fShieldStrengthCurrent = params.fShieldStrengthMax;

		// Peak the param effects
		TriggerShaderParamModifiers(params, params.shaderParams.PowerUpModifiers);

		Agent agent(entityID);
		if (agent.IsValid())
		{
			agent.SetSignal(1, "OnArmorModeEnabled");
		}
	}
	else if (params.bBlocked)
	{
		// Set our state to blocked so the shield is activated once the block is remove (unless we force the transition)
		params.state = SActiveParams::STATE_BLOCKED;
	}
}

//////////////////////////////////////////////////////////////////////////
void ArmorModule::KillEffect(EntityId entityID, SActiveParams &params)
{
	if (SActiveParams::STATE_OFF != params.state)
	{
		params.state = SActiveParams::STATE_OFF;

		HideAttachment(entityID, params, true);

		CActor *pActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(entityID));
		if (pActor)
		{
			pActor->SetSpeedMultipler(SActorParams::eSMR_AIArmorModule, 1.0f);
		}

		Agent agent(entityID);
		if (agent.IsValid())
		{
			agent.SetSignal(1, "OnArmorModeDisabled");
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void ArmorModule::Update(float dt)
{
	const float fCurrTime = gEnv->pTimer->GetCurrTime();

	TActiveIds::iterator itActive = m_ActiveIds.begin();
	TActiveIds::iterator itActiveEnd = m_ActiveIds.end();
	for (; itActive != itActiveEnd; ++itActive)
	{
		EntityId entityID = itActive->first;
		SActiveParams &params = itActive->second;

		// On update (shader params)
		if (SActiveParams::STATE_ON == params.state)
		{
			UpdateShaderParams(entityID, params, dt);
		}
		// Timer update
		else if (SActiveParams::STATE_TRANSITION == params.state)
		{
			if (params.fTransitionEnd < fCurrTime)
			{
				StartEffect(entityID, params, true);
			}
		}
		// Blocked update
		else if (SActiveParams::STATE_BLOCKED == params.state)
		{
			if (!params.bBlocked)
			{
				StartEffect(entityID, params, true);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool ArmorModule::IsShieldActive(EntityId entityID) const
{
	bool bResult = false;

	TActiveIds::const_iterator itFind = m_ActiveIds.find(entityID);
	if (itFind != m_ActiveIds.end())
	{
		const SActiveParams &params = itFind->second;
		bResult = (SActiveParams::STATE_ON == params.state);
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
bool ArmorModule::HandleHit(const HitInfo &hitInfo)
{
	bool bResult = false;

	TActiveIds::iterator itFind = m_ActiveIds.find(hitInfo.targetId);
	if (itFind != m_ActiveIds.end())
	{
		SActiveParams &params = itFind->second;
		if (SActiveParams::STATE_ON == params.state && hitInfo.damage > FLT_EPSILON)
		{
			bool bInstantDamage = false;
			const float fDamageMult = GetShieldDamageMultiplier(params, hitInfo, bInstantDamage);

			if (!bInstantDamage)
			{
				// Forward damage to the shield strength instead
				params.fShieldStrengthCurrent -= hitInfo.damage * fDamageMult;
			}

			// Destroy the shield
			const bool bShieldDestroyed = (bInstantDamage || params.fShieldStrengthCurrent <= 0.0f);
			if (!bShieldDestroyed)
			{
				// Set the entity to have next collision ignored, to stop any bullet effects
				CGameRules *pGameRules = g_pGame->GetGameRules();
				if (pGameRules)
					pGameRules->SetEntityToIgnore(hitInfo.targetId);

				// Hit reaction shader effect
				TriggerShaderParamModifiers(params, params.shaderParams.HitReactionModifiers);

				bResult = true;
			}
			else
			{
				// Break the shield
				KillEffect(hitInfo.targetId, params);

				Agent agent(hitInfo.targetId);
				if (agent.IsValid())
				{
					agent.SetSignal(1, "OnArmorShieldDestroyed");
				}
			}
		}
		else if (SActiveParams::STATE_TRANSITION == params.state)
		{
			Agent agent(hitInfo.targetId);
			if (agent.IsValid())
			{
				agent.SetSignal(1, "OnArmorShieldTransitionDisrupted");
			}
		}
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
float ArmorModule::GetShieldDamageMultiplier(const SActiveParams &params, const HitInfo &hitInfo, bool &bOutInstantDamage)
{
	float fDamageMult = 1.0f;
	bOutInstantDamage = false;
	
	TWeaponDamageMods::iterator itHitType = m_WeaponDamageMods.find(hitInfo.weaponClassId);
	if (itHitType != m_WeaponDamageMods.end())
	{
		fDamageMult = itHitType->second;
		bOutInstantDamage = (fDamageMult < 0.0f);
	}

	return fDamageMult;
}
