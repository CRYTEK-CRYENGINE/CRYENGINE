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
#include "StdAfx.h"

#include "GameObjects/GameObject.h"
#include "IActorSystem.h"
#include "IVehicleSystem.h"
#include "IViewSystem.h"
#include "CryAction.h"
#include "IGameObject.h"
#include <CryAISystem/IAgent.h>

#include "Vehicle.h"
#include "VehicleComponent.h"
#include "VehicleDamageBehaviorDetachPart.h"
#include "VehiclePartAnimatedJoint.h"

#include <CryRenderer/IRenderer.h>
#include <CryRenderer/IRenderAuxGeom.h>
#include "IGameRulesSystem.h"

DEFINE_SHARED_PARAMS_TYPE_INFO(CVehicleComponent::SSharedParams)

#if ENABLE_VEHICLE_DEBUG
namespace
{
ILINE bool DebugDamage() { return VehicleCVars().v_debugdraw == eVDB_Damage; }
}
#endif

//------------------------------------------------------------------------
// Suppressed uninitMemberVar warnings due to use of BEGIN_SHARED_PARAMS macro
// cppcheck-suppress uninitMemberVar
CVehicleComponent::CVehicleComponent()
	: m_pVehicle(nullptr)
	, m_damage(0.0f)
	, m_proportionOfVehicleHealth(0.f)
	, m_lastHitLocalPos(ZERO)
	, m_lastHitRadius(0.f)
	, m_lastHitType(0)
{
	m_bounds.Reset();
}

//------------------------------------------------------------------------
CVehicleComponent::~CVehicleComponent()
{
	for (TVehicleDamageBehaviorVector::iterator ite = m_damageBehaviors.begin(); ite != m_damageBehaviors.end(); ++ite)
		ite->second->Release();
}

//------------------------------------------------------------------------
bool CVehicleComponent::Init(IVehicle* pVehicle, const CVehicleParams& paramsTable)
{
	// Store pointer to vehicle.

	m_pVehicle = static_cast<CVehicle*>(pVehicle);

	// Get name.

	string name = paramsTable.getAttr("name");

	if (name.empty())
	{
		return false;
	}

	// Construct vehicle component name.

	string vehicleComponentName = m_pVehicle->GetEntity()->GetClass()->GetName();

	const char* pModification = m_pVehicle->GetModification();

	if (pModification != 0 && strlen(pModification))
	{
		vehicleComponentName.append("::");

		vehicleComponentName.append(pModification);
	}

	vehicleComponentName.append("::Comp::");

	vehicleComponentName.append(name);

	// Get shared parameters.

	m_pSharedParams = GetSharedParams(vehicleComponentName, paramsTable);

	CRY_ASSERT(m_pSharedParams != 0);

	if (!m_pSharedParams)
	{
		return false;
	}

	m_bounds = m_pSharedParams->bounds;
	m_lastHitType = 0;

	Vec3 position;
	Vec3 size;
	if (paramsTable.getAttr("position", position) && paramsTable.getAttr("size", size))
	{
		bool hasOldBoundInfo = !m_bounds.IsReset();
		if (hasOldBoundInfo)
		{
			GameWarning("Component %s for vehicle %s has maxBound and/or minBound together with position and size properties. Using position and size.", m_pSharedParams->name.c_str(), m_pVehicle->GetEntity()->GetName());
		}
		Vec3 halfSize = size * 0.5f;
		m_bounds.min = position - halfSize;
		m_bounds.max = position + halfSize;
	}

	if (m_bounds.IsReset())
	{
		m_bounds.min.zero();
		m_bounds.max.zero();
	}
	else if (!(m_bounds.min.x <= m_bounds.max.x && m_bounds.min.y <= m_bounds.max.y && m_bounds.min.z <= m_bounds.max.z))
	{
		GameWarning("Invalid bounding box read for %s, component %s", m_pVehicle->GetEntity()->GetName(), m_pSharedParams->name.c_str());
		m_bounds.min.zero();
		m_bounds.max.zero();
	}

	m_damage = 0.0f;

	if (CVehicleParams damageBehaviorsTable = paramsTable.findChild("DamageBehaviors"))
	{
		int i = 0;
		int c = damageBehaviorsTable.getChildCount();
		m_damageBehaviors.reserve(c);

		for (; i < c; i++)
		{
			if (CVehicleParams damageBehaviorTable = damageBehaviorsTable.getChild(i))
			{
				SDamageBehaviorParams behaviorParams;

				if (!damageBehaviorTable.getAttr("damageRatioMin", behaviorParams.damageRatioMin))
				{
					behaviorParams.damageRatioMin = 1.0f;
				}

				if (!damageBehaviorTable.getAttr("damageRatioMax", behaviorParams.damageRatioMax))
				{
					behaviorParams.damageRatioMax = 0.0f;
				}

				if (!damageBehaviorTable.getAttr("ignoreVehicleDestruction", behaviorParams.ignoreOnVehicleDestroyed))
				{
					behaviorParams.ignoreOnVehicleDestroyed = false;
				}

				if (damageBehaviorTable.haveAttr("class"))
				{
					IVehicleSystem* pVehicleSystem = CCryAction::GetCryAction()->GetIVehicleSystem();

					const char* szClass = damageBehaviorTable.getAttr("class");
					IVehicleDamageBehavior* pDamageBehavior = (szClass && szClass[0] ? pVehicleSystem->CreateVehicleDamageBehavior(szClass) : NULL);
					if (pDamageBehavior)
					{
						if (pDamageBehavior->Init(pVehicle, damageBehaviorTable))
							m_damageBehaviors.push_back(TVehicleDamageBehaviorPair(behaviorParams, pDamageBehavior));
						else
							pDamageBehavior->Release();
					}
				}
			}
		}
	}

	m_lastHitLocalPos.zero();
	m_lastHitRadius = 0.0f;

	return true;
}

