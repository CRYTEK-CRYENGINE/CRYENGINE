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

#include "StdAfx.h"
#include "ItemParamsRegistrationOperators.h"
#include "FireModeParams.h"

#include "AmmoParams.h"
#include "WeaponSystem.h"
#include "GameXmlParamReader.h"



IMPLEMENT_OPERATORS(AI_WEAPON_DESCRIPTOR_PARAMS_MEMBERS, AIWeaponDescriptor)
IMPLEMENT_OPERATORS(HAZARD_WEAPON_DESCRIPTOR_PARAMS_MEMBERS, HazardWeaponDescriptor)
IMPLEMENT_OPERATORS(AI_DESCRIPTOR_PARAMS_MEMBERS, SAIDescriptor)
IMPLEMENT_OPERATORS(HAZARD_DESCRIPTOR_PARAMS_MEMBERS, SHazardDescriptor)
IMPLEMENT_OPERATORS(BOW_PARAMS_MEMBERS, SBowParams)
IMPLEMENT_OPERATORS(SPAMMER_PARAMS_MEMBERS, SSpammerParams)
IMPLEMENT_OPERATORS(ANIMATION_PARAMS_MEMBERS, SFireModeAnimationParams)
IMPLEMENT_OPERATORS(THERMAL_PARAMS_MEMBERS, SThermalVisionParams)
IMPLEMENT_OPERATORS_WITH_ARRAYS(FIRE_PARAMS_MEMBERS, FIRE_PARAMS_ARRAYS, SFireParams)
IMPLEMENT_OPERATORS_WITH_ARRAYS(TRACER_PARAMS_MEMBERS, TRACER_PARAMS_ARRAYS, STracerParams)
IMPLEMENT_OPERATORS(SHOTGUN_PARAMS_MEMBERS, SShotgunParams)
IMPLEMENT_OPERATORS(BURST_PARAMS_MEMBERS, SBurstParams)
IMPLEMENT_OPERATORS(THROW_PARAMS_MEMBERS, SThrowParams)
IMPLEMENT_OPERATORS(CHARGE_PARAMS_MEMBERS, SChargeParams)
IMPLEMENT_OPERATORS(RAPID_PARAMS_MEMBERS, SRapidParams)
IMPLEMENT_OPERATORS_WITH_ARRAYS(LTAG_GRENADE_MEMBERS, LTAG_GRENADE_ARRAYS, SLTagGrenades);
IMPLEMENT_OPERATORS(PLANT_PARAMS_MEMBERS, SPlantParams)



//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////


void SAIDescriptor::Reset(const XmlNodeRef& paramsNode, bool defaultInit)
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);

	if (paramsNode)
	{
		paramsNode->getAttr("speed", descriptor.fSpeed);
		paramsNode->getAttr("damage_radius", descriptor.fDamageRadius);
		paramsNode->getAttr("charge_time", descriptor.fChargeTime);
		paramsNode->getAttr("burstBulletCountMin", descriptor.burstBulletCountMin);
		paramsNode->getAttr("burstBulletCountMax", descriptor.burstBulletCountMax);
		paramsNode->getAttr("burstPauseTimeMin", descriptor.burstPauseTimeMin);
		paramsNode->getAttr("burstPauseTimeMax", descriptor.burstPauseTimeMax);
		paramsNode->getAttr("singleFireTriggerTime", descriptor.singleFireTriggerTime);
		paramsNode->getAttr("spreadRadius", descriptor.spreadRadius);
		paramsNode->getAttr("coverFireTime", descriptor.coverFireTime);
		paramsNode->getAttr("sweep_width", descriptor.sweepWidth);
		paramsNode->getAttr("sweep_frequency", descriptor.sweepFrequency);
		paramsNode->getAttr("draw_time", descriptor.drawTime);
		paramsNode->getAttr("projectileGravity", descriptor.projectileGravity);
		paramsNode->getAttr("pressureMultiplier", descriptor.pressureMultiplier);
		paramsNode->getAttr("lobCriticalDistance", descriptor.lobCriticalDistance);
		paramsNode->getAttr("preferredHeight", descriptor.preferredHeight);
		paramsNode->getAttr("closeDistance", descriptor.closeDistance);
		descriptor.preferredHeightForCloseDistance = descriptor.preferredHeight;
		paramsNode->getAttr("preferredHeightForCloseDistance", descriptor.preferredHeightForCloseDistance);
		paramsNode->getAttr("maxAcceptableDistanceFromTarget", descriptor.maxAcceptableDistanceFromTarget);
		paramsNode->getAttr("minimumDistanceFromFriends", descriptor.minimumDistanceFromFriends);

		descriptor.smartObjectClass = paramsNode->getAttr("smartobject_class");
		descriptor.firecmdHandler = paramsNode->getAttr("handler");

		int	signalOnShoot(0);
		paramsNode->getAttr("signal_on_shoot", signalOnShoot);

		descriptor.bSignalOnShoot = signalOnShoot != 0;
	}
}


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////


