// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Fire Mode Parameters

-------------------------------------------------------------------------
History:
- 26:02:2010   13:45 : Created by Claire Allan

*************************************************************************/
#pragma once

#ifndef __FIREMODEPARAMS_H__
#define __FIREMODEPARAMS_H__

#include "ItemSharedParams.h"
#include "ItemResourceCache.h"
#include "Recoil.h"
#include "GameRules.h"
#include "FireModePluginParams.h"
#include "ICryMannequin.h"
#include "UI/UITypes.h"

#include "Weapon.h"

struct SAIDescriptor
{
	SAIDescriptor() 
	{ 
		Reset(XmlNodeRef(NULL)); 
	}

#	define AI_DESCRIPTOR_PARAMS_MEMBERS(f) \
		f(AIWeaponDescriptor,	descriptor)

#	define AI_WEAPON_DESCRIPTOR_PARAMS_MEMBERS(f) \
		f(string,	firecmdHandler) \
		f(float,	fSpeed) \
		f(float,	fDamageRadius) \
		f(float,	fChargeTime) \
		f(float,	fRangeMax) \
		f(float,	fRangeMin) \
		f(bool,		bSignalOnShoot) \
		f(int,		burstBulletCountMin) \
		f(int,		burstBulletCountMax) \
		f(float,	burstPauseTimeMin) \
		f(float,	burstPauseTimeMax) \
		f(float,	singleFireTriggerTime) \
		f(float,	spreadRadius) \
		f(float,	coverFireTime) \
		f(float,	drawTime) \
		f(float,	sweepWidth) \
		f(float,	sweepFrequency) \
		f(float,	pressureMultiplier) \
		f(float,	lobCriticalDistance) \
		f(float,	preferredHeight) \
		f(float,	preferredHeightForCloseDistance) \
		f(float,	maxAcceptableDistanceFromTarget) \
		f(Vec3,		projectileGravity) \
		f(string,	smartObjectClass)

	REGISTER_STRUCT(AI_DESCRIPTOR_PARAMS_MEMBERS, SAIDescriptor)

	void Reset(const XmlNodeRef& paramsNode, bool defaultInit=true);

	void GetMemoryUsage(ICrySizer * s) const {}
};


struct SHazardDescriptor
{
	SHazardDescriptor()
		: m_DefinedFlag(false)
	{ 
		Reset(XmlNodeRef(NULL)); 
	}

#	define HAZARD_DESCRIPTOR_PARAMS_MEMBERS(f) \
	f(HazardWeaponDescriptor,	descriptor)

	// Note: For more information of the meaning of these attributes, see the 
	// HazardWeaponDescriptor structure.

#	define HAZARD_WEAPON_DESCRIPTOR_PARAMS_MEMBERS(f) \
	f(float,	maxHazardDistance) \
	f(float,	hazardRadius) \
	f(float,	startPosNudgeOffset) \
	f(float,	maxHazardApproxPosDeviation) \
	f(float,	maxHazardApproxAngleDeviationDeg)

	// If this flag is raised then the hazard area should be generated in front of
	// the weapon; otherwise not.
	bool		m_DefinedFlag;

	REGISTER_STRUCT(HAZARD_DESCRIPTOR_PARAMS_MEMBERS, SHazardDescriptor)

		void Reset(const XmlNodeRef& paramsNode, bool defaultInit=true);

	void GetMemoryUsage(ICrySizer * s) const {}
};


struct SMeleeParams
{
	struct SCloseRange
	{
		float range;
		float delay;
		float duration;

		float impulse_ai_to_player;

		SCloseRange() 
		{ 
			Reset(XmlNodeRef(NULL)); 
		}

		void Reset(const XmlNodeRef& paramsNode, bool defaultInit = true);

		void GetMemoryUsage(ICrySizer * s) const {}
	};

	struct SPowerParams
	{
		float delay;
		float duration;

		SPowerParams() : delay(0.0f), duration(0.0f)
		{
			Reset(XmlNodeRef(NULL));
		}

		void Reset(const XmlNodeRef& paramsNode, bool defaultInit = true);
	};