//------------------------------------------------------------------------
void CVehicleComponent::Reset()
{
	m_damage = 0.0f;

	for (TVehicleDamageBehaviorVector::iterator ite = m_damageBehaviors.begin(); ite != m_damageBehaviors.end(); ++ite)
	{
		IVehicleDamageBehavior* pVehicleDamageBehavior = ite->second;
		pVehicleDamageBehavior->Reset();
	}

	m_lastHitLocalPos.zero();
	m_lastHitRadius = 0.0f;
	m_lastHitType = 0;

	m_proportionOfVehicleHealth = 0.0f;
}

//------------------------------------------------------------------------
unsigned int CVehicleComponent::GetPartCount() const
{
	return m_parts.size();
}

//------------------------------------------------------------------------
IVehiclePart* CVehicleComponent::GetPart(unsigned int index) const
{
	if (index < m_parts.size())
	{
		return m_parts[index];
	}

	return NULL;
}

//------------------------------------------------------------------------
const AABB& CVehicleComponent::GetBounds()
{
	if (m_pSharedParams->useBoundsFromParts)
	{
		m_bounds.Reset();

		for (TVehiclePartVector::iterator ite = m_parts.begin(), end = m_parts.end(); ite != end; ++ite)
		{
			IVehiclePart* pPart = *ite;
			m_bounds.Add(pPart->GetLocalBounds());
		}

		if (!m_bounds.IsReset())
		{
			// add a tolerance - bbox checks may fail, depending on part geometry
			Vec3 delta(0.05f, 0.05f, 0.05f);
			m_bounds.max += delta;
			m_bounds.min -= delta;
		}
	}
	else if (m_bounds.IsEmpty())
	{
		m_pVehicle->GetEntity()->GetLocalBounds(m_bounds);
	}

	return m_bounds;
}

