// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Implements a standard class for a vehicle component

   -------------------------------------------------------------------------
   History:
   - 12:10:2005: Created by Mathieu Pinard

*************************************************************************/
#ifndef __VEHICLECOMPONENT_H__
#define __VEHICLECOMPONENT_H__

#include "Vehicle.h"
#include "VehicleDamages.h"
#include "ISharedParamsManager.h"
#include "SharedParams/ISharedParams.h"

class CVehicleHUDElement;
class CVehicle;
struct IVehiclePart;

#define COMPONENT_DAMAGE_LEVEL_RATIO 0.25f

class CVehicleComponent
	: public IVehicleComponent
{
public:

	CVehicleComponent();
	~CVehicleComponent();

	bool Init(IVehicle* pVehicle, const CVehicleParams& paramsTable);
	void Reset();
	void Release() { delete this; }

	void GetMemoryUsage(ICrySizer* s) const;

	// IVehicleComponent
	virtual unsigned int  GetPartCount() const;
	virtual IVehiclePart* GetPart(unsigned int index) const;
	virtual const AABB& GetBounds(); // bounds are in vehicle space
	virtual float       GetDamageRatio() const;
	virtual void        SetDamageRatio(float ratio);
	virtual float       GetMaxDamage() const;

	virtual const char* GetComponentName() const
	{
		return m_pSharedParams ? m_pSharedParams->name.c_str() : NULL;
	}
	// ~IVehicleComponent

	const string GetName() const
	{
		return m_pSharedParams ? m_pSharedParams->name : string();
	}

	ILINE bool IsHull() const
	{
		return m_pSharedParams ? m_pSharedParams->isHull : false;
	}

	void OnHit(const HitInfo& hitType, const TVehicleComponentVector* pAffectedComponents = 0);
	void OnVehicleDestroyed();
	void Update(float deltaTime);
	void BroadcastDamage(float damage, EntityId shooterId);

	void Serialize(TSerialize ser, EEntityAspects aspects);

	void AddPart(IVehiclePart* pPart);

#if ENABLE_VEHICLE_DEBUG
	void DebugDraw();
#endif

	bool IsMajorComponent() const
	{
		CRY_ASSERT(m_pSharedParams != 0);

		return m_pSharedParams->isMajorComponent;
	}

	void  SetProportionOfVehicleHealth(float proportion);
	float GetProportionOfVehicleHealth() const { return m_proportionOfVehicleHealth; }

protected:

	float ProcessDamage(const HitInfo& hitInfo, bool impact, const TVehicleComponentVector* pAffectedComponents);

	CVehicle* m_pVehicle;

	AABB      m_bounds;

	float     m_damage;

	struct SDamageBehaviorParams
	{
		float damageRatioMin;
		float damageRatioMax;
		bool  ignoreOnVehicleDestroyed;

		void  GetMemoryUsage(ICrySizer* pSizer) const
		{
		}
	};

	typedef std::pair<SDamageBehaviorParams, IVehicleDamageBehavior*> TVehicleDamageBehaviorPair;
	typedef std::vector<TVehicleDamageBehaviorPair>                   TVehicleDamageBehaviorVector;

	TVehicleDamageBehaviorVector m_damageBehaviors;

	typedef std::vector<IVehiclePart*> TVehiclePartVector;
	TVehiclePartVector m_parts;

	float              m_proportionOfVehicleHealth;

	Vec3               m_lastHitLocalPos;
	float              m_lastHitRadius;
	int                m_lastHitType;

	BEGIN_SHARED_PARAMS(SSharedParams)

	inline SSharedParams() : damageMax(0.0f), hullAffection(1.0f), isHull(false), isMajorComponent(true), isOnlyDamageableByPlayer(false), useBoundsFromParts(false), useDamageLevels(true)
	{
		bounds.Reset();
	}

	string                              name;
	AABB                                bounds;
	float                               damageMax;
	float                               hullAffection;
	bool                                isHull;
	bool                                isMajorComponent;
	bool                                isOnlyDamageableByPlayer;
	bool                                useBoundsFromParts;
	bool                                useDamageLevels;
	CVehicleDamages::TDamageMultipliers damageMultipliersByHitType;
	CVehicleDamages::TDamageMultipliers damageMultipliersByProjectile;

	END_SHARED_PARAMS

	SSharedParamsConstPtr GetSharedParams(const string& vehicleComponentName, const CVehicleParams& paramsTable) const;

	SSharedParamsConstPtr m_pSharedParams;
};

#endif
