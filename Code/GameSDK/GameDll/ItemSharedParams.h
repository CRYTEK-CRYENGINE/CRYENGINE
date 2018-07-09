// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Stores all item parameters that don't mutate in instances...
						 Allows for some nice memory savings...

-------------------------------------------------------------------------
History:
- 3:4:2007   10:54 : Created by Márcio Martins

*************************************************************************/

#pragma once

#ifndef __ITEMSHAREDPARAMS_H__
#define __ITEMSHAREDPARAMS_H__

#include "GameParameters.h"
#include "Item.h"
#include "ItemDefinitions.h"
#include "Audio/GameAudio.h"
#include <IForceFeedbackSystem.h>
#include <CryAnimation/ICryAnimation.h>
#include "WeaponStats.h"
#include "ItemParamsRegistration.h"

class CItemResourceCache;
class CItemGeometryCache;
class CItemMaterialAndTextureCache;

struct SAimAnimsBlock
{
	ItemString anim[WeaponAimAnim::Total];
	
	void Reset();
	void Read( const XmlNodeRef& rootNode );
	void GetMemoryUsage( ICrySizer *s ) const
	{
		for (int i = 0; i < WeaponAimAnim::Total; ++i)
		{
			s->AddObject(anim[i]);
		}
	}	
};


struct SAimLookParameters
{
	SAimLookParameters();

	void Reset();
	void Read(const XmlNodeRef& rootNode);

#define AIMLOOK_PARAMS_MEMBERS(f) \
	f(float,	easeFactorInc) \
	f(float,	easeFactorDec) \
	f(float,	strafeScopeFactor) \
	f(float,	rotateScopeFactor) \
	f(float,	velocityInterpolationMultiplier) \
	f(float,	velocityLowPassFilter) \
	f(float,	accelerationSmoothing) \
	f(float,	accelerationFrontAugmentation) \
	f(float,	verticalVelocityScale) \
	f(bool,		sprintCameraAnimation) \

#define AIMLOOK_ARRAY_MEMBERS(f) \
	f(float, WeaponAimAnim::Total, blendFactors)

	REGISTER_STRUCT_WITH_ARRAYS(AIMLOOK_PARAMS_MEMBERS, AIMLOOK_ARRAY_MEMBERS, SAimLookParameters)
};


struct SParams
{
	SParams()
	{
		Reset();
	};

	void Reset()
	{
		selectable = true;
		droppable = true;
		pickable = true;
		mountable = true;
		usable = true;
		giveable = true;
		unique = true;
		mass = 3.5f;
		drop_impulse = 12.5f;
		select_override = 0.0f;
		auto_droppable = false;
		auto_pickable = true;
		has_first_select = false;
		select_delayed_grab_3P = false;
		attach_to_back = false;
		scopeAttachment = 0;
		attachment_gives_ammo = false;
		can_ledge_grab = true;
		can_rip_off = true;
		check_clip_size_after_drop = true;
		check_bonus_ammo_after_drop = true;
		remove_on_drop = true;
		usable_under_water = false;
		can_overcharge = false;
		fp_offset.Set(0.0f, 0.0f, 0.0f);
		fp_rot_offset.SetIdentity();
		hasAimAnims = false;
		ironsightAimAnimFactor = 1.f;
		fast_select = false;
		sprintToFireDelay = 0.0f;
		sprintToZoomDelay = 0.35f;
		sprintToMeleeDelay = 0.05f;
		runToSprintBlendTime = 0.2f;
		sprintToRunBlendTime = 0.2f;
		autoReloadDelay = 0.0f;
		zoomTimeMultiplier = 1.f;
		selectTimeMultiplier = 1.f;

		tag = "";
		attachment[IItem::eIH_Right] = g_pItemStrings->right_item_attachment;
		attachment[IItem::eIH_Left] = g_pItemStrings->left_item_attachment;
		aiAttachment[IItem::eIH_Right] = g_pItemStrings->right_item_attachment;
		aiAttachment[IItem::eIH_Left] = g_pItemStrings->left_item_attachment;
		bone_attachment_01.clear();
		bone_attachment_02.clear();

		const int numStats = eWeaponStat_NumStats;
		for (int i = 0; i < numStats; i++)
		{
			weaponStats.stats[i] = 5;
		}

		aimAnims.Reset();
		mountedAimAnims.Reset();
		aimLookParams.Reset();

		for (int i = 0; i < MountedTPAimAnim::Total; ++i)
		{
			mountedTPAimAnims[i].clear();;
		}

		crosshairTexture = "";
	}

