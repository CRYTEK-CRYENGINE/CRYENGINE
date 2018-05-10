// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$

   -------------------------------------------------------------------------
   History:
   - 24:05:2006   14:00 : Created by Michael Rauh

*************************************************************************/
#include "StdAfx.h"
#include "GunTurret.h"

#include <CryMath/Cry_GeoOverlap.h>
#include <IActorSystem.h>
#include <IMovementController.h>
#include <CryAISystem/IAIObjectManager.h>
#include <IGameObjectSystem.h>
#include <IVehicleSystem.h>
#include <GameRules.h>
#include "Actor.h"
#include "Game.h"
#include "GameCVars.h"
#include <CryGame/GameUtils.h>
#include "WeaponSystem.h"
#include "Projectile.h"
#include "Player.h"
#include "ItemSharedParams.h"
#include "TacticalManager.h"

namespace
{
enum eGunTurretDebug
{
	eGTD_Off = 0,
	eGTD_Basic,
	eGTD_Prediction,
	eGTD_Sweep,
	eGTD_Search,
	eGTD_Deviation,
	eGTD_AlwaysHostile,
};

const float minLenSqr = sqr(0.001f);

ILINE bool  DebugTurret() { return (g_pGameCVars->i_debug_turrets != 0); }

inline void Interp(float& actual, float goal, float speed, float frameTime, float minSpeed = 0.f)
{
	float delta(goal - actual);
	float absDelta = abs(delta);

	speed = min(1.f, absDelta) * speed;
	if (minSpeed != 0.f)
		speed = max(speed, minSpeed);

	float add = sgn(delta) * min(speed * frameTime, absDelta);

	actual += add;
}

ILINE float To2PI(float a)
{
	assert(a >= -gf_PI && a <= gf_PI);
	return (a < 0.f) ? a + gf_PI2 : a;
}

ILINE float ToPlusMinusPI(float a)
{
	assert(a >= 0.f && a <= gf_PI2);
	return (a > gf_PI) ? a - gf_PI2 : a;
}

void DrawCircle(const Vec3& center, float radius)
{
	Vec3 p0, p1, pos;
	pos = center;
	p0.x = pos.x + radius * sinf(0.0f);
	p0.y = pos.y + radius * cosf(0.0f);
	p0.z = pos.z;
	float step = 10.0f / 180 * gf_PI;
	ColorB col(0, 255, 0, 128);

	for (float angle = step; angle < 360.0f / 180 * gf_PI + step; angle += step)
	{
		p1.x = pos.x + radius * sinf(angle);
		p1.y = pos.y + radius * cosf(angle);
		p1.z = pos.z;
		gEnv->pAuxGeomRenderer->DrawLine(p0, col, p1, col);
		p0 = p1;
	}
}

static const uint32 PROBE_TYPES = ent_terrain | ent_static | ent_sleeping_rigid | ent_rigid;
static const uint32 PROBE_FLAGS = (geom_colltype_ray) << rwi_colltype_bit | rwi_colltype_any | rwi_force_pierceable_noncoll | rwi_ignore_solid_back_faces | 8;

}

namespace GT
{
void RegisterEvents(IGameObjectExtension& goExt, IGameObject& gameObject)
{
	const int eventID[] = { eCGE_Turret_LockedTarget, eCGE_Turret_LostTarget, eCGE_SetTeam };
	gameObject.UnRegisterExtForEvents(&goExt, NULL, 0);
	gameObject.RegisterExtForEvents(&goExt, eventID, sizeof(eventID) / sizeof(int));
}
}

//------------------------------------------------------------------------
CGunTurret::CGunTurret()
	: m_fm2(0),
	m_targetId(0),
	m_goalYaw(0.0f),
	m_goalPitch(0.0f),
	m_updateTargetTimer(0.0f),
	m_abandonTargetTimer(0.f),
	m_checkTACTimer(0.0f),
	m_burstTimer(0.f),
	m_pauseTimer(0.f),
	m_rayTimer(0.f),
	m_searchHint(0),
	m_fireHint(1),
	m_lightId(0),
	m_destroyed(false),
	m_canShoot(false),
	m_deviationPos(0),
	m_vertVel(0.0f)
{
	m_barrelRotation.SetIdentity();
}

//------------------------------------------------------------------------
CGunTurret::~CGunTurret()
{
	SetFiringLocator(0);
}

//------------------------------------------------------------------------
void CGunTurret::PostInit(IGameObject* pGameObject)
{
	CItem::PostInit(pGameObject);

	if (gEnv->bServer)
		UpdateEntityProperties();

	SetOwnerId(GetEntityId());

	if (!gEnv->bServer)
		for (int i = 0; i < 4; i++)
			pGameObject->SetUpdateSlotEnableCondition(this, i, eUEC_InRange);
}

void CGunTurret::SetOwnerId(EntityId ownerId)
{
	CWeapon::SetOwnerId(ownerId);

	g_pGame->GetTacticalManager()->AddEntity(GetEntityId(), CTacticalManager::eTacticalEntity_Item);
}

//------------------------------------------------------------------------
bool CGunTurret::NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int flags)
{
	// call base class
	if (!CWeapon::NetSerialize(ser, aspect, profile, flags))
		return false;

	if (aspect == ASPECT_STATEBITS)
	{
		bool was_destroyed = IsDestroyed();
		bool destr = was_destroyed;
		ser.Value("destroyed", destr, 'bool');
		if (ser.IsReading() && destr != was_destroyed)
		{
			if (destr)
				OnDestroyed();
			else
			{
				OnRepaired();
			}
		}
		bool enabled = m_turretparams.enabled;
		ser.Value("enabled", m_turretparams.enabled, 'bool');
		if (ser.IsReading() && enabled != m_turretparams.enabled)
			Activate(m_turretparams.enabled);

		ser.Value("health", m_stats.health, 'iii');

		if (ser.IsReading())
			UpdateDamageLevel();
	}

	if (aspect == ASPECT_GOALORIENTATION)
	{
		//adjust the range
		if (ser.IsReading())
		{
			float yaw = 0.f;
			ser.Value("target_yaw", yaw, 'frad');
			m_goalYaw = yaw < 0.0f ? yaw + gf_PI2 : yaw;
		}
		else
		{
			float yaw = m_goalYaw > gf_PI ? m_goalYaw - gf_PI2 : m_goalYaw;
			ser.Value("target_yaw", yaw, 'frad');
		}
		ser.Value("target_pitch", m_goalPitch, 'frad');

		EntityId old_id = m_targetId;
		ser.Value("target", m_targetId, 'eid');
		if (old_id != m_targetId)
		{
			OnTargetLocked(m_targetId ? gEnv->pEntitySystem->GetEntity(m_targetId) : 0);
		}

		if (m_weaponsharedparams->pTurretParams->fireParams.deviation_amount != 0.f)
			ser.Value("deviation", m_deviationPos);
	}

	return true;
}

NetworkAspectType CGunTurret::GetNetSerializeAspects()
{
	return CWeapon::GetNetSerializeAspects() | ASPECT_STATEBITS | ASPECT_GOALORIENTATION;
}

void CGunTurret::FullSerialize(TSerialize ser)
{
	CWeapon::FullSerialize(ser);

	ser.BeginGroup("GunTurret");
	ser.Value("target", m_targetId);
	ser.EndGroup();

	if (ser.IsReading())
	{
		if (!IsDestroyed())
		{
			if (IGameObject* pGameObject = GetGameObject())
				PostInit(pGameObject);
		}
	}
}