//------------------------------------------------------------------------
void CVehicleComponent::OnHit(const HitInfo& hitInfo, const TVehicleComponentVector* pAffectedComponents)
{
	if (m_pSharedParams->damageMax <= 0.0f)
		return;

#if ENABLE_VEHICLE_DEBUG
	if (DebugDamage())
		CryLog("=== Hit Component <%s>, base damage %.1f ===", GetComponentName(), hitInfo.damage);
#endif

	Vec3 localPos = m_pVehicle->GetEntity()->GetWorldTM().GetInverted() * hitInfo.pos;

	// direct impact?
	bool impact = !hitInfo.explosion || hitInfo.targetId == m_pVehicle->GetEntityId();

	HitInfo hitInfoLocal = hitInfo;
	hitInfoLocal.pos = localPos;
	float processedDamage = ProcessDamage(hitInfoLocal, impact, pAffectedComponents);

#if ENABLE_VEHICLE_DEBUG
	if (DebugDamage())
		CryLog("final damage: %.1f", processedDamage);
#endif

	if (abs(processedDamage) < 0.001f)
		return;

	// repairing
	if (processedDamage <= 0.0f && (m_pVehicle->IsDestroyed() || m_damage == 0.f))
		return;

	// Repairing: special check here for vehicle damage caused by being submerged in water - otherwise the player
	//	can continually repair the vehicle while it is taking damage in water (gaining lots of PP in MP).
	const SVehicleStatus& status = m_pVehicle->GetStatus();
	const SVehicleDamageParams& damageParams = m_pVehicle->GetDamageParams();
	if (processedDamage <= 0.0f && status.submergedRatio > damageParams.submergedRatioMax)
	{
		return;
	}

	if (m_pSharedParams->isOnlyDamageableByPlayer && processedDamage > 0.f)
	{
		if (IActor* pActor = CCryAction::GetCryAction()->GetIActorSystem()->GetActor(hitInfo.shooterId))
		{
			if (!pActor->IsPlayer())
				return;
		}
	}

	m_lastHitLocalPos = localPos;
	m_lastHitRadius = max(0.1f, min(3.0f, hitInfo.radius));

	m_lastHitType = hitInfo.type;

	float currentDamageRatio = m_damage / m_pSharedParams->damageMax;
	int currentDamageLevel = int(min(currentDamageRatio, 1.0f) / 0.25f);

	m_damage = max(0.0f, m_damage + processedDamage);

	float newDamageRatio = m_damage / m_pSharedParams->damageMax;
	int newDamageLevel = int(min(newDamageRatio, 1.0f) / 0.25f);

	if (gEnv->bServer)
		CHANGED_NETWORK_STATE(m_pVehicle, CVehicle::ASPECT_COMPONENT_DAMAGE);

	if (m_pSharedParams->useDamageLevels && currentDamageLevel == newDamageLevel)
		return;

#if ENABLE_VEHICLE_DEBUG
	if (VehicleCVars().v_debugdraw)
		CryLog("%s - damaged <%s> with %.1f damages (total: %.1f)", m_pVehicle->GetEntity()->GetName(), m_pSharedParams->name.c_str(), processedDamage, m_damage);
#endif

	BroadcastDamage(processedDamage, hitInfo.shooterId);
}

//------------------------------------------------------------------------
void CVehicleComponent::OnVehicleDestroyed()
{
	for (TVehicleDamageBehaviorVector::iterator ite = m_damageBehaviors.begin(); ite != m_damageBehaviors.end(); ++ite)
	{
		const SDamageBehaviorParams& behaviorParams = ite->first;
		if (!behaviorParams.ignoreOnVehicleDestroyed)
		{
			IVehicleDamageBehavior* pVehicleDamageBehavior = ite->second;

			SVehicleDamageBehaviorEventParams eventParams;
			eventParams.componentDamageRatio = 1.0f;
			eventParams.localPos.zero();
			eventParams.hitValue = 1.0f;
			eventParams.radius = 0.0f;
			eventParams.shooterId = 0;
			eventParams.pVehicleComponent = this;

			pVehicleDamageBehavior->OnDamageEvent(eVDBE_VehicleDestroyed, eventParams);
		}
	}
}

//------------------------------------------------------------------------
void CVehicleComponent::Update(float deltaTime)
{
	//CRY_PROFILE_FUNCTION( PROFILE_ACTION );
}

#if ENABLE_VEHICLE_DEBUG
//------------------------------------------------------------------------
void CVehicleComponent::DebugDraw()
{
	if (VehicleCVars().v_draw_components == 1)
	{
		IRenderAuxGeom* pRenderAux = gEnv->pRenderer->GetIRenderAuxGeom();

		const Matrix34& worldTM = m_pVehicle->GetEntity()->GetWorldTM();
		const AABB& localBounds = GetBounds();

		static float drawColor[4] = { 1, 1, 1, 1 };

		char pMessage[256];
		cry_sprintf(pMessage, "%s - %5.2f (%3.2f)", m_pSharedParams->name.c_str(), m_damage, m_damage / max(1.f, m_pSharedParams->damageMax));

		IRenderAuxText::DrawLabelEx(worldTM * localBounds.GetCenter(), 1.2f, drawColor, true, true, pMessage);

		pRenderAux->DrawAABB(localBounds, worldTM, false, RGBA8(255, 0, 0, 255), eBBD_Faceted);

		//Add semi-transparent rendering for the bounding boxes
		pRenderAux->SetRenderFlags(e_Def3DPublicRenderflags | e_AlphaBlended);
		pRenderAux->DrawAABB(localBounds, worldTM, true, RGBA8(255, 0, 0, 32), eBBD_Faceted);
	}
}
#endif