void SHazardDescriptor::Reset(const XmlNodeRef& paramsNode, bool defaultInit)
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);

	m_DefinedFlag = false;

	if (paramsNode)
	{
		m_DefinedFlag = true;

		paramsNode->getAttr("maxHazardDistance", descriptor.maxHazardDistance);				
		paramsNode->getAttr("hazardRadius", descriptor.hazardRadius);
		paramsNode->getAttr("startPosNudgeOffset", descriptor.startPosNudgeOffset);
		paramsNode->getAttr("maxHazardApproxPosDeviation", descriptor.maxHazardApproxPosDeviation);
		paramsNode->getAttr("maxHazardApproxAngleDeviationDeg", descriptor.maxHazardApproxAngleDeviationDeg);

		CRY_ASSERT_MESSAGE(descriptor.maxHazardDistance >= 0.0f, "maxHazardDistance must be >= 0.0f!");
		CRY_ASSERT_MESSAGE(descriptor.hazardRadius >= 0.0f, "hazardRadius must be >= 0.0f!");
		CRY_ASSERT_MESSAGE(descriptor.maxHazardApproxPosDeviation >= 0.0f, "maxHazardApproxPosDeviation must be >= 0.0f!");
		CRY_ASSERT_MESSAGE(descriptor.maxHazardApproxAngleDeviationDeg >= 0.0f, "maxHazardApproxAngleDeviationDeg must be >= 0.0f!");
	}
}


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SMeleeParams::SCloseRange::Reset( const XmlNodeRef& paramsNode, bool defaultInit /*= true*/ )
{
	if (defaultInit)
	{
		range = 1.75f;
		delay = 0.5f;
		duration = 0.5f;
		impulse_ai_to_player = -1.f;
	}

	if (paramsNode)
	{
		CGameXmlParamReader reader(paramsNode);

		reader.ReadParamValue<float>("range", range);
		reader.ReadParamValue<float>("delay", delay);
		reader.ReadParamValue<float>("duration", duration);
		reader.ReadParamValue<float>("impulse_ai_to_player", impulse_ai_to_player);
	}
}

void SMeleeParams::SPowerParams::Reset(const XmlNodeRef& paramsNode, bool defaultInit/* =true */)
{
	if (paramsNode)
	{
		CGameXmlParamReader reader(paramsNode);

		reader.ReadParamValue<float>("delay", delay);
		reader.ReadParamValue<float>("duration", duration);
	}
}

void SMeleeParams::Reset( const XmlNodeRef& paramsNode, bool defaultInit/*=true*/ )
{
	if (defaultInit)
	{
		helper = "";
		range = 1.75f;
		target_range_mult = 1.5f;
		damage = 32;
		damage_ai = 32;
		slide_damage = 32;
		impulse = 50.0f;
		impulse_actor = 200.f;
		impulse_ai_to_player = -1.0f;
		impulse_vehicle = 50.0f;
		delay = 0.5f;
		aiDelay = 0.5f;
		duration = 0.5f;
		knockdown_chance = 0.0f;
		impulse_up_percentage = 0.0f;
		use_melee_weapon_delay = -1.0f;
		weapon_restore_delay = 0.f;
		m_FPSignalId = INVALID_AUDIOSIGNAL_ID;
		m_3PSignalId = INVALID_AUDIOSIGNAL_ID;
		is_melee_weapon = false;
		trigger_client_reaction = false;		
	}

	CGameXmlParamReader reader(paramsNode);

	if (paramsNode)
	{
		helper = reader.ReadParamValue("helper", helper.c_str());
		reader.ReadParamValue<float>("range", range);
		reader.ReadParamValue<float>("target_range_mult", target_range_mult);
		reader.ReadParamValue<short>("damage", damage);
		reader.ReadParamValue<short>("damage_ai", damage_ai);
		reader.ReadParamValue<short>("slide_damage", slide_damage);
		reader.ReadParamValue<float>("impulse", impulse);
		reader.ReadParamValue<float>("impulse_actor", impulse_actor);
		reader.ReadParamValue<float>("impulse_ai_to_player", impulse_ai_to_player);
		reader.ReadParamValue<float>("impulse_vehicle", impulse_vehicle);
		reader.ReadParamValue<float>("aiDelay", aiDelay);
		reader.ReadParamValue<float>("delay", delay);
		reader.ReadParamValue<float>("duration", duration);
		reader.ReadParamValue<float>("knockdown_chance", knockdown_chance);
		reader.ReadParamValue<float>("impulse_up_percentage", impulse_up_percentage);
		reader.ReadParamValue<float>("use_melee_weapon_delay", use_melee_weapon_delay, -1.0f);
		reader.ReadParamValue<float>("weapon_restore_delay", weapon_restore_delay, 0.0f);
		reader.ReadParamValue<bool>("is_melee_weapon", is_melee_weapon, false);
		reader.ReadParamValue<bool>("trigger_client_reaction", trigger_client_reaction);

		const char *NameFPSignal=NULL, *Name3PSignal=NULL;
		NameFPSignal = reader.ReadParamValue("FPSignal", "Melee_FP");
		Name3PSignal = reader.ReadParamValue("3PSignal", "Melee");

		m_FPSignalId = g_pGame->GetGameAudio()->GetSignalID(NameFPSignal);
		m_3PSignalId = g_pGame->GetGameAudio()->GetSignalID(Name3PSignal);
	}

	closeAttack.Reset(reader.FindFilteredChild("CloseAttack"), defaultInit);

	//Init power attack values (default to normal attack ones)
	powerAttack.duration = duration;
	powerAttack.delay = delay;
	powerAttack.Reset(reader.FindFilteredChild("PowerAttack"), defaultInit);
}

