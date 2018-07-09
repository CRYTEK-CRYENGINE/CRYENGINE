// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Item interface.

   -------------------------------------------------------------------------
   History:
   - 29:11:2005   13:52 : Created by Márcio Martins

*************************************************************************/
#ifndef __IITEM_H__
#define __IITEM_H__

#pragma once

#include "IGameObject.h"

enum EViewMode
{
	eIVM_Hidden      = 0,
	eIVM_FirstPerson = 1,
	eIVM_ThirdPerson = 2,
};

struct IWeapon;
struct IInventory;

// Summary
//   Interface to implement a new Item class
struct IItem : public IGameObjectExtension
{
	enum eItemBackAttachment
	{
		eIBA_Primary = 0,
		eIBA_Secondary,
		eIBA_Unknown
	};

	enum eItemHand
	{
		eIH_Right = 0, // indicates the right hand of the actor
		eIH_Left,      // indicates the left hand of the actor
		eIH_Last,
	};

	virtual const char* GetType() const = 0;

	// Summary
	//   Retrieves the owner id
	virtual EntityId GetOwnerId() const = 0;
	virtual void     SetOwnerId(EntityId ownerId) = 0;

	virtual void     EnableUpdate(bool enable, int slot = -1) = 0;
	virtual void     RequireUpdate(int slot) = 0;

	//-------------------- ACTIONS --------------------
	virtual void ForcePendingActions(uint8 blockedActions = 0) = 0;

	//-------------------- EVENTS --------------------
	virtual void OnAction(EntityId actorId, const ActionId& actionId, int activationMode, float value) = 0;
	virtual void OnParentSelect(bool select) = 0;
	virtual void OnAttach(bool attach) = 0;
	virtual void OnPickedUp(EntityId actorId, bool destroyed) = 0;
	virtual void OnHit(float damage, int hitType) = 0;

	//-------------------- HOLDING --------------------
	virtual void         Select(bool select) = 0;
	virtual bool         IsSelected() const = 0;
	virtual bool         CanSelect() const = 0;
	virtual bool         CanDeselect() const = 0;
	virtual void         RemoveOwnerAttachedAccessories() = 0;

	virtual void         Physicalize(bool enable, bool rigid) = 0;
	virtual bool         CanDrop() const = 0;
	virtual void         Drop(float impulseScale = 1.0f, bool selectNext = true, bool byDeath = false) = 0;
	virtual void         UpdateFPView(float frameTime) = 0;
	virtual Vec3         GetMountedAngleLimits() const = 0;
	virtual void         PickUp(EntityId picker, bool sound, bool select = true, bool keepHistory = true, const char* setup = NULL) = 0;
	virtual void         MountAtEntity(EntityId entityId, const Vec3& pos, const Ang3& angles) = 0;
	virtual bool         FilterView(struct SViewParams& viewParams) = 0;
	virtual void         RemoveAllAccessories() = 0;
	virtual void         DetachAllAccessories() = 0;
	virtual void         AttachAccessory(IEntityClass* pClass, bool attach, bool noanim, bool force = false, bool firstTimeAttached = false, bool initialLoadoutSetup = false) = 0;
	virtual void SetCurrentActionController(class IActionController* pActionController) = 0;
	virtual void         UpdateCurrentActionController() = 0;
	virtual const string GetAttachedAccessoriesString(const char* separator = ",") = 0;

	virtual void         SetHand(int hand) = 0;

	virtual void         StartUse(EntityId userId) = 0;
	virtual void         StopUse(EntityId userId) = 0;
	virtual void         SetBusy(bool busy) = 0;
	virtual bool         IsBusy() const = 0;
	virtual bool         CanUse(EntityId userId) const = 0;
	virtual bool         IsUsed() const = 0;
	virtual void         Use(EntityId userId) = 0;

	virtual bool         AttachToHand(bool attach, bool checkAttachment = false) = 0;
	virtual bool         AttachToBack(bool attach) = 0;

	//-------------------- SETTINGS --------------------
	virtual bool        IsModifying() const = 0;
	virtual bool        CheckAmmoRestrictions(IInventory* pInventory) = 0;
	virtual void        Reset() = 0;
	virtual bool        ResetParams() = 0;
	virtual void        PreResetParams() = 0;
	virtual bool        GivesAmmo() = 0;
	virtual const char* GetDisplayName() const = 0;
	virtual void        HideItem(bool hide) = 0;

	virtual void        SetSubContextID(int tagContext) = 0;
	virtual int         GetSubContextID() = 0;

	//-------------------- INTERFACES --------------------
	virtual IWeapon*       GetIWeapon() = 0;
	virtual const IWeapon* GetIWeapon() const = 0;

	// Summary
	// Returns
	//   true if the item is an accessory
	virtual bool IsAccessory() = 0;

	//used to serialize item attachment when loading next level
	virtual void SerializeLTL(TSerialize ser) = 0;

	// gets the original (not current) direction vector of a mounted weapon
	virtual Vec3 GetMountedDir() const = 0;
};

#endif // __IITEM_H__