	SMeleeParams() 
	{ 
		Reset(XmlNodeRef(NULL)); 
	}

	void Reset(const XmlNodeRef& paramsNode, bool defaultInit = true);

	void GetMemoryUsage(ICrySizer * s) const
	{
		s->AddObject(helper);
		s->AddObject(closeAttack);
	}

	ItemString	helper;
	float				range;
	float				target_range_mult;

	short		damage;
	short		damage_ai;
	short		slide_damage;		

	float		impulse;
	float		impulse_actor;
	float		impulse_ai_to_player;
	float		impulse_vehicle;

	float		delay;
	float		aiDelay;
	float		duration;

	float		knockdown_chance;   // probability to knock down, 0-100. 
	float		impulse_up_percentage;

	float		use_melee_weapon_delay; // <0.0f == don't use melee weapon
	float		weapon_restore_delay;

	SCloseRange	closeAttack;
	SPowerParams powerAttack;
	
	TAudioSignalID m_FPSignalId;
	TAudioSignalID m_3PSignalId;

	bool is_melee_weapon;
	bool trigger_client_reaction;
};

struct SMeleeTags
{
	struct STagParams
	{
		uint32 crcTagID;
		uint32 crcHitType;
		TagState  tagState;
		int    hitType;

		STagParams() : crcTagID(0), crcHitType(0), tagState(TAG_STATE_EMPTY), hitType(-1) {}
		STagParams( const char* pTag, const char* pHitType );

		void GetMemoryUsage(ICrySizer * s) const
		{
			s->AddObject(crcTagID);
			s->AddObject(crcHitType);
			s->AddObjectSize(&tagState);
			s->AddObject(hitType);
		}
	};

	struct SMeleeFragData
	{
		SMeleeFragData()
			:
		m_animIndex(0xffffffff),
		m_pMeleeTags(NULL)
		{
		}

		SMeleeFragData( uint32 animIndex, const SMeleeTags::STagParams& meleeTags )
			:
		m_animIndex(animIndex),
		m_pMeleeTags(&meleeTags)
		{}

		const uint32 m_animIndex;
		const SMeleeTags::STagParams* m_pMeleeTags;
	};

	typedef std::vector<STagParams> TTagParamsContainer;
	TTagParamsContainer tag_params_combo_left;
	TTagParamsContainer tag_params_combo_right;
	TTagParamsContainer tag_params_combo_killingblow;

	void Reset( const XmlNodeRef& tagsNode, bool defaultInit = true );

	void GetMemoryUsage(ICrySizer * s) const
	{
		s->AddObject(tag_params_combo_left);
		s->AddObject(tag_params_combo_right);
		s->AddObject(tag_params_combo_killingblow);
	}

	void Resolve( const CTagDefinition* pTagDefinition, FragmentID fragID, const IActionController* piActionController ) const;

private:
	struct FResolve
	{
		FragmentID m_fragID;
		const IActionController* m_piActionController;
		const CGameRules* m_pGameRules;
		const CTagDefinition* m_pTagDefinition;

		FResolve( const CTagDefinition* pTagDefinition, FragmentID fragID, const IActionController* piActionController )
			: m_fragID(fragID)
			, m_piActionController(piActionController)
			, m_pGameRules(g_pGame->GetGameRules())
			, m_pTagDefinition(pTagDefinition)
		{}

		void operator()( const STagParams& tagParams ) const
		{
			STagParams& tagParamsNonConst = const_cast<STagParams&> (tagParams);
			TagID tagID = m_piActionController->GetFragTagID( m_fragID, tagParams.crcTagID );
			m_pTagDefinition->Set( tagParamsNonConst.tagState, tagID, true );
			tagParamsNonConst.hitType = m_pGameRules->GetHitTypeId( tagParams.crcHitType );
		}
	};
};

struct SMeleeActions
{
	SMeleeActions()
	{
		Reset(XmlNodeRef(NULL));
	}

	void GetMemoryUsage(ICrySizer * s) const 
	{
		s->AddObject(attack);
		s->AddObject(attack_closeRange);
		s->AddObject(hit);
		s->AddObject(attack_multipart);
	}

