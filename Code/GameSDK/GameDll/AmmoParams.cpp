// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AmmoParams.h"
#include <CryAISystem/IAgent.h>
#include "GameRules.h"
#include "Effects/GameEffects/HitRecoilGameEffect.h"
#include "ItemResourceCache.h"
#include "GameParameters.h"
#include <CryAnimation/ICryAnimation.h>
#include <CryCore/TypeInfo_impl.h>

#include "GameXmlParamReader.h"

#include "WeaponSystem.h"

SCollisionParams::SCollisionParams(const XmlNodeRef&  collisionParamsNode)
: damageScale(0.2f)
, damageLimit(0.0f)
{
	CGameXmlParamReader reader(collisionParamsNode);

	// Collision damage handling
	reader.ReadParamValue("damageScale", damageScale);
	reader.ReadParamValue("damageLimit", damageLimit);

	// effect params
	pEffectParams = new SCollisionEffectParams(collisionParamsNode);
	if (pEffectParams->effect.empty() && pEffectParams->sound.empty())
	{
		SAFE_DELETE(pEffectParams);
	}
}

SCollisionParams::~SCollisionParams()
{
	SAFE_DELETE(pEffectParams);
}

void SCollisionParams::PreCacheLevelResources(CItemParticleEffectCache& particleCache)
{
	if (pEffectParams)
		pEffectParams->PreCacheLevelResources(particleCache);
}

SCollisionEffectParams::SCollisionEffectParams(const XmlNodeRef&  collisionParamsNode)
: effect("")
, sound("")
, scale(1.0f)
{
	CGameXmlParamReader reader(collisionParamsNode);

	sound = reader.ReadParamValue("sound");
	reader.ReadParamValue<float>("scale", scale);
	effect = reader.ReadParamValue("effect");
}

void SCollisionEffectParams::PreCacheLevelResources(CItemParticleEffectCache& particleCache)
{
	particleCache.CacheParticle(effect);
}

SExplosionParams::SExplosionParams(const XmlNodeRef& explosionParamsNode)
: minRadius(2.5f)
, maxRadius(5.0f)
, soundRadius(5.0f)
, minPhysRadius(2.5f)
, maxPhysRadius(5.0f)
, pressure(200.0f)
, holeSize(0.0f)
, terrainHoleSize(3.0f)
, effectScale(1)
, maxblurdist(10)
, hitTypeId(0)
, effectName("")
, friendlyFire(eFriendyFireNone)
, type("frag")
{
	CGameXmlParamReader reader(explosionParamsNode);

	reader.ReadParamValue<float>("max_radius", maxRadius);
	minRadius = maxRadius * 0.8f;
	maxPhysRadius = min(maxRadius, 5.0f);
	minPhysRadius = maxPhysRadius * 0.8f;

	reader.ReadParamValue<float>("min_radius", minRadius);
	reader.ReadParamValue<float>("sound_radius", soundRadius);
	reader.ReadParamValue<float>("min_phys_radius", minPhysRadius);
	reader.ReadParamValue<float>("max_phys_radius", maxPhysRadius);
	reader.ReadParamValue<float>("pressure", pressure);
	reader.ReadParamValue<float>("hole_size", holeSize);
	reader.ReadParamValue<float>("terrain_hole_size", terrainHoleSize);
	effectName = reader.ReadParamValue("effect");
	failedEffectName = reader.ReadParamValue("failed_effect");
	reader.ReadParamValue<float>("effect_scale", effectScale);
	reader.ReadParamValue<float>("radialblurdist", maxblurdist);
	type = reader.ReadParamValue("type", "frag");

	const char* friendlyFireType = reader.ReadParamValue("friendly_fire", "none");

	if(strcmpi(friendlyFireType, "none") == 0)
	{
		friendlyFire = eFriendyFireNone;
	}
	else if (strcmpi(friendlyFireType, "self") == 0)
	{
		friendlyFire = eFriendyFireSelf;
	}
	else if(strcmpi(friendlyFireType, "team") == 0)
	{
		friendlyFire = eFriendyFireTeam;
	}
	else
	{
		CRY_ASSERT_MESSAGE(false, "Unrecognised friendly_fire type");
	}
}

void SExplosionParams::PreCacheLevelResources(CItemParticleEffectCache& particleCache)
{
	particleCache.CacheParticle(effectName);
	particleCache.CacheParticle(failedEffectName);

	CGameRules *pGameRules = g_pGame->GetGameRules();
	if (pGameRules)
	{
		hitTypeId = pGameRules->GetHitTypeId(type.c_str());
	}
}

SFlashbangParams::SFlashbangParams(const XmlNodeRef& flashbangParamsNode)
: maxRadius(25.0f),
blindAmount(0.7f),
flashbangBaseTime(2.5f)
{
	CGameXmlParamReader reader(flashbangParamsNode);

	reader.ReadParamValue<float>("max_radius", maxRadius);
	reader.ReadParamValue<float>("blindAmount", blindAmount);
	reader.ReadParamValue<float>("flashbangBaseTime", flashbangBaseTime);
}

STrailParams::STrailParams(const XmlNodeRef& trailParamsNode)
: scale(1.0f)
, prime(false)
{
	CGameXmlParamReader reader(trailParamsNode);

	sound = reader.ReadParamValue("sound");

	effect = reader.ReadParamValue("effect");
	effect_fp = reader.ReadParamValue("effect_fp");

	reader.ReadParamValue<float>("scale", scale);
	reader.ReadParamValue<bool>("prime", prime);
}

void STrailParams::PreCacheLevelResources(CItemParticleEffectCache& particleCache)
{
	particleCache.CacheParticle(effect);
	particleCache.CacheParticle(effect_fp);

	REINST("needs verification!");
	/*if (!audioTriggerId.empty())
	{
		gEnv->pSoundSystem->Precache(audioTriggerId.c_str(), 0, FLAG_SOUND_PRECACHE_EVENT_DEFAULT);
	}*/
}

SWhizParams::SWhizParams(const XmlNodeRef& whizParamsNode)
: distanceSq(4.7f)
{
	CGameXmlParamReader reader(whizParamsNode);

	audioTriggerName = reader.ReadParamValue("sound");
	reader.ReadParamValue<float>("distance", distanceSq);
	distanceSq = distanceSq * distanceSq;
}

void SWhizParams::PreCacheLevelResources()
{
	/*if (!audioTriggerId.empty())
	{
		gEnv->pSoundSystem->Precache(audioTriggerId.c_str(), 0, FLAG_SOUND_PRECACHE_EVENT_DEFAULT);
	}*/
}

