// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FlowVehicleSeat.h"

#include "CryAction.h"
#include "IVehicleSystem.h"
#include "VehicleSystem/Vehicle.h"
#include "VehicleSystem/VehicleSeat.h"

#include <CryFlowGraph/IFlowBaseNode.h>

IFlowNodePtr CFlowVehicleSeat::Clone(SActivationInfo* pActivationInfo)
{
	IFlowNode* pNode = new CFlowVehicleSeat(pActivationInfo);
	return pNode;
}

void CFlowVehicleSeat::GetConfiguration(SFlowNodeConfig& nodeConfig)
{
	CFlowVehicleBase::GetConfiguration(nodeConfig);

	static const SInputPortConfig pInConfig[] =
	{
		InputPortConfig_Void("Trigger",  _HELP("Activates the node")),
		InputPortConfig<string>("Seat",  _HELP("Seat to (un)lock")),
		InputPortConfig<bool>("Lock",    _HELP("Action to perform, set to 1 to Lock and 0 to Unlock"),_HELP("(Un)Lock")),
		InputPortConfig<int>("LockType", 0,                                                           _HELP("Who to lock the vehicle for"), NULL,_UICONFIG("enum_int:All=0,Players=1,AI=2")),
		{ 0 }
	};

	static const SOutputPortConfig pOutConfig[] =
	{
		OutputPortConfig<bool>("Success", _HELP("Activated when the seat (un)lock is successful or fails")),
		{ 0 }
	};

	nodeConfig.sDescription = _HELP("Lock and Unlock specific vehicle seats for AI or the Player");
	nodeConfig.nFlags |= EFLN_TARGET_ENTITY;
	nodeConfig.pInputPorts = pInConfig;
	nodeConfig.pOutputPorts = pOutConfig;
	nodeConfig.SetCategory(EFLN_APPROVED);
}

void CFlowVehicleSeat::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	CFlowVehicleBase::ProcessEvent(event, pActInfo); // handles eFE_SetEntityId

	if (event == eFE_Activate)
	{
		if (IsPortActive(pActInfo, eIn_Trigger))
		{
			bool bSuccess = false;

			CVehicle* pVehicle = static_cast<CVehicle*>(GetVehicle());
			if (pVehicle)
			{
				const string seatName = GetPortString(pActInfo, eIn_Seat);
				CVehicleSeat* pSeat = static_cast<CVehicleSeat*>(pVehicle->GetSeatById(pVehicle->GetSeatId(seatName)));
				if (pSeat)
				{
					bool bLock = GetPortBool(pActInfo, eIn_Lock);
					if (bLock)
					{
						EVehicleSeatLockStatus enumLockType = eVSLS_Locked;
						int intLockType = GetPortInt(pActInfo, eIn_LockType);
						switch (intLockType)
						{
						case 0:
							enumLockType = eVSLS_Locked;
							break;
						case 1:
							enumLockType = eVSLS_LockedForPlayers;
							break;
						case 2:
							enumLockType = eVSLS_LockedForAI;
							break;
						default:
							assert(false);
							break;
						}
						pSeat->SetLocked(enumLockType);
						bSuccess = true;
					}
					else
					{
						pSeat->SetLocked(eVSLS_Unlocked);
						bSuccess = true;
					}
				}
			}

			ActivateOutput(pActInfo, eOut_Success, bSuccess);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
class CFlowVehicleChangeSeat : public CFlowBaseNode<eNCT_Singleton>
{
	enum EInputPorts
	{
		eINP_Trigger,
		eINP_VehicleId,
		eINP_Seat
	};

	enum EOutputPorts
	{
		eOUT_Success,
	};
public:
	CFlowVehicleChangeSeat(SActivationInfo* pActInfo) {}
	~CFlowVehicleChangeSeat() {}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inp_config[] = {
			InputPortConfig_Void("Sync",           _HELP("Triggers the seat change"),                   _HELP("Trigger")),
			InputPortConfig<EntityId>("VehicleId", _HELP("Id of the vehicle which contains the seat")),
			InputPortConfig<string>("SeatNumber",  _HELP("New seat"),                                   _HELP("Seat"),    _UICONFIG("dt=vehicleSeats, ref_entity=VehicleId")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<bool>("Success", _HELP("Triggers when the node is processed, outputing 0 or 1 representing fail or success")),
			{ 0 }
		};

		config.sDescription = _HELP("Moves an actor from a seat to another one. Only works if the actor is already inside a vehicle");
		config.pInputPorts = inp_config;
		config.pOutputPorts = out_config;
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, eINP_Trigger))
				{
					bool success = false;

					if (pActInfo->pEntity)
					{
						EntityId actorEntityId = pActInfo->pEntity->GetId();
						EntityId vehicleId = GetPortInt(pActInfo, eINP_VehicleId);
						CVehicle* pVehicle = static_cast<CVehicle*>(CCryAction::GetCryAction()->GetIVehicleSystem()->GetVehicle(vehicleId));
						if (actorEntityId && pVehicle)
						{
							IActor* pActor = gEnv->pGameFramework->GetIActorSystem()->GetActor(actorEntityId);
							if (pActor && pActor->GetLinkedVehicle() == pVehicle)
							{
								const string seatName = GetPortString(pActInfo, eINP_Seat);
								TVehicleSeatId newSeatId = pVehicle->GetSeatId(seatName);

								pVehicle->ChangeSeat(actorEntityId, 0, newSeatId);
								IVehicleSeat* pNewSeat = pVehicle->GetSeatForPassenger(actorEntityId);
								success = pNewSeat && pNewSeat->GetSeatId() == newSeatId;
							}
						}
					}
					ActivateOutput(pActInfo, eOUT_Success, success);
				}
				break;
			}
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

};

