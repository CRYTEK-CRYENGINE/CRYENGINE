// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _HUD_INTERFERENCE_GAME_EFFECT_
#define _HUD_INTERFERENCE_GAME_EFFECT_

#pragma once

// Includes
#include "GameEffect.h"
#include "Effects/GameEffectsSystem.h"

//==================================================================================================
// Name: CHudInterferenceGameEffect
// Desc: Manages hud interference - The effect needs to be managed in 1 global place to stop
//		   different game features fighting over setting the values
// Author: James Chilvers
//==================================================================================================
class CHudInterferenceGameEffect : public CGameEffect
{
public:
	CHudInterferenceGameEffect();
	~CHudInterferenceGameEffect();

	virtual void	Initialise(const SGameEffectParams* gameEffectParams = NULL) override;
	virtual void	Update(float frameTime) override;

	virtual const char* GetName() const override { return "Hud interference"; }
	virtual void ResetRenderParameters() override;

	// These need to be called every frame for it to take affect
	void					SetInterference(float interferenceScale,bool bInterferenceFilter);

private:

	Vec4		m_defaultInterferenceParams;
	float		m_interferenceScale;
	uint8		m_interferenceFilterFlags;

};//------------------------------------------------------------------------------------------------

#endif // _HUD_INTERFERENCE_GAME_EFFECT_