//------------------------------------------------------------------------
void CGunTurret::OnReset()
{
	g_pGame->GetTacticalManager()->AddEntity(GetEntityId(), CTacticalManager::eTacticalEntity_Item);

	if (IScriptTable* pScriptTable = GetEntity()->GetScriptTable())
	{
		SmartScriptTable props;
		if (pScriptTable->GetValue("Properties", props))
			ReadProperties(props);
	}

	CreateAIRepresentation();

	CItem::OnReset();

	Matrix34 tm = GetEntity()->GetSlotLocalTM(eIGS_Aux0, false);

	tm.SetTranslation(m_tripodHelperPos);

	GetEntity()->SetSlotLocalTM(eIGS_Aux0, tm);

	Vec3 w_trans = tm.TransformPoint(m_radarHelperPos);

	tm.SetTranslation(w_trans);

	GetEntity()->SetSlotLocalTM(eIGS_ThirdPerson, tm);

	if (GetEntity()->IsSlotValid(eIGS_Aux1))
	{
		tm.SetTranslation(GetSlotHelperPos(eIGS_ThirdPerson, m_weaponsharedparams->pTurretParams->barrelHelper.c_str(), false));
		GetEntity()->SetSlotLocalTM(eIGS_Aux1, tm);
	}

	m_targetId = 0;
	m_destinationId = 0;
	m_updateTargetTimer = 0.0f;
	m_abandonTargetTimer = 0.0f;
	m_goalYaw = 0.0f;
	m_goalPitch = 0.0f;
	m_burstTimer = 0.0f;
	m_pauseTimer = 0.0f;
	m_searchHint = 0;
	m_fireHint = 1;
	m_deviationPos.zero();

	m_randoms[eRV_UpdateTarget].Range(m_turretparams.update_target_time * m_weaponsharedparams->pTurretParams->fireParams.randomness);
	m_randoms[eRV_AbandonTarget].Range(m_turretparams.abandon_target_time * m_weaponsharedparams->pTurretParams->fireParams.randomness);
	m_randoms[eRV_BurstTime].Range(m_turretparams.burst_time * m_weaponsharedparams->pTurretParams->fireParams.randomness);
	m_randoms[eRV_BurstPause].Range(m_turretparams.burst_pause * m_weaponsharedparams->pTurretParams->fireParams.randomness);

	if (m_lightId)
	{
		DetachEffect(m_lightId);
		m_lightId = 0;
	}

	if (m_turretparams.light_fov > 0.f)
	{
		EntityEffects::SLightAttachParams lightParams;

		lightParams.color = m_weaponsharedparams->pTurretParams->searchParams.light_color * m_weaponsharedparams->pTurretParams->searchParams.light_diffuse_mul;
		lightParams.direction = Vec3(0, 1, 0);
		lightParams.radius = m_turretparams.mg_range;
		lightParams.specularMultiplier = 1.f / m_weaponsharedparams->pTurretParams->searchParams.light_diffuse_mul;
		lightParams.projectFov = m_turretparams.light_fov;
		lightParams.hdrDynamic = m_weaponsharedparams->pTurretParams->searchParams.light_hdr_dyn;
		lightParams.projectTexture = m_weaponsharedparams->pTurretParams->searchParams.light_texture.c_str();
		lightParams.material = m_weaponsharedparams->pTurretParams->searchParams.light_material.c_str();
		lightParams.firstSafeSlot = eIGS_Last;

		m_lightId = AttachLight(
		  eIGS_ThirdPerson,
		  m_weaponsharedparams->pTurretParams->searchParams.light_helper.c_str(),
		  lightParams);

		PlayAction(GetFragmentIds().use_light);
	}

	SetFiringLocator(this);

	if (m_fm2)
		m_fm2->Activate(true);

	EnableUpdate(true, eIUS_General);
}

//------------------------------------------------------------------------
void CGunTurret::OnHit(float damage, int hitType)
{
	damage = GetScaledDamage(damage, hitType);

	const float healthBeforeHit = m_stats.health;

	CItem::OnHit(damage, hitType);

	if (healthBeforeHit != m_stats.health)
		CHANGED_NETWORK_STATE(this, ASPECT_STATEBITS);
}

//------------------------------------------------------------------------
void CGunTurret::OnDestroyed()
{
	CWeapon::OnDestroyed();

	gEnv->pAISystem->GetAIObjectManager()->RemoveObjectByEntityId(GetEntityId());

	if (m_fm2)
	{
		if (m_fm2->IsFiring())
			m_fm2->StopFire();
	}

	g_pGame->GetTacticalManager()->RemoveEntity(GetEntityId(), CTacticalManager::eTacticalEntity_Item);

	if (IsServer())
		CHANGED_NETWORK_STATE(this, ASPECT_STATEBITS);
}

//------------------------------------------------------------------------
void CGunTurret::OnRepaired()
{
	CWeapon::OnRepaired();

	if (m_turretparams.enabled)
		Activate(m_turretparams.enabled);

	if (IsServer())
		CHANGED_NETWORK_STATE(this, ASPECT_STATEBITS);
}

//------------------------------------------------------------------------
void CGunTurret::ReadProperties(IScriptTable* pProperties)
{
	CItem::ReadProperties(pProperties);

	if (pProperties)
	{
		m_turretparams.Reset(pProperties);

		if (IAIObject* aiObject = GetEntity()->GetAI())
			aiObject->SetFactionID(m_turretparams.factionID);

		const char* model = 0;
		if (GetEntityProperty("objBase", model) && model && *model)
		{
			SetGeometry(eIGS_Aux0, model);
			DrawSlot(eIGS_Aux0, true);
		}

		if (GetEntityProperty("objModel", model) && model && *model)
		{
			SetGeometry(eIGS_ThirdPerson, model);
			DrawSlot(eIGS_ThirdPerson, true);
		}

		if (GetEntityProperty("objBarrel", model) && model && *model)
		{
			SetGeometry(eIGS_Aux1, model);
			DrawSlot(eIGS_Aux1, true);
		}

		if (GetEntityProperty("objDestroyed", model) && model && *model)
		{
			SetGeometry(eIGS_Destroyed, model);
			DrawSlot(eIGS_Destroyed, false);
		}
	}
}

bool CGunTurret::Init(IGameObject* pGameObject)
{
	if (!CWeapon::Init(pGameObject))
	{
		return false;
	}

	if (!m_weaponsharedparams->pTurretParams)
	{
		GameWarning("Uninitialised turret params. Is the xml for item %s missing the 'turret' node?", GetEntity()->GetName());

		return false;
	}

	g_pGame->GetTacticalManager()->AddEntity(GetEntityId(), CTacticalManager::eTacticalEntity_Item);

	return true;
}

//------------------------------------------------------------------------
void CGunTurret::InitItemFromParams()
{
	CWeapon::InitItemFromParams();

	m_radarHelperPos.zero();
	m_tripodHelperPos.zero();
	m_barrelHelperPos.zero();
	m_fireHelperPos.zero();
	m_rocketHelperPos.zero();

	m_fireHelperPos = GetSlotHelperPos(eIGS_ThirdPerson, m_weaponsharedparams->pTurretParams->fireHelper.c_str(), false);

	//////////////////////////////////////////////////////////////////////////

	// TODO: Read this correctly from the models
	m_radarHelperPos.Set(0.0f, 0.0f, 1.3f);
	m_turretHelperPos.Set(0.0f, 0.0f, 1.3f);

	//////////////////////////////////////////////////////////////////////////

	m_fm2 = GetFireMode(m_weaponsharedparams->pTurretParams->rocketFireMode.c_str());
	if (m_fm2)
		m_fm2->Activate(true);
}

//------------------------------------------------------------------------
void CGunTurret::OnAction(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	CWeapon::OnAction(actorId, actionId, activationMode, value);
}

//------------------------------------------------------------------------
bool CGunTurret::IsOperational()
{
	return !IsDestroyed() && m_turretparams.enabled && g_pGameCVars->i_auto_turret_target != 0 && !(gEnv->IsEditor() && g_pGame->GetIGameFramework()->IsEditing());
}

//------------------------------------------------------------------------
Vec3 CGunTurret::GetTargetPos(IEntity* pEntity) const
{
	pEntity = ResolveTarget(pEntity);

	if (IPhysicalEntity* pPE = pEntity->GetPhysics())
	{
		pe_status_dynamics dyn;
		if (pPE->GetStatus(&dyn))
		{
			if (dyn.submergedFraction <= 0.05f)
				return dyn.centerOfMass;

			Vec3 pos = dyn.centerOfMass;
			float waterLevel = gEnv->p3DEngine->GetWaterLevel(&pos);
			if (waterLevel >= pos.z)
				pos.z = waterLevel;

			return pos;
		}
	}

	return pEntity->GetWorldPos();
}

//------------------------------------------------------------------------
Vec3 CGunTurret::PredictTargetPos(IEntity* pTarget, bool sec)//sec - weapon to use
{
	pTarget = ResolveTarget(pTarget);
	Vec3 tpos = GetTargetPos(pTarget);

	if (m_turretparams.search_only || m_turretparams.prediction == 0.f)
		return tpos;

	IPhysicalEntity* pe = pTarget->GetPhysics();
	pe_status_dynamics dyn;
	if (!pe || !pe->GetStatus(&dyn))
		return tpos;

	Vec3 vel = dyn.v;
	Vec3 acc = dyn.a;
	float a = acc.len();
	if (a < 0.01f)
		a = 0.f;
	else
		acc /= a;

	Vec3 vpos = GetWeaponPos();
	Vec3 dist = tpos - vpos;
	float d = dist.len();
	if (d < 0.01f)
		return tpos;

	dist /= d;
	float d_speed = vel * dist;

	float speed = 800.0f;
	const SAmmoParams* ammo = g_pGame->GetWeaponSystem()->GetAmmoParams(GetFireMode(0)->GetAmmoType());
	if (!ammo)
		return tpos;

	if (ammo->physicalizationType == ePT_None)
		return tpos;

	speed = ammo->speed;

	float time_to = d / max(1.f, speed - d_speed);

	// MR: clamped acc prediction to reduce jerkyness when targetting objects that are able
	// to do near-instant velocity changes (like players)
	a = min(a, 25.f);

	Vec3 delta = vel * (time_to) + 0.5f * a * acc * time_to * time_to;
	delta *= m_turretparams.prediction;

	if (g_pGameCVars->i_debug_turrets == eGTD_Prediction)
	{
		IPersistantDebug* pDebug = gEnv->pGameFramework->GetIPersistantDebug();
		pDebug->Begin("CGunTurret::PredictTargetPos", false);
		pDebug->AddSphere(tpos + delta, 0.2f, ColorF(1, 0, 0, 1), 1.f);
		IRenderAuxText::DrawLabelF(vpos, 1.4f, "Predict %s: speed %.1f (dspeed %.1f), acc %.1f, time %.1f", pTarget->GetName(), vel.len(), d_speed, a, time_to);
	}

	return tpos + delta;
}

