// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __AMMOPARAMS_H__
#define __AMMOPARAMS_H__

#pragma once

#include <IItemSystem.h>
#include <IWeapon.h>
#include "ItemString.h"
#include <IGameRulesSystem.h>

class CItemParticleEffectCache;
class CItemMaterialAndTextureCache;
class CItemResourceCache;

struct SCollisionEffectParams
{
	string	effect;						
	string	sound;
	float		scale;

	void PreCacheLevelResources(CItemParticleEffectCache& particleCache);

	SCollisionEffectParams(const XmlNodeRef& collisionParamsNode);
};

struct SCollisionParams
{
	// Collision damage handling
	float										damageScale;		// collision damage is the product of speed, projectile mass and this scale factor
	float										damageLimit;		// collision damage is capped to this value. If it's negative it means the damage is not capped

	SCollisionEffectParams*	pEffectParams; 

	void PreCacheLevelResources(CItemParticleEffectCache& particleCache);

	SCollisionParams(const XmlNodeRef& collisionParamsNode);
	~SCollisionParams();
};


struct SExplosionParams
{
	float minRadius;
	float maxRadius;
	float soundRadius;
	float minPhysRadius;
	float maxPhysRadius;
	float pressure;
	float holeSize;
	float terrainHoleSize;
	string effectName;
	string failedEffectName;
	float effectScale;
	string type;
	uint8	hitTypeId;
	float maxblurdist;
	EFriendyFireType friendlyFire;

	void PreCacheLevelResources(CItemParticleEffectCache& particleCache);

	SExplosionParams(const XmlNodeRef&  explosionParamsNode);
};

struct SFlashbangParams
{
	float maxRadius;
	float blindAmount;
	float flashbangBaseTime;

	SFlashbangParams(const XmlNodeRef&  flashbangParamsNode);
};

struct STrailParams
{
	string sound;
	string effect;
	string effect_fp;
	float	 scale;
	bool	 prime;

	void PreCacheLevelResources(CItemParticleEffectCache& particleCache);

	STrailParams(const XmlNodeRef&  trailParamsNode);
};

struct SWhizParams
{
	string audioTriggerName;
	float	 distanceSq;

	void PreCacheLevelResources();

	SWhizParams(const XmlNodeRef&  whizParamsNode);
};

struct SLTagGrenadeParams
{
	struct SCommonParams
	{
		SCommonParams();

		void ReadParams(const XmlNodeRef&  paramsNode);
		void PreCacheLevelResources(CItemParticleEffectCache& particleCache, CItemMaterialAndTextureCache& materialCache);

		float		m_ltagLifetime;
		float		m_safeExplosion;
		float		m_innerBoxDimension;
		float		m_boxDimension;
		float		m_armTime;

		string	m_armSound;
		string	m_disarmSound;
		string	m_armedSound;
		string	m_bounceSound;

		string  m_fireMaterial;
		string  m_armMaterial;
		string  m_disarmMaterial;
		string  m_armedMaterial;
		string  m_bounceMaterial;

		string	m_armedSafeMaterial;
		string	m_armedSafeTrail;

		string  m_fireTrail;
		string  m_armTrail;
		string  m_disarmTrail;
		string  m_armedTrail;
		string  m_bounceTrail;

		string	m_explosionEffect;

		bool		m_detonateOnImpact;
		bool		m_detonateOnActorImpact;
		bool		m_detonateWithProximity;
	};

	SCommonParams m_sticky;
	SCommonParams m_ricochet;

	SLTagGrenadeParams(const XmlNodeRef&  paramsNode);
};

struct SHomingSwarmParams
{
	float targetDistance;
	float maxSpeed;
	float proxyRadius;

	float hommingSpeed;
	float hommingDelay;

	float wanderDistance;
	float wanderSphereRadius;
	float wanderRand;
	float wanderFrequency;
	float wanderDampingTime;

