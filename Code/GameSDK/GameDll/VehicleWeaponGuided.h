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
#if !defined(__VEHICLE_WEAPON_GUIDED_H__)
#define __VEHICLE_WEAPON_GUIDED_H__


#include "VehicleWeapon.h"

struct IVehicle;
struct IVehiclePart;
struct IVehicleSeat;
struct IVehicleAnimation;

class CVehicleWeaponGuided : public CVehicleWeapon
{
public:

	CVehicleWeaponGuided();
	virtual ~CVehicleWeaponGuided() {};
  
	// CWeapon
	virtual void ReadProperties(IScriptTable *pScriptTable);
	virtual bool Init(IGameObject * pGameObject);
	virtual void PostInit(IGameObject * pGameObject);
	virtual void Reset();

	virtual void StartFire();

	virtual void SetDestination(const Vec3& pos);
	virtual const Vec3& GetDestination();

	virtual void Update(SEntityUpdateContext& ctx, int update);

	//Vec3 GetSlotHelperPos(int slot, const char *helper, bool worldSpace, bool relative) const;

protected:

	enum eWeaponGuidedState
	{
		eWGS_INVALID,
		eWGS_PREPARATION,
		eWGS_WAIT,
		eWGS_FIRE,
		eWGS_FIRING,
		eWGS_POSTSTATE,
	};

	eWeaponGuidedState	m_State;
	eWeaponGuidedState	m_NextState;

	Vec3				m_DesiredHomingTarget;
	
	string				m_VehicleAnimName;
	string				m_PreStateName;
	string				m_PostStateName;

	float					m_firedTimer;
	
	IVehicleAnimation	*m_pVehicleAnim;
	int					m_PreState;
	int					m_PostState;
};



#endif //#if !defined(__VEHICLE_WEAPON_GUIDED_H__)