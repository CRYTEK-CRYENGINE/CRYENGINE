// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Implements a flow nodes for vehicles in Game02

   -------------------------------------------------------------------------
   History:
   - 12:12:2005: Created by Mathieu Pinard

*************************************************************************/

#include "StdAfx.h"
#include "Game.h"
#include "IVehicleSystem.h"
#include "VehicleActionEntityAttachment.h"
#include "GameCVars.h"
#include "VehicleMovementBase.h"

#include <CryFlowGraph/IFlowBaseNode.h>

class CVehicleActionEntityAttachment;

//------------------------------------------------------------------------
class CFlowVehicleEntityAttachment
	: public CFlowBaseNode<eNCT_Instanced>
{
public:

	CFlowVehicleEntityAttachment(SActivationInfo* pActivationInfo);
	~CFlowVehicleEntityAttachment() {}

	// CFlowBaseNode
	virtual IFlowNodePtr Clone(SActivationInfo* pActivationInfo);
	virtual void         GetConfiguration(SFlowNodeConfig& nodeConfig);
	virtual void         ProcessEvent(EFlowEvent flowEvent, SActivationInfo* pActivationInfo);
	virtual void         Serialize(SActivationInfo* pActivationInfo, TSerialize ser);

	virtual void         GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
	// ~CFlowBaseNode

protected:

	IVehicle*                       GetVehicle();
	CVehicleActionEntityAttachment* GetVehicleAction();

	IFlowGraph* m_pGraph;
	TFlowNodeId m_nodeID;

	EntityId    m_vehicleId;

	enum EInputs
	{
		IN_DROPATTACHMENTTRIGGER,
	};

	enum EOutputs
	{
		OUT_ENTITYID,
		OUT_ISATTACHED,
	};
};

//------------------------------------------------------------------------
CFlowVehicleEntityAttachment::CFlowVehicleEntityAttachment(SActivationInfo* pActivationInfo)
{
	m_nodeID = pActivationInfo->myID;
	m_pGraph = pActivationInfo->pGraph;

	if (IEntity* pEntity = pActivationInfo->pEntity)
	{
		IVehicleSystem* pVehicleSystem = gEnv->pGameFramework->GetIVehicleSystem();
		assert(pVehicleSystem);

		if (pVehicleSystem->GetVehicle(pEntity->GetId()))
			m_vehicleId = pEntity->GetId();
	}
	else
		m_vehicleId = 0;
}

//------------------------------------------------------------------------
IFlowNodePtr CFlowVehicleEntityAttachment::Clone(SActivationInfo* pActivationInfo)
{
	return new CFlowVehicleEntityAttachment(pActivationInfo);
}

void CFlowVehicleEntityAttachment::Serialize(SActivationInfo* pActivationInfo, TSerialize ser)
{
	// MATHIEU: FIXME!
}

//------------------------------------------------------------------------
void CFlowVehicleEntityAttachment::GetConfiguration(SFlowNodeConfig& nodeConfig)
{
	static const SInputPortConfig pInConfig[] =
	{
		InputPortConfig_Void("DropAttachmentTrigger", _HELP("Trigger to drop the attachment")),
		{ 0 }
	};

	static const SOutputPortConfig pOutConfig[] =
	{
		OutputPortConfig<int>("EntityId",    _HELP("Entity Id of the attachment")),
		OutputPortConfig<bool>("IsAttached", _HELP("If the attachment is still attached")),
		{ 0 }
	};

	nodeConfig.sDescription = _HELP("Handle the entity attachment used as vehicle action");
	nodeConfig.nFlags |= EFLN_TARGET_ENTITY;
	nodeConfig.pInputPorts = pInConfig;
	nodeConfig.pOutputPorts = pOutConfig;
	nodeConfig.SetCategory(EFLN_APPROVED);
}

