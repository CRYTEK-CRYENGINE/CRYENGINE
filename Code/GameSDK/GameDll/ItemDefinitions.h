// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Definitions for typedefs and enumerated types specific to items

-------------------------------------------------------------------------
History:
- 22:10:2009   10:15 : Created by Claire Allan

*************************************************************************/
#ifndef __ITEMDEFINITIONS_H__
#define __ITEMDEFINITIONS_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include "ItemString.h"

struct SAttachmentHelper;
struct SAccessoryParams;
struct SItemAction;
struct SCachedItemAnimation;
struct SLayer;
struct SParentFireModeParams;
struct SParentZoomModeParams;
struct SAccessoryMeleeParams;
struct SPlayerMovementModifiers;
struct SZoomModeParams;

#define ITEM_MAX_NUM_ACCESSORIES (4)

typedef CryFixedStringT<256>																		TempResourceName;
typedef std::vector<SAttachmentHelper>													THelperVector;
typedef CryFixedArray<IEntityClass*, 8>													TDefaultAccessories;
typedef CryFixedArray<IEntityClass*, ITEM_MAX_NUM_ACCESSORIES>	TInitialSetup;
typedef std::map<IEntityClass*, int>														TAccessoryAmmoMap;
typedef std::vector<SAccessoryParams>														TAccessoryParamsVector;
typedef	std::vector<SCachedItemAnimation>												TAnimationIdsCache;
typedef std::map<ItemString, SLayer>														TLayerMap;
typedef std::vector<SParentFireModeParams>											TParentFireModeParamsVector;
typedef std::vector<SParentZoomModeParams>											TParentZoomModeParamsVector;
typedef std::vector<SAccessoryMeleeParams>											TParentMeleeParamsVector;

struct SWeaponAmmo
{
	SWeaponAmmo()
		: pAmmoClass(NULL)
		, count(0)
	{

	}

	SWeaponAmmo(IEntityClass* pClass, int ammoCount)
		: pAmmoClass(pClass)
		, count(ammoCount)
	{

	}

	void GetMemoryUsage(ICrySizer * s) const {}

	IEntityClass* pAmmoClass;
	int						count;
};

typedef std::vector<SWeaponAmmo>						TAmmoVector;

struct SWeaponAmmoUtils
{
	static const SWeaponAmmo* FindAmmoConst(const TAmmoVector& ammoVector, IEntityClass* pAmmoType)
	{
		const int ammoCount = ammoVector.size();
		for (int i = 0; i < ammoCount; ++i)
		{
			if (pAmmoType == ammoVector[i].pAmmoClass)
			{
				return &(ammoVector[i]); 
			}
		}

		return NULL;
	}

	static SWeaponAmmo* FindAmmo(TAmmoVector& ammoVector, IEntityClass* pAmmoType)
	{
		const int ammoCount = ammoVector.size();
		for (int i = 0; i < ammoCount; ++i)
		{
			if (pAmmoType == ammoVector[i].pAmmoClass)
			{
				return &(ammoVector[i]); 
			}
		}

		return NULL;
	}

	// returns true if the the ammo type was already found in the ammoVector
	static bool SetAmmo(TAmmoVector& ammoVector, IEntityClass* pClass, int count)
	{
		SWeaponAmmo* pAmmo = SWeaponAmmoUtils::FindAmmo(ammoVector, pClass);
		if (pAmmo != NULL)
		{
			pAmmo->count = count;
			return true;
		}
		else
		{
			ammoVector.push_back(SWeaponAmmo(pClass, count));
			return false;
		}
	}

	static int GetAmmoCount(const TAmmoVector& ammoVector, IEntityClass* pClass)
	{
		const SWeaponAmmo* pAmmo = SWeaponAmmoUtils::FindAmmoConst(ammoVector, pClass);
		
		return pAmmo ? pAmmo->count : 0;
	}
};

enum eItemChangeFlags
{
	eCF_Hand	,
	eCF_Suffix,
	eCF_String
};

struct MountedTPAimAnim
{
	enum Type
	{
		Up, 
		Down,
		Total
	};
};

struct WeaponAimAnim
{
#	define WeaponAimAnimList(f)		\
		f(Base)						\
		f(BaseModifier)				\
		f(BaseModifier3P)			\
		f(Up)						\
		f(Down)						\
		f(Left)						\
		f(Right)					\
		f(Front)					\
		f(Back)						\
		f(StrafeLeft)				\
		f(StrafeRight)				\
		f(Bump)						\
		f(Run)						\
		f(Idle)						\
		f(Sprint)					\
		f(Crouch)					\
		f(Total)					\

	AUTOENUM_BUILDENUMWITHTYPE(Type, WeaponAimAnimList);
	static const char* s_weaponAimAnimNames[];
};

struct WeaponAnimLayerID
{
#	define WeaponAnimLayerIDList(f)	\
		f(Base)						\
		f(BaseModifier)				\
		f(Vert)						\
		f(Horiz)					\
		f(Front)					\
		f(Side)						\
		f(Run)						\
		f(Idle)						\
		f(Bump)						\
		f(Total)					\