	void Reset(const XmlNodeRef& paramsNode, bool defaultInit = true);

	ItemString attack;
	ItemString attack_closeRange;
	ItemString hit;
	ItemString attack_multipart; //Multi-part melee
};

struct SMeleeModeParams
{
	SMeleeModeParams() {};

	SMeleeParams	meleeparams;
	SMeleeTags    meleetags; // should replace actions post mannequin.
	SMeleeActions meleeactions;

	void GetMemoryUsage(ICrySizer * s) const
	{
		meleeparams.GetMemoryUsage(s);
		meleeactions.GetMemoryUsage(s);
		meleetags.GetMemoryUsage(s);
	}
};

struct SAccessoryMeleeParams
{
	SAccessoryMeleeParams() : pAlteredMeleeParams(NULL) { pAccessories[0] = pAccessories[1] = pAccessories[2] = pAccessories[3] = NULL; }

	IEntityClass* pAccessories[ITEM_MAX_NUM_ACCESSORIES];
	SMeleeModeParams* pAlteredMeleeParams;

	const SMeleeModeParams* FindAccessoryZoomModeParams(IEntityClass* pClass, IEntityClass* pClassTwo = NULL, IEntityClass* pClassThree = NULL, IEntityClass* pClassFour = NULL) const
	{
		if(pClass == pAccessories[0] && pClassTwo == pAccessories[1] && pClassThree == pAccessories[2] && pClassFour == pAccessories[3])
		{
			return pAlteredMeleeParams;
		}

		return NULL;
	}

	void GetMemoryUsage(ICrySizer* s) const
	{
		s->AddObject(pAlteredMeleeParams);
	}

	void Release()
	{
		SAFE_DELETE(pAlteredMeleeParams);
	}
};

struct SPlantParams
{
	SPlantParams() 
	{ 
		Reset(XmlNodeRef(NULL)); 
	}

#define PLANT_PARAMS_MEMBERS(f) \
	f(IEntityClass*,	ammo_type_class) \
	f(int,						damage) \
	f(int,						clip_size) \
	f(bool,						simple) \
	f(float,					delay) \
	f(float,					selectDelay) \
	f(bool,						is_silenced) \

	REGISTER_STRUCT(PLANT_PARAMS_MEMBERS, SPlantParams)

	void Reset(const XmlNodeRef& paramsNode, bool defaultInit=true);

	void GetMemoryUsage(ICrySizer * s) const
	{
	}
};

struct SLTagGrenades
{
	SLTagGrenades()
	{
		Reset(XmlNodeRef(NULL));
	}

	void Reset(const XmlNodeRef& paramsNode, bool defaultInit = true);

	void PreLoadAssets() const;

	void GetMemoryUsage(ICrySizer* s) const
	{
		for (int i = 0; i < ELTAGGrenadeType_LAST; ++i)
		{
			s->AddObject(grenades[i]);
		}
	}

#define LTAG_GRENADE_MEMBERS(f)

#define LTAG_GRENADE_ARRAYS(f) \
	f(ItemString, ELTAGGrenadeType_LAST, grenades)
	
	REGISTER_STRUCT_WITH_ARRAYS(LTAG_GRENADE_MEMBERS, LTAG_GRENADE_ARRAYS, SLTagGrenades);
};

struct SRapidParams
{
	SRapidParams() 
	{ 
		Reset(XmlNodeRef(NULL)); 
	}
	
	void Reset(const XmlNodeRef& paramsNode, bool defaultInit=true);

	void GetMemoryUsage(ICrySizer * s) const
	{
		s->AddObject(barrel_attachment);
	}

#define RAPID_PARAMS_MEMBERS(f) \
	f(float,	min_speed) \
	f(float,	max_speed) \
	f(float,	acceleration) \
	f(float,	deceleration) \
	f(float,	min_firingTimeToStop) \
	f(ItemString, barrel_attachment) \

	REGISTER_STRUCT(RAPID_PARAMS_MEMBERS, SRapidParams)
};

struct SChargeParams
{
	SChargeParams() 
	{ 
		Reset(XmlNodeRef(NULL)); 
	}

