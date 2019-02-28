// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   FlowWeaponNodes.cpp
//  Version:     v1.00
//  Created:     July 7th 2005 by Julien.
//  Compilers:   Visual Studio.NET 2003
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "CryAction.h"
#include "IItemSystem.h"
#include "IWeapon.h"
#include "IActorSystem.h"


#include <CryEntitySystem/IEntitySystem.h>
#include <CryFlowGraph/IFlowBaseNode.h>

namespace
{
IItem* GetItem(EntityId entityId)
{
	IItemSystem* pItemSystem = CCryAction::GetCryAction()->GetIItemSystem();
	return pItemSystem->GetItem(entityId);
}

IWeapon* GetWeapon(EntityId entityId)
{
	IItem* pItem = GetItem(entityId);
	return pItem ? pItem->GetIWeapon() : NULL;
}
} // anon namespace

struct SWeaponListener : public IWeaponEventListener
{
	// IWeaponEventListener
	virtual void OnShoot(IWeapon* pWeapon, EntityId shooterId, EntityId ammoId, IEntityClass* pAmmoType, const Vec3& pos, const Vec3& dir, const Vec3& vel) {}
	virtual void OnStartFire(IWeapon* pWeapon, EntityId shooterId)                                                                                          {}
	virtual void OnStopFire(IWeapon* pWeapon, EntityId shooterId)                                                                                           {}
	virtual void OnFireModeChanged(IWeapon* pWeapon, int currentFireMode)                                                                                   {}
	virtual void OnStartReload(IWeapon* pWeapon, EntityId shooterId, IEntityClass* pAmmoType)                                                               {}
	virtual void OnEndReload(IWeapon* pWeapon, EntityId shooterId, IEntityClass* pAmmoType)                                                                 {}
	virtual void OnSetAmmoCount(IWeapon* pWeapon, EntityId shooterId)                                                                                       {}
	virtual void OnOutOfAmmo(IWeapon* pWeapon, IEntityClass* pAmmoType)                                                                                     {}
	virtual void OnReadyToFire(IWeapon* pWeapon)                                                                                                            {}
	virtual void OnPickedUp(IWeapon* pWeapon, EntityId actorId, bool destroyed)                                                                             {}
	virtual void OnDropped(IWeapon* pWeapon, EntityId actorId)                                                                                              {}
	virtual void OnMelee(IWeapon* pWeapon, EntityId shooterId)                                                                                              {}
	virtual void OnStartTargetting(IWeapon* pWeapon)                                                                                                        {}
	virtual void OnStopTargetting(IWeapon* pWeapon)                                                                                                         {}
	virtual void OnSelected(IWeapon* pWeapon, bool select)                                                                                                  {}
	virtual void OnEndBurst(IWeapon* pWeapon, EntityId shooterId)                                                                                           {}
	// ~IWeaponEventListener

	virtual void AddListener(EntityId weaponId, IWeaponEventListener* pListener)
	{
		if (!(weaponId && pListener))
			return;

		if (IWeapon* pWeapon = GetWeapon(weaponId))
			pWeapon->AddEventListener(pListener, __FUNCTION__);
	}

	virtual void RemoveListener(EntityId weaponId, IWeaponEventListener* pListener)
	{
		if (!(weaponId && pListener))
			return;

		if (IWeapon* pWeapon = GetWeapon(weaponId))
			pWeapon->RemoveEventListener(pListener);
	}
};

