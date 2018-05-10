// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SprintStamina.h"

#include <IItemSystem.h>	// For IItemParamsNode
#include "GameConstantCVars.h"

CSprintStamina::Settings CSprintStamina::m_settings;

CSprintStamina::CSprintStamina()
	:	m_stamina(1.0f)
	, m_staminaRegenDelay(0.0f)
	, m_sprintingTime(0.0f)
	, m_staminaHasChanged(false)
	, m_staminaReducedPrevUpdate(false)
{
	m_runBreathingLoop.SetSignal( "PlayerFeedback_Run" );
}

void CSprintStamina::LoadSettings( const IItemParamsNode* pParamsNode )
{

#define ReadStaminaAttribute( name ) \
	pSettingsNode->GetAttribute( #name, m_settings.m_##name)

	if(pParamsNode != NULL)
	{
		const IItemParamsNode* pSettingsNode = pParamsNode->GetChild( "Settings" );
		if (pSettingsNode != NULL)
		{
			ReadStaminaAttribute( depleteRatePerSec );
			ReadStaminaAttribute( regenDelaySecs );
			ReadStaminaAttribute( regenDelayExtremeSecs );
			ReadStaminaAttribute( regenRatePerSec );
			ReadStaminaAttribute( breatheSoundStaminaThreshold );
			ReadStaminaAttribute( minimumSprintingTimeToPlayStopSound );
		}
	}

#undef  ReadStaminaAttribute

}

void CSprintStamina::Reset( const EntityId playerId )
{
	if (m_stamina < 1.f)
	{
		m_staminaHasChanged = true;
	}

	m_stamina = 1.0f;
	m_staminaRegenDelay = 0.0f;
	m_staminaReducedPrevUpdate = false;
	m_sprintingTime = 0.0f;

	/*if (playerId != 0)
	{
		m_runBreathingLoop.Stop( playerId );
	}*/
}

void CSprintStamina::Update( const CSprintStamina::UpdateContext& updateCtx )
{
	m_staminaHasChanged = false;

	if (updateCtx.sprinting)
	{
		if (updateCtx.localClient)
		{
			if (m_staminaReducedPrevUpdate && (m_stamina < m_settings.m_breatheSoundStaminaThreshold))
			{
				/*if(!m_runBreathingLoop.IsPlaying( updateCtx.playerId ))
				{
					m_runBreathingLoop.Play( updateCtx.playerId );
				}*/
				const float soundStamina = clamp_tpl(1.0f - ((m_settings.m_breatheSoundStaminaThreshold - m_stamina) / m_settings.m_breatheSoundStaminaThreshold), 0.0f, 1.0f);
				m_runBreathingLoop.SetParam( updateCtx.playerId, "stamina", soundStamina);
			}
		}

		if(GetGameConstCVar(g_infiniteSprintStamina))
		{
			m_stamina = 1.0f;
		}
		else
		{
			m_stamina -= min(m_stamina, (updateCtx.frameTime * updateCtx.consumptionScale * m_settings.m_depleteRatePerSec));
			m_staminaHasChanged = true;
		}

		m_staminaRegenDelay = 0.f;
		m_staminaReducedPrevUpdate = true;
		m_sprintingTime += updateCtx.frameTime;
	}
	else
	{
		if (m_staminaReducedPrevUpdate)
		{
			if (updateCtx.localClient && (m_stamina < m_settings.m_breatheSoundStaminaThreshold))
			{
				//m_runBreathingLoop.Stop( updateCtx.playerId );
			
				const bool outOfStamina = (m_stamina == 0.0f);
				if(outOfStamina)
				{
					CAudioSignalPlayer::JustPlay( "PlayerFeedback_OutOfStamina", updateCtx.playerId);
				}
				else if(m_sprintingTime > m_settings.m_minimumSprintingTimeToPlayStopSound)
				{
					CAudioSignalPlayer::JustPlay( "PlayerFeedback_StopRun", updateCtx.playerId);
				}
			}

			m_staminaRegenDelay = (float)__fsel(-m_stamina, m_settings.m_regenDelayExtremeSecs,  m_settings.m_regenDelaySecs);
		}

		if (m_staminaRegenDelay > 0.f)
		{
			m_staminaRegenDelay -= min(updateCtx.frameTime, m_staminaRegenDelay);
		}
		else if (m_stamina < 1.f)
		{
			m_stamina += min((updateCtx.frameTime * m_settings.m_regenRatePerSec), (1.f - m_stamina));
			m_staminaHasChanged = true;
		}

		m_staminaReducedPrevUpdate = false;
		m_sprintingTime = 0.0f;
	}
}

#ifdef DEBUG_SPRINT_STAMINA

#include "GameCVars.h"
#include "Utility/CryWatch.h"

void CSprintStamina::Debug( const IEntity* pEntity )
{
	if (g_pGameCVars->pl_movement.sprintStamina_debug)
	{
		CRY_ASSERT(pEntity != NULL);

		CryWatch("Sprint Stamina: \"%s\": stamina = %.2f, regen delay = %.2f, reduced prev = %d", pEntity->GetName(), m_stamina, m_staminaRegenDelay, m_staminaReducedPrevUpdate);
	}
}

#endif