//------------------------------------------------------------------------
Vec3 CGunTurret::GetSweepPos(IEntity* pTarget, const Vec3& shootPos)
{
	pTarget = ResolveTarget(pTarget);

	// sweep on ground
	int nhints = m_weaponsharedparams->pTurretParams->fireParams.hints.size();
	float sweepTime = m_turretparams.sweep_time / (float)nhints;
	float timeFiring = max(0.f, GetBurstTime() - sweepTime * (m_fireHint - 1));
	float sweepRelTime = min(1.f, timeFiring / sweepTime);

	if (sweepRelTime == 1.f && m_fireHint == nhints)
		return shootPos;

	Vec3 wpos(GetWeaponPos());
	Vec3 dir = shootPos - wpos;
	Vec3 dir2d(dir.x, dir.y, 0.f);
	float dist2d = dir2d.GetLength();

	if (dist2d < 2.f * m_weaponsharedparams->pTurretParams->fireParams.hints[0].y)
		return shootPos; // don't sweep when target too close

	dir2d /= dist2d;
	Vec3 right = Vec3(0, 0, -1) % dir2d;
	Vec3 zoffset(0, 0, 0);

	if (IPhysicalEntity* pPE = pTarget->GetPhysics())
	{
		pe_status_pos ppos;
		if (pPE->GetStatus(&ppos))
			zoffset = Vec3(0, 0, -0.5f * (ppos.BBox[1].z - ppos.BBox[0].z));
	}

	Vec3 lastHintPos(shootPos);
	const Vec2& lastHint = m_weaponsharedparams->pTurretParams->fireParams.hints[m_fireHint - 1];
	lastHintPos += lastHint.y * -dir2d + lastHint.x * right + zoffset;

	Vec3 nextHintPos(shootPos);
	if (m_fireHint < nhints)
	{
		const Vec2& nextHint = m_weaponsharedparams->pTurretParams->fireParams.hints[m_fireHint];
		nextHintPos += nextHint.y * -dir2d + nextHint.x * right + zoffset;
	}

	Vec3 currPos = Vec3::CreateLerp(lastHintPos, nextHintPos, sweepRelTime);

	if (sweepRelTime == 1.f && m_fireHint < nhints)
		++m_fireHint;

	if (g_pGameCVars->i_debug_turrets == eGTD_Sweep)
	{
		IRenderAuxGeom* pGeom = gEnv->pAuxGeomRenderer;
		pGeom->SetRenderFlags(e_Def3DPublicRenderflags);
		ColorB col(0, 255, 255, 128);
		pGeom->DrawSphere(currPos, 0.3f, col);
		pGeom->DrawSphere(lastHintPos, 0.3f, col);
		pGeom->DrawSphere(nextHintPos, 0.3f, col);
		pGeom->DrawLine(lastHintPos, col, nextHintPos, col);
		IRenderAuxText::DrawLabelF(currPos, 1.4f, "sweep, hint %i, ratio %.2f)", m_fireHint, sweepRelTime);
	}

	return currPos;
}

//------------------------------------------------------------------------
Vec3 CGunTurret::GetFiringPos(const Vec3& probableHit) const
{
	return CWeapon::GetFiringPos(probableHit);
}

//------------------------------------------------------------------------
Vec3 CGunTurret::GetWeaponPos() const
{
	//return GetEntity()->GetWorldPos();
	return GetEntity()->GetSlotWorldTM(eIGS_ThirdPerson).GetTranslation();
}

//------------------------------------------------------------------------
Vec3 CGunTurret::GetWeaponDir() const
{
	return GetEntity()->GetSlotWorldTM(eIGS_ThirdPerson).GetColumn1();
}

//------------------------------------------------------------------------
bool CGunTurret::IsTargetDead(IActor* pTarget) const
{
	if (!pTarget)
		return true;

	return pTarget->IsDead();
}

//------------------------------------------------------------------------
bool CGunTurret::IsTargetHostile(IActor* pTarget) const
{
	if (g_pGameCVars->i_debug_turrets == eGTD_AlwaysHostile)
		return true;

	bool hostileAI = false;
	if (gEnv->pAISystem)
	{
		if (IAIObject* aiObject = pTarget->GetEntity()->GetAI())
		{
			hostileAI = gEnv->pAISystem->GetFactionMap().GetReaction(m_turretparams.factionID, aiObject->GetFactionID())
			            == IFactionMap::Hostile;
		}
	}

	int team = g_pGame->GetGameRules()->GetTeam(pTarget->GetEntityId());
	bool sameTeam = (m_turretparams.team == 0) || (team == 0) || (m_turretparams.team == team);

	return hostileAI || !sameTeam;

	/*	IVehicle *pVehicle=g_pGame->GetIGameFramework()->GetIVehicleSystem()->GetVehicle(pTarget->GetId());
	   if (pVehicle)
	   {
	   int seatCount=pVehicle->GetSeatCount();
	   for (int i=0; i<seatCount; i++)
	   {
	   IVehicleSeat *pVehicleSeat=pVehicle->GetSeatById(i);
	   if (pVehicleSeat && pVehicleSeat->GetPassenger())
	   {
	   IEntity *pPassenger=gEnv->pEntitySystem->GetEntity(pVehicleSeat->GetPassenger());
	   if (pPassenger && IsTargetHostile(pPassenger))
	   return true;
	   }
	   }
	   }
	   return false;
	 */
}

//------------------------------------------------------------------------
bool CGunTurret::IsTargetSpectating(IActor* pTarget) const
{
	if (!pTarget)
		return false;

	return (static_cast<CActor*>(pTarget)->GetSpectatorMode() != 0);
}

//------------------------------------------------------------------------
bool CGunTurret::IsTACBullet(IEntity* pTarget) const
{
	const static IEntityClass* pTacClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("tacprojectile");
	const static IEntityClass* pTacGunClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("tacgunprojectile");

	if (pTarget->GetClass() != pTacClass && pTarget->GetClass() != pTacGunClass)
		return false;

	CProjectile* p = g_pGame->GetWeaponSystem()->GetProjectile(pTarget->GetId());
	if (!p)
		return false;

	IActor* pActor = GetActor(p->GetOwnerId());

	if (!pActor || !IsTargetHostile(pActor))
		return false;

	return true;
}

//------------------------------------------------------------------------
CGunTurret::ETargetClass CGunTurret::GetTargetClass(IEntity* pTarget) const
{
	IActor* pActor = GetActor(pTarget->GetId());
	if (!pActor)
	{
		if (IsTACBullet(pTarget))
			return eTC_TACProjectile;
		return eTC_NotATarget;
	}

	if (IsTargetDead(pActor))
		return eTC_NotATarget;

	if (!IsTargetHostile(pActor))
		return eTC_NotATarget;

	if (IsTargetSpectating(pActor))
		return eTC_NotATarget;

	bool vehicle = pActor ? pActor->GetLinkedVehicle() != 0 : false;

	//Vehicles only check
	if (m_turretparams.vehicles_only && !vehicle)
		return eTC_NotATarget;

	if (vehicle)
		return eTC_Vehicle;

	if (IsTargetCloaked(pActor))
		return eTC_NotATarget;

#ifndef _RELEASE
	static ICVar* ignorePlayer = gEnv->pConsole->GetCVar("ai_IgnorePlayer");
	if (pActor && pActor->IsPlayer() && ignorePlayer->GetIVal() == 1)
		return eTC_NotATarget;
#endif

	return eTC_Player;
}

//------------------------------------------------------------------------
bool CGunTurret::IsInRange(const Vec3& pos, ETargetClass cl) const
{
	float r = m_turretparams.mg_range;
	switch (cl)
	{
	case eTC_Player:
		r = m_turretparams.mg_range;
		break;
	case eTC_Vehicle:
		r = std::max(m_turretparams.mg_range, m_turretparams.rocket_range);
		break;
	case eTC_TACProjectile:
		r = m_turretparams.tac_range;
		break;
	}

	float dist = (pos - GetWeaponPos()).len2();
	if (dist > r * r + 0.1f)
	{
		//if (DebugTurret()) CryLog("Target out of range (%.1f > %.1f)", sqrt(dist), r);
		return false;
	}

	float yaw, pitch;
	GetTargetAngles(pos, yaw, pitch);

	if (!IsTargetAimable(yaw, pitch))
	{
		//if (DebugTurret()) CryLog("Target is not aimable");
		return false;
	}
	return true;
}

