// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Implements a flow node to handle vehicle damages

   -------------------------------------------------------------------------
   History:
   - 02:12:2005: Created by Mathieu Pinard

*************************************************************************/
#include "StdAfx.h"
#include "FlowVehicleDamage.h"

#include "CryAction.h"
#include "IGameRulesSystem.h"
#include "IVehicleSystem.h"
#include "VehicleSystem/Vehicle.h"
#include "VehicleSystem/VehicleComponent.h"

#include <CryFlowGraph/IFlowBaseNode.h>

//------------------------------------------------------------------------
IFlowNodePtr CFlowVehicleDamage::Clone(SActivationInfo* pActivationInfo)
{
	IFlowNode* pNode = new CFlowVehicleDamage(pActivationInfo);
	return pNode;
}

//------------------------------------------------------------------------
void CFlowVehicleDamage::GetConfiguration(SFlowNodeConfig& nodeConfig)
{
	CFlowVehicleBase::GetConfiguration(nodeConfig);

	static const SInputPortConfig pInConfig[] =
	{
		InputPortConfig_AnyType("HitTrigger",   _HELP("trigger which cause the vehicle to receive damages")),
		InputPortConfig<float>("HitValue",      _HELP("ratio (0 to 1) of damage vehicle will receive when HitTrigger is set")),
		InputPortConfig<Vec3>("HitPosition",    _HELP("local position at which the vehicle will receive the hit")),
		InputPortConfig<float>("HitRadius",     _HELP("radius of the hit")),
		InputPortConfig<bool>("Indestructible", _HELP("Trigger true to set vehicle to indestructible, false to reset.")),
		InputPortConfig<string>("HitType",      _HELP("an hit type which should be known by the GameRules")),
		InputPortConfig<string>("HitComponent", _HELP("component that will receive the hit")),
		{ 0 }
	};

	static const SOutputPortConfig pOutConfig[] =
	{
		OutputPortConfig<float>("Damaged",  _HELP("hit value received by the vehicle which caused some damage")),
		OutputPortConfig<bool>("Destroyed", _HELP("vehicle gets destroyed")),
		OutputPortConfig<float>("Hit",      _HELP("hit value received by the vehicle which may or may not cause damage")),
		{ 0 }
	};

	nodeConfig.sDescription = _HELP("Handle vehicle damage");
	nodeConfig.nFlags |= EFLN_TARGET_ENTITY;
	nodeConfig.pInputPorts = pInConfig;
	nodeConfig.pOutputPorts = pOutConfig;
	nodeConfig.SetCategory(EFLN_APPROVED);
}

//------------------------------------------------------------------------
void CFlowVehicleDamage::ProcessEvent(EFlowEvent flowEvent, SActivationInfo* pActivationInfo)
{
	CFlowVehicleBase::ProcessEvent(flowEvent, pActivationInfo);

	if (flowEvent == eFE_Initialize)
	{
		ActivateOutput(pActivationInfo, OUT_DAMAGED, 0.0f);
		ActivateOutput(pActivationInfo, OUT_DESTROYED, false);
		ActivateOutput(pActivationInfo, OUT_HIT, 0.0f);
	}

	if (flowEvent == eFE_Activate)
	{
		if (IsPortActive(pActivationInfo, IN_INDESTRUCTIBLE))
		{
			if (CVehicle* pVehicle = static_cast<CVehicle*>(GetVehicle()))
			{
				SVehicleEventParams params;
				params.bParam = GetPortBool(pActivationInfo, IN_INDESTRUCTIBLE);
				pVehicle->BroadcastVehicleEvent(eVE_Indestructible, params);
			}
		}

		if (IsPortActive(pActivationInfo, IN_HITTRIGGER))
		{
			if (IVehicle* pVehicle = GetVehicle())
			{
				float hitValue = GetPortFloat(pActivationInfo, IN_HITVALUE);

				if (hitValue > 0.0f)
				{
					Vec3 hitPos = GetPortVec3(pActivationInfo, IN_HITPOS);
					float hitRadius = GetPortFloat(pActivationInfo, IN_HITRADIUS);
					string hitType = GetPortString(pActivationInfo, IN_HITTYPE);

					bool bBadHitType = hitType.empty();

					IGameRules* pGameRules = CCryAction::GetCryAction()->GetIGameRulesSystem()->GetCurrentGameRules();
					int hitTypeId = 0;

					if (!bBadHitType)
					{
						hitTypeId = pGameRules ? pGameRules->GetHitTypeId(hitType.c_str()) : 0;
						bBadHitType = (hitTypeId == 0);
					}

					if (bBadHitType)
					{
						if (!hitType.empty())
							GameWarning("Forcing unknown flow vehicle damage type '%s' to 'normal'", hitType.c_str());
						hitType = "normal";

						hitTypeId = pGameRules ? pGameRules->GetHitTypeId(hitType.c_str()) : 0;
					}

					IVehicleComponent* pHitComponent = pVehicle->GetComponent(GetPortString(pActivationInfo, IN_HITCOMPONENT));

					if (pHitComponent)
					{
						hitValue *= pHitComponent->GetMaxDamage();
					}
					else
					{
						const TVehicleComponentVector& vehicleComponents = static_cast<CVehicle*>(pVehicle)->GetComponents();

						for (TVehicleComponentVector::const_iterator i = vehicleComponents.begin(), end = vehicleComponents.end(); i != end; ++i)
						{
							const CVehicleComponent* pComponent = *i;

							if (pComponent->IsHull())
							{
								hitValue *= pComponent->GetMaxDamage();

								break;
							}
						}
					}

					HitInfo hitInfo;
					hitInfo.targetId = pVehicle->GetEntityId();
					hitInfo.shooterId = hitInfo.targetId;
					hitInfo.damage = hitValue;
					hitInfo.pos = pVehicle->GetEntity()->GetWorldTM() * hitPos;
					hitInfo.radius = hitRadius;
					pVehicle->OnHit(hitInfo, pHitComponent);
				}
			}
		}
	}
}