SLTagGrenadeParams::SLTagGrenadeParams(const XmlNodeRef& paramsNode)
{
	CGameXmlParamReader reader(paramsNode);

	m_sticky.ReadParams(reader.FindFilteredChild("Sticky"));
	m_ricochet.ReadParams(reader.FindFilteredChild("Ricochet"));
}

SLTagGrenadeParams::SCommonParams::SCommonParams()
: m_safeExplosion(0.0f)
, m_innerBoxDimension(1.0f)
, m_boxDimension(3.0f)
, m_armTime(2.0f)
, m_ltagLifetime(0.0f)
, m_detonateOnImpact(false)
, m_detonateOnActorImpact(false)
, m_detonateWithProximity(true)
{
}


namespace
{



	void PushBackSurfaceString(SMikeBulletParams::TBurnSurfaceIndexMap* pBurnSurfaceIndexMap, const char* pString, int index)
	{
		SMikeBulletParams::SEntry entry(pString);
		pBurnSurfaceIndexMap->push_back(SMikeBulletParams::TBurnSurfaceIndexPair(entry, index));
	}



	void PushBackClassString(SMikeBulletParams::TBurnClassIndexMap* pBurnClassIndexMap, const char* pString, int index)
	{
		IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(pString);
		if(pClass)
		{
			pBurnClassIndexMap->push_back(SMikeBulletParams::TBurnClassIndexPair(pClass, index));
		}
	}



	template<class Key>
	struct ParseString
	{
		typedef std::vector<std::pair<Key, int> > TVectorMap;
		typedef void (*TPushBackFn)(TVectorMap* pVectorMap, const char* pString, int index);

		void operator () (const unsigned char* pString, int materialIndex, TVectorMap* pVectorMap, TPushBackFn pushBackFunction)
		{
			const int stringSize = 128;
			if(pString)
			{
				char subString[stringSize];
				for (int idx = 0; ; ++pString)
				{
					if (isspace(*pString))
						continue;

					if (!*pString || *pString == ';')
					{
						subString[idx] = 0;

						pushBackFunction(pVectorMap, &subString[0], materialIndex);

						idx = 0;
						if (!*pString)
							break;
					}
					else
					{
						subString[idx] = *pString;
						++idx;
					}
				}
			}
		}
	};


}



SMikeBulletParams::SMikeBulletParams(const XmlNodeRef& paramsNode)
:	m_covertedIndexes(false)
{
	CGameXmlParamReader reader(paramsNode);

	XmlNodeRef defaultParamsNode = reader.FindFilteredChild("default");
	if (defaultParamsNode)
	{
		m_defaultBurnParams.ReadParams(defaultParamsNode);
	}

	const int numChildren = reader.GetUnfilteredChildCount();
	for (int i = 0; i < numChildren; ++i)
	{
		XmlNodeRef childNode = reader.GetFilteredChildAt(i);
		
		if (childNode && strcmp(childNode->getTag(), "effect")==0)
		{
			CGameXmlParamReader childReader(childNode);
			
			const unsigned char* materialTypes = (unsigned char*)childReader.ReadParamValue("material");
			const unsigned char* classTypes = (unsigned char*)childReader.ReadParamValue("class");
			
			if (materialTypes || classTypes)
			{
				SBurnParams materialBurnParams;
				materialBurnParams.ReadParams(childNode);

				m_burnEffects.push_back(materialBurnParams);
				int index = (int)(m_burnEffects.size() - 1);

				ParseString<SEntry>()(materialTypes, index, &m_burnSurfaceIndices, &PushBackSurfaceString);
				ParseString<IEntityClass*>()(classTypes, index, &m_burnClassIndices, &PushBackClassString);	
			}	
		}
	}
}



SMikeBulletParams::SBurnParams::SBurnParams()
	:	m_radius(0.75f)
	,	m_buildUp(0.2f)
	,	m_dissipation(0.2f)
	,	m_dissipationDelay(0.0f)
	,	m_damagePerSecond(0.0f)
	,	m_damagePerSecond_AI(0.0f)
{
}


void SMikeBulletParams::SBurnParams::ReadParams(const XmlNodeRef& paramsNode)
{
	CGameXmlParamReader reader(paramsNode);

	m_impactEffectName = reader.ReadParamValue("impactEffectName");
	m_effectName = reader.ReadParamValue("effectName");
	reader.ReadParamValue<float>("radius", m_radius);
	reader.ReadParamValue<float>("buildUp", m_buildUp);
	reader.ReadParamValue<float>("dissipation", m_dissipation);
	reader.ReadParamValue<float>("dissipationDelay", m_dissipationDelay);
	reader.ReadParamValue<float>("damagePerSecond", m_damagePerSecond);
	m_damagePerSecond_AI = m_damagePerSecond;
	reader.ReadParamValue<float>("damagePerSecond_AI", m_damagePerSecond_AI);
}

void SMikeBulletParams::SBurnParams::PreCacheLevelResources(CItemParticleEffectCache& particleCache)
{
	particleCache.CacheParticle(m_impactEffectName);
	particleCache.CacheParticle(m_effectName);
}

const SMikeBulletParams::SBurnParams* SMikeBulletParams::GetBurnClassParams(IEntityClass* pClass) const
{
	CRY_ASSERT(pClass);

	for (size_t i = 0; i < m_burnClassIndices.size(); ++i)
	{
		if (m_burnClassIndices[i].first == pClass)
			return &m_burnEffects[m_burnClassIndices[i].second];
	}
	return NULL;
}

const SMikeBulletParams::SBurnParams* SMikeBulletParams::GetBurnSurfaceParams(int surfaceType) const
{
	for (size_t i = 0; i < m_burnSurfaceIndices.size(); ++i)
	{
		if (m_burnSurfaceIndices[i].first.m_materialTypeId == surfaceType)
			return &m_burnEffects[m_burnSurfaceIndices[i].second];
	}
	return &m_defaultBurnParams;
}

void SMikeBulletParams::PreCacheLevelResources(CItemParticleEffectCache& particleCache)
{
	CacheMaterialIds();

	m_defaultBurnParams.PreCacheLevelResources(particleCache);
	for (TBurnParams::iterator burnIt = m_burnEffects.begin(); burnIt != m_burnEffects.end(); ++burnIt)
		burnIt->PreCacheLevelResources(particleCache);
}