void SMeleeActions::Reset(const XmlNodeRef& paramsNode, bool defaultInit)
{
	if(defaultInit)
	{
		attack = "melee";
		attack_closeRange = "meleeShort";
		hit = "hit";
		attack_multipart = "melee_multipart";
	}

	if(paramsNode)
	{
		CGameXmlParamReader reader(paramsNode);

		attack = reader.ReadParamValue("attack", attack.c_str());
		attack_closeRange = reader.ReadParamValue("attack_closeRange", attack_closeRange.c_str());
		hit = reader.ReadParamValue("hit", hit.c_str());
		attack_multipart = reader.ReadParamValue("attack_multipart", attack_multipart.c_str());
	}
}

SMeleeTags::STagParams::STagParams( const char* pTag, const char* pHitType )
	:
crcTagID(0), crcHitType(0), tagState(TAG_STATE_EMPTY), hitType(-1)
{
	if( pTag )
	{
		crcTagID = CCrc32::ComputeLowercase( pTag );
	}
	if( pHitType )
	{
		crcHitType = CCrc32::ComputeLowercase( pHitType );
	}
}

void SMeleeTags::Reset( const XmlNodeRef& tagsNode, bool defaultInit /* = true */ )
{
	if(tagsNode)
	{
		CGameXmlParamReader reader(tagsNode);

		const int numTags = reader.GetUnfilteredChildCount();

		for( int i=0; i<numTags; ++i )
		{
			const XmlNodeRef& child = reader.GetFilteredChildAt(i);

			const char* pName = NULL;
			child->getAttr( "name", &pName );

			STagParams tagParams;
			tagParams.crcTagID = CCrc32::ComputeLowercase( pName );

			if( pName )
			{
				const char* pHitType = NULL;
				child->getAttr( "hit_type", &pHitType );
				if( pHitType )
				{
					tagParams.crcHitType = CCrc32::ComputeLowercase( pHitType );
				}

				if( strstr( pName, "combo_left" ) )
				{
					tag_params_combo_left.push_back( tagParams );
				}
				else if( strstr( pName, "combo_right" ) )
				{
					tag_params_combo_right.push_back( tagParams );
				}
				else if( strstr( pName, "killingblow" ) )
				{
					tag_params_combo_killingblow.push_back( tagParams );
				}
				else
				{
					CryLog( "[melee] Unknown tag in melee container <%s> Should contain 'left, right or killingblow'.", pName );
				}
			}
		}
	}
}

