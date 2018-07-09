// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ParticleSystem/ParticleCommon.h"
#include "ParticleSystem/ParticleAttributes.h"
#include "ParticleSystem/ParticleEmitter.h"
#include "ParticleSystem/ParticleComponentRuntime.h"
#include "ParamMod.h"

namespace pfx2
{

SERIALIZATION_ENUM_DECLARE(ETargetSource, ,
                           Self,
                           Parent,
                           Target
                           )

// PFX2_TODO : optimize : GetTarget is very inefficient. Make static dispatch and then vectorize it.

class CTargetSource
{
public:
	CTargetSource(ETargetSource source = ETargetSource::Target);

	void Serialize(Serialization::IArchive& ar);
	Vec3 GetTarget(const CParticleComponentRuntime& runtime, TParticleId particleId, bool isParentId = false);

private:
	Vec3          m_offset;
	ETargetSource m_source;
};

}

#include "TargetImpl.h"