void SMikeBulletParams::CacheMaterialIds()
{
	if (m_covertedIndexes)
		return;

	m_covertedIndexes = true;
	for (TBurnSurfaceIndexMap::iterator burnIt = m_burnSurfaceIndices.begin(); burnIt != m_burnSurfaceIndices.end(); ++burnIt)
	{
		ISurfaceType* pSurfaceType = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeManager()->GetSurfaceTypeByName(burnIt->first.m_materialTypeName.c_str());
		if (pSurfaceType)
		{
			burnIt->first.m_materialTypeId = pSurfaceType->GetId();
		}
	}
}



void SLTagGrenadeParams::SCommonParams::ReadParams(const XmlNodeRef& paramsNode)
{
	if (paramsNode == NULL)
		return;

	CGameXmlParamReader reader(paramsNode);

	reader.ReadParamValue<float>("ltagLifetime", m_ltagLifetime);
	reader.ReadParamValue<float>("safeexplosion", m_safeExplosion);
	reader.ReadParamValue<float>("inner_box_dimension", m_innerBoxDimension);
	reader.ReadParamValue<float>("box_dimension", m_boxDimension);
	reader.ReadParamValue<float>("armTime", m_armTime);

	m_armSound = reader.ReadParamValue("armSound");
	m_disarmSound = reader.ReadParamValue("disarmSound");
	m_armedSound = reader.ReadParamValue("armedSound");
	m_bounceSound = reader.ReadParamValue("bounceSound");

	m_fireMaterial = reader.ReadParamValue("fireMaterial");
	m_armMaterial = reader.ReadParamValue("armMaterial");
	m_disarmMaterial = reader.ReadParamValue("disarmMaterial");
	m_armedMaterial = reader.ReadParamValue("armedMaterial");
	m_bounceMaterial = reader.ReadParamValue("bounceMaterial");
	m_armedSafeMaterial = reader.ReadParamValue("armedSafeMaterial");

	m_fireTrail = reader.ReadParamValue("fireTrail");
	m_armTrail = reader.ReadParamValue("armTrail");
	m_disarmTrail = reader.ReadParamValue("disarmTrail");
	m_armedTrail = reader.ReadParamValue("armedTrail");
	m_bounceTrail = reader.ReadParamValue("bounceTrail");
	m_armedSafeTrail = reader.ReadParamValue("armedSafeTrail");

	m_explosionEffect = reader.ReadParamValue("explosionEffect");

	reader.ReadParamValue<bool>("detonateOnImpact", m_detonateOnImpact);
	reader.ReadParamValue<bool>("detonateOnActorImpact", m_detonateOnActorImpact);
	reader.ReadParamValue<bool>("detonateWithProximity", m_detonateWithProximity);
}

void SLTagGrenadeParams::SCommonParams::PreCacheLevelResources(CItemParticleEffectCache& particleCache, CItemMaterialAndTextureCache& materialCache)
{
	//Effects
	particleCache.CacheParticle(m_armedSafeTrail.c_str());
	particleCache.CacheParticle(m_fireTrail.c_str());
	particleCache.CacheParticle(m_armTrail.c_str());
	particleCache.CacheParticle(m_disarmTrail.c_str());
	particleCache.CacheParticle(m_armedTrail.c_str());
	particleCache.CacheParticle(m_bounceTrail.c_str());
	particleCache.CacheParticle(m_explosionEffect.c_str());

	//Sounds
	/*if (!m_armSound.empty())
	{
		gEnv->pSoundSystem->Precache(m_armSound.c_str(), 0, FLAG_SOUND_PRECACHE_EVENT_DEFAULT);
	}
	if (!m_disarmSound.empty())
	{
		gEnv->pSoundSystem->Precache(m_disarmSound.c_str(), 0, FLAG_SOUND_PRECACHE_EVENT_DEFAULT);
	}
	if (!m_armedSound.empty())
	{
		gEnv->pSoundSystem->Precache(m_armedSound.c_str(), 0, FLAG_SOUND_PRECACHE_EVENT_DEFAULT);
	}
	if (!m_bounceSound.empty())
	{
		gEnv->pSoundSystem->Precache(m_bounceSound.c_str(), 0, FLAG_SOUND_PRECACHE_EVENT_DEFAULT);
	}*/

	//Materials
	materialCache.CacheMaterial(m_fireMaterial.c_str());
	materialCache.CacheMaterial(m_armMaterial.c_str());
	materialCache.CacheMaterial(m_disarmMaterial.c_str());
	materialCache.CacheMaterial(m_armedMaterial.c_str());
	materialCache.CacheMaterial(m_bounceMaterial.c_str());
	materialCache.CacheMaterial(m_armedSafeMaterial.c_str());
}


SHomingSwarmParams::SHomingSwarmParams(const XmlNodeRef& paramsNode)
	:	targetDistance(200.0f)
	,	maxSpeed(0.0f)
	,	hommingSpeed(5.0f)
	,	hommingDelay(0.25f)
	,	wanderDistance(100.0f)
	,	wanderSphereRadius(40.0f)
	,	wanderRand(0.65f)
	,	wanderFrequency(30.0f)
	,	wanderDampingTime(0.7f)
	,	curlFrequency(1.0f)
	,	curlAmplitude(0.0f)
	,	curlDelay(0.0f)
	,	proxyRadius(0.0f)
	,	targetMissExplodeDistance(3.0f)
{
	if (paramsNode == NULL)
		return;

	CGameXmlParamReader reader(paramsNode);

	reader.ReadParamValue<float>("targetDistance", targetDistance);
	reader.ReadParamValue<float>("maxSpeed", maxSpeed);
	reader.ReadParamValue<float>("proxyRadius", proxyRadius);

	reader.ReadParamValue<float>("hommingSpeed", hommingSpeed);
	reader.ReadParamValue<float>("hommingDelay", hommingDelay);

	reader.ReadParamValue<float>("wanderDistance", wanderDistance);
	reader.ReadParamValue<float>("wanderSphereRadius", wanderSphereRadius);
	reader.ReadParamValue<float>("wanderRand", wanderRand);
	reader.ReadParamValue<float>("wanderFrequency", wanderFrequency);
	reader.ReadParamValue<float>("wanderDampingTime", wanderDampingTime);

	reader.ReadParamValue<float>("curlFrequency", curlFrequency);
	reader.ReadParamValue<float>("curlAmplitude", curlAmplitude);
	reader.ReadParamValue<float>("curlDelay", curlDelay);

	reader.ReadParamValue<float>("targetMissExplodeDistance", targetMissExplodeDistance);
}



