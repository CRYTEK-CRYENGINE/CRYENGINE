// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   IMFXEffect.h
//  Version:     v1.00
//  Created:     28/11/2006 by JohnN/AlexL
//  Compilers:   Visual Studio.NET
//  Description: Decal effect
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef __MFXDECALEFFECT_H__
#define __MFXDECALEFFECT_H__

#pragma once

#include "MFXEffectBase.h"

struct SMFXDecalParams
{
	string material;
	float  minscale;
	float  maxscale;
	float  rotation;
	float  lifetime;
	float  growTime;
	bool   assemble;
	bool   forceedge;

	SMFXDecalParams()
		: minscale(0.0f)
		, maxscale(0.0f)
		, rotation(0.0f)
		, lifetime(0.0f)
		, growTime(0.0f)
		, assemble(false)
		, forceedge(false)
	{}
};

class CMFXDecalEffect :
	public CMFXEffectBase
{
public:
	CMFXDecalEffect();
	virtual ~CMFXDecalEffect();

	// IMFXEffect
	virtual void Execute(const SMFXRunTimeEffectParams& params) override;
	virtual void LoadParamsFromXml(const XmlNodeRef& paramsNode) override;
	virtual void GetResources(SMFXResourceList& resourceList) const override;
	virtual void PreLoadAssets() override;
	virtual void ReleasePreLoadAssets() override;
	virtual void GetMemoryUsage(ICrySizer* pSizer) const override;
	//~IMFXEffect

private:

	void ReleaseMaterial();

	SMFXDecalParams       m_decalParams;
	_smart_ptr<IMaterial> m_material;
};

#endif
