// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Game.h"
#include "Item.h"
#include "GameRules.h"

#include <CryFlowGraph/IFlowBaseNode.h>
#include <CryAnimation/ICryAnimation.h>

class CFlowHitInfoNode : public CFlowBaseNode<eNCT_Instanced>, public IHitListener
{
public:
	CFlowHitInfoNode(SActivationInfo* pActInfo)
	{
	}

	~CFlowHitInfoNode()
	{
		// safety unregister
		CGameRules* pGR = g_pGame ? g_pGame->GetGameRules() : NULL;
		if (pGR)
			pGR->RemoveHitListener(this);
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowHitInfoNode(pActInfo);
	}

	void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
		if (ser.IsReading())
		{
			RegisterOrUnregisterListener(pActInfo);
		}
	}

	enum EInputPorts
	{
		EIP_Enable = 0,
		EIP_ShooterId,
		EIP_TargetId,
		EIP_Weapon,
		EIP_Ammo
	};

	enum EOutputPorts
	{
		EOP_ShooterId = 0,
		EOP_TargetId,
		EOP_WeaponId,
		EOP_ProjectileId,
		EOP_HitPos,
		EOP_HitDir,
		EOP_HitNormal,
		EOP_HitType,
		EOP_Damage,
		EOP_Material,
		EOP_Attachment,
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig<bool>("Enable",        false,                                                   _HELP("Enable/Disable HitInfo")),
			InputPortConfig<EntityId>("ShooterId", _HELP("When connected, limit HitInfo to this shooter")),
			InputPortConfig<EntityId>("TargetId",  _HELP("When connected, limit HitInfo to this target")),
			InputPortConfig<string>("Weapon",      _HELP("When set, limit HitInfo to this weapon"),         0,                               _UICONFIG("enum_global:weapon")),
			InputPortConfig<string>("Ammo",        _HELP("When set, limit HitInfo to this ammo"),           0,                               _UICONFIG("enum_global:ammos")),
			{ 0 }
		};
		static const SOutputPortConfig outputs[] = {
			OutputPortConfig<EntityId>("ShooterId",    _HELP("EntityID of the Shooter")),
			OutputPortConfig<EntityId>("TargetId",     _HELP("EntityID of the Target")),
			OutputPortConfig<EntityId>("WeaponId",     _HELP("EntityID of the Weapon")),
			OutputPortConfig<EntityId>("ProjectileId", _HELP("EntityID of the Projectile if it was a bullet hit")),
			OutputPortConfig<Vec3>("HitPos",           _HELP("Position of the Hit")),
			OutputPortConfig<Vec3>("HitDir",           _HELP("Direction of the Hit")),
			OutputPortConfig<Vec3>("HitNormal",        _HELP("Normal of the Hit Impact")),
			OutputPortConfig<string>("HitType",        _HELP("Name of the HitType")),
			OutputPortConfig<float>("Damage",          _HELP("Damage amout which was caused")),
			OutputPortConfig<string>("Material",       _HELP("Name of the Material")),
			OutputPortConfig<string>("Attachment",     _HELP("Name of the Attachment")),
			{ 0 }
		};
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Tracks Hits on Actors.\nAll input conditions (ShooterId, TargetId, Weapon, Ammo) must be fulfilled to output.\nIf a condition is left empty/not connected, it is regarded as fulfilled.");
		config.SetCategory(EFLN_ADVANCED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			m_actInfo = *pActInfo;  // fall through and enable/disable listener
		case eFE_Activate:
			RegisterOrUnregisterListener(pActInfo);
			break;
		}
	}

	void RegisterOrUnregisterListener(SActivationInfo* pActInfo)
	{
		CGameRules* pGameRules = g_pGame->GetGameRules();
		if (!pGameRules)
		{
			GameWarning("[flow] CFlowHitInfoNode::RegisterListener: No GameRules!");
			return;
		}

		if (GetPortBool(pActInfo, EIP_Enable))
			pGameRules->AddHitListener(this);
		else
			pGameRules->RemoveHitListener(this);
	}

	const char* GetHitAttachmentName(const HitInfo& hit) const
	{
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(hit.targetId);
		if (!pEntity)
			return 0;

		ICharacterInstance* pCharacter = pEntity->GetCharacter(0);
		if (!pCharacter)
			return 0;

		IPhysicalEntity* pPE = pCharacter->GetISkeletonPose()->GetCharacterPhysics();
		IAttachmentManager* pAttchmentManager = pCharacter->GetIAttachmentManager();
		if (!pPE || !pAttchmentManager)
			return 0;

		pe_status_pos posStatus;
		pe_params_part partParams;
		partParams.partid = hit.partId;

		if (pPE->GetParams(&partParams) && pPE->GetStatus(&posStatus))
		{
			Matrix34 worldTM = Matrix34::Create(Vec3(posStatus.scale, posStatus.scale, posStatus.scale), posStatus.q, posStatus.pos);
			Vec3 pos = worldTM.TransformPoint(partParams.pos);

			int attachmentCount = pAttchmentManager->GetAttachmentCount();

			for (int i = 0; i < attachmentCount; ++i)
			{
				IAttachment* pAttachment = pAttchmentManager->GetInterfaceByIndex(i);
				IAttachmentObject* pAttachmentObj = pAttachment->GetIAttachmentObject();

				if (pAttachmentObj /* && (pAttachmentObj->GetAttachmentType() == IAttachmentObject::eAttachment_StatObj)*/)
				{
					/*IStatObj* pStatObj = pAttachmentObj->GetIStatObj();
					   phys_geometry* pPhysGeom = pStatObj->GetPhysGeom();*/

					const QuatTS attachmentLoc = pAttachment->GetAttWorldAbsolute();
					if (pos.IsEquivalent(attachmentLoc.t, 0.005f))
						return pAttachment->GetName();
				}
			}
		}

		return 0;
	}

	//CGameRules::IHitInfo
	virtual void OnHit(const HitInfo& hitInfo)
	{
		EntityId shooter = GetPortEntityId(&m_actInfo, EIP_ShooterId);
		if (shooter && (shooter != hitInfo.shooterId))
			return;

		EntityId target = GetPortEntityId(&m_actInfo, EIP_TargetId);
		if (target && (target != hitInfo.targetId))
			return;

		// check weapon match
		const string& weapon = GetPortString(&m_actInfo, EIP_Weapon);
		if (!weapon.empty())
		{
			IEntity* pWeapon = gEnv->pEntitySystem->GetEntity(hitInfo.weaponId);
			if (!pWeapon || weapon.compare(pWeapon->GetClass()->GetName()) != 0)
				return;
		}
		// check ammo match
		const string& ammo = GetPortString(&m_actInfo, EIP_Ammo);
		if (!ammo.empty())
		{
			IEntity* pAmmo = gEnv->pEntitySystem->GetEntity(hitInfo.projectileId);
			if (!pAmmo || ammo.compare(pAmmo->GetClass()->GetName()) != 0)
				return;
		}

		const char* hitType = 0;
		const char* surfaceType = 0;
		const char* attachmentName = GetHitAttachmentName(hitInfo);

		if (CGameRules* pGameRules = g_pGame->GetGameRules())
		{
			hitType = pGameRules->GetHitType(hitInfo.type);
			if (ISurfaceType* pSurface = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceType(hitInfo.material))
				surfaceType = pSurface->GetName();
		}

		ActivateOutput(&m_actInfo, EOP_ShooterId, hitInfo.shooterId);
		ActivateOutput(&m_actInfo, EOP_TargetId, hitInfo.targetId);
		ActivateOutput(&m_actInfo, EOP_WeaponId, hitInfo.weaponId);
		ActivateOutput(&m_actInfo, EOP_ProjectileId, hitInfo.projectileId);
		ActivateOutput(&m_actInfo, EOP_HitPos, hitInfo.pos);
		ActivateOutput(&m_actInfo, EOP_HitDir, hitInfo.dir);
		ActivateOutput(&m_actInfo, EOP_HitNormal, hitInfo.normal);
		ActivateOutput(&m_actInfo, EOP_Damage, hitInfo.damage);
		ActivateOutput(&m_actInfo, EOP_Material, string(surfaceType));
		ActivateOutput(&m_actInfo, EOP_HitType, string(hitType));
		ActivateOutput(&m_actInfo, EOP_Attachment, string(attachmentName));
	}

	virtual void OnExplosion(const ExplosionInfo& explosionInfo)
	{
		EntityId shooter = GetPortEntityId(&m_actInfo, EIP_ShooterId);
		if (shooter && (shooter != explosionInfo.shooterId))
			return;

		EntityId target = GetPortEntityId(&m_actInfo, EIP_TargetId);
		if (target && (target != explosionInfo.impact_targetId))
			return;

		// check weapon match
		const string& weapon = GetPortString(&m_actInfo, EIP_Weapon);
		if (!weapon.empty())
		{
			IEntity* pWeapon = gEnv->pEntitySystem->GetEntity(explosionInfo.weaponId);
			if (!pWeapon || weapon.compare(pWeapon->GetClass()->GetName()) != 0)
				return;
		}
		// check ammo match
		const string& ammo = GetPortString(&m_actInfo, EIP_Ammo);
		if (!ammo.empty())
		{
			IEntity* pAmmo = gEnv->pEntitySystem->GetEntity(explosionInfo.projectileId);
			if (!pAmmo || ammo.compare(pAmmo->GetClass()->GetName()) != 0)
				return;
		}

		const char* hitType = nullptr;
		if (CGameRules* pGameRules = g_pGame->GetGameRules())
		{
			hitType = pGameRules->GetHitType(explosionInfo.type);
		}

		ActivateOutput(&m_actInfo, EOP_ShooterId, explosionInfo.shooterId);
		ActivateOutput(&m_actInfo, EOP_TargetId, explosionInfo.impact_targetId);
		ActivateOutput(&m_actInfo, EOP_WeaponId, explosionInfo.weaponId);
		ActivateOutput(&m_actInfo, EOP_ProjectileId, explosionInfo.projectileId);
		ActivateOutput(&m_actInfo, EOP_HitPos, explosionInfo.pos);
		ActivateOutput(&m_actInfo, EOP_HitDir, explosionInfo.dir);
		ActivateOutput(&m_actInfo, EOP_HitNormal, explosionInfo.impact_normal);
		ActivateOutput(&m_actInfo, EOP_Damage, explosionInfo.damage);
		ActivateOutput(&m_actInfo, EOP_Material, string(""));
		ActivateOutput(&m_actInfo, EOP_HitType, string(hitType));
		ActivateOutput(&m_actInfo, EOP_Attachment, string(""));
	}

	virtual void OnServerExplosion(const ExplosionInfo& explosionInfo)
	{
		OnExplosion(explosionInfo);
	}
	//~CGameRules::IHitInfo

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	SActivationInfo m_actInfo;
};