SGrenadeParams::SGrenadeParams(const XmlNodeRef& paramsNode)
: onImpactLifetime(0.f)
, detonateOnActorImpact(false)
, detonateOnImpact(false)
, allowDetonationDelay(true)
{
	CRY_ASSERT(paramsNode != NULL);

	CGameXmlParamReader reader(paramsNode);

	reader.ReadParamValue<float>("onImpactLifetime", onImpactLifetime);
	reader.ReadParamValue<bool>("detonateOnActorImpact", detonateOnActorImpact);
	reader.ReadParamValue<bool>("detonateOnImpact", detonateOnImpact);
	reader.ReadParamValue<bool>("allowDetonationDelay", allowDetonationDelay);
}


SHomingMissileParams::SHomingMissileParams( const XmlNodeRef& paramsNode )
: m_accel(10.0f)
, m_turnSpeed(2.0f)
, m_maxSpeed(20.0f)
, m_cruiseAltitude(100.0f)
, m_alignAltitude(50.0f)
, m_descendDistance(50.0f)
, m_lazyness(0.35f)
, m_bTracksChaff (false)
, m_cruise(true)
, m_controlled(false)
, m_autoControlled(false)
, m_detonationRadius(0.0f)
, m_lockedTimer(0.2f)
, m_maxTargetDistance(200.0f)
, m_cruiseActivationTime(0.0f)
{
	CRY_ASSERT(paramsNode != NULL);

	CGameXmlParamReader reader(paramsNode);

	reader.ReadParamValue<float>("cruise_altitude", m_cruiseAltitude);
	reader.ReadParamValue<float>("accel", m_accel);
	reader.ReadParamValue<float>("turn_speed", m_turnSpeed);
	reader.ReadParamValue<float>("max_speed", m_maxSpeed);
	reader.ReadParamValue<float>("align_altitude", m_alignAltitude);
	reader.ReadParamValue<float>("descend_distance", m_descendDistance);
	reader.ReadParamValue<float>("max_target_distance", m_maxTargetDistance);
	reader.ReadParamValue<bool>("cruise", m_cruise);
	reader.ReadParamValue<bool>("controlled", m_controlled);
	reader.ReadParamValue<bool>("autoControlled",m_autoControlled);
	reader.ReadParamValue<float>("lazyness",m_lazyness);
	reader.ReadParamValue<float>("initial_delay",m_lockedTimer);
	reader.ReadParamValue<float>("detonation_radius",m_detonationRadius);
	reader.ReadParamValue<bool>("tracks_chaff", m_bTracksChaff);
	reader.ReadParamValue<float>("cruiseActivationTime", m_cruiseActivationTime);
	m_launchEffect = reader.ReadParamValue("launch_effect");
}

void SHomingMissileParams::PreCacheLevelResources(CItemParticleEffectCache& particleCache)
{
	particleCache.CacheParticle(m_launchEffect);
}

SKVoltParams::SKVoltParams(const XmlNodeRef& paramsNode)
: damageRadius(0.5f)
, directHitDamageMultiplier(1.0f)
, friendlyEffect("")
, enemyEffect("")
{
	CRY_ASSERT(paramsNode != NULL);

	CGameXmlParamReader reader(paramsNode);

	friendlyEffect = reader.ReadParamValue("friendlyEffect");
	enemyEffect = reader.ReadParamValue("enemyEffect");
	reader.ReadParamValue<float>("DamageRadius", damageRadius );
	reader.ReadParamValue<float>("DirectHitDamageMultiplier", directHitDamageMultiplier );
}

void SKVoltParams::PreCacheLevelResources(CItemParticleEffectCache& particleCache)
{
	particleCache.CacheParticle(friendlyEffect.c_str());
	particleCache.CacheParticle(enemyEffect.c_str());
}

SBulletTimeParams::SBulletTimeParams(const XmlNodeRef& paramsNode)
: geometryName("")
, effectName("")
, disableRifling(false)
, always(false)
{
	CRY_ASSERT(paramsNode != NULL);

	CGameXmlParamReader reader(paramsNode);

	geometryName = reader.ReadParamValue("geometry");
	effectName = reader.ReadParamValue("effect");
	reader.ReadParamValue<bool>("always", always );
	reader.ReadParamValue<bool>("disableRifling", disableRifling );
}

void SBulletTimeParams::PreCacheLevelResources(CItemResourceCache& resourceCache)
{
	resourceCache.GetAmmoGeometryCache().CacheGeometry(geometryName.c_str(), true);
	resourceCache.GetParticleEffectCache().CacheParticle(effectName.c_str());
}



SLightningBoltParams::SLightningBoltParams(const XmlNodeRef&  paramsNode)
	: strikeTime(0.75f)
	, maxSpreadRadius(7.0f)
	, spreadDamageMult(0.5f)
	, impulseStrength(600.0f)
	, maxDamageDistanceThreshold(0.0f)
{
	CGameXmlParamReader reader(paramsNode);

	reader.ReadParamValue<float>("strikeTime", strikeTime);
	reader.ReadParamValue<float>("maxSpreadRadius", maxSpreadRadius);
	reader.ReadParamValue<float>("spreadDamageMult", spreadDamageMult);
	reader.ReadParamValue<float>("impulseStrength", impulseStrength);
	reader.ReadParamValue<float>("maxDamageDistanceThreshold", maxDamageDistanceThreshold);
	chargeEffect = reader.ReadParamValue("chargeEffect");
	strikeEffect = reader.ReadParamValue("strikeEffect");
	sparkEffect = reader.ReadParamValue("sparkEffect");
	material = reader.ReadParamValue("material");
	lightning = reader.ReadParamValue("lightning");
}



void SLightningBoltParams::PreCacheLevelResources(CItemMaterialAndTextureCache& materialCache, CItemParticleEffectCache& particleCache)
{
	particleCache.CacheParticle(chargeEffect.c_str());
	particleCache.CacheParticle(strikeEffect.c_str());
	particleCache.CacheParticle(sparkEffect.c_str());
	materialCache.CacheMaterial(material.c_str());
}