//------------------------------------------------------------------------
void CVehicleComponent::BroadcastDamage(float damage, EntityId shooterId)
{
	float currentDamageRatio = (m_damage - damage) / m_pSharedParams->damageMax;
	float newDamageRatio = m_damage / m_pSharedParams->damageMax;
	int currentDamageLevel = int(min(currentDamageRatio, 1.0f) / 0.25f);
	int newDamageLevel = int(min(newDamageRatio, 1.0f) / 0.25f);

	if (m_pSharedParams->useDamageLevels && currentDamageLevel == newDamageLevel)
		return;

	for (TVehicleDamageBehaviorVector::iterator ite = m_damageBehaviors.begin(), end = m_damageBehaviors.end(); ite != end; ++ite)
	{
		const SDamageBehaviorParams& behaviorParams = ite->first;
		IVehicleDamageBehavior* pVehicleDamageBehavior = ite->second;

		if (newDamageRatio >= behaviorParams.damageRatioMin || damage < 0.f)
		{
			bool hasPassedSecondCheck = true;
			if (damage > 0.f && behaviorParams.damageRatioMax > 0.0f && newDamageRatio >= behaviorParams.damageRatioMax)
			{
				if (currentDamageRatio < behaviorParams.damageRatioMax)
				{
					SVehicleDamageBehaviorEventParams behaviorEventParams;
					pVehicleDamageBehavior->OnDamageEvent(eVDBE_MaxRatioExceeded, behaviorEventParams);
				}
				hasPassedSecondCheck = false;
			}

			if (hasPassedSecondCheck)
			{
				EVehicleDamageBehaviorEvent behaviorEvent = damage > 0.f ? eVDBE_Hit : eVDBE_Repair;

				SVehicleDamageBehaviorEventParams behaviorEventParams;
				behaviorEventParams.componentDamageRatio = newDamageRatio;
				behaviorEventParams.localPos = m_lastHitLocalPos;
				behaviorEventParams.hitValue = damage;
				behaviorEventParams.hitType = m_lastHitType;
				behaviorEventParams.radius = m_lastHitRadius;
				behaviorEventParams.randomness = 0.0f;
				behaviorEventParams.shooterId = shooterId;
				behaviorEventParams.pVehicleComponent = this;

				pVehicleDamageBehavior->OnDamageEvent(behaviorEvent, behaviorEventParams);
			}
		}
	}

	// always adhere to damage levels here
	if (currentDamageLevel == newDamageLevel)
		return;

	// component damaged, so we notify all the parts related to the component
	IVehiclePart::SVehiclePartEvent partEvent;
	partEvent.type = damage > 0.f ? IVehiclePart::eVPE_Damaged : IVehiclePart::eVPE_Repair;
	partEvent.fparam = newDamageRatio;
	partEvent.pData = this;

	for (TVehiclePartVector::iterator ite = m_parts.begin(); ite != m_parts.end(); ++ite)
	{
		IVehiclePart* pPart = *ite;
		pPart->OnEvent(partEvent);
	}
}

//------------------------------------------------------------------------
void CVehicleComponent::Serialize(TSerialize ser, EEntityAspects aspects)
{
	if (ser.GetSerializationTarget() != eST_Network)
	{
		ser.Value("damage", m_damage);

		if (ser.IsReading())
		{
			float damageRatio = 1.0f;
			if (m_pSharedParams->damageMax > 0.0f)
				damageRatio = m_damage / m_pSharedParams->damageMax;
			int damageLevel = int(min(damageRatio, 1.0f) / 0.25f);
		}

		for (TVehicleDamageBehaviorVector::iterator ite = m_damageBehaviors.begin(); ite != m_damageBehaviors.end(); ++ite)
		{
			IVehicleDamageBehavior* pVehicleDamageBehavior = ite->second;

			ser.BeginGroup("damageBehavior");
			pVehicleDamageBehavior->Serialize(ser, aspects);
			ser.EndGroup();
		}
	}
	// network
	else
	{
		if (aspects & CVehicle::ASPECT_COMPONENT_DAMAGE)
		{
			NET_PROFILE_SCOPE("VehicleDamage", ser.IsReading());

			float olddamage = m_damage;
			ser.Value("damage", m_damage, 'vdmg');
			ser.Value("hitType", m_lastHitType, 'hTyp');
			ser.Value("lastHitPos", m_lastHitLocalPos, 'vHPs');
			ser.Value("lastHitRadius", m_lastHitRadius, 'vHRd');

			if (ser.IsReading() && abs(m_damage - olddamage) > 0.001f)
			{
				BroadcastDamage(m_damage - olddamage, 0);
			}
		}
	}
}