	float curlFrequency;
	float curlAmplitude;
	float curlDelay;

	float targetMissExplodeDistance;

	SHomingSwarmParams(const XmlNodeRef&  paramsNode);
};


class SMikeBulletParams
{
public:
	struct SBurnParams
	{
		SBurnParams();

		void ReadParams(const XmlNodeRef&  paramsNode);
		void PreCacheLevelResources(CItemParticleEffectCache& particleCache);

		string m_impactEffectName;
		string m_effectName;
		float m_radius;
		float m_buildUp;
		float m_dissipation;
		float m_dissipationDelay;
		float m_damagePerSecond;
		float m_damagePerSecond_AI;
	};

	struct SEntry
	{
		SEntry(const char* materialTypeName)
			: m_materialTypeName(materialTypeName)
			, m_materialTypeId(0) {}

		ItemString m_materialTypeName;
		int m_materialTypeId;
	};

	typedef std::pair<SEntry, int> TBurnSurfaceIndexPair;
	typedef std::pair<IEntityClass*, int> TBurnClassIndexPair;
	typedef std::vector<TBurnSurfaceIndexPair> TBurnSurfaceIndexMap;
	typedef std::vector<TBurnClassIndexPair> TBurnClassIndexMap;	
	typedef std::vector<SBurnParams> TBurnParams;

	SMikeBulletParams(const XmlNodeRef&  paramsNode);

	const SBurnParams* GetBurnClassParams(IEntityClass* pClass) const;
	const SBurnParams* GetBurnSurfaceParams(int surfaceType) const;
	void PreCacheLevelResources(CItemParticleEffectCache& particleCache);

private:
	void CacheMaterialIds();

	bool m_covertedIndexes;
	SBurnParams m_defaultBurnParams;
	TBurnSurfaceIndexMap m_burnSurfaceIndices;
	TBurnClassIndexMap m_burnClassIndices;
	TBurnParams m_burnEffects;
};


struct SGrenadeParams
{
	SGrenadeParams(const XmlNodeRef&  paramsNode);

	float onImpactLifetime;
	bool	detonateOnActorImpact;
	bool	detonateOnImpact;
	bool	allowDetonationDelay;
};

struct SHomingMissileParams
{
	SHomingMissileParams(const XmlNodeRef&  paramsNode);

	float m_accel;
	float m_turnSpeed;
	float m_maxSpeed;
	float m_cruiseAltitude;
	float m_alignAltitude;
	float m_descendDistance;
	float	m_lazyness;
	float m_cruiseActivationTime;

	float			m_maxTargetDistance;
	float			m_lockedTimer;
	float     m_detonationRadius;

	bool			m_bTracksChaff;
	bool			m_controlled;
	bool      m_autoControlled;
	bool			m_cruise;

	string		m_launchEffect;

	void PreCacheLevelResources(CItemParticleEffectCache& particleCache);

};

struct SKVoltParams
{
	SKVoltParams(const XmlNodeRef&  paramsNode);

	ItemString				friendlyEffect;
	ItemString				enemyEffect;
	float							damageRadius;
	float							directHitDamageMultiplier;

	void PreCacheLevelResources(CItemParticleEffectCache& particleCache);
};

struct SPierceabilityParams
{
	enum EPierceabilityLevels
	{
		ePL_NonPierceable = 0,		//Pierceability 0
		ePL_Level1,								//Pierceability 1, 2, 3
		ePL_Level2,								//Pierceability 4, 5, 6
		ePL_Level3,								//Pierceability 7, 8, 9
		ePL_MaxLevel
	};

	SPierceabilityParams()
		: maxPenetrationThickness (0.5f)
	{
		for (int i = 0; i < ePL_MaxLevel; i++)
		{
			damageFallOffLevels[i] = 100.0f;
		}
	}

	ILINE bool DestroyOnImpact(uint32 pierceability) const
	{
		return (pierceability < s_kMaxPierceability);
	}