SC4ExplosiveParams::SC4ExplosiveParams(const XmlNodeRef&  paramsNode)
	: lightRadius(5.f), specularMultiplier(0.2f), pulseMinColorMultiplier(1.f), pulseBeatsPerSecond(0)
{
	CGameXmlParamReader reader(paramsNode);

	Vec3 armColour, disarmColour;

	armedMaterial = reader.ReadParamValue("armedMaterial");
	disarmedMaterial = reader.ReadParamValue("disarmedMaterial");
	reader.ReadParamValue<Vec3>("armed_light_colour", armColour);
	reader.ReadParamValue<Vec3>("disarmed_light_colour", disarmColour);
	reader.ReadParamValue<float>("light_radius", lightRadius);
	reader.ReadParamValue<float>("specular_multiplier", specularMultiplier);
	reader.ReadParamValue<float>("pulse_min_color_multiplier", pulseMinColorMultiplier);
	reader.ReadParamValue<float>("pulse_beats_per_second", pulseBeatsPerSecond);

	armedLightColour = ColorF(armColour.x, armColour.y, armColour.z);
	disarmedLightColour = ColorF(disarmColour.x, disarmColour.y, disarmColour.z);
}

void SC4ExplosiveParams::PreCacheLevelResources(CItemResourceCache& resourceCache)
{
	CItemMaterialAndTextureCache& materialCache = resourceCache.GetMaterialsAndTextureCache();
	materialCache.CacheMaterial(armedMaterial.c_str());
	materialCache.CacheMaterial(disarmedMaterial.c_str());
}

SHazardAmmoParams::SHazardAmmoParams(const XmlNodeRef&  paramsNode)
	:	maxHazardDistance(40.0f)
	,	hazardRadius(4.0f)
	,	startPosNudgeOffset(0.0f)
	,	maxHazardApproxPosDeviation(5.0f)
	,	maxHazardApproxAngleDeviationDeg(10.0f)
{
	CGameXmlParamReader reader(paramsNode);

	reader.ReadParamValue<float>("maxHazardDistance", maxHazardDistance);
	reader.ReadParamValue<float>("hazardRadius", hazardRadius);
	reader.ReadParamValue<float>("startPosNudgeOffset", startPosNudgeOffset);
	reader.ReadParamValue<float>("maxHazardApproxPosDeviation", maxHazardApproxPosDeviation);
	reader.ReadParamValue<float>("maxHazardApproxAngleDeviationDeg", maxHazardApproxAngleDeviationDeg);
}

SElectricProjectileParams::SElectricProjectileParams(const XmlNodeRef& paramsNode)
	: inWaterRange(6.0f)
{
	CGameXmlParamReader reader(paramsNode);

	reader.ReadParamValue<float>("inWaterRange", inWaterRange);
	hitEffect = reader.ReadParamValue("hitEffect");
}



void SElectricProjectileParams::PreCacheLevelResources(CItemParticleEffectCache& particleCache)
{
	particleCache.CacheParticle(hitEffect.c_str());
}



SAmmoParams::SAmmoParams(const XmlNodeRef& ammoParamsNode, const IEntityClass* pEntityClass_)
: flags(0)
, serverSpawn(false)
, predictSpawn(false)
, reusable(false)
, lifetime(0.0f)
, safeExplosion(0.0f)
, mpProjectileDestructDelay(0.0f)
, showtime(0.0f)
, armTime(0.0f)
, aiType(AIOBJECT_NONE)
, bulletType(-1)
, hitPoints(-1)
, noBulletHits(false)
, quietRemoval(false)
, display_name("")
, ammo_category("")
, physicalizationType(ePT_None)
, mass(1.0f)
, speed(0.0f)
, maxLoggedCollisions(1)
, traceable(0)
, spin(ZERO)
, spinRandom(ZERO)
, pParticleParams(0)
, pCollision(0)
, pExplosion(0)
, pExplosions(0)
, pFlashbang(0)
, pWhiz(0)
, pRicochet(0)
, pTrail(0)
, pTrailUnderWater(0)
, pEntityClass(0)
, sleepTime(0.0f)
, pLTagParams(0)
, pHomingSwarmParams(0)
, pMikeBulletParams(0)
, pGrenadeParams(0)
, pHomingParams(0)
, pKvoltParams(0)
, pBulletTimeParams(0)
, pLightningBoltParams(0)
, pC4ExplosiveParams(0)
, pHazardParams(0)
,	pElectricProjectileParams(0)
, surfaceTypeId(-1)
, trackOnHud(false)
, bounceableBullet(0)
, m_explosion_count(0)
{
	Init(ammoParamsNode, pEntityClass_);
}

SAmmoParams::~SAmmoParams()
{
	delete pCollision;
	delete pFlashbang;
	delete pWhiz;
	delete pRicochet;
	delete pTrail;
	delete pTrailUnderWater;
	
	SAFE_DELETE_ARRAY(pExplosions);
	SAFE_DELETE(pLTagParams);
	SAFE_DELETE(pHomingSwarmParams);
	SAFE_DELETE(pMikeBulletParams);
	SAFE_DELETE(pGrenadeParams);
	SAFE_DELETE(pHomingParams);
	SAFE_DELETE(pKvoltParams);
	SAFE_DELETE(pBulletTimeParams);
	SAFE_DELETE(pParticleParams);
	SAFE_DELETE(pLightningBoltParams);
	SAFE_DELETE(pC4ExplosiveParams);
	SAFE_DELETE(pHazardParams);	
	SAFE_DELETE(pElectricProjectileParams);

	pExplosion = 0;
}

void SAmmoParams::Init(const XmlNodeRef& ammoParamsNode, const IEntityClass* pEntityClass_)
{
	pEntityClass = pEntityClass_;
	fpGeometryName = "";

	if (!ammoParamsNode || !pEntityClass)
	{
		CRY_ASSERT(0);
		return;
	}

	LoadFlagsAndParams(ammoParamsNode);
	LoadPhysics(ammoParamsNode);
	LoadGeometry(ammoParamsNode);
	LoadCollision(ammoParamsNode);
	LoadExplosion(ammoParamsNode);
	LoadFlashbang(ammoParamsNode);
	LoadTrailsAndWhizzes(ammoParamsNode);
	LoadLTagGrenadeParams(ammoParamsNode);
	LoadHomingSwarmParams(ammoParamsNode);
	LoadMikeBulletParams(ammoParamsNode);
	LoadGrenadeParams(ammoParamsNode);
	LoadHomingMissileParams(ammoParamsNode);
	LoadKVoltParams(ammoParamsNode);
	LoadBulletTimeParams(ammoParamsNode);
	LoadLightningBoltParams(ammoParamsNode);
	LoadC4ExplosiveParams(ammoParamsNode);
	LoadHazardParams(ammoParamsNode);
	LoadElectriProjectileParams(ammoParamsNode);
}

