// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Implements the base of the vehicle damages group

   -------------------------------------------------------------------------
   History:
   - 23:02:2006: Created by Mathieu Pinard

*************************************************************************/
#include "StdAfx.h"
#include "CryAction.h"
#include "IVehicleSystem.h"
#include "VehicleDamagesGroup.h"
#include "Vehicle.h"
#include "VehicleDamageBehaviorDestroy.h"
#include "VehicleDamageBehaviorDetachPart.h"
#include "VehiclePartAnimatedJoint.h"

//------------------------------------------------------------------------
CVehicleDamagesGroup::~CVehicleDamagesGroup()
{
	for (TDamagesSubGroupVector::iterator ite = m_damageSubGroups.begin();
	     ite != m_damageSubGroups.end(); ++ite)
	{
		SDamagesSubGroup& subGroup = *ite;
		TVehicleDamageBehaviorVector& damageBehaviors = subGroup.m_damageBehaviors;

		for (TVehicleDamageBehaviorVector::iterator behaviorIte = damageBehaviors.begin(); behaviorIte != damageBehaviors.end(); ++behaviorIte)
		{
			IVehicleDamageBehavior* pBehavior = *behaviorIte;
			pBehavior->Release();
		}
	}

	m_delayedSubGroups.clear();
}

//------------------------------------------------------------------------
bool CVehicleDamagesGroup::Init(CVehicle* pVehicle, const CVehicleParams& table)
{
	m_pVehicle = pVehicle;
	m_name = table.getAttr("name");
	m_damageSubGroups.clear();

	return !m_name.empty() && ParseDamagesGroup(table);
}

