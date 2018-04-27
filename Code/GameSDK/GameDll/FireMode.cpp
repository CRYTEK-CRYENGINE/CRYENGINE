// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Basic Fire Mode Implementation

*************************************************************************/

#include "StdAfx.h"
#include "FireMode.h"

#include "Weapon.h"
#include "WeaponSharedParams.h"
#include "WeaponSystem.h"
#include "FireModePlugin.h"
#include "Actor.h"

#include "Player.h"

CRY_IMPLEMENT_GTI_BASE(CFireMode);

CFireMode::CFireMode() 
: m_pWeapon(NULL)
, m_fireParams(NULL)
, m_parentFireParams(NULL)
, m_enabled(true)
, m_accessoryEnabled(false)
, m_listeners(5)
{

}

CFireMode::~CFireMode()
{
	if(g_pGame)
	{
		CWeaponSystem* pWeaponSystem = g_pGame->GetWeaponSystem();

		const int numPlugins = m_plugins.size();

		for (int i = 0; i < numPlugins; i++)
		{
			m_plugins[i]->Activate(false);
			pWeaponSystem->DestroyFireModePlugin(m_plugins[i]);
		}

		m_plugins.clear();

		for (TFireModeListeners::Notifier notifier(m_listeners); notifier.IsValid(); notifier.Next())
		{
			notifier->OnFireModeDeleted();
		}
		m_listeners.Clear();
	}
}

void CFireMode::InitFireMode( IWeapon* pWeapon, const SParentFireModeParams* pParams)
{
	CRY_ASSERT_MESSAGE(pParams, "Fire Mode Params NULL! Have you set up the weapon xml correctly?");
	CRY_ASSERT_MESSAGE(pParams->pBaseFireMode, "Fire Mode Base Params NULL!");

	m_pWeapon = static_cast<CWeapon *>(pWeapon);
	m_fireParams = pParams->pBaseFireMode;
	m_parentFireParams = pParams;

	ResetParams();
}

void CFireMode::Release()
{
	if (g_pGame)
	{
		g_pGame->GetWeaponSystem()->DestroyFireMode(this);
	}
}

void CFireMode::ResetParams()
{
	CWeaponSystem* pWeaponSystem = g_pGame->GetWeaponSystem();
	const int numPluginParams = m_fireParams->pluginParams.size();

	TFireModePluginVector currentPlugins = m_plugins;

	m_plugins.clear();
	m_plugins.reserve(numPluginParams);

	for(int i = 0; i < numPluginParams; i++)
	{
		if(IFireModePluginParams* pPluginParams = m_fireParams->pluginParams[i])
		{
			IFireModePlugin* pPlugin = NULL;
			const CGameTypeInfo* pluginType = pPluginParams->GetPluginType();

			const int currentNumPlugins = currentPlugins.size();

			for(int a = 0; a < currentNumPlugins; a++)
			{
				if(currentPlugins[a]->GetRunTimeType() == pluginType)
				{
					pPlugin = currentPlugins[a];

					currentPlugins.erase(currentPlugins.begin() + a);
					break;
				}
			}

			if(!pPlugin)
			{
				pPlugin = pWeaponSystem->CreateFireModePlugin(pluginType);
			}

			if(pPlugin && pPlugin->Init(this, pPluginParams))
			{
				m_plugins.push_back(pPlugin);
			}
			else
			{
				GameWarning("Firemode Plugin creation and initialisation failed. This weapon may be missing functionality");
				
				if(pPlugin)
				{
					pWeaponSystem->DestroyFireModePlugin(pPlugin);
				}
			}
		}
	}

	const int numToDelete = currentPlugins.size();

	for(int i = 0; i < numToDelete; i++)
	{
		currentPlugins[i]->Activate(false);
		pWeaponSystem->DestroyFireModePlugin(currentPlugins[i]);
	}
}

void CFireMode::Update(float frameTime, uint32 frameId)
{
	bool keepUpdating = false;

	const int numPlugins = m_plugins.size();

	for (int i = 0; i < numPlugins; i++)
	{
		keepUpdating |= m_plugins[i]->Update(frameTime, frameId);
	}

	if (keepUpdating)
	{
		m_pWeapon->RequireUpdate(eIUS_FireMode);
	}
}

void CFireMode::Activate(bool activate)
{
	const int numPlugins = m_plugins.size();

	for (int i = 0; i < numPlugins; i++)
	{
		m_plugins[i]->Activate(activate);
	}

	for (TFireModeListeners::Notifier notifier(m_listeners); notifier.IsValid(); notifier.Next())
	{
		notifier->OnFireModeActivated(activate);
	}

	if (GetWeapon()->GetOwnerId())
	{
		if (m_pWeapon->IsSelected())
		{
			UpdateMannequinTags(activate);
		}
	}
	else
	{
		UpdateMannequinTags(activate);
	}
}

