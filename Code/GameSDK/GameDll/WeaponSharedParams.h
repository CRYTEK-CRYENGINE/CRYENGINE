// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------

Description: Stores all shared weapon parameters (shared by class)...
							Allows for some memory savings...

-------------------------------------------------------------------------
History:
- 30:1:2008   10:54 : Benito G.R.

*************************************************************************/
#pragma once

#ifndef __WEAPONSHAREDPARAMS_H__
#define __WEAPONSHAREDPARAMS_H__

#include "IronSight.h"
#include "ItemDefinitions.h"
#include "FireModeParams.h"
#include "Stereo3D/StereoZoom.h"


typedef VectorMap<EStance, Vec3>	TStanceWeaponOffset;

struct SAIWeaponOffset
{
	SAIWeaponOffset(): useEyeOffset(false){};

	bool								useEyeOffset;

	TStanceWeaponOffset	stanceWeponOffset;
	TStanceWeaponOffset	stanceWeponOffsetLeanLeft;
	TStanceWeaponOffset	stanceWeponOffsetLeanRight;

	void GetMemoryUsage(ICrySizer* s, const TStanceWeaponOffset& vm) const
	{
		//s->AddObject(vm);
		// explicitly add the objects as it's difficult to provide a generalized overload for AddObject() taking enums in CrySizer.h
		if (vm.capacity())
			s->AddObject(&(*vm.begin()), sizeof(TStanceWeaponOffset::value_type) * vm.capacity());
	}

	void GetMemoryUsage(ICrySizer* s) const
	{
		GetMemoryUsage(s, stanceWeponOffset);
		GetMemoryUsage(s, stanceWeponOffsetLeanLeft);
		GetMemoryUsage(s, stanceWeponOffsetLeanRight);
	}
};

struct SPlayerMovementModifiers
{
	SPlayerMovementModifiers()
		: movementSpeedScale(1.0f)
		, rotationSpeedScale(1.0f)
		, mouseRotationSpeedScale(1.0f)
		, firingMovementSpeedScale(1.0f)
		, firingRotationSpeedScale(1.0f)
		, mouseFiringRotationSpeedScale(1.0f) {}

	void GetMemoryUsage( ICrySizer *pSizer ) const{}

	float movementSpeedScale;
	float rotationSpeedScale;
	float mouseRotationSpeedScale;
	float firingMovementSpeedScale;
	float firingRotationSpeedScale;
	float	mouseFiringRotationSpeedScale;
};

struct SReloadMagazineParams
{
	SReloadMagazineParams()
		: magazineEventCRC32(0)
		, magazineAttachment("")
	{

	}
	void GetMemoryUsage(ICrySizer* s) const
	{
		s->AddObject(magazineAttachment);
	}

	ItemString magazineAttachment;
	uint32 magazineEventCRC32;
};

struct SBulletBeltParams
{
	SBulletBeltParams() 
		: jointName("")
		, beltRefillReloadFraction(1.0f)
		, pAmmoClass(NULL)
		, numBullets(0)
	{ }

	void GetMemoryUsage(ICrySizer * s) const
	{
		s->AddObject(jointName);
	}

	ItemString jointName;
	float beltRefillReloadFraction;
	IEntityClass* pAmmoClass;
	uint16 numBullets;
};

// Flags used by grab type params. Exposed for easy user access + flag checking. 
enum EGrabTypePropertyFlag
{
	EPF_hideInteractionPrompts													= BIT(0),
	EPF_isNPCType																				= BIT(1),
	EPF_use_physical_melee_system												= BIT(2),
	EPF_grab_orientation_override_localOffset_specified	= BIT(3),
	EPF_useSweepTestMelee																= BIT(4),
};

struct SPickAndThrowParams
{
	SPickAndThrowParams()
	{}
	
	struct SKillGrabbedNPCParams
	{
		SKillGrabbedNPCParams () : showKnife(false) {}
		ItemString action;
		bool showKnife;
		
		void Read( const XmlNodeRef& paramsNode );

		void GetMemoryUsage(ICrySizer* s) const
		{
			s->AddObject(action);
		}
	};
	
