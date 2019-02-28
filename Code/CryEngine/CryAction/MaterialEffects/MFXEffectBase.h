// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// ----------------------------------------------------------------------------------------
//  File name:   MFXEffectBase.h
//  Description: Basic and partial implemenation of IMFXEffect which serves as a base for concrete implementations
//
////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _MFX_EFFECT_BASE_H_
#define _MFX_EFFECT_BASE_H_

#pragma once

#include <CryCore/CryFlags.h>
#include "IMFXEffect.h"

class CMFXEffectBase : public IMFXEffect
{
public:

	CMFXEffectBase(const uint16 _typeFilterFlag);

	//IMFXEffect (partial implementation)
	virtual void SetCustomParameter(const char* customParameter, const SMFXCustomParamValue& customParameterValue) override;
	virtual void PreLoadAssets() override;
	virtual void ReleasePreLoadAssets() override;
	//~IMFXEffect

	bool CanExecute(const SMFXRunTimeEffectParams& params) const;

private:
	CCryFlags<uint16> m_runtimeExecutionFilter;
};

typedef _smart_ptr<CMFXEffectBase> TMFXEffectBasePtr;

#endif // _MFX_EFFECT_BASE_H_
