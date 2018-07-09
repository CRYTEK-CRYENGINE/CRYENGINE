// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================
   PostProcessEffects.cpp : 3d engine post processing acess/scripts interfaces

   Revision history:
* 23/02/2005: Re-factored/Converted to CryEngine 2.0 by Tiago Sousa
* Created by Tiago Sousa

   Notes:
* Check PostEffects.h for list of available effects

   =============================================================================*/

#include "StdAfx.h"

void C3DEngine::SetPostEffectParam(const char* pParam, float fValue, bool bForceValue) const
{
	if (pParam && GetRenderer())
		GetRenderer()->EF_SetPostEffectParam(pParam, fValue, bForceValue);
}

void C3DEngine::GetPostEffectParam(const char* pParam, float& fValue) const
{
	if (pParam && GetRenderer())
		GetRenderer()->EF_GetPostEffectParam(pParam, fValue);
}

void C3DEngine::SetPostEffectParamVec4(const char* pParam, const Vec4& pValue, bool bForceValue) const
{
	if (pParam && GetRenderer())
		GetRenderer()->EF_SetPostEffectParamVec4(pParam, pValue, bForceValue);
}

void C3DEngine::GetPostEffectParamVec4(const char* pParam, Vec4& pValue) const
{
	if (pParam && GetRenderer())
		GetRenderer()->EF_GetPostEffectParamVec4(pParam, pValue);
}

void C3DEngine::SetPostEffectParamString(const char* pParam, const char* pszArg) const
{
	if (pParam && pszArg && GetRenderer())
		GetRenderer()->EF_SetPostEffectParamString(pParam, pszArg);
}

void C3DEngine::GetPostEffectParamString(const char* pParam, const char*& pszArg) const
{
	if (pParam && GetRenderer())
		GetRenderer()->EF_GetPostEffectParamString(pParam, pszArg);
}

int32 C3DEngine::GetPostEffectID(const char* pPostEffectName)
{
	return GetRenderer() ? GetRenderer()->EF_GetPostEffectID(pPostEffectName) : 0;
}

void C3DEngine::ResetPostEffects(bool bOnSpecChange) const
{
	if (GetRenderer())
	{
		GetRenderer()->EF_ResetPostEffects(bOnSpecChange);
	}
}