	ILINE bool DestroyOnWaterImpact() const
	{
		return (gEnv->bMultiplayer == false);
	}

	float GetDamageFallOffForPierceability(uint32 pierceability) const
	{
		assert(pierceability >= 0);

		static const int	kPierceabilityToPenetrationLevel[s_kMaxPierceability] = {
			ePL_NonPierceable,
			ePL_Level1, ePL_Level1, ePL_Level1,
			ePL_Level2, ePL_Level2, ePL_Level2,
			ePL_Level3, ePL_Level3, ePL_Level3,
		};
		
		return (pierceability < s_kMaxPierceability) ? damageFallOffLevels[kPierceabilityToPenetrationLevel[pierceability]] : 0.0f;
	}

	ILINE bool SurfaceRequiresBackFaceCheck(uint32 pierceability) const
	{
		return ((pierceability > 0) && (pierceability <s_kMaxPierceability));
	}

	float damageFallOffLevels[ePL_MaxLevel];
	float maxPenetrationThickness;

private:

	static const uint32 s_kMaxPierceability = 10;
};

struct SBulletTimeParams
{
	SBulletTimeParams(const XmlNodeRef&  paramsNode);

	ItemString				geometryName;
	ItemString				effectName;
	bool							disableRifling;
	bool							always;

	void PreCacheLevelResources(CItemResourceCache& resourceCache);
};



struct SLightningBoltParams
{
	SLightningBoltParams(const XmlNodeRef&  paramsNode);

	float strikeTime;
	float maxSpreadRadius;
	float spreadDamageMult;
	float impulseStrength;
	float maxDamageDistanceThreshold;
	string chargeEffect;
	string dryEffect;
	string strikeEffect;
	string sparkEffect;
	string material;
	string lightning;

	void PreCacheLevelResources(CItemMaterialAndTextureCache& materialCache, CItemParticleEffectCache& particleCache);
};


struct SC4ExplosiveParams
{
	SC4ExplosiveParams(const XmlNodeRef&  paramsNode);

	string	armedMaterial;
	string	disarmedMaterial;

	ColorF	armedLightColour;
	ColorF	disarmedLightColour;

	float		lightRadius;
	float		specularMultiplier;
	float		pulseMinColorMultiplier;
	float		pulseBeatsPerSecond;

	void PreCacheLevelResources(CItemResourceCache& resourceCache);
};

struct SHazardAmmoParams
{
	SHazardAmmoParams(const XmlNodeRef&  paramsNode);

	// The maximum look ahead distance for constructing the waring area (>= 0.0f) (in meters).
	float	maxHazardDistance;

	// The radius of the hazard warning area (>= 0.0f) (in meters).
	float	hazardRadius;

	// How far to nudge the start of the warn area in the forwards direction from 
	// the pivot position (in case the gun sticks out quite a bit for example) 
	// (in meters). Negative values will nudge backwards.
	float	startPosNudgeOffset;

	// How much we allow the hazard area approximation to deviate from the actual position of
	// its source (>= 0.0f) (in meters).
	float	maxHazardApproxPosDeviation;

	// How much we allow the hazard area approximation deviate from the actual movement
	// direction of its source (>= 0.0f) (in degrees).
	float	maxHazardApproxAngleDeviationDeg;
};

struct SElectricProjectileParams
{
	SElectricProjectileParams(const XmlNodeRef&  paramsNode);

	void PreCacheLevelResources(CItemParticleEffectCache& particleCache);

	float  inWaterRange;
	string hitEffect;
};

// this structure holds cached XML attributes for fast acccess
struct SAmmoParams
{
	//flags
	uint32	flags;
	bool	serverSpawn;
	bool	predictSpawn;
	bool	reusable;