void SAmmoParams::GetMemoryUsage(ICrySizer *pSizer) const
{
	pSizer->AddObject(this, sizeof(*this));
	pSizer->AddObject(fpGeometryName);
}

SExplosionParams*	SAmmoParams::GetExplosionParams(int index) const
{
	if(pExplosions)
	{
		CRY_ASSERT_MESSAGE(m_explosion_count > index, "SAmmoParams::GetExplosionParams - Rerquesting explosion with index > size of the array");
		return pExplosions[index];
	}

	return NULL;
}

void SAmmoParams::LoadFlagsAndParams(const XmlNodeRef& ammoParamsNode)
{
	CGameXmlParamReader reader(ammoParamsNode);

	XmlNodeRef flagsNode = reader.FindFilteredChild("flags");
	if (flagsNode)
	{
		int flag=0;
		CGameXmlParamReader flagsReader(flagsNode);

		flagsReader.ReadParamValue<int>("ClientOnly", flag); flags |= flag?ENTITY_FLAG_CLIENT_ONLY:0; flag=0;
		flagsReader.ReadParamValue<int>("ServerOnly", flag); flags |= flag?ENTITY_FLAG_SERVER_ONLY:0; flag=0;
		flagsReader.ReadParamValue<bool>("ServerSpawn", serverSpawn);
		if (serverSpawn)
			flagsReader.ReadParamValue<bool>("PredictSpawn", predictSpawn);
		else
			flagsReader.ReadParamValue<bool>("Reusable", reusable);
	}

	XmlNodeRef paramsNode = reader.FindFilteredChild("params");
	if (paramsNode)
	{
		CGameXmlParamReader paramsReader(paramsNode);

		paramsReader.ReadParamValue<float>("lifetime", lifetime);
		paramsReader.ReadParamValue<float>("showtime", showtime);
		paramsReader.ReadParamValue<float>("armTime", armTime);
		paramsReader.ReadParamValue<float>("sleepTime", sleepTime);
		paramsReader.ReadParamValue<float>("safeExplosion", safeExplosion);
		paramsReader.ReadParamValue<float>("mpProjectileDestructDelay", mpProjectileDestructDelay);
		paramsReader.ReadParamValue<int>("bulletType", bulletType);
		paramsReader.ReadParamValue<int>("hitPoints", hitPoints);
		paramsReader.ReadParamValue<bool>("noBulletHits", noBulletHits);
		paramsReader.ReadParamValue<bool>("quietRemoval", quietRemoval);

		display_name = paramsReader.ReadParamValue("display_name");
		ammo_category = paramsReader.ReadParamValue("ammo_category");
		paramsReader.ReadParamValue<bool>("trackOnHud", trackOnHud);
		
		string hitRecoil, hitRecoilArmorMode;
		hitRecoil = paramsReader.ReadParamValue("hitRecoil");
		hitRecoilArmorMode = paramsReader.ReadParamValue("hitRecoilArmorMode");
		m_hitRecoilId = CHitRecoilGameEffect::GetHitRecoilId(hitRecoil);
		m_hitRecoilIdArmorMode = CHitRecoilGameEffect::GetHitRecoilId(hitRecoilArmorMode);

		const char* typeName = paramsReader.ReadParamValue("aitype");
		
		if (typeName!=0 && typeName[0])
		{
			if (!stricmp(typeName, "grenade"))
				aiType=AIOBJECT_GRENADE;
			else if (!stricmp(typeName, "rpg"))
				aiType=AIOBJECT_RPG;
		}
	}
}

void SAmmoParams::LoadPhysics(const XmlNodeRef& ammoParamsNode)
{
	CGameXmlParamReader reader(ammoParamsNode);

	XmlNodeRef physicsNode = reader.FindFilteredChild("physics");
	if (!physicsNode)
		return;

	const char *typ = physicsNode->getAttr("type");
	if (typ)
	{
		if (!strcmpi(typ, "particle"))
		{
			physicalizationType = ePT_Particle;
		}
		else if (!strcmpi(typ, "rigid"))
		{
			physicalizationType = ePT_Rigid;
		}
		else if(!strcmpi(typ, "static"))
		{
			physicalizationType = ePT_Static;
		}
		else
		{
			GameWarning("Unknow physicalization type '%s' for projectile '%s'!", typ, pEntityClass->GetName());
		}
	}

	CGameXmlParamReader physicsReader(physicsNode);

	// material
	material_name = physicsReader.ReadParamValue("material");

	if(physicalizationType != ePT_Static)
	{
		physicsReader.ReadParamValue<float>("mass", mass);
		physicsReader.ReadParamValue<float>("speed", speed);
		physicsReader.ReadParamValue<int>("max_logged_collisions", maxLoggedCollisions);
		physicsReader.ReadParamValue<bool>("traceable", traceable);
		physicsReader.ReadParamValue<Vec3>("spin", spin);
		physicsReader.ReadParamValue<Vec3>("spin_random", spinRandom);
		physicsReader.ReadParamValue<int>("bounceableBullet", bounceableBullet);
	}

	if (physicalizationType == ePT_Particle)
	{
		SAFE_DELETE(pParticleParams);
		pParticleParams = new pe_params_particle();

		float radius = 0.005f;
		physicsReader.ReadParamValue<float>("radius", radius);
		pParticleParams->thickness = radius*2.0f;
		pParticleParams->size = radius*2.0f;
		physicsReader.ReadParamValue<float>("air_resistance", pParticleParams->kAirResistance);
		physicsReader.ReadParamValue<float>("water_resistance", pParticleParams->kWaterResistance);
		physicsReader.ReadParamValue<Vec3>("gravity", pParticleParams->gravity);
		physicsReader.ReadParamValue<Vec3>("water_gravity", pParticleParams->waterGravity);
		physicsReader.ReadParamValue<float>("thrust", pParticleParams->accThrust);
		physicsReader.ReadParamValue<float>("lift", pParticleParams->accLift);
		physicsReader.ReadParamValue<float>("min_bounce_speed", pParticleParams->minBounceVel);
		physicsReader.ReadParamValue<int>("pierceability", pParticleParams->iPierceability);
		physicsReader.ReadParamValue<Vec3>("rollAxis", pParticleParams->rollAxis);

		int flag = 0;
		physicsReader.ReadParamValue<int>("single_contact", flag);
		pParticleParams->flags = flag?particle_single_contact:0; flag=0;
		physicsReader.ReadParamValue<int>("constant_orientation", flag);
		pParticleParams->flags |= flag?particle_constant_orientation:0; flag=0;
		physicsReader.ReadParamValue<int>("no_roll", flag);
		pParticleParams->flags |= flag?particle_no_roll:0; flag=0;
		physicsReader.ReadParamValue<int>("no_spin", flag);
		pParticleParams->flags |= flag?particle_no_spin:0; flag=0;
		physicsReader.ReadParamValue<int>("no_path_alignment", flag);
		pParticleParams->flags |= flag?particle_no_path_alignment:0; flag=0;

		pParticleParams->mass= mass;

		//Particles might contain a pierceability table
		XmlNodeRef pierceabilityLevelsNode = physicsReader.FindFilteredChild("pierceabilityLevels");
		if (pierceabilityLevelsNode != NULL)
		{
			CGameXmlParamReader pierceabilityLevelsReader(pierceabilityLevelsNode);

			pierceabilityLevelsReader.ReadParamValue<float>("level1", pierceabilityParams.damageFallOffLevels[SPierceabilityParams::ePL_Level1]);
			pierceabilityLevelsReader.ReadParamValue<float>("level2", pierceabilityParams.damageFallOffLevels[SPierceabilityParams::ePL_Level2]);
			pierceabilityLevelsReader.ReadParamValue<float>("level3", pierceabilityParams.damageFallOffLevels[SPierceabilityParams::ePL_Level3]);
			pierceabilityLevelsReader.ReadParamValue<float>("maxPenetrationThickness", pierceabilityParams.maxPenetrationThickness);
		}
	}
}