	struct SThrowParams
	{
		SThrowParams() 
		{
			SetDefaultValues();
		}
		ItemString	throw_action;
		ItemString	chargedObjectThrowAction;
		Vec3				chargedThrowInitialFacingDir;
		float				timeToFreeAtThrowing;
		float				timeToRestorePhysicsAfterFree;  // even after is flying away, we keep the constrains for this duration to avoid collisions with the player
		float				throwSpeed;
		float				autoThrowSpeed;
		float				powerThrownDelay;
		float				maxChargedThrowSpeed;
		bool				chargedThrowEnabled; 
		
		void Read( const XmlNodeRef& paramsNode );
		void SetDefaultValues();

		void GetMemoryUsage(ICrySizer* s) const
		{
			s->AddObject(throw_action);
			s->AddObject(chargedObjectThrowAction);
		}
	};
	
	struct SCameraShakeParams
	{
		SCameraShakeParams()
			: time(0)
			, frequency(0)
			, shift(0, 0, 0)
			, rotate(0, 0, 0)
			, viewAngleAttenuation(true)
			, randomDirection(false)
			, enabled(true) {}

		void GetMemoryUsage(ICrySizer* s) const {}

		Vec3		shift;
		Vec3		rotate;
		float		time;
		float		frequency;
		bool		viewAngleAttenuation;
		bool		randomDirection;
		bool		enabled;
	};

	struct SGrabTypeParams
	{
		ItemString helper;   // on the object to be picked
		ItemString helper_3p;   // on the object to be picked
		ItemString attachment;
		ItemString display_name;
		ItemString grab_action_part1;
		ItemString grab_action_part1_alternate; // Used when picking up an environmental weapon that has been unrooted
		ItemString drop_action;
		ItemString melee_action;
		ItemString melee_recovery_action;
		ItemString melee_hit_action;
		ItemString melee_mfxLibrary;
		ItemString NPCtype;
		ItemString animGraphPose;
		ItemString tag;
		ItemString dbaFile;
		TAudioSignalID audioSignalHitOnNPC;
	
		Vec3  grab_orientation_override_localOffset; // A more specific alternative to the dist specified above.. a local offset from the object grab helper

		SKillGrabbedNPCParams killGrabbedNPCParams;
		SThrowParams throwParams;

		// Support for combo attacks of length s_MaxComboAttacksSupported. Just need to tweak this if necessary
		typedef  std::vector< std::pair<ItemString, ItemString> > TMeleeComboActionList;
		TMeleeComboActionList m_meleeComboList;
		
		// Overrides
		Vec3  melee_hit_dir;
	
		// primary attack
		float melee_hit_dir_min_ratio;
		float melee_hit_dir_max_ratio;

		float melee_delay;
		float meleeAction_duration;
		float melee_damage_scale;
		float melee_impulse_scale;
		float post_melee_action_cooldown_primary; 
		float post_melee_action_cooldown_combo; 

		float vehicleMeleeDamage;
		float	vehicleThrowDamage;
		float	vehicleThrowMinVelocity;
		float	vehicleThrowMaxVelocity;
	 
		// Attack lerp
		float complexMelee_snap_target_select_range;   // melee lunge target selection range  2.5f - default
		float complexMelee_snap_end_position_range;    // melee lunge target distance from player - 1.0f default

		// Anim scaling ( Do we want this in release builds ?? )
		float anim_durationOverride_RootedGrab;      
		float anim_durationOverride_Grab;            
		float anim_durationOverride_PrimaryAttack;  
		float anim_durationOverride_ComboAttack;     

		// Melee damage simulation method
		float timePicking;
		float timeToStickAtPicking;
		
		float dropSpeedForward;

		float timeDropping;
		float timeToFreeAtDropping;

		float movementSpeedMultiplier;
				
		float holdingEnergyConsumption;
		int shieldingHits;  // how many hits the grabbed NPC protect the player
		int crossHairType;

		bool shieldsPlayer;  // Does this shield the player when held

		typedef uint8 TPropertyFlags; 
		TPropertyFlags m_propertyFlags; 

