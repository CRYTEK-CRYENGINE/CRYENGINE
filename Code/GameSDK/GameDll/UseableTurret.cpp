// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GameActions.h"
#include "Player.h"
#include "PlayerInput.h"
#include "UseableTurret.h"
#include "WeaponSystem.h"
#include "Projectile.h"



namespace
{


	bool ShouldFilterAction(const ActionId& actionId)
	{
		const CGameActions& actions = g_pGame->Actions();

		const ActionId* actionsToFilter[] =
		{
			&actions.attack2_xi,
			&actions.weapon_change_firemode,
			&actions.use,
			&actions.heavyweaponremove,
			&actions.special,
			&actions.zoom_in,
			&actions.zoom_out,
			&actions.zoom,
		};
		const int numEvents = CRY_ARRAY_COUNT(actionsToFilter);

		for (int i = 0; i < numEvents; ++i)
			if (actionId == *actionsToFilter[i])
				return true;

		return false;
	}


}



CUseableTurret::CUseableTurret()
	:	m_currentMode(ECUTFM_Rapid)
	,	m_lockedEntity(0)
{
}



void CUseableTurret::OnAction(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	if (!ShouldFilterAction(actionId))
		BaseClass::OnAction(actorId, actionId, activationMode, value);

	const CGameActions& actions = g_pGame->Actions();
	if (actionId == actions.toggle_weapon && activationMode == eAAM_OnPress)
		SetFiringMode(GetNextMode());
	else if (actionId == actions.previtem && activationMode == eAAM_OnPress)
		SetFiringMode(ECUTFM_Rockets);
	else if (actionId == actions.nextitem && activationMode == eAAM_OnPress)
		SetFiringMode(ECUTFM_Rapid);
}



void CUseableTurret::Select(bool select)
{
	if (select)
		SetFiringMode(ECUTFM_Rapid);

	CActor* pOwner = GetOwnerActor();
	if (pOwner && pOwner->IsPlayer())
	{
		CPlayer* pLocalPlayer = static_cast<CPlayer*>(pOwner);
		g_pGameActions->FilterItemPickup()->Enable(select);
		if (select)
			CenterPlayerView(pLocalPlayer);
	}

	BaseClass::Select(select);
}



bool CUseableTurret::CanRipOff() const
{
	return false;
}



bool CUseableTurret::CanUse(EntityId userId) const
{
	return true;
}



void CUseableTurret::SetFiringMode(CUseableTurret::ECUTFiringMode mode)
{
	if (!m_zm)
		return;

	int zoomStage = 1;
	int fireMode = 0;
	if (mode == ECUTFM_Rapid)
		zoomStage = 2;
	if (mode == ECUTFM_Rockets)
		fireMode = 1;

	if (!m_zm->IsZoomed())
		m_zm->StartZoom(true, true, zoomStage);
	else if (m_zm->GetCurrentStep() < zoomStage)
		m_zm->ZoomIn();
	else if (m_zm->GetCurrentStep() > zoomStage)
		m_zm->ZoomOut();

	SetCurrentFireMode(fireMode);

	m_currentMode = mode;
	m_zoomTimeMultiplier = 1.0f;
}



CUseableTurret::ECUTFiringMode CUseableTurret::GetNextMode() const
{
	if (m_currentMode == ECUTFM_Rapid)
		return ECUTFM_Rockets;
	return ECUTFM_Rapid;
}



void CUseableTurret::CenterPlayerView(CPlayer* pPlayer)
{
	Quat currentWeaponRotation = GetEntity()->GetWorldRotation();
	Vec3 xAxis = currentWeaponRotation.GetColumn0();

	float minAngle = m_properties.mounted_min_pitch;
	float maxAngle = m_properties.mounted_max_pitch;
	const float startAngle = 0.2f;
	float angle = DEG2RAD(LERP(minAngle, maxAngle, startAngle));

	pPlayer->SetViewRotation(currentWeaponRotation * Quat::CreateRotationAA(angle, xAxis));
}




void CUseableTurret::SetLockedEntity(EntityId lockedEntity)
{
	m_lockedEntity = lockedEntity;
}



float CUseableTurret::GetZoomTimeMultiplier()
{
	const float firstTimeZoomMult = 5.0f;
	if (!IsSelected())
		return firstTimeZoomMult;
	return 1.0f;
}



void CUseableTurret::OnShoot(EntityId shooterId, EntityId ammoId, IEntityClass* pAmmoType, const Vec3 &pos, const Vec3 &dir, const Vec3 &vel)
{
	BaseClass::OnShoot(shooterId, ammoId, pAmmoType, pos, dir, vel);
	if (m_lockedEntity && m_currentMode == ECUTFM_Rockets)
	{
		CProjectile* pProjectile = g_pGame->GetWeaponSystem()->GetProjectile(ammoId);
		pProjectile->SetDestination(m_lockedEntity);
	}
}
