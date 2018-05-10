// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:	CVars for the MaterialEffects subsystem
   -------------------------------------------------------------------------
   History:
   - 02:03:2006  12:00 : Created by AlexL

*************************************************************************/

#include "StdAfx.h"
#include "MaterialEffectsCVars.h"
#include <CryAction.h>

#include "MaterialEffects.h"
#include "MaterialFGManager.h"

CMaterialEffectsCVars* CMaterialEffectsCVars::s_pThis = 0;

namespace
{
void FGEffectsReload(IConsoleCmdArgs* pArgs)
{
	CMaterialEffects* pMaterialFX = static_cast<CMaterialEffects*>(CCryAction::GetCryAction()->GetIMaterialEffects());

	if (pMaterialFX)
	{
		pMaterialFX->GetFGManager()->ReloadFlowGraphs();
	}
}

void MFXReload(IConsoleCmdArgs* pArgs)
{
	CMaterialEffects* pMaterialEffects = static_cast<CMaterialEffects*>(CCryAction::GetCryAction()->GetIMaterialEffects());

	if (pMaterialEffects)
	{
		pMaterialEffects->FullReload();
	}
}

};

CMaterialEffectsCVars::CMaterialEffectsCVars()
{
	assert(s_pThis == 0);
	s_pThis = this;

	REGISTER_CVAR(mfx_ParticleImpactThresh, 2.0f, VF_CHEAT, "Impact threshold for particle effects. Default: 2.0");
	REGISTER_CVAR(mfx_SoundImpactThresh, 1.5f, VF_CHEAT, "Impact threshold for sound effects. Default: 1.5");
	REGISTER_CVAR(mfx_RaisedSoundImpactThresh, 3.5f, VF_CHEAT, "Impact threshold for sound effects if we're rolling. Default: 3.5");
	REGISTER_CVAR(mfx_Debug, 0, 0, "Turns on MaterialEffects debug messages. 1=Collisions, 2=Breakage, 3=Both");
	REGISTER_CVAR(mfx_DebugVisual, 0, 0, "Turns on/off visual debugging for MFX system");
	mfx_DebugVisualFilter = REGISTER_STRING("mfx_DebugVisualFilter", "", VF_CHEAT, "");
	REGISTER_CVAR(mfx_DebugFlowGraphFX, 0, 0, "Turns on Material FlowGraph FX manager debug messages.");
	REGISTER_CVAR(mfx_Enable, 1, VF_CHEAT, "Enables MaterialEffects.");
	REGISTER_CVAR(mfx_pfx_minScale, .5f, 0, "Min scale (when particle is close)");
	REGISTER_CVAR(mfx_pfx_maxScale, 1.5f, 0, "Max scale (when particle is far)");
	REGISTER_CVAR(mfx_pfx_maxDist, 35.0f, 0, "Max dist (how far away before scale is clamped)");
	REGISTER_CVAR(mfx_Timeout, 0.01f, 0, "Timeout (in seconds) to avoid playing effects too often");
	REGISTER_CVAR(mfx_EnableFGEffects, 1, VF_CHEAT, "Enabled Flowgraph based Material effects. Default: On");
	REGISTER_CVAR(mfx_EnableAttachedEffects, 1, VF_CHEAT, "Enable attached effects (characters, entities...)");
	REGISTER_CVAR(mfx_SerializeFGEffects, 1, VF_CHEAT, "Serialize Flowgraph based effects. Default: On");

	//FlowGraph HUD effects
	REGISTER_COMMAND("mfx_ReloadFGEffects", FGEffectsReload, 0, "Reload MaterialEffect's FlowGraphs");
	//Reload Excel Spreadsheet
	REGISTER_COMMAND("mfx_Reload", MFXReload, 0, "Reload MFX Spreadsheet");
}

CMaterialEffectsCVars::~CMaterialEffectsCVars()
{
	assert(s_pThis != 0);
	s_pThis = 0;

	IConsole* pConsole = gEnv->pConsole;

	pConsole->RemoveCommand("mfx_Reload");
	pConsole->RemoveCommand("mfx_ReloadHUDEffects");

	pConsole->UnregisterVariable("mfx_ParticleImpactThresh", true);
	pConsole->UnregisterVariable("mfx_SoundImpactThresh", true);
	pConsole->UnregisterVariable("mfx_RaisedSoundImpactThresh", true);
	pConsole->UnregisterVariable("mfx_Debug", true);
	pConsole->UnregisterVariable("mfx_DebugVisual", true);
	pConsole->UnregisterVariable("mfx_DebugVisualFilter", true);
	pConsole->UnregisterVariable("mfx_Enable", true);
	pConsole->UnregisterVariable("mfx_pfx_minScale", true);
	pConsole->UnregisterVariable("mfx_pfx_maxScale", true);
	pConsole->UnregisterVariable("mfx_pfx_maxDist", true);
	pConsole->UnregisterVariable("mfx_Timeout", true);
	pConsole->UnregisterVariable("mfx_EnableFGEffects", true);
	pConsole->UnregisterVariable("mfx_EnableAttachedEffects", true);
	pConsole->UnregisterVariable("mfx_SerializeFGEffects", true);
}