		SGrabTypeParams() 
			: melee_delay(0)
			, meleeAction_duration(0)
			, melee_damage_scale(5.0f)
			, melee_impulse_scale(5.0f)
			, melee_hit_dir_min_ratio(0.0f)
			, melee_hit_dir_max_ratio(0.0f)
			, crossHairType(eHCH_None)
			, shieldsPlayer(false)
			, timePicking(0)
			, timeToStickAtPicking(0)
			, timeDropping(0)
			, timeToFreeAtDropping(0)
			, dropSpeedForward(0)
			, holdingEnergyConsumption(0)
			, shieldingHits(0)
			, audioSignalHitOnNPC(INVALID_AUDIOSIGNAL_ID)
			, post_melee_action_cooldown_primary(0.0f)
			, post_melee_action_cooldown_combo(0.0f)
			, vehicleMeleeDamage(0.f)
			, vehicleThrowDamage(0.f)
			, vehicleThrowMinVelocity(5.f)
			, vehicleThrowMaxVelocity(20.f)
			, m_propertyFlags(0) // Flags (all false by default)
			, complexMelee_snap_target_select_range(2.5f)
			, complexMelee_snap_end_position_range(1.0f)
			, anim_durationOverride_RootedGrab(-1.0f)     
			, anim_durationOverride_Grab(-1.0f)             
			, anim_durationOverride_PrimaryAttack(-1.0f)  
			, anim_durationOverride_ComboAttack(-1.0f)      
			, movementSpeedMultiplier(1.0f)
		{}
		void Read( const XmlNodeRef& paramsNode );

		void ReadCameraShake( const XmlNodeRef& params, SPickAndThrowParams::SCameraShakeParams* pCameraShake );

		void CacheResources();

		void GetMemoryUsage(ICrySizer* s) const
		{
			s->AddObject(helper);
			s->AddObject(helper_3p);
			s->AddObject(attachment);
			s->AddObject(display_name);
			s->AddObject(grab_action_part1);
			s->AddObject(grab_action_part1_alternate);
			s->AddObject(drop_action);
			s->AddObject(melee_action);
			s->AddObject(melee_recovery_action);
			s->AddObject(melee_hit_action);
			s->AddObject(melee_mfxLibrary);
			s->AddObject(NPCtype);
			s->AddObject(animGraphPose);
			s->AddObject(tag);
			s->AddObject(dbaFile);
			s->AddContainer(m_meleeComboList);
			s->AddObject(killGrabbedNPCParams);
			s->AddObject(throwParams);
		};
	};
	
	void GetMemoryUsage(ICrySizer* s) const
	{
		s->AddObject(grabTypesParams);
	};

	std::vector<SGrabTypeParams> grabTypesParams;
};

struct SWeaponAmmoParams
{
	SWeaponAmmoParams() : extraItems(0) {};

	void GetMemoryUsage(ICrySizer* s) const
	{
		s->AddObject(ammo);
		s->AddObject(bonusAmmo);
		s->AddObject(accessoryAmmo);
		s->AddObject(minDroppedAmmo);
		s->AddObject(capacityAmmo);
	}

	int					extraItems;
	TAmmoVector	ammo;
	TAmmoVector	bonusAmmo;
	TAmmoVector	accessoryAmmo;
	TAmmoVector	minDroppedAmmo;
	TAmmoVector	capacityAmmo;
};

struct STurretSearchParams
{
	STurretSearchParams() 
	{ 
		Reset(XmlNodeRef(NULL)); 
	};
	void Reset(const XmlNodeRef& paramsNode0, bool defaultInit = true);

	void GetMemoryUsage(ICrySizer* s) const
	{
		s->AddObject(hints);
		s->AddObject(light_helper);
		s->AddObject(light_texture);
		s->AddObject(light_material);
	}

	std::vector<Vec2> hints;
	ItemString light_helper;
	ItemString light_texture;
	ItemString light_material;
	Vec3   light_color;
	float  light_diffuse_mul;
	float  light_hdr_dyn;

};

struct STurretFireParams
{
	STurretFireParams()
		: deviation_speed(1.0f)
		, deviation_amount(1.0f)
		, randomness(0.0f)
	{
		Reset(XmlNodeRef(NULL));
	};

	void Reset(const XmlNodeRef& paramsNode, bool defaultInit = true);

	void GetMemoryUsage(ICrySizer* s) const
	{
		s->AddObject(hints);
	}

	std::vector<Vec2> hints;
	float deviation_speed;
	float deviation_amount;
	float randomness;
};