//------------------------------------------------------------------------
bool CGunTurret::IsTargetRocketable(const Vec3& pos) const
{
	float dist2 = (pos - GetWeaponPos()).len2();
	if (m_turretparams.rocket_range * m_turretparams.rocket_range < dist2)
		return false;
	return true;
}

//------------------------------------------------------------------------
bool CGunTurret::IsTargetMGable(const Vec3& pos) const
{
	float dist2 = (pos - GetWeaponPos()).len2();
	if (m_turretparams.mg_range * m_turretparams.mg_range < dist2)
		return false;
	return true;
}

//------------------------------------------------------------------------
bool CGunTurret::IsAiming(const Vec3& pos, float treshold) const
{
	Vec3 tdir = (pos - GetWeaponPos()).normalized();

	Vec3 weaponDir = GetWeaponDir().normalized();
	float dot = weaponDir.Dot(tdir);

	float angle = RAD2DEG(acos_tpl(dot));
	if (fabs_tpl(angle) > treshold)
		return false;
	return true;
}

//------------------------------------------------------------------------
IEntity* CGunTurret::ResolveTarget(IEntity* pTarget) const
{
	IActor* pActor = GetActor(pTarget->GetId());
	if (pActor)
	{
		if (IVehicle* pV = pActor->GetLinkedVehicle())
			return pV->GetEntity();
	}
	return pTarget;
}

//------------------------------------------------------------------------
IEntity* CGunTurret::GetClosestTACShell()
{
	if (g_pGame->GetCVars()->i_auto_turret_target_tacshells == 0)
		return NULL;

	float r = m_turretparams.tac_range;
	if (r == 0.f)
		return NULL;

	Vec3 pos = GetWeaponPos();
	SProjectileQuery pquery;
	pquery.box = AABB(Vec3(pos.x - r, pos.y - r, pos.z - r), Vec3(pos.x + r, pos.y + r, pos.z + r));

	ETargetClass closest = eTC_NotATarget;
	float closestDistSq = r * r;
	IEntity* pClosest = 0;
	for (int i = 0; i < pquery.nCount; ++i)
	{
		IEntity* pEntity = pquery.pResults[i];
		if (!pEntity || pEntity == GetEntity())
			continue;

		ETargetClass t_class = GetTargetClass(pEntity);
		if (t_class == eTC_NotATarget)
			continue;

		Vec3 targetPos = GetTargetPos(pEntity);
		float distSq = (targetPos - GetWeaponPos()).len2();

		if (closest >= t_class && distSq > closestDistSq)
			continue;

		float yaw, pitch;
		GetTargetAngles(targetPos, yaw, pitch);
		if (!IsTargetAimable(yaw, pitch))
			continue;
		bool canShoot = IsTargetShootable(pEntity);
		if (!canShoot)
			t_class = eTC_NotATarget;

		if (distSq < closestDistSq || (t_class >= closest))
		{
			closestDistSq = distSq;
			closest = t_class;
			pClosest = pEntity;
		}
	}
	return pClosest;
}
//------------------------------------------------------------------------
IEntity* CGunTurret::GetClosestTarget() const
{
	float r = std::max(m_turretparams.mg_range, m_turretparams.rocket_range);
	Vec3 pos = GetWeaponPos();

	IEntity* pClosest = 0;

	float closestDistSq = sqr(r);
	ETargetClass closest = eTC_NotATarget;

	SEntityProximityQuery query;
	query.box = AABB(Vec3(pos.x - r, pos.y - r, pos.z - r), Vec3(pos.x + r, pos.y + r, pos.z + r));

	gEnv->pEntitySystem->QueryProximity(query);

	for (int i = 0; i < query.nCount; i++)
	{
		IEntity* pEntity = query.pEntities[i];

		if (!pEntity || pEntity == GetEntity())
			continue;

		// check parent (and siblings) the turret might be linked to, to not attack them
		if (IEntity* pParent = GetEntity()->GetParent())
		{
			if (pEntity == pParent)
				continue;

			bool entityIsChild = false;

			const int nChildren = pParent->GetChildCount();
			for (int childIndex = 0; childIndex < nChildren; ++childIndex)
			{
				if (pParent->GetChild(childIndex) == pEntity)
				{
					entityIsChild = true;
					break;
				}
			}

			if (entityIsChild)
				continue;
		}

		ETargetClass t_class = GetTargetClass(pEntity);
		if (t_class == eTC_NotATarget)
			continue;

		Vec3 tpos = GetTargetPos(pEntity);

		if (!IsInRange(tpos, t_class))
			continue;

		float distSq = (tpos - pos).len2();

		if (closest >= t_class && distSq > closestDistSq)
			continue;

		bool canShoot = IsTargetShootable(pEntity);
		if (!canShoot)
			t_class = eTC_NotATarget;
		else if (distSq < closestDistSq || (t_class >= closest))
		{
			closestDistSq = distSq;
			closest = t_class;
			pClosest = pEntity;
		}
	}

	if (closest == eTC_NotATarget && !m_turretparams.surveillance)
		return 0;
	return pClosest;
}

//------------------------------------------------------------------------
bool CGunTurret::GetTargetAngles(const Vec3& targetPos, float& z, float& x) const
{
	// turret rotation reachability check
	Vec3 weaponPos = GetWeaponPos();
	Vec3 targetDir = targetPos - weaponPos;

	if (targetDir.len2() < minLenSqr)
		return false;

	// local dir to target
	Vec3 targetDirLocal = GetEntity()->GetWorldTM().GetInverted().TransformVector(targetDir);

	// angle around z
	Vec3 targetDirZ(targetDirLocal.x, targetDirLocal.y, 0.f);

	if (targetDirZ.len2() < minLenSqr)
		z = 0.f;
	else
	{
		targetDirZ.Normalize();
		float cosZ = FORWARD_DIRECTION * targetDirZ;
		z = acos_tpl(cosZ) * -sgnnz(targetDirZ.x);
		if (z < 0.0f)
			z += gf_PI2;
	}

	// angle around x
	Vec3 targetDirX(targetDirLocal);
	Vec3 targetDirBase(targetDirLocal.x, targetDirLocal.y, 0);

	if (targetDirBase.len2() < minLenSqr)
		x = 0.0f;
	else
	{
		targetDirX.Normalize();
		targetDirBase.Normalize();

		float cosX = targetDirBase * targetDirX;
		x = acos_tpl(cosX) * sgnnz(targetDirX.z);
	}

	return true;
}

//------------------------------------------------------------------------
bool CGunTurret::IsTargetAimable(float angleYaw, float anglePitch) const
{
	bool ok = true;
	float testYaw = ToPlusMinusPI(angleYaw);

	ok &= m_turretparams.yaw_range == 0.f || abs(testYaw) < 0.5f * DEG2RAD(m_turretparams.yaw_range);
	ok &= (m_turretparams.max_pitch == 0.f && m_turretparams.min_pitch == 0.f) ||
	      (anglePitch<DEG2RAD(m_turretparams.max_pitch) && anglePitch> DEG2RAD(m_turretparams.min_pitch));

	return ok;
}

//------------------------------------------------------------------------
bool CGunTurret::RayCheck(IEntity* pTarget, const Vec3& pos, const Vec3& dir) const
{
	ray_hit rayhit;
	IPhysicalEntity* pSkipEnts[1];
	pSkipEnts[0] = GetEntity()->GetPhysics();
	int nSkip = 1;

	//make sure you are not inside geometry when casting
	Vec3 newPos = pos + 0.3f * dir;

	if (gEnv->pPhysicalWorld->RayWorldIntersection(newPos, dir, ent_all, rwi_stop_at_pierceable | rwi_colltype_any, &rayhit, 1, pSkipEnts, nSkip))
	{
		if (rayhit.pCollider)
		{
			IEntity* pEntity = (IEntity*)rayhit.pCollider->GetForeignData(PHYS_FOREIGN_ID_ENTITY);

			if (pEntity == pTarget)
				return true;
			else if (CActor* pActor = GetActor(pTarget->GetId()))
			{
				IVehicle* pLinkedVehicle = pActor->GetLinkedVehicle();
				if (pLinkedVehicle && pLinkedVehicle->GetEntity() == pEntity)
					return true;
			}
		}
		return false;
	}
	return true;
}

//------------------------------------------------------------------------
bool CGunTurret::IsTargetShootable(IEntity* pTarget) const
{
	// raycast shootability check
	Vec3 pos = m_weaponsharedparams->pTurretParams->fireHelper.empty() ? GetWeaponPos() : GetSlotHelperPos(eIGS_ThirdPerson, m_weaponsharedparams->pTurretParams->fireHelper.c_str(), true);
	Vec3 tpos = GetTargetPos(pTarget);
	Vec3 dir = tpos - pos;

	bool shootable = RayCheck(pTarget, pos, dir);

	if (!shootable)
	{
		// fallback for actors
		// todo: also use this for shooting pos!
		CActor* pActor = GetActor(pTarget->GetId());
		if (pActor && pActor->GetMovementController())
		{
			SMovementState state;
			pActor->GetMovementController()->GetMovementState(state);
			dir = state.eyePosition - pos;
			shootable = RayCheck(pTarget, pos, dir);
		}
	}

	return shootable;
}