	void Reset(const XmlNodeRef& paramsNode, bool defaultInit=true);

	void GetMemoryUsage(ICrySizer* s) const {};

#define CHARGE_PARAMS_MEMBERS(f) \
	f(float,	time) \
	f(int,		max_charges) \
	f(bool,		shoot_on_stop) \
	f(bool,		reset_spinup)

	REGISTER_STRUCT(CHARGE_PARAMS_MEMBERS, SChargeParams)
};

struct SThrowParams
{
	SThrowParams() 
	{ 
		Reset(XmlNodeRef(NULL)); 
	}

	void Reset(const XmlNodeRef& paramsNode, bool defaultInit=true);

	void GetMemoryUsage(ICrySizer * s) const
	{
		s->AddObject(neutralSuffixAG);
	};

#define THROW_PARAMS_MEMBERS(f) \
	f(ItemString,	neutralSuffixAG) \
	f(float,			throw_delay) \
	f(float,			prime_delay) \
	f(bool,				prime_timer) \
	f(bool,				prime_enabled) \
	f(bool,				crosshairblink_enabled) \
	f(bool,				display_trajectory)

	REGISTER_STRUCT(THROW_PARAMS_MEMBERS, SThrowParams)
};

struct SBurstParams
{
	SBurstParams() 
	{ 
		Reset(XmlNodeRef(NULL)); 
	}

	void Reset(const XmlNodeRef& paramsNode, bool defaultInit=true);

	void GetMemoryUsage(ICrySizer * s) const
	{
		s->AddObject(multiPipeHelper);
	};

#define BURST_PARAMS_MEMBERS(f) \
	f(ItemString,	multiPipeHelper) \
	f(short,			nshots) \
	f(short,			rate) \
	f(bool,				useBurstSoundParam) \
	f(bool,				multiPipe) \
	f(short,			spreadAngle)

	REGISTER_STRUCT(BURST_PARAMS_MEMBERS, SBurstParams)
};

struct SShotgunParams
{
	SShotgunParams() 
	{ 
		Reset(XmlNodeRef(NULL)); 
	}

	void Reset(const XmlNodeRef& paramsNode, bool defaultInit=true);

	void GetMemoryUsage(ICrySizer * s) const { }

#define SHOTGUN_PARAMS_MEMBERS(f) \
	f(short,	pellets) \
	f(short,	pelletdamage) \
	f(float,	spread) \
	f(bool,		partial_reload) \
	f(short,	npc_additional_damage) \
	f(uint8,	loadShellOnEndModes) \
	f(bool,		fully_automated) \
	f(float,	reloadBreakTime) \
	f(float, endReloadSpeedOverride)

	REGISTER_STRUCT(SHOTGUN_PARAMS_MEMBERS, SShotgunParams)
};

struct STracerParams
{
	STracerParams() 
	{ 
		Reset(XmlNodeRef(NULL)); 
	}

	void Reset(const XmlNodeRef& paramsNode, bool defaultInit=true);

#define TRACER_PARAMS_MEMBERS(f) \
	f(ItemString,	geometry) \
	f(ItemString,	effect) \
	f(ItemString,	geometryFP) \
	f(ItemString,	effectFP) \
	f(float,			speed) \
	f(float,			speedFP) \
	f(float,			thickness) \
	f(float,			thicknessFP) \
	f(float,			lifetime) \
	f(float,			delayBeforeDestroy) \
	f(float,			slideFraction) \
	f(int,				frequency)

#define TRACER_PARAMS_ARRAYS(f) \
	f(ItemString, 2, helper)

	REGISTER_STRUCT_WITH_ARRAYS(TRACER_PARAMS_MEMBERS, TRACER_PARAMS_ARRAYS, STracerParams)