class CFlowExplosionInfoNode : public CFlowBaseNode<eNCT_Instanced>, public IHitListener
{
public:
	CFlowExplosionInfoNode(SActivationInfo* pActInfo)
	{
	}

	~CFlowExplosionInfoNode()
	{
		// safety unregister
		CGameRules* pGR = g_pGame ? g_pGame->GetGameRules() : NULL;
		if (pGR)
			pGR->RemoveHitListener(this);
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowExplosionInfoNode(pActInfo);
	}

	void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
		if (ser.IsReading())
			DoRegister(pActInfo);
	}

	enum EInputPorts
	{
		EIP_Enable = 0,
		EIP_ShooterId,
		EIP_WeaponLegacyDontUse,
		EIP_Ammo,
		EIP_ImpactTargetId,
	};

	enum EOutputPorts
	{
		EOP_ShooterId = 0,
		EOP_Ammo,
		EOP_Pos,
		EOP_Dir,
		EOP_Radius,
		EOP_Damage,
		EOP_Pressure,
		EOP_HoleSize,
		EOP_Type,
		EOP_ImpactTargetId,
		EOP_ImpactNormal,
		EOP_ImpactVelocity,
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig<bool>("Enable",             false,                                                                                  _HELP("Enable/Disable ExplosionInfo")),
			InputPortConfig<EntityId>("ShooterId",      _HELP("When connected, limit ExplosionInfo to this shooter")),
			InputPortConfig<string>("Weapon",           _HELP("!DONT USE! -> Use Ammo!"),                                                       _HELP("!DONT USE! -> Use Ammo!"),      _UICONFIG("enum_global:ammos")),
			InputPortConfig<string>("Ammo",             _HELP("When set, limit ExplosionInfo to this ammo"),                                    0,                                     _UICONFIG("enum_global:ammos")),
			InputPortConfig<EntityId>("ImpactTargetId", _HELP("When connected, limit ExplosionInfo to this Impact target (e.g. for Rockets)")),
			{ 0 }
		};
		static const SOutputPortConfig outputs[] = {
			OutputPortConfig<EntityId>("ShooterId",      _HELP("EntityID of the Shooter")),
			OutputPortConfig<string>("Ammo",             _HELP("Ammo Class")),
			OutputPortConfig<Vec3>("Pos",                _HELP("Position of the Explosion")),
			OutputPortConfig<Vec3>("Dir",                _HELP("Direction of the Explosion")),
			OutputPortConfig<float>("Radius",            _HELP("Radius of Explosion")),
			OutputPortConfig<float>("Damage",            _HELP("Damage amout which was caused")),
			OutputPortConfig<float>("Pressure",          _HELP("Pressure amout which was caused")),
			OutputPortConfig<float>("HoleSize",          _HELP("Hole size which was caused")),
			OutputPortConfig<string>("Type",             _HELP("Name of the Explosion type")),
			OutputPortConfig<EntityId>("ImpactTargetId", _HELP("EntityID of the Impact Target (e.g. for Rockets)")),
			OutputPortConfig<Vec3>("ImpactNormal",       _HELP("Impact Normal (e.g. for Rockets)")),
			OutputPortConfig<float>("ImpactVelocity",    _HELP("Impact Normal (e.g. for Rockets)")),
			{ 0 }
		};
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Tracks Explosions.\nAll input conditions (ShooterId, Ammo, ImpactTargetId) must be fulfilled to output.\nIf a condition is left empty/not connected, it is regarded as fulfilled.");
		config.SetCategory(EFLN_ADVANCED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			m_actInfo = *pActInfo;  // fall through and enable/disable listener
		case eFE_Activate:
			if (IsPortActive(pActInfo, EIP_Enable))
			{
				DoRegister(pActInfo);
			}
			break;
		}
	}

	void DoRegister(SActivationInfo* pActInfo)
	{
		CGameRules* pGR = g_pGame->GetGameRules();
		if (!pGR)
		{
			GameWarning("[flow] CFlowExplosionInfoNode::DoRegister No GameRules!");
			return;
		}

		bool bEnable = GetPortBool(pActInfo, EIP_Enable);
		if (bEnable)
			pGR->AddHitListener(this);
		else
			pGR->RemoveHitListener(this);
	}

	//CGameRules::IHitInfo
	virtual void OnHit(const HitInfo& hitInfo)
	{
	}

	virtual void OnExplosion(const ExplosionInfo& explosionInfo)
	{
		if (GetPortBool(&m_actInfo, EIP_Enable) == false)
			return;

		const EntityId shooter = GetPortEntityId(&m_actInfo, EIP_ShooterId);
		if (shooter != 0 && shooter != explosionInfo.shooterId)
			return;

		const EntityId impactTarget = GetPortEntityId(&m_actInfo, EIP_ImpactTargetId);
		if (impactTarget != 0 && explosionInfo.impact && impactTarget != explosionInfo.impact_targetId)
			return;

		const IEntity* const pTempEntity = gEnv->pEntitySystem->GetEntity(explosionInfo.projectileId);

		// check ammo match
		const string& ammo = GetPortString(&m_actInfo, EIP_Ammo);
		if (ammo.empty() == false)
		{
			if (pTempEntity == 0 || ammo.compare(pTempEntity->GetClass()->GetName()) != 0)
				return;
		}
		string ammoClass = pTempEntity ? pTempEntity->GetClass()->GetName() : "";

		const char* szHitType = "";
		if (CGameRules* pGameRules = g_pGame->GetGameRules())
			szHitType = pGameRules->GetHitType(explosionInfo.type);
		ActivateOutput(&m_actInfo, EOP_ShooterId, explosionInfo.shooterId);
		ActivateOutput(&m_actInfo, EOP_Ammo, ammoClass);
		ActivateOutput(&m_actInfo, EOP_Pos, explosionInfo.pos);
		ActivateOutput(&m_actInfo, EOP_Dir, explosionInfo.dir);
		ActivateOutput(&m_actInfo, EOP_Radius, explosionInfo.radius);
		ActivateOutput(&m_actInfo, EOP_Damage, explosionInfo.damage);
		ActivateOutput(&m_actInfo, EOP_Pressure, explosionInfo.pressure);
		ActivateOutput(&m_actInfo, EOP_HoleSize, explosionInfo.hole_size);
		ActivateOutput(&m_actInfo, EOP_Type, string(szHitType));
		if (explosionInfo.impact)
		{
			ActivateOutput(&m_actInfo, EOP_ImpactTargetId, explosionInfo.impact_targetId);
			ActivateOutput(&m_actInfo, EOP_ImpactNormal, explosionInfo.impact_normal);
			ActivateOutput(&m_actInfo, EOP_ImpactVelocity, explosionInfo.impact_velocity);
		}
	}

	virtual void OnServerExplosion(const ExplosionInfo& explosion)
	{

	}
	//~CGameRules::IHitInfo

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	SActivationInfo m_actInfo;
};

REGISTER_FLOW_NODE("Weapon:HitInfo", CFlowHitInfoNode);
REGISTER_FLOW_NODE("Weapon:ExplosionInfo", CFlowExplosionInfoNode);
