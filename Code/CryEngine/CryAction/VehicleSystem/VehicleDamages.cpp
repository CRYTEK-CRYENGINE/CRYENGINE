// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Implements the base of the vehicle damages

   -------------------------------------------------------------------------
   History:
   - 23:02:2006: Created by Mathieu Pinard

*************************************************************************/
#include "StdAfx.h"
#include "IVehicleSystem.h"
#include "VehicleDamages.h"
#include "VehicleDamagesGroup.h"
#include "VehicleSystem/Vehicle.h"
#include "VehicleSystem/VehicleComponent.h"
#include "VehicleSystem/VehicleCVars.h"
#include "IActorSystem.h"
#include "IGameRulesSystem.h"
#include "CryAction.h"
//------------------------------------------------------------------------
namespace
{
void GetAndInsertMultiplier(CVehicleDamages::TDamageMultipliers& multipliers, const CVehicleParams& multiplierTable, int typeID)
{
	CVehicleDamages::SDamageMultiplier mult;

	if (multiplierTable.getAttr("multiplier", mult.mult) && mult.mult >= 0.0f)
	{
		multiplierTable.getAttr("splash", mult.splash);
		multipliers.insert(CVehicleDamages::TDamageMultipliers::value_type(typeID, mult));
	}
}
}
//------------------------------------------------------------------------
void CVehicleDamages::InitDamages(CVehicle* pVehicle, const CVehicleParams& table)
{
	m_pVehicle = pVehicle;

	if (CVehicleParams damagesTable = table.findChild("Damages"))
	{
		if (CVehicleParams damagesGroupTable = damagesTable.findChild("DamagesGroups"))
		{
			int c = damagesGroupTable.getChildCount();
			int i = 0;

			m_damagesGroups.reserve(c);

			for (; i < c; i++)
			{
				if (CVehicleParams groupTable = damagesGroupTable.getChild(i))
				{
					CVehicleDamagesGroup* pDamageGroup = new CVehicleDamagesGroup;
					if (pDamageGroup->Init(pVehicle, groupTable))
						m_damagesGroups.push_back(pDamageGroup);
					else
						delete pDamageGroup;
				}
			}
		}

		damagesTable.getAttr("submergedRatioMax", m_damageParams.submergedRatioMax);
		damagesTable.getAttr("submergedDamageMult", m_damageParams.submergedDamageMult);

		damagesTable.getAttr("collDamageThreshold", m_damageParams.collisionDamageThreshold);
		damagesTable.getAttr("groundCollisionMinMult", m_damageParams.groundCollisionMinMult);
		damagesTable.getAttr("groundCollisionMaxMult", m_damageParams.groundCollisionMaxMult);
		damagesTable.getAttr("groundCollisionMinSpeed", m_damageParams.groundCollisionMinSpeed);
		damagesTable.getAttr("groundCollisionMaxSpeed", m_damageParams.groundCollisionMaxSpeed);
		damagesTable.getAttr("vehicleCollisionDestructionSpeed", m_damageParams.vehicleCollisionDestructionSpeed);
		damagesTable.getAttr("aiKillPlayerSpeed", m_damageParams.aiKillPlayerSpeed);
		damagesTable.getAttr("playerKillAISpeed", m_damageParams.playerKillAISpeed);
		damagesTable.getAttr("aiKillAISpeed", m_damageParams.aiKillAISpeed);

		assert(m_pVehicle->GetEntity());
		ParseDamageMultipliers(m_damageMultipliersByHitType, m_damageMultipliersByProjectile, damagesTable, *m_pVehicle->GetEntity());
	}
}

//------------------------------------------------------------------------
void CVehicleDamages::ParseDamageMultipliers(TDamageMultipliers& multipliersByHitType, TDamageMultipliers& multipliersByProjectile, const CVehicleParams& table, const IEntity& entity)
{
	CVehicleParams damageMultipliersTable = table.findChild("DamageMultipliers");
	if (!damageMultipliersTable)
		return;

	int i = 0;
	int c = damageMultipliersTable.getChildCount();

	IGameRules* pGR = CCryAction::GetCryAction()->GetIGameRulesSystem()->GetCurrentGameRules();
	assert(pGR);

	for (; i < c; i++)
	{
		if (CVehicleParams multiplierTable = damageMultipliersTable.getChild(i))
		{
			string damageType = multiplierTable.getAttr("damageType");
			if (!damageType.empty())
			{
				int hitTypeId = 0;
				if (pGR && damageType != "default")
					hitTypeId = pGR->GetHitTypeId(damageType.c_str());

				if (hitTypeId != 0 || damageType == "default")
				{
					GetAndInsertMultiplier(multipliersByHitType, multiplierTable, int(hitTypeId));
				}
				else
				{
					GameWarning("The HitType %s is not registered and was ignored while parsing DamageMultipliers (referenced in entity %s of class %s)", damageType.c_str(), entity.GetName(), entity.GetClass()->GetName());
				}
			}

			string ammoType = multiplierTable.getAttr("ammoType");
			if (!ammoType.empty())
			{
				int projectileType = 0;
				if (pGR && ammoType != "default")
				{
					uint16 classId(~uint16(0));

					if (ammoType == "default" || gEnv->pGameFramework->GetNetworkSafeClassId(classId, ammoType.c_str()))
					{
						GetAndInsertMultiplier(multipliersByProjectile, multiplierTable, int(classId));
					}
				}
			}
		}
	}
}