struct STurretParams
{
	STurretParams() : desiredHoverHeight(0.f) {};

	void GetMemoryUsage(ICrySizer* s) const
	{
		s->AddObject(searchParams);
		s->AddObject(fireParams);
		s->AddObject(radarHelper);
		s->AddObject(tripodHelper);
		s->AddObject(barrelHelper);
		s->AddObject(fireHelper);
		s->AddObject(rocketHelper);
		s->AddObject(rocketFireMode);
	}

	STurretSearchParams searchParams;
	STurretFireParams		fireParams;

	ItemString radarHelper;
	ItemString tripodHelper;
	ItemString barrelHelper;
	ItemString fireHelper;
	ItemString rocketHelper;
	ItemString rocketFireMode;

	float desiredHoverHeight;
};

struct SZoomParams
{
  struct SStageParams
  {
    SStageParams()
      : stage(1.5f)
	  , rotationSpeedScale(1.0f)
	  , movementSpeedScale(1.0f)
      , useDof(true)
    {

    }

		void GetMemoryUsage(ICrySizer* s) const {}

    float stage;
	float rotationSpeedScale;
	float movementSpeedScale;
    bool  useDof;
  };

	SZoomParams()
	{ 
		Reset(XmlNodeRef(NULL)); 
	};

	void Reset(const XmlNodeRef& paramsNode, bool defaultInit = true);
	void GetMemoryUsage(ICrySizer * s) const
	{
		s->AddObject(stages);
		s->AddObject(suffix);
		s->AddObject(suffixAG);
		s->AddObject(blur_mask);
		s->AddObject(zoom_overlay);
	}

	std::vector<SStageParams>	stages;
	Vec3								fp_offset;
	Quat								fp_rot_offset;
	ItemString					suffix;
	ItemString					suffixAG;
	float								blur_amount;
	ItemString					blur_mask;
	ItemString					dof_mask;
	ItemString					zoom_overlay;
	float								dof_focusMin;
	float								dof_focusMax;
	float								dof_focusLimit;
	float								dof_shoulderMinZ;
	float								dof_shoulderMinZScale;
	float								dof_minZ;
	float								dof_minZScale;
	float								zoom_in_time;
	float								zoom_out_time;
	float								zoom_out_delay;
	float								stage_time;
	float								muzzle_flash_scale;
	float								shoulderRotationAnimFactor;
	float								shoulderStrafeAnimFactor;
	float								shoulderMovementAnimFactor;
	float								ironsightRotationAnimFactor;
	float								ironsightStrafeAnimFactor;
	float								ironsightMovementAnimFactor;
	float								cameraShakeMultiplier;

	float								scope_nearFov;
	bool								scope_mode;
	bool								hide_weapon;
	bool								dof;
	bool								target_snap_enabled;
	bool								hide_crosshair;
	bool								near_fov_sync;
	bool								armor_bonus_zoom;
};

struct SZoomSway
{
	SZoomSway() 
	{ 
		Reset(XmlNodeRef(NULL)); 
	};

	void Reset(const XmlNodeRef& paramsNode, bool defaultInit = true);

	float maxX;
	float maxY;
	float stabilizeTime;
	float holdBreathScale;
	float holdBreathTime;
	float minScale;
	float scaleAfterFiring;

	//Stance modifiers
	float	crouchScale;

	void GetMemoryUsage(ICrySizer * s) const {};
};

struct SScopeParams
{
	SScopeParams() 
	{ 
		Reset(XmlNodeRef(NULL)); 
	};

	void Reset(const XmlNodeRef& paramsNode, bool defaultInit = true);

	void GetMemoryUsage(ICrySizer * s) const
	{
		s->AddObject(helper_name);
		s->AddObject(crosshair_sub_material);
	}

	ItemString	helper_name;
	ItemString	crosshair_sub_material;
	float		dark_out_time;
	float		dark_in_time;
	float		blink_frequency;
	bool		techvisor;
};

struct SZoomModeParams
{
	SZoomModeParams() {};

	void CacheResources();

	void GetMemoryUsage(ICrySizer* s) const
	{
		s->AddObject(zoomParams);
		s->AddObject(zoomSway);
		s->AddObject(spreadModParams);
		s->AddObject(recoilModParams);
		s->AddObject(scopeParams);	
	}