	void PreLoadAssets() const
	{
		CItemResourceCache& itemResourceCache = g_pGame->GetGameSharedParametersStorage()->GetItemResourceCache();
		
		itemResourceCache.GetItemGeometryCache().CacheGeometry(geometryFP.c_str(), true);
		itemResourceCache.GetItemGeometryCache().CacheGeometry(geometry.c_str(), true);

#if !defined(_RELEASE)
		if(!gEnv->IsDedicated())
		{
			if(!effect.empty() && !gEnv->pParticleManager->FindEffect(effect.c_str()))
			{
				if (gEnv->IsEditor())
					GameWarning("Particle effect not found: %s! Remove reference or fix asset", effect.c_str());
				else
					GameWarning("!Particle effect not found: %s! Remove reference or fix asset", effect.c_str());
			}
			if(!effectFP.empty() && !gEnv->pParticleManager->FindEffect(effectFP.c_str()))
			{
				if (gEnv->IsEditor())
					GameWarning("Particle effect not found: %s! Remove reference or fix asset", effectFP.c_str());
				else
					GameWarning("!Particle effect not found: %s! Remove reference or fix asset", effectFP.c_str());
			}
		}
#endif
	}
	void GetMemoryUsage(ICrySizer * s) const
	{
		s->AddObject(geometry);
		s->AddObject(effect);
		s->AddObject(geometryFP);
		s->AddObject(effectFP);
		s->AddObject(helper[0]);
		s->AddObject(helper[1]);
	}
};

struct SFireParams
{
	SFireParams() 
	{ 
		Reset(XmlNodeRef(NULL)); 
	}

	void Reset(const XmlNodeRef& paramsNode, bool defaultInit=true);

	void GetMemoryUsage(ICrySizer * s) const
	{
		s->AddObject(helper[0]);
		s->AddObject(helper[1]);
		s->AddObject(suffix);
		s->AddObject(suffixAG);
		s->AddObject(hit_type);
	}

#define FIRE_PARAMS_MEMBERS(f) \
	f(ItemString,	hit_type) \
	f(ItemString,	suffix) \
	f(ItemString,	suffixAG) \
	f(ItemString, tag) \
	f(float,			changeFMFireDelayFraction) \
	f(float,			endReloadFraction) \
	f(float,			fillAmmoReloadFraction) \
	f(float,			stabilization) \
	f(float,			speed_override) \
	f(int,				damage) \
	f(float,			damage_drop_per_meter) \
	f(float,			damage_drop_min_distance) \
	f(float,			damage_drop_min_damage) \
	f(float,			point_blank_amount) \
	f(float,			point_blank_distance) \
	f(float,			point_blank_falloff_distance) \
	f(int,				npc_additional_damage) \
	f(float,			ai_reload_time) \
	f(float,			spin_up_time) \
	f(float,			min_damage_for_knockDown) \
	f(int,				knockdown_chance_leg) \
	f(float,			min_damage_for_knockDown_leg) \
	f(float,			fire_anim_damp) \
	f(float,			ironsight_fire_anim_damp) \
	f(float,			holdbreath_fire_anim_damp) \
	f(float,			holdbreath_ffeedback_damp) \
	f(ECrosshairTypes, crosshair) \
	f(IEntityClass*,	ammo_type_class) \
	f(IEntityClass*,	spawn_ammo_class) \
	f(short,			rate) \
	f(short,			fake_fire_rate) \
	f(short,			minimum_ammo_count) \
	f(short,			clip_size) \
	f(short,			max_clips) \
	f(short,			barrel_count) \
	f(bool,				no_cock) \
	f(int8,				bullet_chamber) \
	f(bool,				ai_infiniteAmmo) \
	f(bool,				knocks_target) \
	f(int8,				bullet_pierceability_modifier) \
	f(bool,				is_silenced) \
	f(bool,				muzzleFromFiringLocator) \
	f(bool,				autoReload) \
	f(bool,				autoSwitch) \
	f(bool,				ignore_damage_falloff) \
	f(bool,				laser_beam_uses_spread)

#define FIRE_PARAMS_ARRAYS(f) \
	f(ItemString, 2, helper)

	REGISTER_STRUCT_WITH_ARRAYS(FIRE_PARAMS_MEMBERS, FIRE_PARAMS_ARRAYS, SFireParams)

	// this is special! It gets filled out later so we don't want to use it in the comparison. 
	// everything else should always be registered with the macros
	mutable int	hitTypeId;
};

struct SInitialiseParams
{
	SInitialiseParams() : enabled(false) {};