class CFlowNode_AutoSightWeapon : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_AutoSightWeapon(SActivationInfo* pActInfo)
	{
	}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_ports[] =
		{
			InputPortConfig<Vec3>("enemy", _HELP("Connect the enemy position vector to shoot here")),
			{ 0 }
		};
		config.pInputPorts = in_ports;
		config.pOutputPorts = 0;
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		/*if (event == eFE_Activate && IsPortActive(pActInfo,0))
		   {
		   IScriptSystem * pSS = gEnv->pScriptSystem;
		   SmartScriptTable table;
		   pSS->GetGlobalValue("Weapon", table);
		   Script::CallMethod( table, "StartFire");
		   }
		   else */
		if (event == eFE_Initialize)
		{
			pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
		}
		else if (event == eFE_Update)
		{
			IEntity* pEntity = pActInfo->pEntity;
			if (pEntity)
			{
				Vec3 dir = GetPortVec3(pActInfo, 0) - pEntity->GetWorldTM().GetTranslation();

				dir.normalize();

				Vec3 up(0, 0, 1);
				Vec3 right(dir ^ up);

				if (right.len2() < 0.01f)
				{
					right = dir.GetOrthogonal();
				}

				Matrix34 tm(pEntity->GetWorldTM());

				tm.SetColumn(1, dir);
				tm.SetColumn(0, right.normalize());
				tm.SetColumn(2, right ^ dir);

				pEntity->SetWorldTM(tm);
			}
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

private:
};

class CFlowNode_WeaponListener : public CFlowBaseNode<eNCT_Instanced>, public SWeaponListener
{
public:

	enum EInputs
	{
		IN_ENABLE = 0,
		IN_DISABLE,
		IN_WEAPONID,
		IN_WEAPONCLASS,
		IN_AMMO,
	};

	enum EOutputs
	{
		OUT_ONSHOOT = 0,
		OUT_AMMOLEFT,
		OUT_ONMELEE,
		OUT_ONDROPPED,
	};

	CFlowNode_WeaponListener(SActivationInfo* pActInfo)
	{
		m_active = false;
		m_ammo = 0;
		m_weapId = 0;
	}

	~CFlowNode_WeaponListener()
	{
		RemoveListener(m_weapId, this);
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		IFlowNode* pNode = new CFlowNode_WeaponListener(pActInfo);
		return pNode;
	}

	void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
		ser.Value("active", m_active);
		ser.Value("ammo", m_ammo);
		ser.Value("weaponId", m_weapId);