//------------------------------------------------------------------------
bool CGunTurret::IsTargetCloaked(IActor* pActor) const
{
	// cloak check
	if (m_turretparams.find_cloaked)
		return false;

	// if destinationId assigned, target can always be found
	if (m_destinationId && pActor->GetEntityId() == m_destinationId)
		return false;

	bool inside = false;

	if (m_turretparams.light_fov != 0.f)
	{
		// if cloaked, target can only be found with searchlight
		// check if target inside light cone
		const Matrix34& weaponTM = GetEntity()->GetSlotWorldTM(eIGS_ThirdPerson);
		Vec3 wpos(weaponTM.GetTranslation());
		Vec3 wdir(weaponTM.GetColumn1());
		Vec3 tpos(GetTargetPos(pActor->GetEntity()));

		float epsilon = 0.8f;
		Quat rot = Quat::CreateRotationAA(epsilon * 0.5f * DEG2RAD(m_turretparams.light_fov), weaponTM.GetColumn2());
		Vec3 a = wpos + m_turretparams.mg_range * (wdir * rot);
		Vec3 b = wpos + m_turretparams.mg_range * (wdir * rot.GetInverted());
		inside = Overlap::PointInTriangle(tpos, wpos, a, b, weaponTM.GetColumn2());

		if (inside)
		{
			rot = Quat::CreateRotationAA(0.5f * DEG2RAD(m_turretparams.light_fov), weaponTM.GetColumn0());
			a = wpos + m_turretparams.mg_range * (wdir * rot);
			b = wpos + m_turretparams.mg_range * (wdir * rot.GetInverted());
			inside = Overlap::PointInTriangle(tpos, wpos, a, b, weaponTM.GetColumn0());
		}

		if (g_pGameCVars->i_debug_turrets == eGTD_Search)
		{
			IRenderAuxGeom* pGeom = gEnv->pAuxGeomRenderer;
			pGeom->SetRenderFlags(e_Def3DPublicRenderflags);
			float color[] = { 1, 1, 1, 1 };
			Vec3 points[] = { wpos, a, b };
			pGeom->DrawPolyline(points, 3, true, ColorB(0, 255, 0, 255));

			if (inside)
				IRenderAuxText::Draw2dLabel(200, 200, 1.4f, color, false, "target inside cone");
		}
	}

	return !inside;
}

//------------------------------------------------------------------------
void CGunTurret::UpdateGoal(IEntity* pTarget, float deltaTime)
{
	Vec3 shootPos = PredictTargetPos(pTarget, false);

	if (m_turretparams.sweep_time != 0.f && !m_weaponsharedparams->pTurretParams->fireParams.hints.empty())
		shootPos = GetSweepPos(pTarget, shootPos);

	if (m_weaponsharedparams->pTurretParams->fireParams.deviation_amount != 0.f)
		UpdateDeviation(deltaTime, shootPos);

	float goalYaw(0.f), goalPitch(0.f);
	if (!GetTargetAngles(shootPos, goalYaw, goalPitch))
		return;

	if (!IsTargetAimable(goalYaw, goalPitch))
	{
		if (DebugTurret()) CryLog("UpdateGoal: %s IsTargetAimable failed (yaw: %.2f, pitch: %.2f)", pTarget->GetName(), goalYaw, goalPitch);
		return;
	}

	// if (fabs_tpl(m_goalYaw-goalYaw)<0.0001f && fabs_tpl(m_goalPitch-m_goalPitch)<0.0001f )
	//     return;
	SetGoalPitchAndYaw(goalPitch, goalYaw);
}

//------------------------------------------------------------------------
void CGunTurret::UpdateDeviation(float deltaTime, const Vec3& shootPos)
{
	float burstTime = GetBurstTime();
	float t = burstTime * m_weaponsharedparams->pTurretParams->fireParams.deviation_speed; // x/(1/speed)
	float sinT = sinf(gf_PI2 * t);

	// reduce deviation towards end of burst
	float deviationEndTime = (m_turretparams.sweep_time != 0.f) ? m_turretparams.sweep_time : (m_turretparams.burst_time != 0.f) ? m_turretparams.burst_time : 5.f;
	float amount = m_weaponsharedparams->pTurretParams->fireParams.deviation_amount * (1.f - 0.75f * min(1.f, burstTime / deviationEndTime));

	Vec3 dev(amount * sinT, amount * sinT, 0.f);
	m_deviationPos = dev;

	if (g_pGameCVars->i_debug_turrets == eGTD_Deviation)
	{
		float color[] = { 0.8f, 1.f, 0.8f, 1.f };
		IRenderAuxText::Draw2dLabel(100, 400, 1.5f, color, false, "dev: %.2f (amount: %.2f)", dev.x, amount);
	}
}

//------------------------------------------------------------------------
float CGunTurret::GetYaw() const
{
	Ang3 angles(GetEntity()->GetSlotLocalTM(eIGS_Aux0, false));

	return angles.z;
}

//------------------------------------------------------------------------
float CGunTurret::GetPitch() const
{
	Ang3 angles(GetEntity()->GetSlotLocalTM(eIGS_ThirdPerson, false));

	return angles.x;
}

//------------------------------------------------------------------------
float CGunTurret::GetBurstTime() const
{
	return max(0.f, m_burstTimer - m_fm->GetSpinUpTime());
}

//------------------------------------------------------------------------
void CGunTurret::UpdateOrientation(float deltaTime)
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);

	IEntity& entity = *GetEntity();

	bool changed = false;
	const bool searching = (m_targetId == 0 && m_turretparams.searching);
	const float speed = searching ? m_turretparams.search_speed : m_turretparams.turn_speed;

	assert(m_goalYaw >= 0.f && m_goalYaw <= gf_PI2);

	const int turretModelSlot = eIGS_ThirdPerson;

	// update turret
	Matrix34 turretTransform = entity.GetSlotLocalTM(turretModelSlot, false);
	Ang3 turretAngles(turretTransform);

	if (turretAngles.z < 0.0f)
		turretAngles.z += gf_PI2;

	if (fabs_tpl(m_goalYaw - turretAngles.z) > gf_PI)
	{
		if (m_goalYaw >= gf_PI)
			turretAngles.z += gf_PI2;
		else
			turretAngles.z -= gf_PI2;
	}

	if (m_turretparams.yaw_range < 360.f)
	{
		// reverse, to avoid forbidden range
		if (m_goalYaw > gf_PI && turretAngles.z < gf_PI)
			turretAngles.z += gf_PI2;
		else if (m_goalYaw < gf_PI && turretAngles.z > gf_PI)
			turretAngles.z -= gf_PI2;
	}

	if (fabs_tpl(turretAngles.z - m_goalYaw) > 0.001f)
	{
		Interp(turretAngles.z, m_goalYaw, speed, deltaTime, 0.25f * speed);

		if (gEnv->IsClient())
			PlayAction(GetFragmentIds().turret);

		changed = true;
	}

	if (fabs_tpl(turretAngles.x - m_goalPitch) > 0.001f)
	{
		Interp(turretAngles.x, m_goalPitch, speed, deltaTime, 0.25f * speed);

		if (gEnv->IsClient())
			PlayAction(GetFragmentIds().cannon);
		changed = true;
	}

	if (changed)
	{
		turretTransform.SetRotationXYZ(turretAngles, m_turretHelperPos);
		entity.SetSlotLocalTM(turretModelSlot, turretTransform);

		if (gEnv->IsClient())
		{
			m_effectsController.UpdateEntitySlotEffectLocationsFromHelpers();
		}
	}

	UpdatePhysics();

	if (g_pGameCVars->i_debug_turrets == eGTD_Basic)
	{
		DrawDebug();
		//IRenderAuxText::DrawLabelF(entity.GetWorldPos(), 1.4f, "%s yaw: %.2f, goalYaw: %.2f (%.2f), goalPitch: %.2f (%.2f/%.2f)", searching?"[search]":"", RAD2DEG(turretAngles.z), RAD2DEG(m_goalYaw), 0.5f*(m_turretparams.yaw_range), RAD2DEG(m_goalPitch), m_turretparams.min_pitch, m_turretparams.max_pitch);
	}
}