	SAimLookParameters		aimLookParams;
	SZoomParams				zoomParams;
	SZoomSway					zoomSway;
	SSpreadModParams	spreadModParams;
	SRecoilModParams	recoilModParams;
	SScopeParams			scopeParams;
  Stereo3D::Zoom::Parameters stereoParams;
};

struct SAccessoryZoomModeParams
{
	SAccessoryZoomModeParams() { pAccessories[0] = pAccessories[1] = pAccessories[2] = pAccessories[3] = NULL; }

	IEntityClass* pAccessories[ITEM_MAX_NUM_ACCESSORIES];
	SZoomModeParams alteredParams;

	void GetMemoryUsage(ICrySizer* s) const
	{
		s->AddObject(alteredParams);
	}
};

struct SParentZoomModeParams
{
	SParentZoomModeParams() {};

	typedef std::vector<SAccessoryZoomModeParams> TAccessoryZoomModeParamsVector;

	void GetMemoryUsage(ICrySizer* s) const 
	{
		s->AddObject(baseZoomMode);
		s->AddObject(initialiseParams);	
		s->AddObject(accessoryChangedParams);
	};

	void CacheResources()
	{
		baseZoomMode.CacheResources();

		TAccessoryZoomModeParamsVector::iterator acpEndIt = accessoryChangedParams.end();
		for (TAccessoryZoomModeParamsVector::iterator acpIt = accessoryChangedParams.begin(); acpIt != acpEndIt; ++acpIt)
		{
			acpIt->alteredParams.CacheResources();
		}
	}

	const SZoomModeParams* FindAccessoryZoomModeParams(IEntityClass* pClass, IEntityClass* pClassTwo = NULL, IEntityClass* pClassThree = NULL, IEntityClass* pClassFour = NULL) const
	{
		int numAccessoryParams = accessoryChangedParams.size();

		for(int i = 0; i < numAccessoryParams; i++)
		{
			const SAccessoryZoomModeParams* pParams = &accessoryChangedParams[i];

			if(pClass == pParams->pAccessories[0] && pClassTwo == pParams->pAccessories[1] && pClassThree == pParams->pAccessories[2] && pClassFour == pParams->pAccessories[3])
			{
				return &pParams->alteredParams;
			}
		}

		return NULL;
	}
		
	SZoomModeParams baseZoomMode;
	SInitialiseParams initialiseParams;
	TAccessoryZoomModeParamsVector accessoryChangedParams;
};

struct SVehicleGuided
{
	ItemString animationName;
	ItemString preState;
	ItemString postState;

	float postStateWaitTime;
	bool	waitWhileFiring;

	SVehicleGuided() 
	{ 
		Reset(XmlNodeRef(NULL)); 
	};

	void Reset(const XmlNodeRef& paramsNode, bool defaultInit = true);

	void GetMemoryUsage(ICrySizer * s) const
	{
		s->AddObject(animationName);
		s->AddObject(preState);
		s->AddObject(postState);
	}
};

struct SC4Params
{
	int armedLightMatIndex;

	float armedLightGlowAmount;
	float disarmedLightGlowAmount;

	ColorF armedLightColour;
	ColorF disarmedLightColour;

	SC4Params() 
	{ 
		Reset(XmlNodeRef(NULL)); 
	};

	void Reset(const XmlNodeRef& paramsNode, bool defaultInit = true);

	void GetMemoryUsage(ICrySizer * s) const
	{
	}
};

class CWeaponSharedParams : public IGameSharedParameters
{
	typedef std::map<int, _smart_ptr<IGameSharedParameters> > TSharedMap;
	typedef std::vector<SPlayerMovementModifiers> TMovementModifiers;

public:
	CWeaponSharedParams(): pickupSound(""), pVehicleGuided(NULL), pPickAndThrowParams(NULL), pTurretParams(NULL), pMeleeModeParams(NULL), pC4Params(NULL) {};
	virtual ~CWeaponSharedParams();

	//IGameSharedParameters
	virtual void GetMemoryUsage(ICrySizer *s) const;
	virtual const char* GetDataType() const { return "WeaponParams"; }
	virtual void ReleaseLevelResources();
	//~IGameSharedParameters