	void GetMemoryUsage(ICrySizer* s) const
	{
		s->AddObject(modeName);
		s->AddObject(modeType);
	}
	
	ItemString modeName;
	ItemString modeType;
	bool enabled;
};

struct SThermalVisionParams
{
	SThermalVisionParams() 
	{ 
		Reset(XmlNodeRef(NULL)); 
	};

	void Reset(const XmlNodeRef& paramsNode, bool defaultInit=true);
	void GetMemoryUsage(ICrySizer* s) const {}

#define THERMAL_PARAMS_MEMBERS(f) \
	f(float,	weapon_shootHeatPulse) \
	f(float,	weapon_shootHeatPulseTime) \
	f(float,	owner_shootHeatPulse) \
	f(float,	owner_shootHeatPulseTime) 

	REGISTER_STRUCT(THERMAL_PARAMS_MEMBERS, SThermalVisionParams)
};


struct SFireModeAnimationParams
{
	SFireModeAnimationParams() 
	{
		Reset(XmlNodeRef(NULL));
	}

	void Reset(const XmlNodeRef& paramsNode, bool defaultInit = true);

	void GetMemoryUsage(ICrySizer* s) const {}

#define ANIMATION_PARAMS_MEMBERS(f) \
	f(bool,	useBaseModifier) 

	REGISTER_STRUCT(ANIMATION_PARAMS_MEMBERS, SFireModeAnimationParams)
};

struct SSpammerParams
{
	SSpammerParams()
	{
		Reset(XmlNodeRef(NULL));
	}

	void Reset(const XmlNodeRef& paramsNode, bool defaultInit = true);
	void GetMemoryUsage(ICrySizer* s) const {}

#define SPAMMER_PARAMS_MEMBERS(f) \
	f(int,		maxNumRockets) \
	f(int,		loadInRate) \
	f(int,		burstRate) \
	f(float,	targetingTolerance)  \
	f(float,	minLockOnDistance)  \
	f(float,	maxLockOnDistance)  \
	f(bool,		targetingFlatMode)

	REGISTER_STRUCT(SPAMMER_PARAMS_MEMBERS, SSpammerParams)
};

struct SBowParams
{
public:
	SBowParams()
	{
		Reset(XmlNodeRef(NULL));
	}

	void Reset(const XmlNodeRef& paramsNode, bool defaultInit = true);
	void GetMemoryUsage(ICrySizer* s) const {}

#define BOW_PARAMS_MEMBERS(f) \
	f(float,	chargeTime) \
	f(float,	minChargePercent) \
	f(float,	minChargeDamage) \
	f(float,	minChargeSpeed) \
	f(float,	maxChargeDamage) \
	f(float,	maxChargeSpeed) \
	f(float,	stringStrength) \
	f(float,	pinChargeTime) \
	f(bool,		impactDamage)

	REGISTER_STRUCT(BOW_PARAMS_MEMBERS, SBowParams)
};

typedef std::vector<IFireModePluginParams*> TPluginParamsVector;

#define INITIALISE_FIREMODE_PARAMS(structType, structName) , structName(g_pGame->GetGameSharedParametersStorage()->Store##structType(params.structName))
#define INSTANCE_FIREMODE_UNPACKED(structType, structName) structType structName;
#define INSTANCE_FIREMODE_PACKED(structType, structName) const structType& structName;
#define FIREMODE_PARAM_MEMBER_STRUCTS(f) \
	f(SAimLookParameters,				aimLookParams) \
	f(SFireParams,							fireparams) \
	f(STracerParams,						tracerparams) \
	f(SEffectParams,						muzzleflash) \
	f(SEffectParams,						muzzlesmoke) \
	f(SEffectParams,						muzzlebeam) \
	f(SEffectParams,						spinup) \
	f(SRecoilParams,						recoilparamsCopy) \
	f(SRecoilHints,							recoilHints) \
	f(SProceduralRecoilParams,	proceduralRecoilParams) \
	f(SSpreadParams,						spreadparamsCopy) \
	f(SShotgunParams,						shotgunparams) \
	f(SBurstParams,							burstparams) \
	f(SThrowParams,							throwparams) \
	f(SEffectParams,						chargeeffect) \
	f(SChargeParams,						chargeparams) \
	f(SRapidParams,							rapidparams) \
	f(SLTagGrenades,						lTagGrenades) \
	f(SPlantParams,							plantparams) \
	f(SThermalVisionParams,			thermalVisionParams) \
	f(SFireModeAnimationParams,	animationParams) \
	f(SSpammerParams,						spammerParams) \
	f(SBowParams,								bowParams) \
	f(SWeaponStatsData,					weaponStatChangesParams) \
	f(SAIDescriptor,					aiDescriptor) \
	f(SHazardDescriptor,				hazardDescriptor)