//------------------------------------------------------------------------
bool CGunTurret::GetFiringDir(EntityId weaponId, const IFireMode* pFireMode, Vec3& dir, const Vec3& probableHit, const Vec3& firingPos)
{
	bool deviation = (pFireMode == m_fm && !m_deviationPos.IsZero());
	bool helper = pFireMode->HasFireHelper();

	dir = helper ? pFireMode->GetFireHelperDir() : GetWeaponDir();

	if (deviation)
	{
		Vec3 firePos = (!firingPos.IsZero()) ? firingPos : helper ? pFireMode->GetFireHelperPos() : GetWeaponPos();
		Vec3 hitPos = (!probableHit.IsZero()) ? probableHit : firePos + m_turretparams.mg_range * dir;
		hitPos += m_deviationPos;

		dir = hitPos - firePos;
		dir.Normalize();
	}

	return true;
}
//------------------------------------------------------------------------
bool CGunTurret::GetActualWeaponDir(EntityId weaponId, const IFireMode* pFireMode, Vec3& dir, const Vec3& probableHit, const Vec3& firingPos)
{
	return GetFiringDir(weaponId, pFireMode, dir, probableHit, firingPos);
}

//------------------------------------------------------------------------
void CGunTurret::UpdateSearchingGoal(float deltaTime)
{
	const static float epsilon_goal = DEG2RAD(1.f);

	m_goalPitch = DEG2RAD((m_turretparams.max_pitch + m_turretparams.min_pitch) / 2.0f);

	float max_yaw = min(gf_PI, 0.5f * DEG2RAD(m_turretparams.yaw_range));
	float yaw = To2PI(GetYaw());
	float pitch = GetPitch();

	if (!m_weaponsharedparams->pTurretParams->searchParams.hints.empty())
	{
		// search by hints
		const Vec2& hint = m_weaponsharedparams->pTurretParams->searchParams.hints[m_searchHint];

		m_goalYaw = hint.x * max_yaw;
		m_goalYaw = (m_goalYaw <= 0.f) ? -m_goalYaw : gf_PI2 - m_goalYaw; // [0..PI2]

		m_goalPitch = abs(hint.y) * DEG2RAD((hint.y > 0.f) ? m_turretparams.max_pitch : m_turretparams.min_pitch);

		if (abs(m_goalYaw - yaw) < epsilon_goal && abs(m_goalPitch - pitch) < epsilon_goal)
		{
			if (++m_searchHint == m_weaponsharedparams->pTurretParams->searchParams.hints.size())
				m_searchHint = 0;
		}
	}
	else
	{
		// horizontal search within angle limits
		m_goalYaw = (m_searchHint) ? max_yaw : -max_yaw;
		m_goalYaw = (m_goalYaw <= 0.f) ? -m_goalYaw : gf_PI2 - m_goalYaw;

		if (abs(m_goalYaw - yaw) < epsilon_goal)
			m_searchHint ^= 1;
	}

	CHANGED_NETWORK_STATE(this, ASPECT_GOALORIENTATION);
}

//------------------------------------------------------------------------
void CGunTurret::UpdatePhysics()
{
	int slots[] = { eIGS_Aux0, eIGS_Aux1, eIGS_ThirdPerson };
	int slots_num = 3;

	if (m_destroyed)
	{
		slots[1] = eIGS_Destroyed;
		slots_num = 2;
	}

	if (IEntity* pParent = GetEntity()->GetParent())
	{
		IPhysicalEntity* pParentPhysics = pParent->GetPhysics();
		if (pParentPhysics)
		{
			Matrix34 localTM = GetEntity()->GetLocalTM();
			localTM.OrthonormalizeFast();
			Quat localQ = Quat(localTM);

			for (int i = 0; i < slots_num; ++i)
			{
				pe_params_part params;

				params.partid = slots[i] + GetEntity()->GetPhysicalEntityPartId0();

				const Matrix34& slotTM = GetEntity()->GetSlotLocalTM(slots[i], false);

				params.pos = slotTM.GetTranslation();
				params.q = Quat(slotTM);

				params.pos = localTM * params.pos;
				params.q = localQ * params.q;
				pParentPhysics->SetParams(&params);
			}
		}
	}
	else
	{
		if (IPhysicalEntity* pPhysics = GetEntity()->GetPhysics())
		{
			for (int i = 0; i < slots_num; ++i)
			{
				pe_params_part params;
				params.partid = slots[i];

				Matrix34 slotTM = GetEntity()->GetSlotLocalTM(slots[i], false);
				params.pMtx3x4 = &slotTM;

				pPhysics->SetParams(&params);
			}
		}
	}
}

//------------------------------------------------------------------------
void CGunTurret::InternalStartFire(bool sec)
{
	if (g_pGameCVars->i_debug_turrets)
		CryLog("%s InternalStartFire(%i)", GetEntity()->GetName(), (sec) ? 2 : 1);

	if (!sec)
		CWeapon::StartFire();
	else if (m_fm2)
		m_fm2->StartFire();

	m_fireHint = 1;
}

//------------------------------------------------------------------------
void CGunTurret::InternalStopFire(bool sec)
{
	if (g_pGameCVars->i_debug_turrets)
		CryLog("%s InternalStopFire(%i)", GetEntity()->GetName(), (sec) ? 2 : 1);

	if (!sec)
		CWeapon::StopFire();
	else if (m_fm2)
		m_fm2->StopFire();
}

//------------------------------------------------------------------------
bool CGunTurret::InternalIsFiring(bool sec)
{
	if (!sec)
		return m_fm->IsFiring();
	else if (m_fm2)
		return m_fm2->IsFiring();

	return false;
}

//------------------------------------------------------------------------
void CGunTurret::ChangeTargetTo(IEntity* pTarget)
{
	m_updateTargetTimer = 0.0f;
	m_rayTimer = 0.f;
	int new_id = pTarget ? pTarget->GetId() : 0;
	if (new_id != m_targetId)
	{
		m_targetId = new_id;
		OnTargetLocked(pTarget);
	}
}

//------------------------------------------------------------------------
void CGunTurret::OnTargetLocked(IEntity* pTarget)
{
	if (pTarget)
		PlayAction(GetFragmentIds().lock);

	if (IsServer())
	{
		// if this turret has others linked, notify them about acquired target
		for (IEntityLink* pLink = GetEntity()->GetEntityLinks(); pLink; pLink = pLink->next)
		{
			if (0 != strcmp(pLink->name, "TargetInfo"))
				continue;

			IItem* pItem = g_pGame->GetIGameFramework()->GetIItemSystem()->GetItem(pLink->entityId);
			if (pItem)
			{
				SGameObjectEvent event(pTarget ? eCGE_Turret_LockedTarget : eCGE_Turret_LostTarget, eGOEF_ToExtensions);
				event.ptr = pTarget;
				pItem->GetGameObject()->SendEvent(event);
			}
		}
	}
}

//------------------------------------------------------------------------
void CGunTurret::HandleEvent(const SGameObjectEvent& event)
{
	CItem::HandleEvent(event);

	switch (event.event)
	{
	case (eCGE_Turret_LockedTarget):
		{
			if (event.ptr)
			{
				m_destinationId = ((IEntity*)event.ptr)->GetId();
			}
		}
		break;
	case (eCGE_Turret_LostTarget):
		m_destinationId = 0;
		break;
	case eCGE_SetTeam:
		const STeamChangeInfo* teamChangeInfo = (const STeamChangeInfo*)event.param;
		SetTeamNum(teamChangeInfo->newTeamNum);
		break;
	}
}

//------------------------------------------------------------------------
void CGunTurret::Activate(bool active)
{
	if (!IsDestroyed())
		PlayAction(active ? GetFragmentIds().activate : GetFragmentIds().deactivate);
}

//------------------------------------------------------------------------
bool CGunTurret::UpdateBurst(float deltaTime)
{
	if (!InternalIsFiring(false))
	{
		// not firing/pausing
		m_pauseTimer -= deltaTime;
		return (m_pauseTimer <= 0.f);
	}
	else
	{
		if (GetBurstTime() > m_turretparams.burst_time + m_randoms[eRV_BurstTime].Val())
		{
			m_pauseTimer = m_turretparams.burst_pause + m_randoms[eRV_BurstPause].Val();

			m_randoms[eRV_BurstTime].New();
			m_randoms[eRV_BurstPause].New();

			return false;
		}

		return true;
	}
}