	void ReadWeaponParams(const XmlNodeRef& paramsNode, CItemSharedParams* pItemParams, const char* weaponClassName);
	void CacheResources();
	SMeleeModeParams* FindAccessoryMeleeParams(IEntityClass* pClass, IEntityClass* pClassTwo = NULL, IEntityClass* pClassThree = NULL, IEntityClass* pClassFour = NULL) const;

	SAIDescriptor					aiWeaponDescriptor;
	SAIWeaponOffset							aiWeaponOffsets;	
	SHazardDescriptor				hazardWeaponDescriptor;
	SPlayerMovementModifiers		defaultMovementModifiers;
	SReloadMagazineParams				reloadMagazineParams;
	SBulletBeltParams						bulletBeltParams;
	TMovementModifiers					zoomedMovementModifiers;

	SWeaponAmmoParams						ammoParams;
	TParentFireModeParamsVector firemodeParams;
	TParentZoomModeParamsVector zoommodeParams;
	TParentMeleeParamsVector		meleeParams;
	ItemString									pickupSound;

	SVehicleGuided*						pVehicleGuided;
	SPickAndThrowParams*				pPickAndThrowParams;
	STurretParams*						pTurretParams;
	SMeleeModeParams*					pMeleeModeParams;
	SC4Params*								pC4Params;

protected:
	void ReadFireModeParams(const XmlNodeRef& paramsNode, CItemSharedParams* pItemParams);
	void ReadFireMode(const XmlNodeRef& paramsNode, CItemSharedParams* pItemParams, SFireModeParamsUnpacked* pFireMode, bool defaultInit);
	void ReadFireModePluginParams(const XmlNodeRef& paramsNode, SFireModeParamsUnpacked* pFireMode, bool defaultInit);
	void ReadMeleeParams(const XmlNodeRef& meleeFireModeNode, CItemSharedParams* pItemParams);
	void ReadMeleeMode(const XmlNodeRef& paramsNode, SMeleeModeParams* pMelee, bool defaultInit);
	void ReadZoomModeParams(const XmlNodeRef& paramsNode, CItemSharedParams* pItemParams);
	void ReadZoomMode(const XmlNodeRef& paramsNode, CItemSharedParams* pItemParams, SZoomModeParams* pZoomMode, bool defaultInit);
	void ReadAmmoParams(const XmlNodeRef& paramsNode);
	void ReadReloadMagazineParams(const XmlNodeRef& paramsNode);
	void ReadBulletBeltParams(const XmlNodeRef& paramsNode);
	void ReadMovementModifierParams(const XmlNodeRef& paramsNode);
	void ReadPickAndThrowParams(const XmlNodeRef& paramsNode);
	void ReadAIParams(const XmlNodeRef& paramsNode);
	void ReadAIOffsets(const XmlNodeRef& paramsNode);
	void ReadHazardParams(const XmlNodeRef& paramsNode);
	void ReadTurretParams(const XmlNodeRef& turretNode, const XmlNodeRef& paramsNode);
	XmlNodeRef FindAccessoryPatchForMode(const char* modeType, const char *modeName, const SAccessoryParams* pAccessory);
	void PatchFireMode(SFireModeParamsUnpacked* pParams, CItemSharedParams* pItemParams, const XmlNodeRef& patchOne, const XmlNodeRef& patchTwo = XmlNodeRef(NULL), const XmlNodeRef& patchThree = XmlNodeRef(NULL));
	void PatchZoomMode(SZoomModeParams* pParams, CItemSharedParams* pItemParams, const XmlNodeRef& patchOne, const XmlNodeRef& patchTwo = XmlNodeRef(NULL), const XmlNodeRef& patchThree = XmlNodeRef(NULL));
	void PatchMeleeMode(SMeleeModeParams* pParams, const XmlNodeRef& patchOne, const XmlNodeRef& patchTwo = XmlNodeRef(NULL), const XmlNodeRef& patchThree = XmlNodeRef(NULL));
	void ReadVehicleGuided(const XmlNodeRef& paramsNode);
	void ReadC4Params(const XmlNodeRef& paramsNode);

private:
	void ResetInternal();
};

#endif //__WEAPONSHAREDPARAMS_H__
