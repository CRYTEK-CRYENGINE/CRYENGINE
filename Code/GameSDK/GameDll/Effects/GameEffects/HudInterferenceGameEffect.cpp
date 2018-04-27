// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// Manages hud interference - The effect needs to be managed in 1 global place to stop
// different game features fighting over setting the values.

// Includes
#include "StdAfx.h"
#include "HudInterferenceGameEffect.h"

//==================================================================================================
// Name: EInterferenceFilterFlags
// Desc: Interference filter flags
// Author: James Chilvers
//==================================================================================================
enum EInterferenceFilterState
{
	eINTERFERENCE_FILTER_NotSet					= 0,
	eINTERFERENCE_FILTER_True						=	(1<<0),
	eINTERFERENCE_FILTER_False					= (1<<1),
};//------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: CHudInterferenceGameEffect
// Desc: Constructor
//--------------------------------------------------------------------------------------------------
CHudInterferenceGameEffect::CHudInterferenceGameEffect()
{
	m_defaultInterferenceParams = Vec4(0.0f,0.0f,0.0f,0.0f);
	m_interferenceScale = 0.0f;
	m_interferenceFilterFlags = eINTERFERENCE_FILTER_NotSet;
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: ~CHudInterferenceGameEffect
// Desc: Destructor
//--------------------------------------------------------------------------------------------------
CHudInterferenceGameEffect::~CHudInterferenceGameEffect()
{
	
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: Initialise
// Desc: Initializes game effect
//--------------------------------------------------------------------------------------------------
void CHudInterferenceGameEffect::Initialise(const SGameEffectParams* gameEffectParams)
{
	CGameEffect::Initialise(gameEffectParams);

	gEnv->p3DEngine->GetPostEffectParamVec4("HUD3D_InterferenceParams0",m_defaultInterferenceParams);
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: SetInterference
// Desc: Sets interference params
//--------------------------------------------------------------------------------------------------
void CHudInterferenceGameEffect::SetInterference(float interferenceScale,bool bInterferenceFilter)
{
	Limit(interferenceScale,0.0f,1.0f);
	m_interferenceScale = max(m_interferenceScale,interferenceScale);
	if(m_interferenceScale > 0.0f)
	{
		SetActive(true);
	}

	if(bInterferenceFilter)
	{
		m_interferenceFilterFlags |= eINTERFERENCE_FILTER_True;
	}
	else
	{
		m_interferenceFilterFlags |= eINTERFERENCE_FILTER_False;
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: Update
// Desc: Updates interference effect
//--------------------------------------------------------------------------------------------------
void CHudInterferenceGameEffect::Update(float frameTime)
{
	// Set interference values
	const bool bForceValue = true;
	gEnv->p3DEngine->SetPostEffectParam( "HUD3D_Interference", m_interferenceScale, bForceValue );

	Vec4 interferenceParams(m_defaultInterferenceParams);
	if(m_interferenceFilterFlags != eINTERFERENCE_FILTER_NotSet)
	{
		if(IS_FLAG_SET(m_interferenceFilterFlags,eINTERFERENCE_FILTER_True) &&
			 !IS_FLAG_SET(m_interferenceFilterFlags,eINTERFERENCE_FILTER_False))
		{
			interferenceParams.x = 0.0f; // Filter applied
		}
	}

	gEnv->p3DEngine->SetPostEffectParamVec4( "HUD3D_InterferenceParams0", interferenceParams, bForceValue);

	// Reset values
	if(m_interferenceScale == 0.0f)
	{
		SetActive(false);
	}
	else
	{
		m_interferenceScale = 0.0f;
	}
	m_interferenceFilterFlags = eINTERFERENCE_FILTER_NotSet;
}//-------------------------------------------------------------------------------------------------

void CHudInterferenceGameEffect::ResetRenderParameters()
{
	gEnv->p3DEngine->SetPostEffectParam( "HUD3D_Interference", 0.0f, true );
}
