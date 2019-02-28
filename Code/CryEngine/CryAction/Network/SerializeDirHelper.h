// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __DIR_SERIALIZE_HELPER_H__
#define __DIR_SERIALIZE_HELPER_H__

#include "SerializeBits.h"

// Ideaally this could go into the network layer as a policy

static inline void SerializeDirHelper(TSerialize ser, Vec3& dir, int policyYaw, int policyElev)
{
	// Serialise look direction as yaw and elev. Gives better compression
	float yaw, elev;

	if (!ser.IsReading())
	{
		float xy = dir.GetLengthSquared2D();
		if (xy > 0.001f)
		{
			yaw = atan2_tpl(dir.y, dir.x);
			elev = asin_tpl(dir.z);
		}
		else
		{
			yaw = 0.f;
			elev = (float)__fsel(dir.z, +1.f, -1.f) * (gf_PI * 0.5f);
		}
	}

	ser.Value("playerLookYaw", yaw, policyYaw);
	ser.Value("playerLookElev", elev, policyElev);

	if (ser.IsReading())
	{
		float sinYaw, cosYaw;
		float sinElev, cosElev;

		sincos_tpl(yaw, &sinYaw, &cosYaw);
		sincos_tpl(elev, &sinElev, &cosElev);

		dir.x = cosYaw * cosElev;
		dir.y = sinYaw * cosElev;
		dir.z = sinElev;
	}
}

static inline void SerializeDirHelper(CBitArray& array, Vec3& dir, int nYawBits, int nElevBits)
{
	// Serialise look direction as yaw and elev. Gives better compression
	float yaw, elev;

	if (array.IsReading() == 0)
	{
		float xy = dir.GetLengthSquared2D();
		if (xy > 0.001f)
		{
			yaw = atan2_tpl(dir.y, dir.x);
			elev = asin_tpl(dir.z);
		}
		else
		{
			yaw = 0.f;
			elev = (float)__fsel(dir.z, +1.f, -1.f) * (gf_PI * 0.5f);
		}
	}

	array.Serialize(yaw, -gf_PI, +gf_PI, nYawBits, 1);    // NB: reduce range by 1 so that zero is correctly replicated
	array.Serialize(elev, -gf_PI * 0.5f, +gf_PI * 0.5f, nElevBits, 1);

	if (array.IsReading())
	{
		float sinYaw, cosYaw;
		float sinElev, cosElev;

		sincos_tpl(yaw, &sinYaw, &cosYaw);
		sincos_tpl(elev, &sinElev, &cosElev);

		dir.x = cosYaw * cosElev;
		dir.y = sinYaw * cosElev;
		dir.z = sinElev;
	}
}

static inline void SerializeDirVector(CBitArray& array, Vec3& dir, float maxLength, int nScaleBits, int nYawBits, int nElevBits)
{
	float length;
	if (array.IsReading())
	{
		SerializeDirHelper(array, dir, nYawBits, nElevBits);
		array.Serialize(length, 0.f, maxLength, nScaleBits);
		dir = dir * length;
	}
	else
	{
		length = dir.GetLength();
		Vec3 tmp = (length > 0.f) ? dir / length : dir;
		SerializeDirHelper(array, tmp, nYawBits, nElevBits);
		array.Serialize(length, 0.f, maxLength, nScaleBits);
	}
}

#endif