//------------------------------------------------------------------------
float CVehicleComponent::GetDamageRatio() const
{
	return (m_pSharedParams->damageMax > 0.f) ? min(1.f, m_damage / m_pSharedParams->damageMax) : 0.f;
}

//------------------------------------------------------------------------
void CVehicleComponent::SetDamageRatio(float ratio)
{
	if (m_pSharedParams->damageMax > 0.f)
		m_damage = m_pSharedParams->damageMax * min(1.0f, max(0.0f, ratio));
}

//------------------------------------------------------------------------
float CVehicleComponent::GetMaxDamage() const
{
	return m_pSharedParams->damageMax;
}

//------------------------------------------------------------------------
void CVehicleComponent::AddPart(IVehiclePart* pPart)
{
	m_parts.push_back(pPart);
}

//------------------------------------------------------------------------
float CVehicleComponent::ProcessDamage(const HitInfo& hitInfo, bool impact, const TVehicleComponentVector* pAffectedComponents)
{
	// determine hit ratio/splash damage

	bool splash = false;

	float damage = hitInfo.damage;

	if (hitInfo.type == CVehicle::s_repairHitTypeId)
	{
		damage = -damage;
	}

	if (hitInfo.explosion && hitInfo.radius > 0.0f)
	{
		const AABB& localBounds = GetBounds();
		bool inside = localBounds.IsContainPoint(hitInfo.pos);
		if (!(impact && inside))
		{
			// no direct hit, apply splash damage
			splash = true;

			// todo: more accurate ratio calculation
			float distanceSq = (hitInfo.pos - localBounds.GetCenter()).GetLengthSquared();
			float hitRatio = max(0.f, 1.f - distanceSq / sqr(hitInfo.radius));

			damage *= hitRatio;

#if ENABLE_VEHICLE_DEBUG
			if (DebugDamage())
				CryLog("splash damage, ratio: %.2f", hitRatio);
#endif
		}
	}

	if (IsHull() && pAffectedComponents && impact)
	{
		// check if the hit hit one or more other components. if so, get the hull affection from these.
		float affection = 0.f;
		int affected = 0;

		for (TVehicleComponentVector::const_iterator it = pAffectedComponents->begin(), end = pAffectedComponents->end(); it != end; ++it)
		{
			CVehicleComponent* pComponent = *it;
			if (pComponent == this)
				continue;

			if (hitInfo.radius > 0.f)
			{
				const AABB& compBounds = pComponent->GetBounds();
				if (!compBounds.IsContainPoint(hitInfo.pos))
					continue;
			}

			affection += (*it)->m_pSharedParams->hullAffection;
			++affected;

#if ENABLE_VEHICLE_DEBUG
			if (DebugDamage())
				CryLog("component <%s> affecting hull, affection %.2f", (*it)->GetComponentName(), (*it)->m_pSharedParams->hullAffection);
#endif
		}

		affection = (affected == 0) ? 1.f : affection / affected;
		damage *= affection;

#if ENABLE_VEHICLE_DEBUG
		if (DebugDamage())
			CryLog("total affection: %.2f", affection);
#endif
	}

	CVehicleDamages::TDamageMultipliers::const_iterator ite = m_pSharedParams->damageMultipliersByProjectile.find(hitInfo.projectileClassId), end = m_pSharedParams->damageMultipliersByProjectile.end();

#if ENABLE_VEHICLE_DEBUG
	const char* pDisplayDamageType = NULL;
#endif
	if (ite == end)
	{
		ite = m_pSharedParams->damageMultipliersByHitType.find(hitInfo.type), end = m_pSharedParams->damageMultipliersByHitType.end();

		if (ite == end)
		{
#if ENABLE_VEHICLE_DEBUG
			pDisplayDamageType = "default";
#endif
			// 0 is the 'default' damage multiplier, check for it if we didn't find the specified one
			ite = m_pSharedParams->damageMultipliersByHitType.find((int)CVehicleDamages::DEFAULT_HIT_TYPE);
		}
		else
		{
#if ENABLE_VEHICLE_DEBUG
			pDisplayDamageType = CCryAction::GetCryAction()->GetIGameRulesSystem()->GetCurrentGameRules()->GetHitType(hitInfo.type);
#endif
		}
	}
	else
	{
#if ENABLE_VEHICLE_DEBUG
		char str[256];
		if (gEnv->pGameFramework->GetNetworkSafeClassName(str, sizeof(str), hitInfo.projectileClassId))
		{
			pDisplayDamageType = str;
		}
		else
		{
			pDisplayDamageType = "Unknown_Projectile_Type";
		}
#endif
	}

	if (ite != end)
	{
		const CVehicleDamages::SDamageMultiplier& mult = ite->second;
		damage *= mult.mult;
		damage *= splash ? mult.splash : 1.f;

#if ENABLE_VEHICLE_DEBUG
		if (DebugDamage())
		{
			CryLog("mults for %s: %.2f, splash %.2f", pDisplayDamageType, mult.mult, mult.splash);
		}
#endif
	}
	else
		m_pVehicle->ProcessHit(damage, hitInfo, splash);

	// major components don't get repaired at the normal rate, the repair amount is determined
	//	by how important the component is (what proportion of the vehicle's health it is)
	if (hitInfo.type == CVehicle::s_repairHitTypeId && damage < 0.0f && m_pSharedParams->isMajorComponent && m_proportionOfVehicleHealth > 0.0f)
	{
		damage *= m_proportionOfVehicleHealth;
	}

	return damage;
}