//------------------------------------------------------------------------
void CFlowVehicleEntityAttachment::ProcessEvent(EFlowEvent flowEvent, SActivationInfo* pActivationInfo)
{
	if (flowEvent == eFE_SetEntityId)
	{
		if (IEntity* pEntity = pActivationInfo->pEntity)
		{
			IVehicleSystem* pVehicleSystem = gEnv->pGameFramework->GetIVehicleSystem();
			assert(pVehicleSystem);

			if (pEntity->GetId() != m_vehicleId)
				m_vehicleId = 0;

			if (pVehicleSystem->GetVehicle(pEntity->GetId()))
				m_vehicleId = pEntity->GetId();
		}
		else
		{
			m_vehicleId = 0;
		}
	}
	else if (flowEvent == eFE_Activate)
	{
		if (CVehicleActionEntityAttachment* pAction = GetVehicleAction())
		{
			if (IsPortActive(pActivationInfo, IN_DROPATTACHMENTTRIGGER))
			{
				pAction->DetachEntity();

				SFlowAddress addr(m_nodeID, OUT_ISATTACHED, true);
				m_pGraph->ActivatePort(addr, pAction->IsEntityAttached());
			}

			SFlowAddress addr(m_nodeID, OUT_ENTITYID, true);
			m_pGraph->ActivatePort(addr, pAction->GetAttachmentId());
		}
	}
}

//------------------------------------------------------------------------
IVehicle* CFlowVehicleEntityAttachment::GetVehicle()
{
	if (!m_vehicleId)
		return NULL;

	IVehicleSystem* pVehicleSystem = gEnv->pGameFramework->GetIVehicleSystem();
	assert(pVehicleSystem);

	return pVehicleSystem->GetVehicle(m_vehicleId);
}

//------------------------------------------------------------------------
CVehicleActionEntityAttachment* CFlowVehicleEntityAttachment::GetVehicleAction()
{
	if (!m_vehicleId)
		return NULL;

	IVehicleSystem* pVehicleSystem = gEnv->pGameFramework->GetIVehicleSystem();
	assert(pVehicleSystem);

	if (IVehicle* pVehicle = pVehicleSystem->GetVehicle(m_vehicleId))
	{
		for (int i = 1; i < pVehicle->GetActionCount(); i++)
		{
			IVehicleAction* pAction = pVehicle->GetAction(i);
			assert(pAction);
			PREFAST_ASSUME(pAction);

			if (CVehicleActionEntityAttachment* pAttachment =
			      CAST_VEHICLEOBJECT(CVehicleActionEntityAttachment, pAction))
			{
				return pAttachment;
			}
		}
	}

	return NULL;
}