	void GetMemoryUsage(ICrySizer * s) const
	{
		s->AddObject(tag);
		s->AddObject(display_name);
		s->AddObject(bone_attachment_01);
		s->AddObject(bone_attachment_02);
		
		for (int i = 0; i < IItem::eIH_Last; i++)
		{
			s->AddObject(attachment[i]);
		}
		s->AddObject(aimAnims);
		s->AddObject(mountedAimAnims);
		
		for (int i = 0; i < MountedTPAimAnim::Total; i++)
		{
			s->AddObject(mountedTPAimAnims[i]);
		}
	}
	
	bool	selectable;
	bool	droppable;
	bool	pickable;
	bool	mountable;
	bool	usable;
	bool	giveable;
	bool	unique;
	bool	auto_droppable;
	bool	auto_pickable;
	bool	attachment_gives_ammo;
	bool	has_first_select;
	bool	fast_select;
	bool	select_delayed_grab_3P;
	bool	can_ledge_grab;
	bool	can_rip_off;
	bool	usable_under_water;
	bool	can_overcharge;
	bool  check_clip_size_after_drop;
	bool  check_bonus_ammo_after_drop;
	bool	remove_on_drop;
	float	sprintToFireDelay;
	float	sprintToZoomDelay;
	float	sprintToMeleeDelay;
	float	autoReloadDelay;
	float	runToSprintBlendTime;
	float	sprintToRunBlendTime;

	int   scopeAttachment;

	SWeaponStatsData weaponStats;

	float	mass;
	float	drop_impulse;
	float	select_override;
	float zoomTimeMultiplier;
	float selectTimeMultiplier;

	Vec3	fp_offset;
	Quat	fp_rot_offset;

	ItemString	tag;
	ItemString  itemClass;
	ItemString	aiAttachment[IItem::eIH_Last];
	ItemString	attachment[IItem::eIH_Last];
	ItemString	display_name;

	ItemString	adbFile;
	ItemString	soundAdbFile;
	ItemString  actionControllerFile;

	ItemString  bone_attachment_01;
	ItemString  bone_attachment_02;
	bool				attach_to_back;

	bool				hasAimAnims;
	float				ironsightAimAnimFactor;
	SAimAnimsBlock aimAnims;
	SAimAnimsBlock mountedAimAnims;
	SAimLookParameters aimLookParams;
	ItemString	mountedTPAimAnims[MountedTPAimAnim::Total];

	ItemString crosshairTexture;
};


struct SMountParams
{
	SMountParams()
	{
		Reset();
	};

	void Reset()
	{
		body_distance = 0.55f;
		ground_distance = 0.0f;
		fpBody_offset.Set(0.0f,-1.0f,-0.1f);
		fpBody_offset_ironsight.Set(0.0f,-1.0f,-0.1f);

		pivot.clear();
		left_hand_helper.clear();
		right_hand_helper.clear();
		rotate_sound_fp.clear();
		rotate_sound_tp.clear();
	}

	void GetMemoryUsage(ICrySizer * s) const
	{
		s->AddObject(pivot);
		s->AddObject(left_hand_helper);
		s->AddObject(right_hand_helper);
	}

	float		body_distance;
	float		ground_distance;
	Vec3		fpBody_offset;
	Vec3		fpBody_offset_ironsight;
	ItemString	pivot;
	ItemString	left_hand_helper;
	ItemString	right_hand_helper;
	ItemString  rotate_sound_fp;
	ItemString  rotate_sound_tp;
};

struct SAnimation
{
	enum EFlags
	{
		ANIMATE_CAMERA						= BIT(0),
		USE_UNMODIFIED_BASEPOSE		= BIT(1),
		IDLEPOSE_ACTION						= BIT(2),
		CONSTANT_INTERPOLATION		= BIT(4),
		FULL_BODY									= BIT(5),
	};

	SAnimation()
		: flags(0)
		, layer(0)
		, speed(0.0f)
		, blend(0.0f)
		, blendBackToModifiedBasePoseTime(0.0f)
	{};

	void GetMemoryUsage(ICrySizer * s) const
	{
		s->AddObject(name);
	}

	ItemString	name;
	float				speed;
	float				blend;
	float				blendBackToModifiedBasePoseTime;
	uint8				layer;
	uint8				flags;
};

struct SForceFeedback
{
	SForceFeedback()
		: combatModeMultiplier(1), delay(0), fxId(InvalidForceFeedbackFxId) {}

	float combatModeMultiplier;
	float delay;
	ForceFeedbackFxId fxId;
};

struct SCameraShake
{
	SCameraShake()
		: time(0)
		, shift(0, 0, 0)
		, rotate(0, 0, 0) {}

	Vec3		shift;
	Vec3		rotate;
	float		time;
};

struct SAudio
{
	SAudio():	isstatic(false), sphere(0.0f), airadius(0.0f),issynched(false) {};

	void GetMemoryUsage(ICrySizer * s) const
	{
		s->AddObject(name);
	}