struct SFireModeParamsUnpacked
{
	SFireModeParamsUnpacked(){};

	IFireModePluginParams* FindPluginOfType(const CGameTypeInfo* pluginType);
	const IFireModePluginParams* FindPluginOfType(const CGameTypeInfo* pluginType) const;

	FIREMODE_PARAM_MEMBER_STRUCTS(INSTANCE_FIREMODE_UNPACKED)

	TPluginParamsVector				pluginParams;
};


struct SFireModeParams
{
	explicit SFireModeParams(const SFireModeParamsUnpacked& params);

	void GetMemoryUsage(ICrySizer* s) const
	{
		s->AddObject(fireparams);
		s->AddObject(tracerparams);
		s->AddObject(muzzleflash);
		s->AddObject(muzzlesmoke);
		s->AddObject(muzzlebeam);
		s->AddObject(spinup);
		s->AddObject(recoilparamsCopy);
		s->AddObject(recoilHints);

		s->AddObject(proceduralRecoilParams);
		s->AddObject(spreadparamsCopy);

		s->AddObject(shotgunparams);
		s->AddObject(burstparams);
		s->AddObject(throwparams);
		s->AddObject(chargeeffect);
		s->AddObject(chargeparams);
		s->AddObject(rapidparams);
		s->AddObject(lTagGrenades);
		s->AddObject(plantparams);

		s->AddObject(thermalVisionParams);
		s->AddObject(animationParams);
		s->AddObject(weaponStatChangesParams);
		s->AddObject(spammerParams);
		s->AddObject(bowParams);

		s->AddContainer(pluginParams);

		const int numPlugins = pluginParams.size();

		for (int i = 0; i < numPlugins; i++)
		{
			s->AddObject(pluginParams[i]);
		}
	}

	void Release();

	void CacheAmmoResources() const;
	void CacheResources();

	FIREMODE_PARAM_MEMBER_STRUCTS(INSTANCE_FIREMODE_PACKED)

	TPluginParamsVector				pluginParams;
};

struct SAccessoryFireModeParams
{
	SAccessoryFireModeParams() : pAlteredParams(NULL) { pAccessories[0] = pAccessories[1] = pAccessories[2] = pAccessories[3] = NULL; }

	IEntityClass* pAccessories[ITEM_MAX_NUM_ACCESSORIES];
	SFireModeParams* pAlteredParams;

	void GetMemoryUsage(ICrySizer* s) const
	{
		s->AddObject(pAlteredParams);
	}
};

struct SParentFireModeParams
{
	SParentFireModeParams() : pBaseFireMode(NULL) {};

	typedef std::vector<SAccessoryFireModeParams> TAccessoryFireModeParamsVector;

	void GetMemoryUsage(ICrySizer *pSizer) const
	{
		pSizer->AddObject(pBaseFireMode);
		pSizer->AddObject(initialiseParams);
		pSizer->AddObject(accessoryChangedParams);
	}

	void CacheResources();
	void Release();
	const SFireModeParams* FindAccessoryFireModeParams(IEntityClass* pClass, IEntityClass* pClassTwo = NULL, IEntityClass* pClassThree = NULL, IEntityClass* pClassFour = NULL) const;


	SFireModeParams* pBaseFireMode;
	SInitialiseParams	initialiseParams;
	TAccessoryFireModeParamsVector	accessoryChangedParams;
};

#endif //__FIREMODEPARAMS_H__