	AUTOENUM_BUILDENUMWITHTYPE(Type, WeaponAnimLayerIDList);
	static const char* s_weaponAnimLayerIDNames[];
};

enum eGeometrySlot
{
	eIGS_FirstPerson = 0,
	eIGS_ThirdPerson,
	eIGS_Owner,
	eIGS_OwnerAnimGraph,
	eIGS_OwnerAnimGraphLooped,
	eIGS_Aux0,
	eIGS_Destroyed,
	eIGS_Aux1,
	eIGS_ThirdPersonAux,
	eIGS_Last,
	eIGS_LastAnimSlot = eIGS_OwnerAnimGraphLooped + 1,
	eIGS_LastLayerSlot = eIGS_ThirdPerson + 1
};


enum ELTAGGrenadeType
{
	ELTAGGrenadeType_RICOCHET,
	ELTAGGrenadeType_STICKY,
	ELTAGGrenadeType_LAST
};

struct SItemAnimationEvents
{
	SItemAnimationEvents()
		: m_initialized(false)
	{

	}

	void Init()
	{
		if (!m_initialized)
		{
			m_ltagUpdateGrenades = CCrc32::ComputeLowercase("UpdateGrenades");
		}

		m_initialized = true;
	}

	uint32 m_ltagUpdateGrenades;

private:
	bool   m_initialized;
};

struct SItemFragmentTagCRCs
{
	void Init()
	{
		ammo_clipRemaining = CCrc32::ComputeLowercase("ammo_clipRemaining");
		ammo_empty = CCrc32::ComputeLowercase("ammo_empty");
		ammo_last1 = CCrc32::ComputeLowercase("ammo_last1");
		ammo_last2 = CCrc32::ComputeLowercase("ammo_last2");
		ammo_last3 = CCrc32::ComputeLowercase("ammo_last3");

		inventory_last1 = CCrc32::ComputeLowercase("inventory_last1");
		inventory_last2 = CCrc32::ComputeLowercase("inventory_last2");

		doubleclip_fast = CCrc32::ComputeLowercase("doubleclip_fast");

		ammo_partiallycharged = CCrc32::ComputeLowercase("ammo_partiallycharged");
		ammo_fullycharged = CCrc32::ComputeLowercase("ammo_fullycharged");

		holo = CCrc32::ComputeLowercase("holo");
		typhoonAttachment = CCrc32::ComputeLowercase("typhoonAttachment");
	}
	
	uint32 ammo_clipRemaining;
	uint32 ammo_empty;
	uint32 ammo_last1;
	uint32 ammo_last2;
	uint32 ammo_last3;

	uint32 inventory_last2;
	uint32 inventory_last1;

	uint32 doubleclip_fast;

	uint32 ammo_partiallycharged;
	uint32 ammo_fullycharged;

	uint32 holo;
	uint32 typhoonAttachment;

};

struct SItemActionParamCRCs
{
	void Init()
	{
		concentratedFire = CCrc32::ComputeLowercase("concentration");
		ammoLeft = CCrc32::ComputeLowercase("ammo_left");
		burstFire = CCrc32::ComputeLowercase("burst");
		spinSpeed = CCrc32::ComputeLowercase("spinSpeed");
		ffeedbackScale = CCrc32::ComputeLowercase("forceFeedback");
		fired = CCrc32::ComputeLowercase("fired");
		firstFire = CCrc32::ComputeLowercase("firstFire");
		bowChargeLevel = CCrc32::ComputeLowercase("ChargeLevel");
		bowChargeSound = CCrc32::ComputeLowercase("charge");
		bowFireIntensity = CCrc32::ComputeLowercase("intensity");
		bowStrength = CCrc32::ComputeLowercase("strength");
		aimDirection = CCrc32::ComputeLowercase("aim_direction");
		zoomTransition = CCrc32::ComputeLowercase("zoom_transition");
		inputMove = CCrc32::ComputeLowercase("input_move");
		inputRot = CCrc32::ComputeLowercase("input_rot");
		velocity = CCrc32::ComputeLowercase("velocity");
		fallFactor = CCrc32::ComputeLowercase("fall_factor");
	}

	uint32 concentratedFire;
	uint32 ammoLeft;
	uint32 burstFire;
	uint32 spinSpeed;
	uint32 ffeedbackScale;
	uint32 fired;
	uint32 firstFire;
	uint32 bowChargeLevel;
	uint32 bowChargeSound;
	uint32 bowFireIntensity;
	uint32 bowStrength;
	uint32 aimDirection;
	uint32 zoomTransition;
	uint32 inputMove;
	uint32 inputRot;
	uint32 velocity;
	uint32 fallFactor;
};

struct SItemStrings
{
	SItemStrings();
	~SItemStrings();

	ItemString nw;									// "nw";

	ItemString left_item_attachment; // "left_item_attachment";
	ItemString right_item_attachment;// "right_item_attachment";

	ItemString bottom;						// "bottom";
	ItemString barrel;						// "barrel";
	ItemString scope;							// "scope";
	ItemString ammo;							// "ammo";
};

extern struct SItemStrings* g_pItemStrings;

#endif //__ITEMDEFINITIONS_H__