	ItemString		name;
	float			    airadius;
	float			    sphere;
	bool			    isstatic;
	bool          issynched;
};

struct SDamageLevel
{
	SDamageLevel()
		: max_health(100)
		, min_health(0)
		, scale(1.0f)
	{};

	int max_health;
	int min_health;
	float scale;
	ItemString effect;
	ItemString helper;

	void GetMemoryUsage(ICrySizer * s) const
	{
		s->AddObject(effect);
		s->AddObject(helper);
	}
};

struct SLaserParams
{   
	SLaserParams() 
	{ 
		laser_geometry_tp.clear();
		laser_dot[0].clear(); laser_dot[1].clear();
		laser_range[0] = laser_range[1] = 50.0f;
		laser_thickness[0] = laser_thickness[1] = 1.f;
		show_dot = true;
	}

	void CacheResources(CItemGeometryCache& geometryCache) const;

	void GetMemoryUsage(ICrySizer * s) const
	{
		s->AddObject(laser_dot[0]); 
		s->AddObject(laser_dot[1]);
		s->AddObject(laser_geometry_tp);
	}

	string		laser_geometry_tp;
	string		laser_dot[2];
	float			laser_range[2];
	float			laser_thickness[2];
	bool			show_dot;
};

struct SFlashLightParams
{   
	SFlashLightParams();

	void CacheResources(CItemMaterialAndTextureCache& textureCache) const;

	ItemString lightCookie;
	Vec3 color;
	float diffuseMult;
	float specularMult;
	float HDRDynamic;
	float distance;
	float fov;
	int		style;
	float	animSpeed;

	Vec3 fogVolumeColor;
	float fogVolumeRadius;
	float fogVolumeSize;
	float fogVolumeDensity;
};

enum EItemCategoryType
{
	eICT_None						= 0,
	eICT_Primary				= BIT(0),
	eICT_Secondary			= BIT(1),
	eICT_Heavy					= BIT(2),
	eICT_Explosive			= BIT(3),
	eICT_Grenade				= BIT(4),
	eICT_Medium					= BIT(5),
	eICT_Special				= BIT(6),
	eDF_All							= 0xFFFF
};

int GetItemCategoryType( const char* category );
int GetItemCategoryTypeByClass( IEntityClass* pClass );

struct SAccessoryParams
{
	void GetMemoryUsage(ICrySizer * s) const
	{
		s->AddObject(attach_helper);
		s->AddObject(switchToFireMode);
		s->AddObject(secondaryFireMode);
		s->AddObject(zoommode);
		s->AddObject(zoommodeSecondary);
		s->AddObject(select);
		s->AddObject(select_empty);
		s->AddObject(category);
		
		s->AddContainer(firemodes);		
		s->AddContainer(disableFiremodes);		
	}

	IEntityClass* pAccessoryClass;

	ItemString		attach_helper;
	ItemString		show_helper;
	ItemString		switchToFireMode;
	ItemString		secondaryFireMode;
	ItemString		zoommode;
	ItemString		zoommodeSecondary;
	ItemString		select;
	ItemString		select_empty;
	ItemString		category;
	std::vector<ItemString>	firemodes;
	std::vector<ItemString>	disableFiremodes; 
	XmlNodeRef		tempAccessoryParams;

	SWeaponStatsData weaponStats;

	float reloadSpeedMultiplier;

	bool exclusive;
	bool selectable;		//Whether the attachment appears on the infiction weapon customization menu for selection (Currently only used in MP)
	bool defaultAccessory;
	bool client_only;
	bool attachToOwner;
	bool enableBaseModifier;
	bool alsoAttachDefault;
	bool extendsMagazine;
	bool beginsDetached; 
};

struct SGeometryDef
{
	SGeometryDef()
		: modelPath("")
		, pos(Vec3(ZERO))
		, angles(Ang3(ZERO))
		, scale(1.0f)
		, slot(eIGS_Last)
		, useCgfStreaming(false)
		, useParentMaterial(false)
	{};

	ItemString modelPath;
	ItemString material;
	Vec3 pos;
	Ang3 angles;
	float scale;
	int slot;
	bool useCgfStreaming;
	bool useParentMaterial;

	void GetMemoryUsage(ICrySizer * s) const
	{
		s->AddObject(modelPath);
		s->AddObject(material);
	}
};


struct SAttachmentHelper
{
	ItemString	name;
	ItemString	bone;
	int			slot;
	void GetMemoryUsage(ICrySizer * s) const
	{		
		s->AddObject(name);
		s->AddObject(bone);
	}
};

struct SCachedItemAnimation
{

	enum ECacheVariableFlags
	{
		eCVF_Hand		 = BIT(0),
		eCVF_Suffix	 = BIT(1)
	};