//////////////////////////////////////////////////////////////////////////
class CFlowVehicleLock : public CFlowBaseNode<eNCT_Singleton>
{
	enum EInputPorts
	{
		eINP_Lock = 0,
		eINP_Unlock,
		eINP_LockFor,
	};

public:
	CFlowVehicleLock(SActivationInfo* pActInfo) {}
	~CFlowVehicleLock() {}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inp_config[] = {
			InputPortConfig_Void("Lock",     _HELP("Locks the vehicle")),
			InputPortConfig_Void("Unlock",   _HELP("Unlocks the vehicle")),
			InputPortConfig<int>("LockType", 0,                            _HELP("Who to lock the vehicle for"),NULL, _UICONFIG("enum_int:All=0,Players=1,AI=2")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			{ 0 }
		};

		config.sDescription = _HELP("Locks/Unlocks all seats of a vehicle");
		config.pInputPorts = inp_config;
		config.pOutputPorts = out_config;
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			{
				IVehicle* pVehicle = pActInfo->pEntity ? CCryAction::GetCryAction()->GetIVehicleSystem()->GetVehicle(pActInfo->pEntity->GetId()) : NULL;

				if (pVehicle)
				{
					int lockFor = GetPortInt(pActInfo, eINP_LockFor);

					if (IsPortActive(pActInfo, eINP_Lock))
						Lock(pVehicle, true, lockFor);
					if (IsPortActive(pActInfo, eINP_Unlock))
						Lock(pVehicle, false, 0);
				}
				break;
			}
		}
	}

	void Lock(IVehicle* pVehicle, bool lock, int lockFor)
	{
		EVehicleSeatLockStatus lockType = eVSLS_Unlocked;
		if (lock)
		{
			switch (lockFor)
			{
			case 0:
				lockType = eVSLS_Locked;
				break;
			case 1:
				lockType = eVSLS_LockedForPlayers;
				break;
			case 2:
				lockType = eVSLS_LockedForAI;
				break;

			default:
				assert(false);
				break;
			}
		}

		for (uint32 i = 0; i < pVehicle->GetSeatCount(); ++i)
		{
			CVehicleSeat* pSeat = static_cast<CVehicleSeat*>(pVehicle->GetSeatById(i + 1));         // vehicle seats start at id 1
			if (pSeat)
			{
				pSeat->SetLocked(lockType);
			}
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

};

//////////////////////////////////////////////////////////////////////////
// Get vehicle seat helper positions
//////////////////////////////////////////////////////////////////////////
void CFlowNode_GetSeatHelperVehicle::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void("Get",           _HELP("Trigger to get the values"),                    _HELP("Trigger")),
		InputPortConfig<string>("SeatNumber", _HELP("Seat that is connected to the 'enter' helper"), _HELP("Seat"),    _UICONFIG("dt=vehicleSeats, ref_entity=entityId")),
		{ 0 }
	};
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<Vec3>("pos", _HELP("Position of the seat helper")),
		OutputPortConfig<Vec3>("dir", _HELP("Direction of the seat helper")),
		{ 0 }
	};

	config.sDescription = _HELP("Gets the position and orientation of a seat's 'enter' helper in world space");
	config.nFlags |= EFLN_TARGET_ENTITY;
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.SetCategory(EFLN_APPROVED);
}

//
//-------------------------------------------------------------------------------------------------------------
void CFlowNode_GetSeatHelperVehicle::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	CFlowVehicleBase::ProcessEvent(event, pActInfo); // handles eFE_SetEntityId

	if (event == eFE_Activate && IsPortActive(pActInfo, eIn_Get))
	{
		CVehicle* pVehicle = static_cast<CVehicle*>(GetVehicle());
		if (pVehicle)
		{
			const string seatName = GetPortString(pActInfo, eIn_Seat);
			CVehicleSeat* pSeat = static_cast<CVehicleSeat*>(pVehicle->GetSeatById(pVehicle->GetSeatId(seatName)));
			if (pSeat)
			{
				Matrix34 tm;
				pSeat->GetEnterHelper()->GetWorldTM(tm);
				ActivateOutput(pActInfo, eOut_Pos, tm.GetTranslation());
				ActivateOutput(pActInfo, eOut_Dir, tm.GetColumn(1));
			}
		}
	}
}


REGISTER_FLOW_NODE("Vehicle:Seat", CFlowVehicleSeat);
REGISTER_FLOW_NODE("Vehicle:ChangeSeat", CFlowVehicleChangeSeat)
REGISTER_FLOW_NODE("Vehicle:Lock", CFlowVehicleLock)
REGISTER_FLOW_NODE("Vehicle:GetSeatHelper", CFlowNode_GetSeatHelperVehicle)