//------------------------------------------------------------------------
void CVehicleDamages::ReleaseDamages()
{
	for (TVehicleDamagesGroupVector::iterator ite = m_damagesGroups.begin(); ite != m_damagesGroups.end(); ++ite)
	{
		CVehicleDamagesGroup* pDamageGroup = *ite;
		pDamageGroup->Release();
	}
}

//------------------------------------------------------------------------
void CVehicleDamages::ResetDamages()
{
	for (TVehicleDamagesGroupVector::iterator ite = m_damagesGroups.begin(); ite != m_damagesGroups.end(); ++ite)
	{
		CVehicleDamagesGroup* pDamageGroup = *ite;
		pDamageGroup->Reset();
	}
}

//------------------------------------------------------------------------
void CVehicleDamages::UpdateDamages(float frameTime)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	for (TVehicleDamagesGroupVector::iterator ite = m_damagesGroups.begin(), end = m_damagesGroups.end(); ite != end; ++ite)
	{
		CVehicleDamagesGroup* pDamageGroup = *ite;
		pDamageGroup->Update(frameTime);
	}
}

//------------------------------------------------------------------------
bool CVehicleDamages::ProcessHit(float& damage, const HitInfo& hitInfo, bool splash)
{
#if ENABLE_VEHICLE_DEBUG
	string displayDamageType = NULL;
#endif
	CVehicleDamages::TDamageMultipliers::const_iterator ite = m_damageMultipliersByProjectile.find(hitInfo.projectileClassId), end = m_damageMultipliersByProjectile.end();

	bool bFound = false;

	if (ite == end)
	{
		ite = m_damageMultipliersByHitType.find(hitInfo.type), end = m_damageMultipliersByHitType.end();

		if (ite == end)
		{
#if ENABLE_VEHICLE_DEBUG
			displayDamageType = "default";
#endif
			// 0 is the 'default' damage multiplier, check for it if we didn't find the specified one
			ite = m_damageMultipliersByHitType.find((int)CVehicleDamages::DEFAULT_HIT_TYPE);
		}
		else
		{
			bFound = true;

#if ENABLE_VEHICLE_DEBUG
			displayDamageType = "HitType: ";
			displayDamageType += CCryAction::GetCryAction()->GetIGameRulesSystem()->GetCurrentGameRules()->GetHitType(hitInfo.type);
#endif
		}
	}
	else
	{
		bFound = true;

#if ENABLE_VEHICLE_DEBUG
		char str[256];
		if (gEnv->pGameFramework->GetNetworkSafeClassName(str, sizeof(str), hitInfo.projectileClassId))
		{
			displayDamageType = "ProjClass: ";
			displayDamageType += str;
		}
		else
		{
			displayDamageType = "Unknown_Projectile_Type";
		}
#endif
	}

	if (bFound)
	{
		const SDamageMultiplier& mult = ite->second;
		damage *= mult.mult * (splash ? mult.splash : 1.f);

#if ENABLE_VEHICLE_DEBUG
		if (VehicleCVars().v_debugdraw == eVDB_Damage)
		{
			CryLog("mults for %s: %.2f, splash %.2f", displayDamageType.c_str(), mult.mult, mult.splash);
		}
#endif

		return true;
	}

	return false;
}

//------------------------------------------------------------------------
CVehicleDamagesGroup* CVehicleDamages::GetDamagesGroup(const char* groupName)
{
	for (TVehicleDamagesGroupVector::iterator ite = m_damagesGroups.begin(); ite != m_damagesGroups.end(); ++ite)
	{
		CVehicleDamagesGroup* pDamageGroup = *ite;
		if (!strcmp(pDamageGroup->GetName().c_str(), groupName))
		{
			return pDamageGroup;
		}
	}

	return NULL;
}

//------------------------------------------------------------------------
void CVehicleDamages::GetDamagesMemoryStatistics(ICrySizer* pSizer) const
{
	pSizer->AddObject(m_damagesGroups);
	pSizer->AddObject(m_damageMultipliersByHitType);
}
