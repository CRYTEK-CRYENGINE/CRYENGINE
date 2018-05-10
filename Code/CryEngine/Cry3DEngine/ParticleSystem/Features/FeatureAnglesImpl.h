// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

namespace pfx2
{

ILINE Vec3 GetParticleAxis(EAlignParticleAxis particleAxis, Quat orientation)
{
	switch (particleAxis)
	{
	case EAlignParticleAxis::Forward: return orientation.GetColumn1();
	case EAlignParticleAxis::Normal: return orientation.GetColumn2();
	default: return Vec3(ZERO);
	}
}

ILINE Vec3 GetParticleOtherAxis(EAlignParticleAxis particleAxis, Quat orientation)
{
	switch (particleAxis)
	{
	case EAlignParticleAxis::Forward: return orientation.GetColumn0();
	case EAlignParticleAxis::Normal: return orientation.GetColumn1();
	default: return Vec3(ZERO);
	}
}

}
