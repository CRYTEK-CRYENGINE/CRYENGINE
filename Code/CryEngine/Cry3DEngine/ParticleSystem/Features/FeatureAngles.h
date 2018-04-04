// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#pragma once

namespace pfx2
{

SERIALIZATION_DECLARE_ENUM(EAlignParticleAxis,
	Forward,
	Normal
	)

SERIALIZATION_DECLARE_ENUM(EAlignType,
	Screen,
	Camera,
	Velocity,
	Parent
	)

SERIALIZATION_DECLARE_ENUM(EAlignView,
	None,
	Screen,
	Camera
	)

ILINE Vec3 GetParticleAxis(EAlignParticleAxis particleAxis, Quat orientation);
ILINE Vec3 GetParticleOtherAxis(EAlignParticleAxis particleAxis, Quat orientation);

}

#include "FeatureAnglesImpl.h"
