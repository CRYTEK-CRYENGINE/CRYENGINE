// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------

Description: Generates water ripplets when moving across a water surface

-------------------------------------------------------------------------
History:
- 17:05:2012: Created by Benito Gangoso Rodriguez

*************************************************************************/

#ifndef __WATER_RIPPLES_GENERATOR_H__
#define __WATER_RIPPLES_GENERATOR_H__

#pragma once

#include <IGameObject.h>


#define WATER_RIPPLES_EDITING_ENABLED 1


class CWaterRipplesGenerator : public CGameObjectExtensionHelper<CWaterRipplesGenerator, IGameObjectExtension>
{

	struct SProperties
	{
		SProperties()
			: m_scale(1.0f)
			, m_strength(1.0f)
			, m_frequency(1.0f)
			, m_randFrequency(0.0f)
			, m_randScale(0.0f)
			, m_randStrength(0.0f)
			, m_enabled(true)
			, m_autoSpawn(false)
			, m_spawnOnMovement(true)
			, m_randomOffset(0,0)
		{

		}

		void InitFromScript( const IEntity& entity );

		float m_scale;
		float m_strength;
		float m_frequency;
		float m_randFrequency;
		float m_randScale;
		float m_randStrength;
		bool  m_enabled;
		bool  m_autoSpawn;
		bool m_spawnOnMovement;
		Vec2 m_randomOffset;
	};

public:
	CWaterRipplesGenerator();
	virtual ~CWaterRipplesGenerator();

	// IGameObjectExtension
	virtual bool Init(IGameObject *pGameObject);
	virtual void InitClient(int channelId) {};
	virtual void PostInit(IGameObject *pGameObject);
	virtual void PostInitClient(int channelId) {};
	virtual bool ReloadExtension( IGameObject * pGameObject, const SEntitySpawnParams &params );
	virtual void PostReloadExtension( IGameObject * pGameObject, const SEntitySpawnParams &params ) {}
	virtual bool GetEntityPoolSignature( TSerialize signature );
	virtual void Release();
	virtual bool NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int pflags) { return true; }
	virtual void FullSerialize(TSerialize ser);
	virtual void PostSerialize() {}
	virtual void SerializeSpawnInfo( TSerialize ser ) {}
	virtual ISerializableInfoPtr GetSpawnInfo() {return 0;}
	virtual void Update( SEntityUpdateContext &ctx, int updateSlot);
	virtual void PostUpdate(float frameTime ) {};
	virtual void PostRemoteSpawn() {};
	virtual void HandleEvent( const SGameObjectEvent &gameObjectEvent );
	virtual void ProcessEvent(const SEntityEvent& );
	virtual uint64 GetEventMask() const;
	virtual void ProcessHit(bool isMoving);
	virtual void SetChannelId(uint16 id) {}
	virtual void GetMemoryUsage(ICrySizer *pSizer) const { pSizer->Add(*this); }

	//~IGameObjectExtension

private:

	void Reset();
	void ActivateGeneration( const bool activate );

	SProperties m_properties;
	float				m_lastSpawnTime;

#if WATER_RIPPLES_EDITING_ENABLED
	bool m_currentLocationOk;
#endif

};

#endif //__WATER_RIPPLES_GENERATOR_H__