		if (ser.IsReading())
		{
			if (m_active && m_weapId != 0)
			{
				IItemSystem* pItemSys = CCryAction::GetCryAction()->GetIItemSystem();

				IItem* pItem = pItemSys->GetItem(m_weapId);

				if (!pItem || !pItem->GetIWeapon())
				{
					GameWarning("[flow] CFlowNode_WeaponListener: Serialize no item/weapon.");
					return;
				}
				IWeapon* pWeapon = pItem->GetIWeapon();
				// set weapon listener
				pWeapon->AddEventListener(this, "CFlowNode_WeaponListener");
				// CryLog("[flow] CFlowNode_WeaponListener::Serialize() successfully created on '%s'", pItem->GetEntity()->GetName());
			}
			else
			{
				Reset();
			}
		}
	}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] =
		{
			InputPortConfig_Void("Enable",         _HELP("Enable Listener")),
			InputPortConfig_Void("Disable",        _HELP("Disable Listener")),
			InputPortConfig<EntityId>("WeaponId",  _HELP("Listener will be set on this weapon if active")),
			InputPortConfig<string>("WeaponClass", _HELP("Use this if you want to specify the players weapon by name"),     0,                     _UICONFIG("enum_global:weapon")),
			InputPortConfig<int>("Ammo",           _HELP("Number of times the listener can be triggered. 0 means infinite"),_HELP("ShootCount")),
			{ 0 }
		};
		static const SOutputPortConfig outputs[] =
		{
			OutputPortConfig_Void("OnShoot",      _HELP("OnShoot")),
			OutputPortConfig<int>("AmmoLeft",     _HELP("Shoots left"),                        _HELP("ShootsLeft")),
			OutputPortConfig_Void("OnMelee",      _HELP("Triggered on melee attack")),
			OutputPortConfig<string>("OnDropped", _HELP("Triggered when weapon was dropped.")),
			{ 0 }
		};
		config.sDescription = "Listens on [WeaponId] (or players [WeaponClass], or as fallback current players weapon) and triggers OnShoot when shot.";
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.SetCategory(EFLN_APPROVED);
	}

	void Reset()
	{
		if (m_weapId != 0)
		{
			RemoveListener(m_weapId, this);
			m_weapId = 0;
		}
		m_active = false;
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			{
				m_actInfo = *pActInfo;
				Reset();
			}
			break;

		case eFE_Activate:
			{
				IItemSystem* pItemSys = CCryAction::GetCryAction()->GetIItemSystem();

				// create listener
				if (IsPortActive(pActInfo, IN_DISABLE))
				{
					Reset();
				}
				if (IsPortActive(pActInfo, IN_ENABLE))
				{
					Reset();
					IItem* pItem = 0;

					EntityId weaponId = GetPortEntityId(pActInfo, IN_WEAPONID);
					if (weaponId != 0)
					{
						pItem = pItemSys->GetItem(weaponId);
					}
					else
					{
						IActor* pActor = CCryAction::GetCryAction()->GetClientActor();
						if (!pActor)
							return;

						IInventory* pInventory = pActor->GetInventory();
						if (!pInventory)
							return;

						const string& weaponClass = GetPortString(pActInfo, IN_WEAPONCLASS);
						if (!weaponClass.empty())
						{
							// get actor weapon by class
							IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(weaponClass);
							pItem = pItemSys->GetItem(pInventory->GetItemByClass(pClass));
						}
						else
						{
							// get current actor weapon
							pItem = pItemSys->GetItem(pInventory->GetCurrentItem());
						}
					}

					if (!pItem || !pItem->GetIWeapon())
					{
						GameWarning("[flow] CFlowNode_WeaponListener: no item/weapon.");
						return;
					}

					m_weapId = pItem->GetEntity()->GetId();
					IWeapon* pWeapon = pItem->GetIWeapon();

					// set initial ammo
					m_ammo = GetPortInt(pActInfo, IN_AMMO);
					if (m_ammo == 0)
						m_ammo = -1; // 0 input means infinite

					// set weapon listener
					pWeapon->AddEventListener(this, "CFlowNode_WeaponListener");

					m_active = true;

					//CryLog("WeaponListener successfully created on %s", pItem->GetEntity()->GetName());
				}
				break;
			}
		}
	}

	virtual void OnShoot(IWeapon* pWeapon, EntityId shooterId, EntityId ammoId, IEntityClass* pAmmoType, const Vec3& pos, const Vec3& dir, const Vec3& vel)
	{
		if (m_active)
		{
			if (m_ammo != 0)
			{
				ActivateOutput(&m_actInfo, OUT_ONSHOOT, true);
				if (m_ammo != -1)
					--m_ammo;
			}
			ActivateOutput(&m_actInfo, OUT_AMMOLEFT, m_ammo);
		}
	}

	virtual void OnDropped(IWeapon* pWeapon, EntityId actorId)
	{
		if (m_active)
			ActivateOutput(&m_actInfo, OUT_ONDROPPED, true);
	}

	virtual void OnMelee(IWeapon* pWeapon, EntityId shooterId)
	{
		if (m_active)
			ActivateOutput(&m_actInfo, OUT_ONMELEE, true);
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

private:
	SActivationInfo m_actInfo;
	EntityId        m_weapId;
	int             m_ammo;
	bool            m_active;
};

class CFlowNode_WeaponAmmo : public CFlowBaseNode<eNCT_Singleton>
{
	enum EInputs
	{
		IN_SET = 0,
		IN_GET,
		IN_AMMOTYPE,
		IN_AMMOCOUNT,
		IN_ADD
	};

	enum EOutputs
	{
		OUT_MAGAZINE = 0,
		OUT_INVENTORY,
		OUT_TOTAL
	};

public:
	CFlowNode_WeaponAmmo(SActivationInfo* pActInfo) {}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_ports[] =
		{
			InputPortConfig_Void("Set",         _HELP("")),
			InputPortConfig_Void("Get",         _HELP("")),
			InputPortConfig<string>("AmmoType", _HELP("Select Ammo type"),         _HELP(""),   _UICONFIG("enum_global:ammos")),
			InputPortConfig<int>("AmountCount", _HELP("Select amount to add/set"), _HELP("")),
			InputPortConfig<bool>("Add",        _HELP("Add or Set the ammo count"),_HELP("")),
			{ 0 }
		};

