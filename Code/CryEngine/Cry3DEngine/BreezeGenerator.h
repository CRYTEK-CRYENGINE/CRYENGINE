// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Cry3DEngineBase.h"
#include <Cry3DEngine/IBreezeGenerator.h>

struct SBreeze;

// Spawns wind volumes around the camera to emulate breezes
class CBreezeGenerator : public IBreezeGenerator, public Cry3DEngineBase
{
	friend class C3DEngine;
public:
	CBreezeGenerator();
	~CBreezeGenerator();

	void           Initialize() override;
	void           Shutdown() override;

	virtual void   SetEnabled(bool enabled) override                  { m_enabled = enabled; }
	virtual bool   GetEnabled() const override                        { return m_enabled;  }

	virtual void   SetStrength(float strength) override               { m_strength = strength; }
	virtual float  GetStrength() const override                       { return m_strength; }

	virtual void   SetMovementSpeed(float movementSpeed) override     { m_movement_speed = movementSpeed; }
	virtual float  GetMovementSpeed() const override                  { return m_movement_speed; }

	virtual void   SetVariance(float variance) override               { m_variance = variance; }
	virtual float  GetVariance() const override                       { return m_variance; }

	virtual void   SetLifetime(float lifetime) override               { m_lifetime = lifetime; }
	virtual float  GetLifetime() const override                       { return m_lifetime; }

	virtual void   SetCount(uint32 count) override                    { m_count = count; }
	virtual uint32 GetCount() const override                          { return m_count; }

	virtual void   SetSpawnRadius(float spawnRadius) override         { m_spawn_radius = spawnRadius; }
	virtual float  GetSpawnRadius() const override                    { return m_spawn_radius; }

	virtual void   SetSpread(float spread) override                   { m_spread = spread; }
	virtual float  GetSpread() const override                         { return m_spread; }

	virtual void   SetRadius(float radius) override                   { m_radius = radius; }
	virtual float  GetRadius() const override                         { return m_radius; }

	virtual void   SetAwakeThreshold(float awakeThreshSpeed) override { m_awake_thresh = awakeThreshSpeed; }
	virtual float  GetAwakeThreshold() const override                 { return m_awake_thresh; }

private:
	void Reset();
	void Update();

	// The array of active breezes
	SBreeze* m_breezes;

	float    m_spawn_radius;
	float    m_spread;
	uint32   m_count;
	float    m_radius;
	float    m_lifetime;
	float    m_variance;
	float    m_strength;
	float    m_movement_speed;
	Vec3     m_wind_speed;
	float    m_fixed_height;
	float    m_awake_thresh;
	bool     m_enabled;
};