//------------------------------------------------------------------------
class CFlowVehicleDriveForward : public CFlowBaseNode<eNCT_Instanced>, IVehicleEventListener
{
private:
	SActivationInfo        m_actInfo;
	float                  m_fSpeed;
	float                  m_fDuration;
	float                  m_fStartedTime;
	EVehicleSeatLockStatus m_prevSeatLockStatus;
	EntityId               m_entityId;
	bool                   m_bActive;
	bool                   m_bNeedsCleanup; // Vehicle listener can't be removed inside the event processing

public:
	CFlowVehicleDriveForward(SActivationInfo* pActivationInfo)
		: m_fSpeed(0.0f)
		, m_fDuration(0.0f)
		, m_fStartedTime(0.0f)
		, m_prevSeatLockStatus(eVSLS_Unlocked)
		, m_entityId(0)
		, m_bActive(false)
		, m_bNeedsCleanup(false)
	{};
	~CFlowVehicleDriveForward()
	{
		Cleanup();
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	virtual IFlowNodePtr Clone(SActivationInfo* pActivationInfo)
	{
		return new CFlowVehicleDriveForward(pActivationInfo);
	}

	enum Inputs
	{
		EIP_StartDriving,
		EIP_Time,
		EIP_Speed,
	};

	enum Outputs
	{
		EOP_Started,
		EOP_TimeComplete,
		EOP_Collided,
	};

	virtual void GetConfiguration(SFlowNodeConfig& nodeConfig)
	{
		static const SInputPortConfig pInConfig[] =
		{
			InputPortConfig_Void("StartDriving", _HELP("Starts the vehicle driving forward")),
			InputPortConfig<float>("Time",       1.0f,                                        _HELP("Time to drive")),
			InputPortConfig<float>("Speed",      30.0f,                                       _HELP("Speed at which to drive")),
			{ 0 }
		};

		static const SOutputPortConfig pOutConfig[] =
		{
			OutputPortConfig_Void("Started",      _HELP("Triggers when starts showing.")),
			OutputPortConfig_Void("TimeComplete", _HELP("Triggers when time is complete")),
			OutputPortConfig_Void("Collided",     _HELP("Triggers when collision occurs")),
			{ 0 }
		};

		nodeConfig.sDescription = _HELP("Node to drive vehicle forward");
		nodeConfig.nFlags |= EFLN_TARGET_ENTITY;
		nodeConfig.pInputPorts = pInConfig;
		nodeConfig.pOutputPorts = pOutConfig;
		nodeConfig.SetCategory(EFLN_ADVANCED);
	}

	void Cleanup()
	{
		if (m_bActive)
		{
			SetActive(false);
		}

		if (!m_bNeedsCleanup)
			return;

		m_bNeedsCleanup = false;

		IFlowGraph* pGraph = m_actInfo.pGraph;
		if (pGraph)
		{
			pGraph->SetRegularlyUpdated(m_actInfo.myID, false);
		}

		IVehicle* pVehicle;
		pVehicle = gEnv->pGameFramework->GetIVehicleSystem()->GetVehicle(m_entityId);

		if (pVehicle)
		{
			pVehicle->UnregisterVehicleEventListener(this);
		}
	}

	void SetActive(const bool bActive)
	{
		if (m_bActive == bActive)
			return;

		m_bActive = bActive;

		IVehicle* pVehicle;
		pVehicle = gEnv->pGameFramework->GetIVehicleSystem()->GetVehicle(m_entityId);

		if (bActive)
		{
			CRY_ASSERT(pVehicle);
			m_fStartedTime = gEnv->pTimer->GetFrameStartTime().GetSeconds();
			m_actInfo.pGraph->SetRegularlyUpdated(m_actInfo.myID, true);

			if (pVehicle)
			{
				pVehicle->RegisterVehicleEventListener(this, "CFlowVehicleDriveForward");
			}

			ActivateOutput(&m_actInfo, EOP_Started, true);
		}
		else
		{
			m_bNeedsCleanup = true; // To ensure event listener gets cleaned up when its safe to do so

			if (pVehicle)
			{
				if (pVehicle->GetSeatCount() != 0) // Restore back
				{
					IVehicleSeat* pSeat = pVehicle->GetSeatById(1);
					if (pSeat)
					{
						pSeat->SetLocked(m_prevSeatLockStatus);
						//pSeat->ExitRemotely();
					}
				}
			}
		}
	}

	virtual void ProcessEvent(EFlowEvent flowEvent, SActivationInfo* pActivationInfo)
	{
		if (flowEvent == eFE_Activate && IsPortActive(pActivationInfo, EIP_StartDriving))
		{
			IEntity* pEntity = pActivationInfo->pEntity;
			if (!pEntity)
				return;

			IVehicle* pVehicle;
			pVehicle = gEnv->pGameFramework->GetIVehicleSystem()->GetVehicle(pEntity->GetId());
			if (!pVehicle || pVehicle->IsDestroyed())
			{
				return;
			}

			IVehicleMovement* pMovement = pVehicle->GetMovement();
			if (!pMovement)
				return;

			CVehicleMovementBase* pMovementBase = StaticCast_CVehicleMovementBase(pMovement);
			if (!pMovementBase)
				return;

			IActor* pPlayer = g_pGame->GetIGameFramework()->GetClientActor();
			if (!pPlayer)
				return;

			const EntityId localPlayer = pPlayer->GetEntityId();
			if (pVehicle->GetSeatCount() == 0) // Don't need to remotely enter
			{
				pMovement->StartDriving(localPlayer);
			}
			else
			{
				pVehicle->EvictAllPassengers();

				IVehicleSeat* pSeat = pVehicle->GetSeatById(1);
				if (pSeat)
				{
					// Can't use remote entering to control otherwise if vehicle blows up, player dies
					//pSeat->EnterRemotely(localPlayer);

					pMovement->StartDriving(localPlayer);
					m_prevSeatLockStatus = pSeat->GetLockedStatus();
					pSeat->SetLocked(eVSLS_Locked);
				}
			}

			m_fDuration = GetPortFloat(pActivationInfo, EIP_Time);
			m_fSpeed = GetPortFloat(pActivationInfo, EIP_Speed);
			m_actInfo = *pActivationInfo;
			m_entityId = pEntity->GetId();

			SetActive(true);
		}
		else if (flowEvent == eFE_Update)
		{
			if (!m_bActive)
			{
				if (m_bNeedsCleanup)
				{
					Cleanup();
				}

				return;
			}

			IEntity* pEntity = pActivationInfo->pEntity;
			if (!pEntity)
			{
				SetActive(false);
				return;
			}

			IVehicle* pVehicle;
			pVehicle = gEnv->pGameFramework->GetIVehicleSystem()->GetVehicle(pEntity->GetId());
			if (!pVehicle || pVehicle->IsDestroyed())
			{
				SetActive(false);
				return;
			}

			const float curTime = gEnv->pTimer->GetFrameStartTime().GetSeconds();
			if ((curTime - m_fStartedTime) >= m_fDuration)
			{
				SetActive(false);

				ActivateOutput(pActivationInfo, EOP_TimeComplete, true);
			}
			else // Update every frame
			{
				IVehicleMovement* pMovement = pVehicle->GetMovement();
				if (pMovement)
				{
					// prevent full pedal being kept pressed, but give it a bit
					pMovement->OnAction(eVAI_MoveForward, eAAM_OnPress, 1.0f);
				}
			}
		}
	}

	void OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params)
	{
		if ((event == eVE_Destroyed) ||
		    (event == eVE_PreVehicleDeletion) ||
		    (event == eVE_Hide))   // Safer to ensure the listener is removed at this point
		{
			SetActive(false);
		}
		else if (event == eVE_VehicleDeleted) // This shouldn't be called due to above but just in case
		{
			SetActive(false);
		}
		else if (event == eVE_Collision) // Currently disabled in C2 code
		{
			ActivateOutput(&m_actInfo, EOP_Collided, true);
		}
		else if (event == eVE_Hit) // Needed to detect collision in C2 code since above is disabled
		{
			IGameRules* pGameRules = gEnv->pGameFramework->GetIGameRulesSystem()->GetCurrentGameRules();
			if (pGameRules)
			{
				const int collisionHitType = pGameRules->GetHitTypeId("collision");

				if (params.iParam == collisionHitType)
				{
					ActivateOutput(&m_actInfo, EOP_Collided, true);
				}
			}
		}
	}
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_VehicleDebugDraw : public CFlowBaseNode<eNCT_Instanced>
{

public:

	enum EInputs
	{
		IN_SHOW,
		IN_SIZE,
		IN_PARTS,
	};

	enum EOutputs
	{

	};

	CFlowNode_VehicleDebugDraw(SActivationInfo* pActInfo)
		: column1(0.0f)
		, column2(0.0f)
		, loops(0)
	{

	}

	~CFlowNode_VehicleDebugDraw()
	{

	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNode_VehicleDebugDraw(pActInfo);
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] =
		{
			InputPortConfig_Void("Trigger",               _HELP("show debug informations on screen")),
			InputPortConfig<float>("Size",                1.5f,                                       _HELP("font size")),
			InputPortConfig<string>("vehicleParts_Parts", _HELP("select vehicle parts"),              0, _UICONFIG("ref_entity=entityId")),
			{ 0 }
		};

		static const SOutputPortConfig out_config[] =
		{
			{ 0 }
		};

		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_DEBUG);
	}

	string currentParam;
	string currentSetting;

	float  column1;
	float  column2;
	int    loops;

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		IVehicleSystem* pVehicleSystem = NULL;
		IVehicle* pVehicle = NULL;

		switch (event)
		{
		case eFE_Initialize:
			{
				pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
				break;
			}

		case eFE_Activate:
			{
				if (!pActInfo->pEntity)
					return;

				pVehicleSystem = gEnv->pGameFramework->GetIVehicleSystem();
				pVehicle = pVehicleSystem->GetVehicle(pActInfo->pEntity->GetId());

				if (!pVehicleSystem || !pVehicle)
					return;

				string givenString = GetPortString(pActInfo, IN_PARTS);
				currentParam = givenString.substr(0, givenString.find_first_of(":"));
				currentSetting = givenString.substr(givenString.find_first_of(":") + 1, (givenString.length() - givenString.find_first_of(":")));

				column1 = 10.f;
				column2 = 100.f;

				if (IsPortActive(pActInfo, IN_SHOW))
					pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);

				break;
			}

		case eFE_Update:
			{
				pVehicleSystem = gEnv->pGameFramework->GetIVehicleSystem();
				pVehicle = pVehicleSystem->GetVehicle(pActInfo->pEntity->GetId());

				if (!pVehicleSystem || !pActInfo->pEntity || !pVehicle)
				{
					return;
				}

				IRenderAuxText::Draw2dLabel(column1, 10, GetPortFloat(pActInfo, IN_SIZE) + 2.f, Col_Cyan, false, "%s", pActInfo->pEntity->GetName());

				if (currentParam == "Seats")
				{
					loops = 0;

					for (uint32 i = 0; i < pVehicle->GetSeatCount(); i++)
					{
						IVehicleSeat* currentSeat;

						if (currentSetting == "All")
						{
							currentSeat = pVehicle->GetSeatById(i + 1);
						}
						else
						{
							currentSeat = pVehicle->GetSeatById(pVehicle->GetSeatId(currentSetting));
							i = pVehicle->GetSeatCount() - 1;
						}

						loops += 1;

						// column 1
						string pMessage = string().Format("%s:", currentSeat->GetSeatName());

						if (column2 < pMessage.size() * 8 * GetPortFloat(pActInfo, IN_SIZE))
						{
							column2 = pMessage.size() * 8 * GetPortFloat(pActInfo, IN_SIZE);
						}

						IRenderAuxText::Draw2dLabel(column1, (15 * (float(loops + 1)) * GetPortFloat(pActInfo, IN_SIZE)), GetPortFloat(pActInfo, IN_SIZE), Col_Cyan, false, "%s", pMessage.c_str());

						// column 2
						if (currentSeat->GetPassenger(true))
						{
							pMessage = string().Format("- %s", gEnv->pEntitySystem->GetEntity(currentSeat->GetPassenger(true))->GetName());
							IRenderAuxText::Draw2dLabel(column2, (15 * (float(loops + 1)) * GetPortFloat(pActInfo, IN_SIZE)), GetPortFloat(pActInfo, IN_SIZE), Col_Cyan, false, "%s", pMessage.c_str());
						}
					}
				}
				else if (currentParam == "Wheels")
				{
					IRenderAuxText::Draw2dLabel(column1, 50.f, GetPortFloat(pActInfo, IN_SIZE) + 1.f, Col_Red, false, "!");
				}
				else if (currentParam == "Weapons")
				{
					loops = 0;

					for (int i = 0; i < pVehicle->GetWeaponCount(); i++)
					{
						IItemSystem* pItemSystem = gEnv->pGameFramework->GetIItemSystem();
						IWeapon* currentWeapon;
						EntityId currentEntityId;
						IItem* pItem;

						if (currentSetting == "All")
						{
							currentEntityId = pVehicle->GetWeaponId(i + 1);
						}
						else
						{
							currentEntityId = gEnv->pEntitySystem->FindEntityByName(currentSetting)->GetId();
							i = pVehicle->GetWeaponCount() - 1;
						}

						if (!pItemSystem->GetItem(currentEntityId))
							return;

						pItem = pItemSystem->GetItem(currentEntityId);
						currentWeapon = pItem->GetIWeapon();

						loops += 1;

						// column 1
						string pMessageName = string().Format("%s", gEnv->pEntitySystem->GetEntity(currentEntityId)->GetName());
						IRenderAuxText::Draw2dLabel(column1, (15 * (float(loops + 1)) * GetPortFloat(pActInfo, IN_SIZE)), GetPortFloat(pActInfo, IN_SIZE), Col_Cyan, false, "%s", pMessageName.c_str());

						if (column2 < pMessageName.size() * 8 * GetPortFloat(pActInfo, IN_SIZE))
						{
							column2 = pMessageName.size() * 8 * GetPortFloat(pActInfo, IN_SIZE);
						}

						// column 2
						string pMessageValue = string().Format("seat: %s firemode: %i", pVehicle->GetWeaponParentSeat(currentEntityId)->GetSeatName(), currentWeapon->GetCurrentFireMode()).c_str();
						IRenderAuxText::Draw2dLabel(column2, (15 * (float(loops + 1)) * GetPortFloat(pActInfo, IN_SIZE)), GetPortFloat(pActInfo, IN_SIZE), Col_Cyan, false, "%s", pMessageName.c_str());
					}
				}
				else if (currentParam == "Components")
				{
					loops = 0;

					for (int i = 0; i < pVehicle->GetComponentCount(); i++)
					{
						IVehicleComponent* currentComponent;

						if (currentSetting == "All")
						{
							currentComponent = pVehicle->GetComponent(i);
						}
						else
						{
							currentComponent = pVehicle->GetComponent(currentSetting);
							i = pVehicle->GetComponentCount() - 1;
						}

						loops += 1;

						ColorF labelColor;
						labelColor = ColorF(currentComponent->GetDamageRatio(), (1.f - currentComponent->GetDamageRatio()), 0.f);

						// column 1
						string pMessageName = string().Format("%s", currentComponent->GetComponentName()).c_str();
						IRenderAuxText::Draw2dLabel(column1, (15 * (float(loops + 1)) * GetPortFloat(pActInfo, IN_SIZE)), GetPortFloat(pActInfo, IN_SIZE), labelColor, false, "%s", pMessageName.c_str());

						if (column2 < pMessageName.size() * 8 * GetPortFloat(pActInfo, IN_SIZE))
						{
							column2 = pMessageName.size() * 8 * GetPortFloat(pActInfo, IN_SIZE);
						}

						// column 2
						string pMessageValue = string().Format("%5.2f (%3.2f)", currentComponent->GetDamageRatio() * currentComponent->GetMaxDamage(), currentComponent->GetDamageRatio()).c_str();
						IRenderAuxText::Draw2dLabel(column2, (15 * (float(loops + 1)) * GetPortFloat(pActInfo, IN_SIZE)), GetPortFloat(pActInfo, IN_SIZE), labelColor, false, "%s", pMessageName.c_str());
					}
				}
				else
				{
					IRenderAuxText::Draw2dLabel(column1, 50.f, GetPortFloat(pActInfo, IN_SIZE) + 1.f, Col_Red, false, "no component selected!");
				}
				break;
			}
		}
	};
};

REGISTER_FLOW_NODE("Vehicle:EntityAttachment", CFlowVehicleEntityAttachment);
REGISTER_FLOW_NODE("Vehicle:DriveForward", CFlowVehicleDriveForward);
REGISTER_FLOW_NODE("Vehicle:DebugDraw", CFlowNode_VehicleDebugDraw);