		static const SOutputPortConfig out_ports[] =
		{
			OutputPortConfig<int>("MagazineAmmo",  _HELP("")),
			OutputPortConfig<int>("InventoryAmmo", _HELP("")),
			OutputPortConfig<int>("TotalAmmo",     _HELP("")),
			{ 0 }
		};

		config.sDescription = _HELP("Add a specified amount of ammo, of a specified ammo type to the inventory.");
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (event == eFE_Activate && (IsPortActive(pActInfo, IN_GET) || IsPortActive(pActInfo, IN_SET)))
		{
			IActor* pActor = GetInputActor(pActInfo);
			if (!pActor)
				return;

			IInventory* pInventory = pActor->GetInventory();
			if (pInventory)
			{
				const string& ammoType = GetPortString(pActInfo, IN_AMMOTYPE);
				IEntityClass* pAmmoClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(ammoType.c_str());

				if (pAmmoClass)
				{
					if (IsPortActive(pActInfo, IN_SET))
					{
						const bool bAdd = GetPortBool(pActInfo, IN_ADD);
						const int ammoAmount = GetPortInt(pActInfo, IN_AMMOCOUNT);

						pInventory->SetAmmoCount(pAmmoClass, bAdd ? (ammoAmount + pInventory->GetAmmoCount(pAmmoClass)) : (ammoAmount));
					}

					int magazineAmmo = 0;
					int inventoryAmmo = pInventory->GetAmmoCount(pAmmoClass);

					if (IItem* pItem = pActor->GetCurrentItem())
					{
						IWeapon* pCurrentWeapon = GetWeapon(pItem->GetEntityId());

						if (pCurrentWeapon)
						{
							magazineAmmo = pCurrentWeapon->GetAmmoCount(pAmmoClass);
						}
					}

					ActivateOutput(pActInfo, OUT_MAGAZINE, magazineAmmo);
					ActivateOutput(pActInfo, OUT_INVENTORY, inventoryAmmo);
					ActivateOutput(pActInfo, OUT_TOTAL, (magazineAmmo + inventoryAmmo));
				}
			}
		}
	}

	//-------------------------------------------------------------
	// returns the actor associated with the input entity. In single player, it returns the local player if that actor does not exists.
	IActor* GetInputActor(const IFlowNode::SActivationInfo* const pActInfo) const
	{
		IActor* pActor = pActInfo->pEntity ? gEnv->pGameFramework->GetIActorSystem()->GetActor(pActInfo->pEntity->GetId()) : nullptr;
		if (!pActor && !gEnv->bMultiplayer)
		{
			pActor = gEnv->pGameFramework->GetClientActor();
		}

		return pActor;
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};
//#define DEBUG_NODEFIREWEAPON

#ifdef DEBUG_NODEFIREWEAPON
static Vec3 posTarget = Vec3(0, 0, 0);
static Vec3 posOrig = Vec3(0, 0, 0);
static Vec3 posShot = Vec3(0, 0, 0);
#endif

class CFlowNode_FireWeapon : public CFlowBaseNode<eNCT_Instanced>, public SWeaponListener
{
public:
	enum EInputs
	{
		IN_TARGETID = 0,
		IN_TARGETPOS,
		IN_ALIGNTOTARGET,
		IN_STARTFIRE,
		IN_STOPFIRE,
		IN_NUMBEROFSHOTS,
		IN_ACCURACY,
	};

	enum EOutputs
	{
		OUT_STARTEDFIRING = 0,
		OUT_STOPPEDFIRING,
	};