//------------------------------------------------------------------------
void CFlowVehicleDamage::Serialize(SActivationInfo* pActivationInfo, TSerialize ser)
{
	CFlowVehicleBase::Serialize(pActivationInfo, ser);
}

//------------------------------------------------------------------------
void CFlowVehicleDamage::OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params)
{
	CFlowVehicleBase::OnVehicleEvent(event, params);

	if (event == eVE_Damaged)
	{
		SFlowAddress addr(m_nodeID, OUT_DAMAGED, true);
		m_pGraph->ActivatePort(addr, params.fParam);
	}
	else if (event == eVE_Destroyed)
	{
		SFlowAddress addr(m_nodeID, OUT_DESTROYED, true);
		m_pGraph->ActivatePort(addr, true);
	}
	else if (event == eVE_Hit)
	{
		SFlowAddress addr(m_nodeID, OUT_HIT, true);
		m_pGraph->ActivatePort(addr, params.fParam);
	}
}

//------------------------------------------------------------------------
class CFlowVehicleDestroy : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowVehicleDestroy(SActivationInfo* pActivationInfo) {};
	~CFlowVehicleDestroy() {}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	enum Inputs
	{
		EIP_Destroy,
	};

	virtual void GetConfiguration(SFlowNodeConfig& nodeConfig)
	{
		static const SInputPortConfig pInConfig[] =
		{
			InputPortConfig_Void("Destroy", _HELP("Trigger to destroy")),
			{ 0 }
		};

		nodeConfig.sDescription = _HELP("Node to destroy vehicle");
		nodeConfig.nFlags |= EFLN_TARGET_ENTITY;
		nodeConfig.pInputPorts = pInConfig;
		nodeConfig.pOutputPorts = 0;
		nodeConfig.SetCategory(EFLN_ADVANCED);
	}

	virtual void ProcessEvent(EFlowEvent flowEvent, SActivationInfo* pActivationInfo)
	{
		if (flowEvent == eFE_Activate && IsPortActive(pActivationInfo, EIP_Destroy))
		{
			IEntity* pEntity = pActivationInfo->pEntity;
			if (!pEntity)
				return;

			IVehicle* pVehicle;
			pVehicle = gEnv->pGameFramework->GetIVehicleSystem()->GetVehicle(pEntity->GetId());
			if (pVehicle && !pVehicle->IsDestroyed())
			{
				pVehicle->Destroy();
			}
		}
	}
};

REGISTER_FLOW_NODE("Vehicle:Damage", CFlowVehicleDamage);
REGISTER_FLOW_NODE("Vehicle:Destroy", CFlowVehicleDestroy);
