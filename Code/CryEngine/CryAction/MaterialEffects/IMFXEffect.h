// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   IMFXEffect.h
//  Version:     v1.00
//  Created:     28/11/2006 by JohnN/AlexL
//  Compilers:   Visual Studio.NET
//  Description: Virtual base class for all derived effects
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef __IMFXEFFECT_H__
#define __IMFXEFFECT_H__

#pragma once

#include <CryAction/IMaterialEffects.h>

typedef string TMFXNameId; // we use strings, and with clever assigning we minimize duplicates

struct IMFXEffect : public _reference_target_t
{
	virtual ~IMFXEffect() {};

	virtual void Execute(const SMFXRunTimeEffectParams& params) = 0;
	virtual void LoadParamsFromXml(const XmlNodeRef& paramsNode) = 0;
	virtual void SetCustomParameter(const char* customParameter, const SMFXCustomParamValue& customParameterValue) = 0;
	virtual void GetResources(SMFXResourceList& resourceList) const = 0;
	virtual void PreLoadAssets() = 0;
	virtual void ReleasePreLoadAssets() = 0;
	virtual void GetMemoryUsage(ICrySizer* pSizer) const = 0;
};

#endif
