// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __VEHICLEDAMAGEBEHAVIORINDICATOR_H__
#define __VEHICLEDAMAGEBEHAVIORINDICATOR_H__

#include "IVehicleSystem.h"

class CVehicle;

//! Light or sound indicator according to damage ratio.
class CVehicleDamageBehaviorIndicator
	: public IVehicleDamageBehavior
{
	IMPLEMENT_VEHICLEOBJECT
public:

	CVehicleDamageBehaviorIndicator();
	virtual ~CVehicleDamageBehaviorIndicator() {}

	virtual bool Init(IVehicle* pVehicle, const CVehicleParams& table) override;
	virtual void Reset() override;
	virtual void Release() override { delete this; }

	virtual void Serialize(TSerialize ser, EEntityAspects aspects) override;
	virtual void Update(const float deltaTime) override;

	virtual void OnDamageEvent(EVehicleDamageBehaviorEvent event, const SVehicleDamageBehaviorEventParams& behaviorParams) override;
	virtual void OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params) override {}

	virtual void GetMemoryUsage(ICrySizer* s) const override;

protected:
	void GetMaterial();
	void SetActive(bool active);
	void EnableLight(bool enable);

	CVehicle* m_pVehicle;

	bool      m_isActive;
	float     m_ratioMin;
	float     m_ratioMax;
	float     m_currentDamageRatio;

	// Light
	string          m_material;
	IMaterial*      m_pSubMtl;

	string          m_sound;
	float           m_soundRatioMin;
	IVehicleHelper* m_pHelper;

	int             m_soundsPlayed;
	float           m_lastDamageRatio;

	float           m_lightUpdate;
	float           m_lightTimer;
	bool            m_lightOn;

	static float    m_frequencyMin;
	static float    m_frequencyMax;
};

#endif