void CFireMode::UpdateMannequinTags(bool enable)
{
	IActionController* pActionController = m_pWeapon->GetActionController();
	if (pActionController)
	{
		const uint32 firemodeCrC = CCrc32::ComputeLowercase(m_fireParams->fireparams.tag.c_str());
		if (firemodeCrC)
		{
			SAnimationContext& animationContext = pActionController->GetContext();
			animationContext.state.SetByCRC(firemodeCrC, enable);
		}
	}
}

void CFireMode::PatchSpreadMod( const SSpreadModParams &sSMP, float modMultiplier )
{

}

void CFireMode::ResetSpreadMod()
{

}

void CFireMode::PatchRecoilMod( const SRecoilModParams &sRMP, float modMultiplier )
{

}

void CFireMode::ResetRecoilMod()
{

}

bool CFireMode::CanZoom() const
{
	return true;
}

float CFireMode::GetRange() const
{
	return 0.f;
}

bool CFireMode::FillAmmo(bool fromInventory)
{
	return false;
}

int CFireMode::GetClipSize() const
{
	return 0;
}

int CFireMode::GetChamberSize() const
{
	return 0;
}

void CFireMode::OnEnterFirstPerson()
{

}

void CFireMode::GetMemoryUsage( ICrySizer * pSizer ) const
{
	pSizer->AddObject(this, sizeof(*this));
	GetInternalMemoryUsage(pSizer);
}

void CFireMode::GetInternalMemoryUsage( ICrySizer * pSizer ) const
{
	pSizer->AddObject(m_pWeapon);
	pSizer->AddObject(m_fireParams);
	pSizer->AddObject(m_parentFireParams);
	pSizer->AddObject(m_name);
}

void CFireMode::SetProjectileSpeedScale( float fSpeedScale )
{
}

bool CFireMode::PluginsAllowFire() const
{
	const int numPlugins = m_plugins.size();

	for (int i = 0; i < numPlugins; i++)
	{
		if(!m_plugins[i]->AllowFire())
		{
			return false;
		}
	}

	return true;
}

void CFireMode::OnShoot(EntityId shooterId, EntityId ammoId, IEntityClass* pAmmoType, const Vec3 &pos, const Vec3 &dir, const Vec3 &vel)
{
	m_pWeapon->OnShoot(shooterId, ammoId, pAmmoType, pos, dir, vel);

	const int numPlugins = m_plugins.size();

	for (int i = 0; i < numPlugins; i++)
	{
		m_plugins[i]->OnShoot();
	}
}

void CFireMode::OnReplayShoot()
{
	const int numPlugins = m_plugins.size();

	for (int i = 0; i < numPlugins; i++)
	{
		m_plugins[i]->OnReplayShoot();
	}
}


void CFireMode::AlterFiringDirection(const Vec3& firingPos, Vec3& rFiringDir) const
{
	const int numPlugins = m_plugins.size();
	for (int i = 0; i < numPlugins; i++)
	{
		m_plugins[i]->AlterFiringDirection(firingPos, rFiringDir);
	}
}

void CFireMode::OnSpawnProjectile(CProjectile* pAmmo)
{
}

const IFireModePlugin* CFireMode::FindPluginOfType(const CGameTypeInfo* pluginType) const
{
	const int numPlugins = m_plugins.size();

	for (int i = 0; i < numPlugins; i++)
	{
		if(m_plugins[i]->GetRunTimeType() == pluginType)
		{
			return m_plugins[i];
		}
	}

	return NULL;
}

bool CFireMode::CanOverheat() const
{
	return (FindPluginOfType(CFireModePlugin_Overheat::GetStaticType()) != NULL);
}

float CFireMode::GetHeat() const
{
	const CFireModePlugin_Overheat* pHeatPlugin = crygti_cast<const CFireModePlugin_Overheat*>(FindPluginOfType(CFireModePlugin_Overheat::GetStaticType()));

	if(pHeatPlugin)
	{
		return pHeatPlugin->GetHeat();
	}

	return 0.f;
}

float CFireMode::GetTimeBetweenShotsMultiplier(const CPlayer* pPlayer) const
{
	if(gEnv->bMultiplayer)
	{
		return pPlayer->GetModifiableValues().GetValue(kPMV_WeaponTimeBetweenShotsMultiplier);
	}

	return 1.f;
}
