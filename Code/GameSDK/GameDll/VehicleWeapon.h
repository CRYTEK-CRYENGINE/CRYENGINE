// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: VehicleWeapon Implementation

-------------------------------------------------------------------------
History:
- 20:04:2006   13:01 : Created by Márcio Martins

*************************************************************************/
#ifndef __VEHICLE_WEAPON_H__
#define __VEHICLE_WEAPON_H__

#if _MSC_VER > 1000
# pragma once
#endif


#include <IItemSystem.h>
#include <CryAnimation/CryCharAnimationParams.h>
#include "Weapon.h"
#include <IVehicleSystem.h>

struct IVehicle;
struct IVehiclePart;
struct IVehicleSeat;

class CVehicleWeapon: public CWeapon, public IVehicleEventListener
{
public:

  CVehicleWeapon();
  virtual ~CVehicleWeapon();
  
  // CWeapon
  virtual bool Init(IGameObject * pGameObject);
  virtual void PostInit(IGameObject * pGameObject);
  virtual void Reset();

  virtual void MountAtEntity(EntityId entityId, const Vec3 &pos, const Ang3 &angles);

	virtual void StartUse(EntityId userId);
	virtual void StopUse(EntityId userId);
  virtual bool FilterView(SViewParams& viewParams);
	virtual void ApplyViewLimit(EntityId userId, bool apply) {}; // should not be done for vehicle weapons

  virtual void StartFire();
   
  virtual void Update(SEntityUpdateContext& ctx, int update);

  virtual void SetAmmoCount(IEntityClass* pAmmoType, int count);
  virtual void SetInventoryAmmoCount(IEntityClass* pAmmoType, int count);

  virtual void UpdateIKMounted(IActor* pActor, const Vec3& vGunXAxis);
  virtual bool CanZoom() const;

	virtual void UpdateFPView(float frameTime);

	virtual void GetMemoryUsage(ICrySizer * s) const
	{
		s->AddObject(this, sizeof(*this));
		CWeapon::GetInternalMemoryUsage(s); // collect memory of parent class
	}

  virtual bool ApplyActorRecoil() const { return m_bOwnerInSeat; }  
  virtual bool IsVehicleWeapon() const { return true; }
  // ~CWeapon

	//IVehicleEventListener
	void OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params);
	//~IVehicleEventListener

	virtual void UpdateCurrentActionController();

	IVehicle* GetVehicle() const;

protected:

  bool CheckWaterLevel() const;
  void CheckForFriendlyAI(float frameTime);
	void CheckForFriendlyPlayers(float frameTime);

	enum EAudioCacheType
	{
		eACT_None,
		eACT_fp,
		eACT_3p,
	};
	void AudioCache( bool enable, bool isThirdPerson = true );
	void GetCacheName(const IEntityClass* pClass, const bool bIsThirdPerson, CryFixedStringT<32> &outCacheName);

	EntityId m_vehicleId;
	EAudioCacheType m_audioCacheType;
	bool m_bOwnerInSeat;

private:
  float   m_dtWaterLevelCheck;
};


#endif//__VEHICLE_WEAPON_H__