void SAmmoParams::LoadGeometry(const XmlNodeRef& ammoParamsNode)
{
	CGameXmlParamReader reader(ammoParamsNode);

	XmlNodeRef geometryNode = reader.FindFilteredChild("geometry");
	if (!geometryNode)
		return;

	CGameXmlParamReader geometryReader(geometryNode);

	XmlNodeRef firstpersonNode = geometryReader.FindFilteredChild("firstperson");
	if (firstpersonNode)
	{
		const char *modelName = firstpersonNode->getAttr("name");

		if (modelName && modelName[0])
		{
			Ang3 angles(0,0,0);
			Vec3 position(0,0,0);
			float scale=1.0f;
			firstpersonNode->getAttr("position", position);
			firstpersonNode->getAttr("angles", angles);
			firstpersonNode->getAttr("scale", scale);

			fpLocalTM = Matrix34(Matrix33::CreateRotationXYZ(DEG2RAD(angles)));
			fpLocalTM.ScaleColumn(Vec3(scale, scale, scale));
			fpLocalTM.SetTranslation(position);

			fpGeometryName = modelName;
		}
	}
}

void SAmmoParams::CacheResources() const
{
	CItemResourceCache& resourceCache = g_pGame->GetGameSharedParametersStorage()->GetItemResourceCache();
	if(resourceCache.AreClassResourcesCached(pEntityClass))
		return;

	CItemParticleEffectCache& particleCache = resourceCache.GetParticleEffectCache();
	CItemMaterialAndTextureCache& materialCache = resourceCache.GetMaterialsAndTextureCache();

	resourceCache.GetAmmoGeometryCache().CacheGeometry(fpGeometryName.c_str(), true);

	if (!material_name.empty() && (surfaceTypeId == -1))
	{
		ISurfaceType* pSurfaceType = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeByName(material_name.c_str());
		if (pSurfaceType)
			surfaceTypeId = pSurfaceType->GetId();
	}

	if(pCollision)
	{
		pCollision->PreCacheLevelResources(particleCache);
	}
	if(pExplosions)
	{
		//t count = CRY_ARRAY_COUNT(pExplosions);
		for(int i = 0; i < m_explosion_count; ++i)
		{
			pExplosions[i]->PreCacheLevelResources(particleCache);
		}
	}
	if (pTrail)
	{
		pTrail->PreCacheLevelResources(particleCache);
	}
	if (pTrailUnderWater)
	{
		pTrailUnderWater->PreCacheLevelResources(particleCache);
	}
	if (pWhiz)
	{
		pWhiz->PreCacheLevelResources();
	}
	if (pRicochet)
	{
		pRicochet->PreCacheLevelResources();
	}
	if (pLTagParams)
	{
		pLTagParams->m_ricochet.PreCacheLevelResources(particleCache, materialCache);
		pLTagParams->m_sticky.PreCacheLevelResources(particleCache, materialCache);
	}
	if (pMikeBulletParams)
	{
		pMikeBulletParams->PreCacheLevelResources(particleCache);
	}
	if (pHomingParams)
	{
		pHomingParams->PreCacheLevelResources(particleCache);
	}
	if(pKvoltParams)
	{
		pKvoltParams->PreCacheLevelResources(particleCache);
	}
	if (pBulletTimeParams)
	{
		pBulletTimeParams->PreCacheLevelResources(resourceCache);
	}
	if (pLightningBoltParams)
	{
		pLightningBoltParams->PreCacheLevelResources(materialCache, particleCache);
	}
	if (pC4ExplosiveParams)
	{
		pC4ExplosiveParams->PreCacheLevelResources(resourceCache);
	}
	if (pElectricProjectileParams)
	{
		pElectricProjectileParams->PreCacheLevelResources(particleCache);
	}

	resourceCache.CachedResourcesForClassDone(pEntityClass);

}

void SAmmoParams::LoadCollision(const XmlNodeRef& ammoParamsNode)
{
	CGameXmlParamReader reader(ammoParamsNode);

	XmlNodeRef collisionNode = reader.FindFilteredChild("collision");
	if (collisionNode)
	{
		pCollision = new SCollisionParams(collisionNode);
	}
}

void SAmmoParams::LoadExplosion(const XmlNodeRef& ammoParamsNode)
{
	static const int maxExplosions = 4;

	CGameXmlParamReader reader(ammoParamsNode);

	const int childCount = ammoParamsNode->getChildCount();
	//int explosionCount = 0;
	XmlNodeRef explosions[maxExplosions];
	
	int finalExplosionCount = 0;
	for (int i= 0; (i < childCount) && (finalExplosionCount < maxExplosions); ++i)
	{
		XmlNodeRef child = reader.GetFilteredChildAt(i);
		if(child && stricmp(child->getTag(), "explosion") == 0)
		{
			explosions[finalExplosionCount] = child;			
			++finalExplosionCount;
		}
	}

	m_explosion_count = finalExplosionCount;

	if(m_explosion_count)
	{
		pExplosions = new SExplosionParams*[m_explosion_count];
		for(int i = 0; i < m_explosion_count; ++i)
		{
			pExplosions[i] = new SExplosionParams(explosions[i]);
		}

		pExplosion = pExplosions[0];
	}
}

