// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

// -------------------------------------------------------------------------
//  Created:     09/04/2015 by Benjamin Block
//  Description:
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#pragma once

#include "StdAfx.h"
#include "ParticleSystem/ParticleRender.h"
#include <CryCore/BitFiddling.h>

CRY_PFX2_DBG

namespace pfx2
{

SERIALIZATION_DECLARE_ENUM(EGpuSpritesSortMode,
                           None = 0,
                           BackToFront,
                           FrontToBack,
                           OldToNew,
                           NewToOld
                           );

SERIALIZATION_DECLARE_ENUM(EGpuSpritesFacingMode,
                           Screen = 0,
                           Velocity
                           );

// maybe inherit from CParticleRenderBase to inherit some of the
// draw call logic already in place if that makes sense.
class CFeatureRenderGpuSprites : public CParticleRenderBase
{
public:
	struct ConvertNextPowerOfTwo
	{
		typedef uint TType;
		static TType From(TType val) { return NextPower2(val); }
		static TType To(TType val)   { return NextPower2(val); }
	};
	typedef TValue<uint, THardLimits<1024, 1024*1024>, ConvertNextPowerOfTwo> UIntNextPowerOfTwo;

	CRY_PFX2_DECLARE_FEATURE

	CFeatureRenderGpuSprites();

	virtual void         ResolveDependency(CParticleComponent* pComponent) override;
	virtual void         Serialize(Serialization::IArchive& ar) override;
	virtual EFeatureType GetFeatureType() override { return EFT_Render; }
	virtual void         AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override;
private:
	UIntNextPowerOfTwo    m_maxParticles;
	UIntNextPowerOfTwo    m_maxNewBorns;
	EGpuSpritesSortMode   m_sortMode;
	EGpuSpritesFacingMode m_facingMode;
	UFloat10              m_axisScale;
	SFloat                m_sortBias;
};
}
