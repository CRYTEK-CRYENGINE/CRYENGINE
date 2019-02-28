// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PlayerPlugin.h"
#include "Player.h"
#include "GameCVars.h"
#include "PlayerPluginEventDistributor.h"

#if PLAYER_PLUGIN_DEBUGGING
AUTOENUM_BUILDNAMEARRAY(CPlayerPlugin::s_playerPluginEventNames, PlayerPluginEventList);
#endif

CPlayerPlugin::CPlayerPlugin()
{
	m_ownerPlayer = NULL;
	m_entered = false;
}

CPlayerPlugin::~CPlayerPlugin()
{
	if (m_ownerPlayer)
	{
		CRY_ASSERT_MESSAGE(!m_entered, ("[PLAYER PLUG-IN] <%s %s \"%s\"> %s", m_ownerPlayer->IsClient() ? "Local" : "Remote", m_ownerPlayer->GetEntity()->GetClass()->GetName(), m_ownerPlayer->GetEntity()->GetName(), string().Format("Player plug-in is being destroyed without having been shut down cleanly!").c_str()));
	}
	else
	{
		CRY_ASSERT_MESSAGE(!m_entered, ("[PLAYER PLUG-IN] <NULL> %s", string().Format("Player plug-in is being destroyed without having been shut down cleanly!").c_str()));
	}
}

void CPlayerPlugin::SetIsNowActive(bool nowOn)
{
	PlayerPluginAssert (m_entered, "Shouldn't activate/deactivate a player plug-in which hasn't been entered!");
	InformActiveHasChanged();
}

void CPlayerPlugin::Enter( CPlayerPluginEventDistributor* pEventDist )
{
	PlayerPluginLog ("Entering");
	PlayerPluginAssert(m_entered == false, "Entering a player plug-in which is entered already!");
	
	m_entered = true;

	HandleEvent(EPE_Reset);
}

void CPlayerPlugin::Leave()
{
	CPlayerPluginEventDistributor* pEventDist = m_ownerPlayer->GetPlayerPluginEventDistributor();
	pEventDist->UnregisterReceiver(this);

	PlayerPluginLog ("Leaving");
	PlayerPluginAssert(m_entered == true, "Leaving a player plug-in which is not entered!");
	SetIsNowActive(false);
	m_entered = false;
}

void CPlayerPlugin::HandleEvent(EPlayerPlugInEvent theEvent, void * data)
{
}

const void* CPlayerPlugin::DefaultData(EPlayerPlugInData dataType)
{
	const static bool  m_defaultBool  = false;
	const static float m_defaultFloat = 0.0f;
	const static float m_defaultFloatOne = 1.0f;
	const static int   m_defaultInt = 0;

	switch(dataType)
	{
		case EPD_ProximityDistance:
		case EPD_PhantomTimeToKill:
		case EPD_SilentFeetRange:
		case EPD_ECMRange:
		case EPD_CloakAwarenessTimeFromUnCloak:
		case EPD_ElectronicInterferenceAmount:
		case EPD_CounterMeasuresEffectAmount:
		case EPD_GetCloakTimeDelay:
		case EPD_SuperChargeTime:
		{
			return &m_defaultFloat;
		}
		case EPD_GetCloakBlendSpeedScale:
		{
			return &m_defaultFloatOne;
		}
		case EPD_Stamp:
		case EPD_MuteFootsteps:
		case EPD_MuteJumping:
		case EPD_SuitBoostActive:
		case EPD_IgnoreRadarJammed:
		{
			return &m_defaultBool;
		}
		case EPD_SuperChargesCounter:
		{
			return &m_defaultInt;
		}
	}

	CRY_ASSERT_MESSAGE(false, "We always want to provide default data");
	return NULL;
}