void CVehicleComponent::SetProportionOfVehicleHealth(float proportion)
{
	m_proportionOfVehicleHealth = clamp_tpl(proportion, 0.0f, 1.0f);
}

void CVehicleComponent::GetMemoryUsage(ICrySizer* s) const
{
	s->AddObject(this, sizeof(*this));
	s->AddObject(m_damageBehaviors);
	s->AddObject(m_parts);
}

//------------------------------------------------------------------------
CVehicleComponent::SSharedParamsConstPtr CVehicleComponent::GetSharedParams(const string& vehicleComponentName, const CVehicleParams& paramsTable) const
{
	ISharedParamsManager* pSharedParamsManager = CCryAction::GetCryAction()->GetISharedParamsManager();

	CRY_ASSERT(pSharedParamsManager);

	SSharedParamsConstPtr pSharedParams = CastSharedParamsPtr<SSharedParams>(pSharedParamsManager->Get(vehicleComponentName));

	if (!pSharedParams)
	{
		SSharedParams sharedParams;

		sharedParams.name = paramsTable.getAttr("name");

		sharedParams.bounds.Reset();
		{
			Vec3 bound;

			if (paramsTable.getAttr("minBound", bound))
			{
				sharedParams.bounds.Add(bound);
			}

			if (paramsTable.getAttr("maxBound", bound))
			{
				sharedParams.bounds.Add(bound);
			}
		}

		if (!paramsTable.getAttr("damageMax", sharedParams.damageMax))
		{
			sharedParams.damageMax = 0.0f;
		}

		if (!paramsTable.getAttr("hullAffection", sharedParams.hullAffection))
		{
			sharedParams.hullAffection = 1.0f;
		}

		sharedParams.isHull = sharedParams.name == "hull" || sharedParams.name == "Hull";

		if (!paramsTable.getAttr("major", sharedParams.isMajorComponent))
		{
			sharedParams.isMajorComponent = true;
		}

		if (!paramsTable.getAttr("isOnlyDamageableByPlayer", sharedParams.isOnlyDamageableByPlayer))
		{
			sharedParams.isOnlyDamageableByPlayer = false;
		}

		if (!paramsTable.getAttr("useBoundsFromParts", sharedParams.useBoundsFromParts))
		{
			sharedParams.useBoundsFromParts = false;
		}

		if (!paramsTable.getAttr("useDamageLevels", sharedParams.useDamageLevels))
		{
			sharedParams.useDamageLevels = true;
		}

		assert(m_pVehicle->GetEntity());
		CVehicleDamages::ParseDamageMultipliers(sharedParams.damageMultipliersByHitType, sharedParams.damageMultipliersByProjectile, paramsTable, *m_pVehicle->GetEntity());

		pSharedParams = CastSharedParamsPtr<SSharedParams>(pSharedParamsManager->Register(vehicleComponentName, sharedParams));

		CRY_ASSERT(pSharedParams != 0);
	}

	return pSharedParams;
}