void SMeleeTags::Resolve( const CTagDefinition* pTagDefinition, FragmentID fragID, const IActionController* piActionController ) const
{
	std::for_each( tag_params_combo_left.begin(),					tag_params_combo_left.end(),				FResolve( pTagDefinition, fragID, piActionController ) );
	std::for_each( tag_params_combo_right.begin(),				tag_params_combo_right.end(),				FResolve( pTagDefinition, fragID, piActionController ) );
	std::for_each( tag_params_combo_killingblow.begin(),	tag_params_combo_killingblow.end(), FResolve( pTagDefinition, fragID, piActionController ) );
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

SFireModeParams::SFireModeParams(const SFireModeParamsUnpacked& params)
: pluginParams(params.pluginParams)
FIREMODE_PARAM_MEMBER_STRUCTS(INITIALISE_FIREMODE_PARAMS)
{
}

void SFireModeParams::CacheAmmoResources() const
{
	if (fireparams.spawn_ammo_class)
	{
		const SAmmoParams* pAmmoParams = g_pGame->GetWeaponSystem()->GetAmmoParams(fireparams.spawn_ammo_class);
		if (pAmmoParams)
		{
			pAmmoParams->CacheResources();
		}
	}

	if (plantparams.ammo_type_class)
	{
		const SAmmoParams* pAmmoParams = g_pGame->GetWeaponSystem()->GetAmmoParams(plantparams.ammo_type_class);
		if (pAmmoParams)
		{
			pAmmoParams->CacheResources();
		}
	}
}

void SFireModeParams::CacheResources()
{
	CGameRules* pGameRules = g_pGame->GetGameRules();

	if(pGameRules)
	{
		fireparams.hitTypeId = pGameRules->GetHitTypeId(fireparams.hit_type.c_str());
	}

	tracerparams.PreLoadAssets();
	muzzleflash.PreLoadAssets();
	muzzlesmoke.PreLoadAssets();
	muzzlebeam.PreLoadAssets();
	spinup.PreLoadAssets();
	chargeeffect.PreLoadAssets();
	lTagGrenades.PreLoadAssets();

	const int numPlugins = pluginParams.size();

	for (int i = 0; i < numPlugins; i++)
	{
		pluginParams[i]->PreLoadAssets();
	}

	CacheAmmoResources();
}

void SFireModeParams::Release()
{
	const int numPlugins = pluginParams.size();

	for (int i = 0; i < numPlugins; i++)
	{
		SAFE_DELETE(pluginParams[i]);
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

IFireModePluginParams* SFireModeParamsUnpacked::FindPluginOfType(const CGameTypeInfo* pluginType)
{
	const int numPlugins = pluginParams.size();

	for (int i = 0; i < numPlugins; i++)
	{
		if(pluginParams[i]->GetPluginType() == pluginType)
		{
			return pluginParams[i];
		}
	}

	return NULL;
}

const IFireModePluginParams* SFireModeParamsUnpacked::FindPluginOfType(const CGameTypeInfo* pluginType) const
{
	const int numPlugins = pluginParams.size();

	for (int i = 0; i < numPlugins; i++)
	{
		if(pluginParams[i]->GetPluginType() == pluginType)
		{
			return pluginParams[i];
		}
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SPlantParams::Reset( const XmlNodeRef& paramsNode, bool defaultInit/*=true*/ )
{
	CryFixedStringT<32> ammo_type = "";

	if (defaultInit)
	{
		clip_size = 3;
		damage = 100;
		delay = 0.25f;
		selectDelay = 1.0f;
		simple = false;
		is_silenced = false;
	}

	if (paramsNode)
	{
		CGameXmlParamReader reader(paramsNode);

		ammo_type = reader.ReadParamValue("ammo_type", ammo_type.c_str());
		reader.ReadParamValue<int>("clip_size", clip_size);	
		reader.ReadParamValue<int>("damage", damage);	
		reader.ReadParamValue<float>("delay", delay);	
		reader.ReadParamValue<float>("selectDelay", selectDelay);	
		reader.ReadParamValue<bool>("simple", simple);	
		reader.ReadParamValue<bool>("is_silenced", is_silenced);
	}

	if (defaultInit || !ammo_type.empty())
	{
		ammo_type_class = gEnv->pEntitySystem->GetClassRegistry()->FindClass(ammo_type.c_str());
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SLTagGrenades::Reset( const XmlNodeRef& paramsNode, bool defaultInit /*= true*/ )
{
	if (defaultInit)
	{
		grenades[ELTAGGrenadeType_STICKY] = "";
		grenades[ELTAGGrenadeType_RICOCHET] = "";
	}

	if (paramsNode)
	{
		CGameXmlParamReader reader(paramsNode);

		grenades[ELTAGGrenadeType_STICKY] = reader.ReadParamValue("sticky", grenades[ELTAGGrenadeType_STICKY].c_str());
		grenades[ELTAGGrenadeType_RICOCHET] = reader.ReadParamValue("ricochet", grenades[ELTAGGrenadeType_RICOCHET].c_str());
	}
}

void SLTagGrenades::PreLoadAssets() const 
{
	CItemResourceCache& itemResourceCache = g_pGame->GetGameSharedParametersStorage()->GetItemResourceCache();

	for (int i = 0; i < ELTAGGrenadeType_LAST; ++i)
	{
		if (!grenades[i].empty())
		{
			itemResourceCache.GetItemGeometryCache().CacheGeometry(grenades[i].c_str(), false);
		}
	}
}
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SRapidParams::Reset( const XmlNodeRef& paramsNode, bool defaultInit/*=true*/ )
{
	if (defaultInit)
	{
		min_speed = 1.5f;
		max_speed = 3.0f;
		acceleration = 3.0f;
		deceleration = 3.0f;
		barrel_attachment = "";
		min_firingTimeToStop = 0.1f;
	}

	if (paramsNode)
	{
		CGameXmlParamReader reader(paramsNode);

		reader.ReadParamValue<float>("min_speed", min_speed);
		reader.ReadParamValue<float>("max_speed", max_speed);
		reader.ReadParamValue<float>("acceleration", acceleration);
		reader.ReadParamValue<float>("deceleration", deceleration);
		barrel_attachment = reader.ReadParamValue("barrel_attachment", barrel_attachment.c_str());
		reader.ReadParamValue<float>("min_firingTimeToStop", min_firingTimeToStop);
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SChargeParams::Reset( const XmlNodeRef& paramsNode, bool defaultInit/*=true*/ )
{
	if (defaultInit)
	{
		time = 0.5f;
		max_charges = 1;
		shoot_on_stop = false;
		reset_spinup = false;
	}

	if (paramsNode)
	{
		CGameXmlParamReader reader(paramsNode);

		reader.ReadParamValue<float>("time", time);
		reader.ReadParamValue<int>("max_charges", max_charges);
		reader.ReadParamValue<bool>("shoot_on_stop", shoot_on_stop);
		reader.ReadParamValue<bool>("reset_spinup", reset_spinup);
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SThrowParams::Reset( const XmlNodeRef& paramsNode, bool defaultInit/*=true*/ )
{
	if (defaultInit)
	{
		throw_delay = 0.15f;
		prime_delay = 0.15f;
		prime_timer = true;
		prime_enabled = true;
		crosshairblink_enabled = false;
		display_trajectory = true;
		neutralSuffixAG = "_neutral";
	}

	if (paramsNode)
	{
		CGameXmlParamReader reader(paramsNode);

		reader.ReadParamValue<float>("throw_delay", throw_delay);
		reader.ReadParamValue<float>("prime_delay", prime_delay);
		reader.ReadParamValue<bool>("prime_timer", prime_timer);
		reader.ReadParamValue<bool>("prime_enabled", prime_enabled);
		reader.ReadParamValue<bool>("crosshairblink", crosshairblink_enabled);
		reader.ReadParamValue<bool>("display_trajectory", display_trajectory);
		neutralSuffixAG = reader.ReadParamValue("neutralSuffixAG", neutralSuffixAG.c_str());
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SBurstParams::Reset( const XmlNodeRef& paramsNode, bool defaultInit/*=true*/ )
{
	if (defaultInit)
	{
		nshots = 3;
		rate = 32;
		useBurstSoundParam = false;
		multiPipe = false;
		multiPipeHelper = "";
		spreadAngle = 0;
	}

	if (paramsNode)
	{
		CGameXmlParamReader reader(paramsNode);

		reader.ReadParamValue<short>("nshots", nshots);
		reader.ReadParamValue<short>("rate", rate);
		reader.ReadParamValue<bool>("useBurstSoundParam", useBurstSoundParam);
		reader.ReadParamValue<bool>("multiPipe", multiPipe);
		multiPipeHelper = reader.ReadParamValue("multiPipeHelper", multiPipeHelper.c_str());
		reader.ReadParamValue<short>("spreadAngle",spreadAngle);
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SShotgunParams::Reset( const XmlNodeRef& paramsNode, bool defaultInit/*=true*/ )
{
	if (defaultInit)
	{
		loadShellOnEndModes = 0;

		pellets = 10;
		pelletdamage = 20;
		spread = 0.1f;
		npc_additional_damage = 0;
		partial_reload = true;
		fully_automated = false;
		reloadBreakTime = 0.3f;
		endReloadSpeedOverride = 1.75f;
	}

	if (paramsNode)
	{
		CGameXmlParamReader reader(paramsNode);

		reader.ReadParamValue<short>("pellets", pellets);
		reader.ReadParamValue<short>("pelletdamage", pelletdamage);
		reader.ReadParamValue<float>("spread", spread);
		reader.ReadParamValue<short>("npc_additional_damage", npc_additional_damage);
		reader.ReadParamValue<bool>("partial_reload", partial_reload);
		reader.ReadParamValue<bool>("fully_automated", fully_automated);
		reader.ReadParamValue<float>("reloadBreakTime", reloadBreakTime);
		reader.ReadParamValue<float>("endReloadSpeedOverride", endReloadSpeedOverride);
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void STracerParams::Reset( const XmlNodeRef& paramsNode, bool defaultInit/*=true*/ )
{
	if (defaultInit)
	{
		geometry = "";
		effect = "";
		geometryFP = "";
		effectFP = "";
		speed = 200.0f;
		speedFP = 400.0f;
		thickness = 1.f;
		thicknessFP = 1.f;
		lifetime = 1.5f;
		frequency =	1;
		delayBeforeDestroy = 0.0f;
		slideFraction = 0.5f;
		helper[0] =	"";
		helper[1] =	"";
	}

	if (paramsNode)
	{
		CGameXmlParamReader reader(paramsNode);

		geometry = reader.ReadParamValue("geometry", geometry.c_str());
		effect = reader.ReadParamValue("effect", effect.c_str());
		geometryFP = reader.ReadParamValue("geometryFP", geometryFP.c_str());
		effectFP = reader.ReadParamValue("effectFP", effectFP.c_str());
	
		reader.ReadParamValue<float>("speed", speed);
		reader.ReadParamValue<float>("speedFP", speedFP);

		reader.ReadParamValue<float>("thickness", thickness);
		reader.ReadParamValue<float>("thicknessFP", thicknessFP);

		reader.ReadParamValue<float>("lifetime", lifetime);
		reader.ReadParamValue<int>("frequency", frequency);
		reader.ReadParamValue<float>("delayBeforeDestroy", delayBeforeDestroy);
		reader.ReadParamValue<float>("slideFraction", slideFraction);

		helper[0] = reader.ReadParamValue("helper_fp", helper[0].c_str());
		helper[1] = reader.ReadParamValue("helper_tp", helper[1].c_str());
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SFireParams::Reset(const XmlNodeRef& paramsNode, bool defaultInit/*=true*/)
{
	CryFixedStringT<32> ammo_type = "";
	CryFixedStringT<32> ammo_spawn_type = "";

	if (defaultInit)
	{
		suffix = "";
		suffixAG = "";
		tag = "";
		rate = 400;
		fake_fire_rate = 0;
		minimum_ammo_count = 0;
		clip_size = 30;
		max_clips = 20;
		hit_type = "bullet";

		hitTypeId = 0;

		changeFMFireDelayFraction = 0.0f;
		endReloadFraction = 1.0f;
		fillAmmoReloadFraction = 1.0f;
		autoReload = true;
		autoSwitch = true;
		stabilization = 0.2f;
		speed_override = 0.0f;
		bullet_chamber = 0;
		damage = 32;
		damage_drop_per_meter = 0.0f;
		damage_drop_min_distance = 0.0f;
		damage_drop_min_damage = 0.0f;
		point_blank_amount = 1.0f;
		point_blank_distance = 0.0f;
		point_blank_falloff_distance = 0.0f;
		ai_infiniteAmmo = false;
		npc_additional_damage = 0;
		ai_reload_time = 2.5f;
		crosshair = eHCH_Normal;
		no_cock	= true;
		helper[0] = "";
		helper[1] = "";
		barrel_count = 1;

		spin_up_time = 0.0f;
		fire_anim_damp = 1.0f;
		ironsight_fire_anim_damp = 1.0f;
		holdbreath_fire_anim_damp = 0.5f;
		holdbreath_ffeedback_damp = 0.5f;

		knocks_target = false;
		min_damage_for_knockDown = 0;
		knockdown_chance_leg = 0;
		min_damage_for_knockDown_leg = 0;
		bullet_pierceability_modifier = 0;
		is_silenced = false;
		ignore_damage_falloff = false;
		laser_beam_uses_spread = false;

		muzzleFromFiringLocator = false;
	}

	if (paramsNode)
	{
		CGameXmlParamReader reader(paramsNode);

		suffix = reader.ReadParamValue("suffix", suffix.c_str());
		suffixAG = reader.ReadParamValue("suffixAG", suffixAG.c_str());
		tag = reader.ReadParamValue("tag", tag.c_str());
		reader.ReadParamValue<short>("rate", rate);
		reader.ReadParamValue<short>("fake_fire_rate", fake_fire_rate);
		reader.ReadParamValue<short>("minimum_ammo_count", minimum_ammo_count);
		reader.ReadParamValue<short>("clip_size", clip_size);
		reader.ReadParamValue<short>("max_clips", max_clips);
		hit_type = reader.ReadParamValue("hit_type", hit_type.c_str());
		ammo_type = reader.ReadParamValue("ammo_type", ammo_type.c_str());
		ammo_spawn_type = reader.ReadParamValue("ammo_spawn_type", ammo_spawn_type.c_str());

		reader.ReadParamValue<float>("changeFMFireDelayFraction", changeFMFireDelayFraction);
		reader.ReadParamValue<float>("endReloadFraction", endReloadFraction);
		reader.ReadParamValue<float>("fillAmmoReloadFraction", fillAmmoReloadFraction);
		reader.ReadParamValue<bool>("autoReload", autoReload);
		reader.ReadParamValue<bool>("autoSwitch", autoSwitch);
		reader.ReadParamValue<float>("stabilization", stabilization);
		reader.ReadParamValue<float>("speed_override", speed_override);

#ifndef _RELEASE
		if (!defaultInit && endReloadFraction < fillAmmoReloadFraction)
		{
			const char* weaponName = "";
			for (XmlNodeRef currentNode = paramsNode;;currentNode=currentNode->getParent())
			{
				if (currentNode->getParent())
					continue;
				weaponName = currentNode->getAttr("name");
				break;
			}
			XmlNodeRef fireModeNode = paramsNode->getParent();
			const char* fireModeName = fireModeNode->getAttr("name");
			gEnv->pLog->LogWarning(
				"endReloadFraction is smaller than fillAmmoReloadFraction on '%s' at '%s'",
				weaponName, fireModeName);
		}
#endif
		endReloadFraction = max(endReloadFraction, fillAmmoReloadFraction);
		
		int bulletChamber = 0;
		if (reader.ReadParamValue<int>("bullet_chamber", bulletChamber))
		{
			bullet_chamber = (uint8)bulletChamber;
		}
		
		reader.ReadParamValue<int>("damage", damage);
		reader.ReadParamValue<float>("damage_drop_per_meter", damage_drop_per_meter);
		reader.ReadParamValue<float>("damage_drop_min_distance", damage_drop_min_distance);
		reader.ReadParamValue<float>("damage_drop_min_damage", damage_drop_min_damage);
		reader.ReadParamValue<float>("point_blank_amount", point_blank_amount);
		reader.ReadParamValue<float>("point_blank_distance", point_blank_distance);
		reader.ReadParamValue<float>("point_blank_falloff_distance", point_blank_falloff_distance);

		reader.ReadParamValue<bool>("ai_infiniteAmmo", ai_infiniteAmmo);
		reader.ReadParamValue<int>("npc_additional_damage", npc_additional_damage);

		reader.ReadParamValue<float>("ai_reload_time", ai_reload_time);

		int crosshairType = (ECrosshairTypes)eHCH_Normal;
		if (reader.ReadParamValue<int>("crosshair", crosshairType))
		{
			crosshair = (ECrosshairTypes)crosshairType;
		}

		reader.ReadParamValue<bool>("no_cock", no_cock);

		helper[0] = reader.ReadParamValue("helper_fp", helper[0].c_str());
		helper[1] = reader.ReadParamValue("helper_tp", helper[1].c_str());

		reader.ReadParamValue<short>("barrel_count", barrel_count);

		reader.ReadParamValue<float>("spin_up_time", spin_up_time);
		reader.ReadParamValue<float>("fire_anim_damp", fire_anim_damp);
		reader.ReadParamValue<float>("ironsight_fire_anim_damp", ironsight_fire_anim_damp);
		reader.ReadParamValue<float>("holdbreath_fire_anim_damp", holdbreath_fire_anim_damp);
		reader.ReadParamValue<float>("holdbreath_ffeedback_damp", holdbreath_ffeedback_damp);

		reader.ReadParamValue<bool>("knocks_target", knocks_target);
		reader.ReadParamValue<float>("min_damage_for_knockDown", min_damage_for_knockDown);
		reader.ReadParamValue<int>("knockdown_chance_leg", knockdown_chance_leg);
		reader.ReadParamValue<float>("min_damage_for_knockDown_leg", min_damage_for_knockDown_leg);

		int pierceabilityMod = 0;
		reader.ReadParamValue<int>("bullet_pierceability_modifier", pierceabilityMod);
		bullet_pierceability_modifier = (int8)pierceabilityMod;

		reader.ReadParamValue<bool>("is_silenced", is_silenced);
		reader.ReadParamValue<bool>("muzzleFromFiringLocator", muzzleFromFiringLocator);
		reader.ReadParamValue<bool>("ignore_damage_falloff", ignore_damage_falloff);
		reader.ReadParamValue<bool>("laser_beam_uses_spread", laser_beam_uses_spread);
	}

	if (defaultInit || !ammo_type.empty())
	{
		ammo_type_class = gEnv->pEntitySystem->GetClassRegistry()->FindClass(ammo_type.c_str());
	}

	spawn_ammo_class = 0;
	if (defaultInit || !ammo_spawn_type.empty())
	{
		spawn_ammo_class = gEnv->pEntitySystem->GetClassRegistry()->FindClass(ammo_spawn_type.c_str());
	}
	if (spawn_ammo_class == 0)
	{
		spawn_ammo_class = ammo_type_class;
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SFireModeAnimationParams::Reset( const XmlNodeRef& paramsNode, bool defaultInit/*=true*/ )
{
	if (defaultInit)
	{
		useBaseModifier = false;
	}

	if (paramsNode)
	{
		CGameXmlParamReader reader(paramsNode);

		reader.ReadParamValue<bool>("useBaseModifier", useBaseModifier);
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SThermalVisionParams::Reset( const XmlNodeRef& paramsNode, bool defaultInit/*=true*/ )
{
	if (defaultInit)
	{
		weapon_shootHeatPulse = 0.1f;
		weapon_shootHeatPulseTime = 0.15f;

		owner_shootHeatPulse = 0.075f;
		owner_shootHeatPulseTime = 0.1f;
	}

	if (paramsNode)
	{
		CGameXmlParamReader reader(paramsNode);

		reader.ReadParamValue<float>("weapon_shootHeatPulse", weapon_shootHeatPulse);
		reader.ReadParamValue<float>("weapon_shootHeatPulseTime", weapon_shootHeatPulseTime);

		reader.ReadParamValue<float>("owner_shootHeatPulse", owner_shootHeatPulse);
		reader.ReadParamValue<float>("owner_shootHeatPulseTime", owner_shootHeatPulseTime);
	}
}



//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////


void SSpammerParams::Reset(const XmlNodeRef& paramsNode, bool defaultInit)
{
	if (defaultInit)
	{
		maxNumRockets = 12;
		loadInRate = 300;
		burstRate = 1100;
		targetingTolerance = 20.0f;
		minLockOnDistance = 2.0f;
		maxLockOnDistance = 500.0f;
		targetingFlatMode = false;
	}

	if (paramsNode)
	{
		CGameXmlParamReader reader(paramsNode);

		reader.ReadParamValue<int>("maxNumRockets", maxNumRockets);
		reader.ReadParamValue<int>("loadInRate", loadInRate);
		reader.ReadParamValue<int>("burstRate", burstRate);
		reader.ReadParamValue<float>("targetingTolerance", targetingTolerance);
		reader.ReadParamValue<float>("minLockOnDistance", minLockOnDistance);
		reader.ReadParamValue<float>("maxLockOnDistance", maxLockOnDistance);
		reader.ReadParamValue<bool>("targetingFlatMode", targetingFlatMode);
	}
}


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////



void SBowParams::Reset(const XmlNodeRef& paramsNode, bool defaultInit)
{
	if (defaultInit)
	{
		chargeTime = 2.0f;
		minChargePercent = 0.4f;
		minChargeDamage = 0.4f;
		minChargeSpeed = 0.4f;
		maxChargeDamage = 1.0f;
		maxChargeSpeed = 1.0f;
		stringStrength = 1.0f;
		pinChargeTime = 0.2f;
		impactDamage = false;
	}

	if (paramsNode)
	{
		CGameXmlParamReader reader(paramsNode);

		reader.ReadParamValue<float>("chargeTime", chargeTime);
		reader.ReadParamValue<float>("minChargePercent", minChargePercent);
		reader.ReadParamValue<float>("minChargeDamage", minChargeDamage);
		reader.ReadParamValue<float>("minChargeSpeed", minChargeSpeed);
		reader.ReadParamValue<float>("maxChargeDamage", maxChargeDamage);
		reader.ReadParamValue<float>("maxChargeSpeed", maxChargeSpeed);
		reader.ReadParamValue<float>("stringStrength", stringStrength);
		reader.ReadParamValue<float>("pinChargeTime", pinChargeTime);
		reader.ReadParamValue<bool>("impactDamage", impactDamage);
	}

}

/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////

void SParentFireModeParams::CacheResources()
{
	pBaseFireMode->CacheResources();

	TAccessoryFireModeParamsVector::iterator acpEndIt = accessoryChangedParams.end();
	for (TAccessoryFireModeParamsVector::iterator acpIt = accessoryChangedParams.begin(); acpIt != acpEndIt; ++acpIt)
	{
		acpIt->pAlteredParams->CacheResources();
	}
}

void SParentFireModeParams::Release()
{
	if(pBaseFireMode)
	{
		pBaseFireMode->Release();
	}
	SAFE_DELETE(pBaseFireMode);

	TAccessoryFireModeParamsVector::iterator acpEndIt = accessoryChangedParams.end();
	for (TAccessoryFireModeParamsVector::iterator acpIt = accessoryChangedParams.begin(); acpIt != acpEndIt; ++acpIt)
	{
		acpIt->pAlteredParams->Release();
		SAFE_DELETE(acpIt->pAlteredParams);
	}
}

const SFireModeParams* SParentFireModeParams::FindAccessoryFireModeParams(IEntityClass* pClass, IEntityClass* pClassTwo, IEntityClass* pClassThree, IEntityClass* pClassFour) const
{
	int numAccessoryParams = accessoryChangedParams.size();

	for(int i = 0; i < numAccessoryParams; i++)
	{
		const SAccessoryFireModeParams* pParams = &accessoryChangedParams[i];

		if(pClass == pParams->pAccessories[0] && pClassTwo == pParams->pAccessories[1] && pClassThree == pParams->pAccessories[2] && pClassFour == pParams->pAccessories[3])
		{
			return pParams->pAlteredParams;
		}
	}

	return NULL;
}

