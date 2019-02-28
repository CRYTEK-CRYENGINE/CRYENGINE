// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __VEHICLEDAMAGEBEHAVIORSPAWNDEBRIS_H__
#define __VEHICLEDAMAGEBEHAVIORSPAWNDEBRIS_H__

class CVehiclePartAnimated;

//! When triggered, this behavior spawns geometry with a randomized impulse and an optional particle effect.
//! The debris is automatically generated from all the vehicle animated parts that have a configured destroyed geometry.
//! The geometry will be spawned from the joint the part is attached to.
//! This a behaviour applied to the entire vehicle on destruction.
class CVehicleDamageBehaviorSpawnDebris
	: public IVehicleDamageBehavior
{
	IMPLEMENT_VEHICLEOBJECT
public:

	CVehicleDamageBehaviorSpawnDebris();
	~CVehicleDamageBehaviorSpawnDebris();

	virtual bool Init(IVehicle* pVehicle, const CVehicleParams& table) override;
	virtual void Reset() override;
	virtual void Release() override                                         { delete this; }

	virtual void Serialize(TSerialize ser, EEntityAspects aspects) override {}
	virtual void Update(const float deltaTime) override;

	virtual void OnDamageEvent(EVehicleDamageBehaviorEvent event, const SVehicleDamageBehaviorEventParams& params) override;
	virtual void OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params) override;

	virtual void GetMemoryUsage(ICrySizer* s) const override;

protected:

	IEntity* SpawnDebris(IStatObj* pStatObj, Matrix34 vehicleTM, float force);
	void     AttachParticleEffect(IEntity* pDetachedEntity);
	void     GiveImpulse(IEntity* pDetachedEntity, float force);

	IVehicle* m_pVehicle;
	string    m_particleEffect;

	typedef std::list<EntityId> TEntityIdList;
	TEntityIdList m_spawnedDebris;

	struct SDebrisInfo
	{
		CVehiclePartAnimated* pAnimatedPart;
		int                   jointId; // id of the joint in the intact model
		int                   slot;
		int                   index;
		EntityId              entityId;
		float                 time;
		float                 force;
		void                  GetMemoryUsage(class ICrySizer* pSizer) const {}
	};

	typedef std::list<SDebrisInfo> TDebrisInfoList;
	TDebrisInfoList m_debris;
	bool            m_pickableDebris;
};

#endif