void SAmmoParams::LoadFlashbang(const XmlNodeRef& ammoParamsNode)
{
	CGameXmlParamReader reader(ammoParamsNode);

	XmlNodeRef flashbangNode = reader.FindFilteredChild("flashbang");
	if (flashbangNode)
	{
		pFlashbang = new SFlashbangParams(flashbangNode);
	}
}

void SAmmoParams::LoadTrailsAndWhizzes(const XmlNodeRef& ammoParamsNode)
{
	CGameXmlParamReader reader(ammoParamsNode);

	XmlNodeRef whizNode = reader.FindFilteredChild("whiz");
	if (whizNode)
	{
		pWhiz = new SWhizParams(whizNode);
		if (pWhiz->audioTriggerName.empty())
		{
			delete pWhiz;
			pWhiz = 0;
		}
	}
	
	XmlNodeRef ricochetNode = reader.FindFilteredChild("ricochet");
	if (ricochetNode)
	{
		pRicochet = new SWhizParams(ricochetNode);
		if (pRicochet->audioTriggerName.empty())
		{
			delete pRicochet;
			pRicochet = 0;
		}
	}

	XmlNodeRef trailNode = reader.FindFilteredChild("trail");
	if (trailNode)
	{
		pTrail = new STrailParams(trailNode);
		if (pTrail->sound.empty() && pTrail->effect.empty())
		{
			delete pTrail;
			pTrail = 0;
		}
	}

	XmlNodeRef trailUnderWaterNode = reader.FindFilteredChild("trailUnderWater");
	if (trailUnderWaterNode)
	{
		pTrailUnderWater = new STrailParams(trailUnderWaterNode);
		if (pTrailUnderWater->sound.empty() && pTrailUnderWater->effect.empty())
		{
			delete pTrailUnderWater;
			pTrailUnderWater = 0;
		}
	}
}

void SAmmoParams::LoadLTagGrenadeParams(const XmlNodeRef& ammoParamsNode)
{
	CGameXmlParamReader reader(ammoParamsNode);

	XmlNodeRef lTagGrenadeNode = reader.FindFilteredChild("LTagProgrammableGrenade");
	if(lTagGrenadeNode == NULL)
		return;

	pLTagParams = new SLTagGrenadeParams(lTagGrenadeNode);
}

void SAmmoParams::LoadHomingSwarmParams(const XmlNodeRef& ammoParamsNode)
{
	CGameXmlParamReader reader(ammoParamsNode);

	XmlNodeRef homingSwarmParamsNode = reader.FindFilteredChild("HomingSwarmParams");
	if(homingSwarmParamsNode == NULL)
		return;

	pHomingSwarmParams = new SHomingSwarmParams(homingSwarmParamsNode);
}

void SAmmoParams::LoadMikeBulletParams(const XmlNodeRef& ammoParamsNode)
{
	CGameXmlParamReader reader(ammoParamsNode);

	XmlNodeRef mikeBulletNode = reader.FindFilteredChild("MikeBurnParams");
	if(mikeBulletNode == NULL)
		return;

	pMikeBulletParams = new SMikeBulletParams(mikeBulletNode);
}

void SAmmoParams::LoadGrenadeParams(const XmlNodeRef& ammoParamsNode)
{
	CGameXmlParamReader reader(ammoParamsNode);

	XmlNodeRef grenadeNode = reader.FindFilteredChild("Grenade");
	if(grenadeNode == NULL)
		return;

	pGrenadeParams = new SGrenadeParams(grenadeNode);
}

void SAmmoParams::LoadHomingMissileParams( const XmlNodeRef& ammoParamsNode )
{
	CGameXmlParamReader reader(ammoParamsNode);

	XmlNodeRef homingNode = reader.FindFilteredChild("HomingParams");
	if (!homingNode)
		return;

	pHomingParams = new SHomingMissileParams(homingNode);
}

void SAmmoParams::LoadKVoltParams( const XmlNodeRef& ammoParamsNode )
{
	CGameXmlParamReader reader(ammoParamsNode);

	XmlNodeRef kvoltParamsNode = reader.FindFilteredChild("KVoltParams");
	if (!kvoltParamsNode)
		return;

	pKvoltParams = new SKVoltParams(kvoltParamsNode);
}

void SAmmoParams::LoadBulletTimeParams( const XmlNodeRef& ammoParamsNode )
{
	CGameXmlParamReader reader(ammoParamsNode);

	XmlNodeRef bulletTimeNode = reader.FindFilteredChild("BulletTimeParams");
	if (!bulletTimeNode)
		return;

	pBulletTimeParams = new SBulletTimeParams(bulletTimeNode);
}


void SAmmoParams::LoadLightningBoltParams(const XmlNodeRef&  ammoParamsNode)
{
	CGameXmlParamReader reader(ammoParamsNode);

	XmlNodeRef lightningBoltNode = reader.FindFilteredChild("LightningBoltParams");
	if (!lightningBoltNode)
		return;

	pLightningBoltParams = new SLightningBoltParams(lightningBoltNode);
}

void SAmmoParams::LoadC4ExplosiveParams(const XmlNodeRef&  ammoParamsNode)
{
	CGameXmlParamReader reader(ammoParamsNode);

	XmlNodeRef c4ExplosiveParamsNode = reader.FindFilteredChild("c4explosiveparams");
	if (!c4ExplosiveParamsNode)
		return;

	pC4ExplosiveParams = new SC4ExplosiveParams(c4ExplosiveParamsNode);
}

void SAmmoParams::LoadHazardParams(const XmlNodeRef&  ammoParamsNode)
{
	CGameXmlParamReader reader(ammoParamsNode);

	XmlNodeRef hazardNode = reader.FindFilteredChild("HazardParams");
	if (!hazardNode)
		return;

	pHazardParams = new SHazardAmmoParams(hazardNode);
}

void SAmmoParams::LoadElectriProjectileParams(const XmlNodeRef& ammoParamsNode)
{
	CGameXmlParamReader reader(ammoParamsNode);

	XmlNodeRef electricProjectile = reader.FindFilteredChild("ElectricProjectileParams");
	if (!electricProjectile)
		return;

	pElectricProjectileParams = new SElectricProjectileParams(electricProjectile);
}
