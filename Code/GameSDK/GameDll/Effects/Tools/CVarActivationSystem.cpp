// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// Simple data driven system to activate cvars.

// Includes
#include "StdAfx.h"
#include "CVarActivationSystem.h"
#include "IItemSystem.h"
#include "Effects/GameEffectsSystem.h"

//--------------------------------------------------------------------------------------------------
// Name: Initialise
// Desc: Initialises cvar activation system from data
//       Uses the xml node name for the cvar, and activeValue attribute
//			 eg <cl_fov activeValue="85"/>
//--------------------------------------------------------------------------------------------------
void CCVarActivationSystem::Initialise(const IItemParamsNode* cvarListXmlNode)
{
	if(cvarListXmlNode)
	{
		const IItemParamsNode* cvarXmlNode = NULL;
		SCVarParam* param = NULL;
		int cvarCount = cvarListXmlNode->GetChildCount();
		m_cvarParam.resize(cvarCount);
		for(int i=0; i<cvarCount; i++)
		{
			param = &m_cvarParam[i];
			cvarXmlNode = cvarListXmlNode->GetChild(i);
			param->cvar = gEnv->pConsole->GetCVar(cvarXmlNode->GetName());
			FX_ASSERT_MESSAGE(param->cvar,"Failed to find a CVAR for a game effect");
			cvarXmlNode->GetAttribute("activeValue",param->activeValue);
			param->originalValue = 0.0f;
		}
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: Release
// Desc: Releases data used for CVar activation system
//--------------------------------------------------------------------------------------------------
void CCVarActivationSystem::Release()
{
	m_cvarParam.Free();
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: StoreCurrentValues
// Desc: Stores current values of CVars
//--------------------------------------------------------------------------------------------------
void CCVarActivationSystem::StoreCurrentValues()
{
	SCVarParam* param = NULL;
	for(uint32 i=0; i<m_cvarParam.Size(); i++)
	{
		param = &m_cvarParam[i];

		if(param->cvar)
		{
			param->originalValue = param->cvar->GetFVal();
		}
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: SetCVarsActive
// Desc: Sets active status of cvars
//--------------------------------------------------------------------------------------------------
void CCVarActivationSystem::SetCVarsActive(bool isActive)
{
	SCVarParam* param = NULL;
	float value = 0.0f;
	for(uint32 i=0; i<m_cvarParam.Size(); i++)
	{
		param = &m_cvarParam[i];

		if(param->cvar)
		{
			if(isActive)
			{
				param->cvar->Set(param->activeValue);
			}
			else
			{
				param->cvar->Set(param->originalValue);
			}
		}
	}
}//-------------------------------------------------------------------------------------------------