	SCachedItemAnimation( const uint32 hash, const int8 hand, const uint32 suffixHash, const uint32 modelHash)
		: m_stringHash(hash)
		, m_hand(hand)
		, m_suffixHash(suffixHash)
		, m_animationId(-1)
		, m_modelHash(modelHash)
		, m_variableFlags(0)
	{}

	inline uint32 GetChangeFlags(const SCachedItemAnimation &n) const
	{
		uint32 ret = 0;
		ret |= ((n.m_stringHash!=m_stringHash) << eCF_String);
		ret |= ((n.m_hand!=m_hand) << eCF_Hand);
		ret |= ((n.m_suffixHash!=m_suffixHash) << eCF_Suffix);

		return ret;
	}

	bool operator==( const SCachedItemAnimation &n ) const
	{
		return	(n.m_stringHash == m_stringHash) && 
				(n.m_modelHash == m_modelHash) &&
						((n.m_hand == m_hand) || IsVariableFlagNotSet(eCVF_Hand)) && 
						((n.m_suffixHash == m_suffixHash) || IsVariableFlagNotSet(eCVF_Suffix));
	}

	ILINE void SetAnimationId(int animationId) { m_animationId = animationId; }
	ILINE int GetAnimationId() const { return m_animationId; }
	void GetMemoryUsage( ICrySizer *pSizer ) const{}
	ILINE void SetVariableFlag(ECacheVariableFlags flag) { m_variableFlags |= flag; }
	ILINE bool IsVariableFlagNotSet(ECacheVariableFlags flag) const { return ((m_variableFlags&flag) == 0); } 

private:

	uint32				m_suffixHash;
	uint32				m_stringHash;
	uint32				m_modelHash;
	int					m_animationId;
	int8					m_hand;
	int8					m_variableFlags;
};

struct SAnimationPreCache
{
	ItemString DBAfile;
	bool			 thirdPerson;
};

class CItemSharedParams : public IGameSharedParameters
{
public:

	typedef std::vector<SDamageLevel>								TDamageLevelVector;
	typedef std::vector<SGeometryDef>								TGeometryDefVector;
	typedef std::vector<SAnimationPreCache>					TAnimationPrecacheVector;

	CItemSharedParams();
	virtual ~CItemSharedParams();

	//IGameSharedParameters
	virtual void GetMemoryUsage(ICrySizer *s) const;
	virtual const char* GetDataType() const { return "ItemParams"; }
	virtual void ReleaseLevelResources();
	//~IGameSharedParameters

	static const SLaserParams& GetDefaultLaserParameters();

	bool ReadItemParams(const XmlNodeRef& rootNode);
	void ReadOverrideItemParams(const XmlNodeRef& overrideParamsNode);

	void CacheResources(CItemResourceCache& itemResourceCache, const IEntityClass* pItemClass);
	void CacheResourcesForLevelStartMP(CItemResourceCache& itemResourceCache, const IEntityClass* pItemClass);

	const SGeometryDef* GetGeometryForSlot(eGeometrySlot geomSlot) const;
	void LoadGeometryForItem(CItem* pItem, eGeometrySlot skipSlot = eIGS_Last) const;

	TAccessoryParamsVector	accessoryparams;
	THelperVector						helpers;
	SParams									params;
	TDamageLevelVector			damageLevels;
	TDefaultAccessories			defaultAccessories;
	TInitialSetup						initialSetup;
	TAccessoryAmmoMap				bonusAccessoryAmmo;
	TAccessoryAmmoMap				accessoryAmmoCapacity;
	SLaserParams*						pLaserParams;
	SFlashLightParams*			pFlashLightParams;
	SMountParams*						pMountParams;
		
	ItemString	animationGroup;
	TAnimationPrecacheVector animationPrecache;

protected:
	bool ReadParams(const XmlNodeRef& paramsNode);
	bool ReadGeometry(const XmlNodeRef& paramsNode);
	bool ReadDamageLevels(const XmlNodeRef& paramsNode);
	bool ReadAccessories(const XmlNodeRef& paramsNode);
	bool ReadAccessoryParams(const XmlNodeRef& paramsNode, SAccessoryParams* params);
	bool ReadAccessoryAmmo(const XmlNodeRef& paramsNode);
	bool ReadLaserParams(const XmlNodeRef& paramsNode);
	bool ReadFlashLightParams(const XmlNodeRef& paramsNode);
	int	 TargetToSlot(const char* name);
	void PrefixPathIfFilename(const char* pPath, ItemString& filename);

	bool ItemClassUsesDefaultLaser(const IEntityClass* pItemClass) const;

	TGeometryDefVector			geometry;
	
};

#endif //__ITEMSHAREDPARAMS_H__