	// common parameters
	float	lifetime;
	float	showtime;
	float armTime;
	float sleepTime;
	float safeExplosion;
	float mpProjectileDestructDelay;
	int		bulletType;
	int		hitPoints;
	int		m_hitRecoilId;
	int		m_hitRecoilIdArmorMode;
	uint16 aiType;
	bool  noBulletHits;
	bool	quietRemoval;

	ItemString	display_name;
	ItemString	material_name;
	ItemString	ammo_category;

	// physics parameters
	EPhysicalizationType	physicalizationType;
	float mass;
	float speed;
	int		bounceableBullet;
	int		maxLoggedCollisions;
	bool	traceable;
	bool	trackOnHud;
	Vec3	spin;
	Vec3	spinRandom;

	mutable int					surfaceTypeId;
	pe_params_particle*		pParticleParams;
	SPierceabilityParams	pierceabilityParams;

	// firstperson geometry
	string		fpGeometryName;
	Matrix34	fpLocalTM;
	
	SCollisionParams*			pCollision;
	SExplosionParams*			pExplosion;
	SFlashbangParams*			pFlashbang;
	SWhizParams*					pWhiz;
	SWhizParams*					pRicochet;
	STrailParams*					pTrail;
	STrailParams*					pTrailUnderWater;
	SLTagGrenadeParams*		pLTagParams;
	SHomingSwarmParams*			pHomingSwarmParams;
	SMikeBulletParams*		pMikeBulletParams;
	SGrenadeParams*				pGrenadeParams;
	SHomingMissileParams* pHomingParams;
	SKVoltParams* pKvoltParams;
	SBulletTimeParams*		pBulletTimeParams;
	SLightningBoltParams*		pLightningBoltParams;
	SC4ExplosiveParams*		pC4ExplosiveParams;
	SHazardAmmoParams*			pHazardParams;
	SElectricProjectileParams* pElectricProjectileParams;

	const IEntityClass*			pEntityClass;

	SAmmoParams(const XmlNodeRef&  ammoParamsNode = XmlNodeRef(NULL), const IEntityClass* pEntityClass_=0);
	~SAmmoParams();

	void Init(const XmlNodeRef&  ammoParamsNode, const IEntityClass* pEntityClass_);

	void CacheResources() const;

	void GetMemoryUsage(ICrySizer *pSizer) const;

	SExplosionParams*	GetExplosionParams(int index) const;

private:
	void LoadFlagsAndParams(const XmlNodeRef&  ammoParamsNode);
	void LoadPhysics(const XmlNodeRef&  ammoParamsNode);
	void LoadGeometry(const XmlNodeRef&  ammoParamsNode);
	void LoadCollision(const XmlNodeRef&  ammoParamsNode);
	void LoadExplosion(const XmlNodeRef&  ammoParamsNode);
	void LoadFlashbang(const XmlNodeRef&  ammoParamsNode);
	void LoadTrailsAndWhizzes(const XmlNodeRef&  ammoParamsNode);
	void LoadLTagGrenadeParams(const XmlNodeRef&  ammoParamsNode);
	void LoadHomingSwarmParams(const XmlNodeRef&  ammoParamsNode);
	void LoadMikeBulletParams(const XmlNodeRef&  ammoParamsNode);
	void LoadGrenadeParams(const XmlNodeRef&  ammoParamsNode);
	void LoadHomingMissileParams(const XmlNodeRef&  ammoParamsNode);
	void LoadKVoltParams(const XmlNodeRef&  ammoParamsNode);
	void LoadBulletTimeParams(const XmlNodeRef&  ammoParamsNode);
	void LoadLightningBoltParams(const XmlNodeRef&  ammoParamsNode);
	void LoadC4ExplosiveParams(const XmlNodeRef&  ammoParamsNode);
	void LoadHazardParams(const XmlNodeRef&  ammoParamsNode);
	void LoadElectriProjectileParams(const XmlNodeRef& ammoParamsNode);

	SExplosionParams**		pExplosions;
	int m_explosion_count;
};

#endif//__AMMOPARAMS_H__