//------------------------------------------------------------------------
bool CVehicleDamagesGroup::ParseDamagesGroup(const CVehicleParams& table)
{
	if (table.haveAttr("useTemplate"))
	{
		IVehicleSystem* pVehicleSystem = gEnv->pGameFramework->GetIVehicleSystem();
		CRY_ASSERT(pVehicleSystem);

		if (IVehicleDamagesTemplateRegistry* pDamageTemplReg = pVehicleSystem->GetDamagesTemplateRegistry())
			pDamageTemplReg->UseTemplate(table.getAttr("useTemplate"), this);
	}

	if (CVehicleParams damagesSubGroupsTable = table.findChild("DamagesSubGroups"))
	{
		int i = 0;
		int c = damagesSubGroupsTable.getChildCount();

		for (; i < c; i++)
		{
			if (CVehicleParams groupTable = damagesSubGroupsTable.getChild(i))
			{
				m_damageSubGroups.resize(m_damageSubGroups.size() + 1);
				SDamagesSubGroup& subGroup = m_damageSubGroups.back();

				subGroup.id = static_cast<TDamagesSubGroupId>(m_damageSubGroups.size() - 1);
				subGroup.m_isAlreadyInProcess = false;

				if (!groupTable.getAttr("delay", subGroup.m_delay))
					subGroup.m_delay = 0.0f;

				if (!groupTable.getAttr("randomness", subGroup.m_randomness))
					subGroup.m_randomness = 0.0f;

				if (CVehicleParams damageBehaviorsTable = groupTable.findChild("DamageBehaviors"))
				{
					int k = 0;
					int numDamageBehaviors = damageBehaviorsTable.getChildCount();

					subGroup.m_damageBehaviors.reserve(c);

					for (; k < numDamageBehaviors; k++)
					{
						if (CVehicleParams behaviorTable = damageBehaviorsTable.getChild(k))
						{
							if (IVehicleDamageBehavior* pDamageBehavior = ParseDamageBehavior(behaviorTable))
							{
								subGroup.m_damageBehaviors.push_back(pDamageBehavior);

								CVehicleDamageBehaviorDestroy* pDamageDestroy = CAST_VEHICLEOBJECT(CVehicleDamageBehaviorDestroy, pDamageBehavior);
								if (pDamageDestroy)
								{
									TVehiclePartVector& parts = m_pVehicle->GetParts();
									for (TVehiclePartVector::iterator ite = parts.begin(); ite != parts.end(); ++ite)
									{
										IVehiclePart* pPart = ite->second;
										if (CVehiclePartAnimatedJoint* pAnimJoint = CAST_VEHICLEOBJECT(CVehiclePartAnimatedJoint, pPart))
										{
											if (pAnimJoint->IsPhysicalized() && !pAnimJoint->GetDetachBaseForce().IsZero())
											{
												CVehicleDamageBehaviorDetachPart* pDetachBehavior = new CVehicleDamageBehaviorDetachPart;
												pDetachBehavior->Init(m_pVehicle, pAnimJoint->GetName(), pDamageDestroy->GetEffectName());

												subGroup.m_damageBehaviors.push_back(pDetachBehavior);
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}

	return true;
}

//------------------------------------------------------------------------
IVehicleDamageBehavior* CVehicleDamagesGroup::ParseDamageBehavior(const CVehicleParams& table)
{
	string className = table.getAttr("class");
	if (!className.empty())
	{
		IVehicleSystem* pVehicleSystem = CCryAction::GetCryAction()->GetIVehicleSystem();

		if (IVehicleDamageBehavior* pDamageBehavior = pVehicleSystem->CreateVehicleDamageBehavior(className))
		{
			if (pDamageBehavior->Init((IVehicle*) m_pVehicle, table))
				return pDamageBehavior;
			else
				pDamageBehavior->Release();
		}
	}

	return NULL;
}

//------------------------------------------------------------------------
void CVehicleDamagesGroup::Reset()
{
	for (TDamagesSubGroupVector::iterator ite = m_damageSubGroups.begin();
	     ite != m_damageSubGroups.end(); ++ite)
	{
		SDamagesSubGroup& subGroup = *ite;
		TVehicleDamageBehaviorVector& damageBehaviors = subGroup.m_damageBehaviors;

		for (TVehicleDamageBehaviorVector::iterator behaviorIte = damageBehaviors.begin(); behaviorIte != damageBehaviors.end(); ++behaviorIte)
		{
			IVehicleDamageBehavior* pBehavior = *behaviorIte;
			pBehavior->Reset();
		}

		subGroup.m_isAlreadyInProcess = false;
	}

	m_delayedSubGroups.clear();
}

//------------------------------------------------------------------------
void CVehicleDamagesGroup::Serialize(TSerialize ser, EEntityAspects aspects)
{
	ser.BeginGroup("SubGroups");
	for (TDamagesSubGroupVector::iterator ite = m_damageSubGroups.begin();
	     ite != m_damageSubGroups.end(); ++ite)
	{
		ser.BeginGroup("SubGroup");

		SDamagesSubGroup& subGroup = *ite;
		TVehicleDamageBehaviorVector& damageBehaviors = subGroup.m_damageBehaviors;

		for (TVehicleDamageBehaviorVector::iterator behaviorIte = damageBehaviors.begin(); behaviorIte != damageBehaviors.end(); ++behaviorIte)
		{
			IVehicleDamageBehavior* pBehavior = *behaviorIte;
			ser.BeginGroup("Behavior");
			pBehavior->Serialize(ser, aspects);
			ser.EndGroup();
		}
		ser.EndGroup();
	}
	ser.EndGroup();

	int size = m_delayedSubGroups.size();
	ser.Value("DelayedSubGroupEntries", size);
	if (ser.IsWriting())
	{
		for (TDelayedDamagesSubGroupList::iterator ite = m_delayedSubGroups.begin(); ite != m_delayedSubGroups.end(); ++ite)
		{
			ser.BeginGroup("SubGroup");
			SDelayedDamagesSubGroupInfo& delayedInfo = *ite;
			ser.Value("delayedInfoId", delayedInfo.subGroupId);
			ser.Value("delayedInfoDelay", delayedInfo.delay);
			delayedInfo.behaviorParams.Serialize(ser, m_pVehicle);
			ser.EndGroup();
		}
	}
	else if (ser.IsReading())
	{
		m_delayedSubGroups.clear();
		for (int i = 0; i < size; ++i)
		{
			ser.BeginGroup("SubGroup");
			SDelayedDamagesSubGroupInfo delayInfo;
			ser.Value("delayedInfoId", delayInfo.subGroupId);
			ser.Value("delayedInfoDelay", delayInfo.delay);
			delayInfo.behaviorParams.Serialize(ser, m_pVehicle);
			ser.EndGroup();
		}
	}
}

//------------------------------------------------------------------------
void CVehicleDamagesGroup::OnDamageEvent(EVehicleDamageBehaviorEvent event, const SVehicleDamageBehaviorEventParams& behaviorParams)
{
	if (m_pVehicle->IsDestroyed() || event == eVDBE_VehicleDestroyed)
		return;

	if (event == eVDBE_Repair)
	{
		m_delayedSubGroups.clear();
	}

	for (TDamagesSubGroupVector::iterator subGroupIte = m_damageSubGroups.begin(); subGroupIte != m_damageSubGroups.end(); ++subGroupIte)
	{
		SDamagesSubGroup& subGroup = *subGroupIte;
		TVehicleDamageBehaviorVector& damageBehaviors = subGroup.m_damageBehaviors;

		if (!subGroup.m_isAlreadyInProcess && subGroup.m_delay > 0.f && event != eVDBE_Repair)
		{
			m_delayedSubGroups.resize(m_delayedSubGroups.size() + 1);
			subGroup.m_isAlreadyInProcess = true;

			SDelayedDamagesSubGroupInfo& delayedSubGroupInfo = m_delayedSubGroups.back();

			delayedSubGroupInfo.delay = subGroup.m_delay;
			delayedSubGroupInfo.behaviorParams = behaviorParams;
			delayedSubGroupInfo.subGroupId = subGroup.id;
		}
		else
		{
			SVehicleDamageBehaviorEventParams params(behaviorParams);
			params.randomness = subGroup.m_randomness;

			for (TVehicleDamageBehaviorVector::iterator behaviorIte = damageBehaviors.begin(); behaviorIte != damageBehaviors.end(); ++behaviorIte)
			{
				IVehicleDamageBehavior* pBehavior = *behaviorIte;
				pBehavior->OnDamageEvent(event, params);
			}
		}
	}
}

//------------------------------------------------------------------------
void CVehicleDamagesGroup::Update(float frameTime)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	TDelayedDamagesSubGroupList::iterator ite = m_delayedSubGroups.begin();
	TDelayedDamagesSubGroupList::iterator next;
	for (; ite != m_delayedSubGroups.end(); ite = next)
	{
		next = ite;
		++next;

		SDelayedDamagesSubGroupInfo& delayedInfo = *ite;
		delayedInfo.delay -= frameTime;

		if (delayedInfo.delay <= 0.0f)
		{
			TDamagesSubGroupId id = delayedInfo.subGroupId;
			SDamagesSubGroup* pSubGroup = &m_damageSubGroups[id];
			delayedInfo.behaviorParams.randomness = pSubGroup->m_randomness;
			TVehicleDamageBehaviorVector& damageBehaviors = pSubGroup->m_damageBehaviors;

			for (TVehicleDamageBehaviorVector::iterator behaviorIte = damageBehaviors.begin(); behaviorIte != damageBehaviors.end(); ++behaviorIte)
			{
				IVehicleDamageBehavior* pBehavior = *behaviorIte;
				pBehavior->OnDamageEvent(eVDBE_ComponentDestroyed, delayedInfo.behaviorParams);
			}

			m_delayedSubGroups.erase(ite);
		}
	}

	if (!m_delayedSubGroups.empty())
		m_pVehicle->NeedsUpdate();
}

//------------------------------------------------------------------------
bool CVehicleDamagesGroup::IsPotentiallyFatal()
{
	for (TDamagesSubGroupVector::iterator subGroupIte = m_damageSubGroups.begin(); subGroupIte != m_damageSubGroups.end(); ++subGroupIte)
	{
		SDamagesSubGroup& subGroup = *subGroupIte;
		TVehicleDamageBehaviorVector& damageBehaviors = subGroup.m_damageBehaviors;

		for (TVehicleDamageBehaviorVector::iterator behaviorIte = damageBehaviors.begin(); behaviorIte != damageBehaviors.end(); ++behaviorIte)
		{
			//IVehicleDamageBehavior* pBehavio
			if (CVehicleDamageBehaviorDestroy* pBehaviorDestroy =
			      CAST_VEHICLEOBJECT(CVehicleDamageBehaviorDestroy, (*behaviorIte)))
			{
				return true;
			}
		}
	}

	return false;
}

//------------------------------------------------------------------------
void CVehicleDamagesGroup::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->AddObject(m_damageSubGroups);
}