//------------------------------------------------------------------------
void CGunTurret::ServerUpdate(SEntityUpdateContext& ctx, int update)
{
	//update parameters. SNH: cache these in MP since they never change.
	if (!gEnv->bMultiplayer)
	{
		UpdateEntityProperties();
	}

	IEntity* pCurrentTarget = gEnv->pEntitySystem->GetEntity(m_targetId);

	bool mg = false;
	bool rocket = false;
	if (IsOperational())
	{
		bool renew_target = false;
		//do this before, cause it's more important
		if (m_turretparams.TAC_check_time != 0.f)
		{
			if (m_checkTACTimer > m_turretparams.TAC_check_time)
			{
				m_checkTACTimer = 0.0f;
				IEntity* pClosest = GetClosestTACShell();
				if (pClosest)
				{
					ChangeTargetTo(pClosest);
					pCurrentTarget = pClosest;
				}
			}
			else
				m_checkTACTimer += ctx.fFrameTime;
		}

		//actually if(...), break at end
		while (pCurrentTarget)
		{
			if (InternalIsFiring(false))
				m_burstTimer += ctx.fFrameTime;
			else
				m_burstTimer = 0.f;

			ETargetClass t_class = GetTargetClass(pCurrentTarget);
			bool validClass = (t_class != eTC_NotATarget);

			Vec3 tpos = PredictTargetPos(pCurrentTarget, false);
			bool inrange = IsInRange(tpos, t_class);

			if (m_rayTimer <= 0.f)
			{
				m_canShoot = IsTargetShootable(pCurrentTarget);
				m_rayTimer = (m_canShoot) ? 0.5f : 0.2f;
			}
			else
				m_rayTimer -= ctx.fFrameTime;

			if (!(validClass && inrange && m_canShoot))
			{
				m_abandonTargetTimer += ctx.fFrameTime;
			}
			else
				m_abandonTargetTimer = 0.0f;

			if (m_abandonTargetTimer > m_turretparams.abandon_target_time + m_randoms[eRV_AbandonTarget].val)
			{
				renew_target = true;
				m_randoms[eRV_AbandonTarget].New();
				break;
			}
			bool aim = inrange && m_canShoot && IsAiming(tpos, m_turretparams.aim_tolerance);
			mg = aim && !m_turretparams.search_only && IsTargetMGable(tpos);

			bool burst = (m_turretparams.burst_time == 0.f || UpdateBurst(ctx.fFrameTime));
			mg &= burst;

			// a bit less tolerant for rockets
			aim = aim && IsAiming(tpos, m_turretparams.aim_tolerance * 0.5f);

			rocket = aim && !m_turretparams.search_only && m_fm2 && t_class == eTC_Vehicle && IsTargetRocketable(tpos);

			if (g_pGameCVars->i_debug_turrets)
			{
				static float white[4] = { 1, 1, 1, 1 };
				float x = 5.f, y = 50.f, step1 = 15.f, size = 1.3f;

				IRenderAuxText::Draw2dLabel(x, y += step1, size, white, false, "Target: %s", pCurrentTarget->GetName());
				IRenderAuxText::Draw2dLabel(x, y += step1, size, white, false, "InRange: %i", inrange);
				IRenderAuxText::Draw2dLabel(x, y += step1, size, white, false, "CanShoot: %i", m_canShoot);
				IRenderAuxText::Draw2dLabel(x, y += step1, size, white, false, "IsAiming: %i", aim);
				IRenderAuxText::Draw2dLabel(x, y += step1, size, white, false, "Burst: %i", burst);
				IRenderAuxText::Draw2dLabel(x, y += step1, size, white, false, "MG: %i", mg);
				IRenderAuxText::Draw2dLabel(x, y += step1, size, white, false, "BurstTimer: %.2f", m_burstTimer);
				//IRenderAuxText::Draw2dLabel(x, y+=step1, size, white, false, "Rocket: %i", rocket);
				IRenderAuxText::Draw2dLabel(x, y += step1, size, white, false, "TargetPos: %.1f %.1f %.1f", tpos.x, tpos.y, tpos.z);
				IRenderAuxText::Draw2dLabel(x, y += step1, size, white, false, "Abandon: %.2f", m_abandonTargetTimer);
				IRenderAuxText::Draw2dLabel(x, y += step1, size, white, false, "Update: %.2f", m_updateTargetTimer);
				IRenderAuxText::Draw2dLabel(x, y += step1, size, white, false, "GoalYaw: %.2f, GoalPitch: %.2f", m_goalYaw, m_goalPitch);
			}

			break;
		}

		m_updateTargetTimer += ctx.fFrameTime;
		if (renew_target || m_updateTargetTimer > m_turretparams.update_target_time + m_randoms[eRV_UpdateTarget].Val())
		{
			IEntity* pClosestTAC = GetClosestTACShell();
			IEntity* pClosest = (pClosestTAC) ? pClosestTAC : GetClosestTarget();

			// change target if tac shell, or other target closer than current
			// otherwise, only change after abandoning time is exceeded
			if (pClosestTAC || (pClosest && pClosest->GetId() != m_targetId) || (!pClosest && !(m_abandonTargetTimer > 0.f && m_abandonTargetTimer <= m_turretparams.abandon_target_time)))
			{
				ChangeTargetTo(pClosest);
				pCurrentTarget = pClosest;
			}
			m_updateTargetTimer = 0.f;
			m_randoms[eRV_UpdateTarget].New();
		}

		UpdateAiming(pCurrentTarget, ctx);
	}

	OverrideShouldShoot(pCurrentTarget, mg);

	if (m_fm && mg != InternalIsFiring(false))
	{
		if (mg)
			InternalStartFire(false);
		else
			InternalStopFire(false);
	}

	if (m_fm2 && rocket != InternalIsFiring(true))
	{
		if (rocket)
			InternalStartFire(true);
		else
			InternalStopFire(true);
	}
}

//------------------------------------------------------------------------
void CGunTurret::UpdateAiming(IEntity* pCurrentTarget, SEntityUpdateContext& ctx)
{
	if (pCurrentTarget)
	{
		if (m_turretparams.surveillance || IsTargetShootable(pCurrentTarget))
			UpdateGoal(pCurrentTarget, ctx.fFrameTime);
	}
	else
	{
		if (m_turretparams.searching)
		{
			UpdateSearchingGoal(ctx.fFrameTime);
		}
	}
}

//------------------------------------------------------------------------
void CGunTurret::UpdateEntityProperties()
{
	bool enabled = m_turretparams.enabled;
	GetEntityProperty("GunTurret", "bEnabled", m_turretparams.enabled);
	if (enabled != m_turretparams.enabled)
	{
		Activate(m_turretparams.enabled);
		CHANGED_NETWORK_STATE(this, ASPECT_STATEBITS);
	}

	GetEntityProperty("GunTurret", "bSearching", m_turretparams.searching);
	GetEntityProperty("GunTurret", "bSurveillance", m_turretparams.surveillance);

	const char* teamName = 0;
	GetEntityProperty("teamName", teamName);
	if (teamName && teamName[0])
	{
		SetTeamNum(g_pGame->GetGameRules()->GetTeamId(teamName));
	}
}

//------------------------------------------------------------------------
void CGunTurret::Update(SEntityUpdateContext& ctx, int update)
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);

	CWeapon::Update(ctx, update);

	if (IAIObject* aiObject = GetEntity()->GetAI())
		aiObject->SetPos(GetWeaponPos(), GetWeaponDir());

	if (update == eIUS_FireMode && m_fm2)
		m_fm2->Update(ctx.fFrameTime, ctx.frameID);

	if (update != eIUS_General)
		return;

	if (IsServer())
		ServerUpdate(ctx, update);

	UpdateOrientation(ctx.fFrameTime);  // rotate turret towards target, adhering to angle limits

	if (g_pGameCVars->i_debug_turrets == eGTD_Search)
	{
		DrawCircle(GetEntity()->GetWorldPos(), m_turretparams.mg_range);
	}

	//--- Scan for the floor
	if (m_weaponsharedparams->pTurretParams->desiredHoverHeight > 0.0f)
	{
		UpdateHover(ctx);
	}

	//
	/*
	   if(InternalIsFiring(false))
	   {
	   IEntity* pCurrentTarget = gEnv->pEntitySystem->GetEntity(m_targetId);
	   if(!pCurrentTarget)
	   return;
	   CSingle* pFM = (CSingle*)GetFireMode(0);
	   Vec3 hit = pFM->GetProbableHit(2000.0f);
	   Vec3 dir = pFM->NetGetFiringDir(hit, pFM->NetGetFiringPos(hit));
	   Vec3 pos = GetTargetPos(pCurrentTarget);
	   Vec3 wpos = GetWeaponPos();
	   Vec3 tdir = (pos-wpos).normalized();
	   float angcos = tdir*dir;
	   float ang = RAD2DEG(acos_tpl(angcos));
	   if(ang>2.0f)
	   {
	   CryLog("%X Shooting while not in cone, %.3f",GetEntity()->GetId(),ang);
	   }
	   float yaw, pitch;
	   GetTargetAngles(pos,yaw,pitch);
	   if(!IsAiming(pos,5.0f))
	   {
	   CryLog("Turret %X firing at %X. Goal %.2f %.2f, target %.2f %.2f, diff %.2f %.2f",GetEntity()->GetId(),m_targetId,m_goalYaw,m_goalPitch,yaw,pitch,fabs_tpl(m_goalYaw-yaw),fabs_tpl(m_goalPitch-pitch));
	   }
	   }*/
}