	CFlowNode_FireWeapon(SActivationInfo* pActInfo)
	{
		m_weapId = 0;
		m_isFiring = false;
		m_numShotsDone = 0;
		m_numShots = 0;
		m_firingPos = Vec3(0, 0, 0);
		m_lastPos = Vec3(0, 0, 0);
		m_lastRotation = Quat(1, 0, 0, 0);
	}
	~CFlowNode_FireWeapon()
	{
		RemoveListener(m_weapId, this);
	}

	void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
		ser.Value("weaponId", m_weapId);
		ser.Value("firing", m_isFiring);
		ser.Value("numShotsDone", m_numShotsDone);
		ser.Value("numShots", m_numShots);
		ser.Value("lastPos", m_lastPos);
		ser.Value("lastRotation", m_lastRotation);
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		IFlowNode* pNode = new CFlowNode_FireWeapon(pActInfo);
		return pNode;
	}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] =
		{
			InputPortConfig<EntityId>("TargetId",  _HELP("Target entity")),
			InputPortConfig<Vec3>("TargetPos",     _HELP("Alternatively, Target position")),
			InputPortConfig<bool>("AlignToTarget", true,                                             _HELP("If true, weapon entity will be aligned to target direction. Using this can have side effects. Do not use on self-aligning weapons.")),
			InputPortConfig_Void("StartFire",      _HELP("Trigger this to start fire.")),
			InputPortConfig_Void("StopFire",       _HELP("Trigger this to stop fire.")),
			InputPortConfig<int>("NumberOfShots",  _HELP("number of shots to fire (0 = unlimited)")),
			InputPortConfig<float>("Accuracy",     1.f,                                              _HELP("value between 0..1.  1 = 100% accuracy.")),
			{ 0 }
		};
		static const SOutputPortConfig outputs[] = {
			OutputPortConfig<bool>("FireStarted", _HELP("True if StartFire command was successful")),
			OutputPortConfig<bool>("FireStopped", _HELP("True if StopFire command was successful or max number of shots completed")),
			{ 0 }
		};

		config.sDescription = _HELP("Fires a weapon and sets a target entity or a target position.");
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.SetCategory(EFLN_APPROVED);
	}

	IWeapon* GetWeapon(SActivationInfo* pActInfo)
	{
		if (pActInfo->pEntity)
			return ::GetWeapon(pActInfo->pEntity->GetId());

		return NULL;
	}

	//////////////////////////////////////////////////////////////////////////
	Vec3 GetTargetPos(SActivationInfo* pActInfo)
	{
		EntityId targetId = GetPortEntityId(pActInfo, IN_TARGETID);

		Vec3 targetPos(0, 0, 0);

		if (targetId)
		{
			IEntity* pTarget = gEnv->pEntitySystem->GetEntity(targetId);
			if (pTarget)
			{
				AABB box;
				pTarget->GetWorldBounds(box);
				targetPos = box.GetCenter();
			}
		}
		else
		{
			targetPos = GetPortVec3(pActInfo, IN_TARGETPOS);
		}
		return targetPos;
	}

	//////////////////////////////////////////////////////////////////////////
	void CalcFiringPosition(SActivationInfo* pActInfo, IWeapon* pWeapon)
	{
		Vec3 realTargetPos = GetTargetPos(pActInfo);
		m_firingPos = realTargetPos;

		float acc = GetPortFloat(pActInfo, IN_ACCURACY);
		if (acc < 1)
		{
			bool itHits = cry_random(0.0f, 1.0f) < acc;
			if (!itHits)
			{
				// what this code does is:
				// - in the plane perpendicular to the shooting vector, and located at the target, it builds a vector, centered in the target, and rotates it to point in a random direction, but always in the plane
				// - then it moves along that vector, a random distance (error distance)
				// - and that is the final to-fire position

				const float MAX_ERROR_LENGTH = 6.f; // meters
				const float MIN_ERROR_ANGLE = 2.f;  // degrees
				const float MAX_ERROR_ANGLE = 5.f;  // degrees

				// error angle from weapon to target
				float errorAngle = cry_random(MIN_ERROR_ANGLE, MAX_ERROR_ANGLE);

				// 2d angle, in the plane normal to the vector from weapon to target.
				float dirErrorAngle = cry_random(0.0f, 360.0f);

				// could be done with just 1 sqrt instead 2, but is not really worth it here.
				Vec3 vecToTarget = pActInfo->pEntity->GetPos() - realTargetPos;
				Vec3 vecToTargetNorm = vecToTarget.GetNormalizedSafe();
				Vec3 dirError2D = vecToTargetNorm.GetOrthogonal();
				dirError2D = dirError2D.GetRotated(vecToTargetNorm, DEG2RAD(dirErrorAngle));

				float dist = vecToTarget.len();
				float errorLen = std::min(dist * tanf(DEG2RAD(errorAngle)), MAX_ERROR_LENGTH);

				m_firingPos = realTargetPos + (dirError2D * errorLen);

#ifdef DEBUG_NODEFIREWEAPON
				posTarget = realTargetPos;
				posOrig = pActInfo->pEntity->GetPos();
				posShot = m_firingPos;
#endif
			}
		}
		if (GetPortBool(pActInfo, IN_ALIGNTOTARGET))
		{
			UpdateWeaponTM(pActInfo, m_firingPos);
			m_lastPos = pActInfo->pEntity->GetWorldPos();
			m_lastRotation = pActInfo->pEntity->GetWorldRotation();
		}
		pWeapon->SetDestination(m_firingPos);
	}

	//////////////////////////////////////////////////////////////////////////
	void UpdateWeaponTM(SActivationInfo* pActInfo, const Vec3& targetPos)
	{
		// update entity rotation
		IEntity* pEntity = pActInfo->pEntity;
		Vec3 dir = targetPos - pEntity->GetWorldPos();
		dir.NormalizeSafe(Vec3Constants<float>::fVec3_OneY);

		if (IEntity* pParent = pEntity->GetParent())
		{
			dir = pParent->GetWorldRotation().GetInverted() * dir;
		}

		Matrix33 rotation = Matrix33::CreateRotationVDir(dir);
		pEntity->SetRotation(Quat(rotation));
	}

	//////////////////////////////////////////////////////////////////////////
	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		IWeapon* pWeapon = GetWeapon(pActInfo);

		if (!pWeapon)
			return;

		switch (event)
		{
		case eFE_Initialize:
			{
				m_isFiring = false;
				m_numShots = GetPortInt(pActInfo, IN_NUMBEROFSHOTS);
				m_actInfo = *pActInfo;
				m_numShotsDone = 0;

				pWeapon->StopFire();

				if (pActInfo->pEntity->GetId() != m_weapId)
					RemoveListener(m_weapId, this);

				m_weapId = pActInfo->pEntity->GetId();
				pWeapon->AddEventListener(this, __FUNCTION__);

#ifdef DEBUG_NODEFIREWEAPON
				pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
#endif
				break;
			}

		case eFE_Activate:
			{
				m_actInfo = *pActInfo;
				if (IsPortActive(pActInfo, IN_NUMBEROFSHOTS))
					m_numShots = GetPortBool(pActInfo, IN_NUMBEROFSHOTS);

				if (IsPortActive(pActInfo, IN_STOPFIRE))
				{
					StopFiring(pActInfo, pWeapon);
				}
				if (IsPortActive(pActInfo, IN_STARTFIRE))
				{
					m_numShotsDone = 0;
					ReplenishAmmo(pWeapon);
					pWeapon->StopFire();
					StartFiring(pActInfo, pWeapon);
				}
				break;
			}

		case eFE_Update:
			{
				// this fixes the problem when the entity is being externally moved/rotated, in the interval of time between when the weapon is aimed an the actual shot happens
				if (m_isFiring && GetPortBool(pActInfo, IN_ALIGNTOTARGET))
					if (pActInfo->pEntity->GetWorldPos() != m_lastPos || pActInfo->pEntity->GetWorldRotation() != m_lastRotation)
						CalcFiringPosition(pActInfo, pWeapon);

#ifdef DEBUG_NODEFIREWEAPON
				ColorB colorRed(255, 0, 0);
				ColorB colorGreen(0, 255, 0);
				gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(posOrig, colorRed, posTarget, colorRed);
				gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(posTarget, colorGreen, posShot, colorGreen);
#endif
				break;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	virtual void StartFiring(SActivationInfo* pActInfo, IWeapon* pWeapon)
	{
		CalcFiringPosition(pActInfo, pWeapon);
		pWeapon->StartFire();
		m_isFiring = true;

		ActivateOutput(pActInfo, OUT_STARTEDFIRING, true);
		pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
	}

	//////////////////////////////////////////////////////////////////////////
	virtual void StopFiring(SActivationInfo* pActInfo, IWeapon* pWeapon)
	{
		pWeapon->StopFire();
		m_isFiring = false;

		ActivateOutput(pActInfo, OUT_STOPPEDFIRING, true);
		pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
	}

	//////////////////////////////////////////////////////////////////////////
	virtual void OnShoot(IWeapon* pWeapon, EntityId shooterId, EntityId ammoId, IEntityClass* pAmmoType, const Vec3& pos, const Vec3& dir, const Vec3& vel)
	{
		++m_numShotsDone;

		if (m_numShots != 0 && m_numShotsDone >= m_numShots)
			StopFiring(&m_actInfo, pWeapon);
		else
		{
			CalcFiringPosition(&m_actInfo, pWeapon);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void ReplenishAmmo(IWeapon* pWeapon)
	{
		IFireMode* pFireMode = pWeapon->GetFireMode(pWeapon->GetCurrentFireMode());
		if (pFireMode)
			pWeapon->SetAmmoCount(pFireMode->GetAmmoType(), pFireMode->GetClipSize());
	}

	//////////////////////////////////////////////////////////////////////////
	virtual void OnOutOfAmmo(IWeapon* pWeapon, IEntityClass* pAmmoType)
	{
		ReplenishAmmo(pWeapon);
		StartFiring(&m_actInfo, pWeapon);
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

private:
	EntityId        m_weapId;
	SActivationInfo m_actInfo;
	int             m_numShotsDone;
	int             m_numShots;
	bool            m_isFiring;
	Vec3            m_firingPos; // if accuracy is not 1, this could be not the actual target position
	Vec3            m_lastPos;
	Quat            m_lastRotation;

};

class CFlowNode_ChangeFireMode : public CFlowBaseNode<eNCT_Singleton>
{
public:
	enum EInputs
	{
		IN_SWITCH,
	};

	CFlowNode_ChangeFireMode(SActivationInfo* pActInfo)
	{
	}
	~CFlowNode_ChangeFireMode()
	{
	}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] =
		{
			InputPortConfig_Void("Switch", _HELP("Trigger this to switch fire modes.")),
			{ 0 }
		};
		static const SOutputPortConfig outputs[] = {
			{ 0 }
		};

		config.sDescription = _HELP("Switched the weapon fire mode.");
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.SetCategory(EFLN_APPROVED);
	}

	IWeapon* GetWeapon(SActivationInfo* pActInfo)
	{
		if (pActInfo->pEntity)
			return ::GetWeapon(pActInfo->pEntity->GetId());

		return NULL;
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		IWeapon* pWeapon = GetWeapon(pActInfo);

		if (!pWeapon)
			return;

		switch (event)
		{
		case eFE_Activate:
			if (pWeapon)
				pWeapon->StartChangeFireMode();
			break;
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

REGISTER_FLOW_NODE("Weapon:AutoSightWeapon", CFlowNode_AutoSightWeapon);
REGISTER_FLOW_NODE("Weapon:FireWeapon", CFlowNode_FireWeapon);
REGISTER_FLOW_NODE("Weapon:Listener", CFlowNode_WeaponListener);
REGISTER_FLOW_NODE("Weapon:Ammo", CFlowNode_WeaponAmmo);
REGISTER_FLOW_NODE("Weapon:ChangeFireMode", CFlowNode_ChangeFireMode);