void CGunTurret::UpdateHover(SEntityUpdateContext& ctx)
{
	static const float MAXSPEED_HEIGHTDIFF = 0.5f;
	static const float PROBE_BUFFER = 0.25f;
	static const float SPEED_FACTOR = 1.0f;
	static const float ACCELERATION = 100.0f;
	static const float FLUCTUATION = 0.25f;

	const Vec3& curPos = GetEntity()->GetPos();
	Vec3 probeFrom = curPos;
	probeFrom.z += PROBE_BUFFER;
	Vec3 probe(0.0f, 0.0f, -(m_weaponsharedparams->pTurretParams->desiredHoverHeight + MAXSPEED_HEIGHTDIFF + PROBE_BUFFER));
	IPhysicalWorld* pWorld = gEnv->pPhysicalWorld;
	ray_hit hit;
	float targetVel = -MAXSPEED_HEIGHTDIFF * SPEED_FACTOR;
	if (pWorld->RayWorldIntersection(probeFrom, probe, PROBE_TYPES, PROBE_FLAGS, &hit, 1))
	{
		float floorHeight = hit.pt.z;
		float heightDiff = (floorHeight + m_weaponsharedparams->pTurretParams->desiredHoverHeight + (sin_tpl(ctx.fCurrTime) * FLUCTUATION)) - curPos.z;
		targetVel = heightDiff * SPEED_FACTOR;
	}

	float velDiff = targetVel - m_vertVel;
	float velStep = ACCELERATION * ctx.fFrameTime;
	if (fabs_tpl(velDiff) < velStep)
	{
		m_vertVel = targetVel;
	}
	else if (velDiff < 0.0f)
	{
		m_vertVel -= velStep;
	}
	else
	{
		m_vertVel += velStep;
	}

	Vec3 newPos = curPos;
	newPos.z += m_vertVel * ctx.fFrameTime;
	GetEntity()->SetPos(newPos);
}

//----------------------------------------------------------------------
void CGunTurret::DestroyedGeometry(bool use)
{
	m_destroyed = use;

	CItem::DestroyedGeometry(use);

	if (!m_geometry[eIGS_Destroyed].empty())
	{
		if (use)
		{
			GetEntity()->SetSlotLocalTM(eIGS_Destroyed, GetEntity()->GetSlotLocalTM(eIGS_ThirdPerson, false));
		}

		DrawSlot(eIGS_Aux1, !use);
		SetAspectProfile(eEA_Physics, eIPhys_PhysicalizedRigid);
		UpdatePhysics();
	}

	DrawSlot(eIGS_ThirdPerson, !use);

	if (use)
	{
		EnableUpdate(false, eIUS_General);
		Quiet();
	}
}

//------------------------------------------------------------------------
void CGunTurret::SetCharacterAttachmentLocalTM(int slot, const char* name, const Matrix34& tm)
{
	if (slot == eIGS_ThirdPerson && !strcmp(name, "barrel"))
	{
		m_barrelRotation = tm;
	}
	else
		CWeapon::SetCharacterAttachmentLocalTM(slot, name, tm);
}

//------------------------------------------------------------------------
bool CGunTurret::SetAspectProfile(EEntityAspects aspect, uint8 profile)
{
	if (!CWeapon::SetAspectProfile(aspect, profile))
		return false;

	if (aspect == eEA_Physics && profile == eIPhys_PhysicalizedRigid)
	{
		SEntityPhysicalizeParams params;
		params.type = PE_RIGID;
		params.mass = m_sharedparams->params.mass;

		pe_params_buoyancy buoyancy;
		buoyancy.waterDamping = 1.5;
		buoyancy.waterResistance = 1000;
		buoyancy.waterDensity = 0;
		params.pBuoyancy = &buoyancy;

		params.nSlot = eIGS_Aux0;
		GetEntity()->PhysicalizeSlot(eIGS_Aux0, params);

		if (m_destroyed)
		{
			GetEntity()->UnphysicalizeSlot(eIGS_ThirdPerson);
			params.nSlot = eIGS_Destroyed;
			GetEntity()->PhysicalizeSlot(eIGS_Destroyed, params);
		}
		else
		{
			params.nSlot = eIGS_Aux1;
			GetEntity()->PhysicalizeSlot(eIGS_Aux1, params);
		}

		params.nSlot = eIGS_ThirdPersonAux;
		params.mass *= 100.f;
		GetEntity()->PhysicalizeSlot(eIGS_ThirdPersonAux, params);

	}

	return true;
}

//--------------------------------------------------------------------
void CGunTurret::DrawDebug()
{
	IPersistantDebug* pDebug = gEnv->pGameFramework->GetIPersistantDebug();
	pDebug->Begin("CGunTurret::DrawDebug", true);

	Vec3 gun(ZERO), rocket(ZERO), radar(ZERO), barrel(ZERO);

	gun = GetSlotHelperPos(eIGS_ThirdPerson, m_weaponsharedparams->pTurretParams->fireHelper.c_str(), true);
	rocket = GetSlotHelperPos(eIGS_ThirdPerson, m_weaponsharedparams->pTurretParams->rocketHelper.c_str(), true);
	barrel = GetSlotHelperPos(eIGS_ThirdPerson, m_weaponsharedparams->pTurretParams->barrelHelper.c_str(), true);
	radar = GetSlotHelperPos(eIGS_Aux0, m_weaponsharedparams->pTurretParams->radarHelper.c_str(), true);

	pDebug->AddSphere(gun, 0.2f, ColorF(1, 0, 0, 1), 1.f);
	pDebug->AddSphere(rocket, 0.2f, ColorF(0, 1, 0, 1), 1.f);
	pDebug->AddSphere(radar, 0.2f, ColorF(0, 0, 1, 1), 1.f);
	pDebug->AddSphere(barrel, 0.2f, ColorF(1, 0, 1, 1), 1.f);
}

//--------------------------------------------------------------------
// Test to see if a deployment is viable & where the spawn point should be
//--------------------------------------------------------------------
bool CGunTurret::QueryDeployment(const Vec3& pos, const Vec3& direction, float radius, Vec3& hitPos)
{
	static float MINDIST = 1.0f;
	static float TARGETDIST = 3.0f;
	static float MAXDROP = 5.0f;
	static float BUFFER = 0.2f;
	const Vec3 DOWN(0.0f, 0.0f, -1.0f);

	ColorB fromCol(0x00, 0x00, 0xff, 0xff);
	ColorB clearCol(0x00, 0x00, 0xff, 0xff);
	ColorB hitCol(0xff, 0x00, 0x00, 0xff);
	gEnv->pAuxGeomRenderer->DrawSphere(pos, radius, fromCol);

	primitives::sphere sweepSphere;
	sweepSphere.center = pos;
	sweepSphere.r = radius;
	float d = gEnv->pPhysicalWorld->PrimitiveWorldIntersection(sweepSphere.type, &sweepSphere, direction * TARGETDIST, PROBE_TYPES, NULL);

	bool fail = ((d > 0.0f) && (d < MINDIST));
	gEnv->pAuxGeomRenderer->DrawSphere(pos + (direction * d), radius, fail ? hitCol : clearCol);
	gEnv->pAuxGeomRenderer->DrawLine(pos, fromCol, pos + (direction * d), fail ? hitCol : clearCol);
	if (fail)
	{
		return false;
	}
	else if (d == 0.0f)
	{
		d = TARGETDIST;
	}

	sweepSphere.center = pos + (direction * (d - BUFFER));
	d = gEnv->pPhysicalWorld->PrimitiveWorldIntersection(sweepSphere.type, &sweepSphere, DOWN * MAXDROP, PROBE_TYPES, NULL);

	fail = (d == 0.0f);
	gEnv->pAuxGeomRenderer->DrawSphere(sweepSphere.center + (DOWN * d), radius, fail ? hitCol : clearCol);
	gEnv->pAuxGeomRenderer->DrawLine(sweepSphere.center, fromCol, sweepSphere.center + (DOWN * d), fail ? hitCol : clearCol);
	if (fail)
	{
		return false;
	}
	hitPos = sweepSphere.center + (DOWN * (d + radius));

	return true;
}

bool CGunTurret::NetAllowUpdate(bool requireActor)
{
	return !gEnv->bServer;
}

void CGunTurret::SetFaction(const uint8 factionID)
{
	m_turretparams.factionID = factionID;

	if (IAIObject* aiObject = GetEntity()->GetAI())
		aiObject->SetFactionID(factionID);
}

float CGunTurret::GetScaledDamage(float damage, int hitType) const
{
	static int explosionHitType = g_pGame->GetGameRules()->GetHitTypeId("explosion");
	static int fragHitType = g_pGame->GetGameRules()->GetHitTypeId("frag");

	assert(explosionHitType != 0);
	assert(fragHitType != 0);

	if (hitType == explosionHitType || hitType == fragHitType)
		damage *= m_turretparams.explosive_damage_multiplier;

	return damage;
}

void CGunTurret::CreateAIRepresentation()
{
	IEntity* entity = GetEntity();
	AIObjectParams params(AIOBJECT_TARGET, NULL, GetEntityId());
	gEnv->pAISystem->GetAIObjectManager()->CreateAIObject(params);

	if (IAIObject* aiObject = entity->GetAI())
	{
		aiObject->SetFactionID(m_turretparams.factionID);
	}
}
