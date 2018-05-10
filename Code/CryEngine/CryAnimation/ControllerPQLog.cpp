// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "ControllerPQLog.h"

#include "CharacterManager.h"
#include <float.h>

CControllerPQLog::CControllerPQLog()
	: m_arrKeys()
	, m_arrTimes()
	, m_lastValue()
	, m_lastTime(-FLT_MAX)
{}

inline static f64 DLength(Vec3& v)
{
	const f64 dx = v.x * v.x;
	const f64 dy = v.y * v.y;
	const f64 dz = v.z * v.z;
	return sqrt(dx + dy + dz);
}

/**
 * Adjusts the rotation of these PQs: if necessary, flips them or one of them (effectively NOT changing the whole rotation, but
 * changing the rotation angle to Pi-X and flipping the rotation axis simultaneously) this is needed for blending between animations
 * represented by quaternions rotated by ~PI in quaternion space (and thus ~2*PI in real space)
 */
void AdjustLogRotations(Vec3& vRotLog1, Vec3& vRotLog2) // TODO: Should be moved to the CControllerPQLog class.
{
	const f64 dLen1 = DLength(vRotLog1);
	if (dLen1 > gf_PI / 2)
	{
		vRotLog1 *= (f32)(1.0f - (gf_PI / dLen1));
		// now the length of the first vector is actually gf_PI - dLen1,
		// and it's closer to the origin than its complementary
		// but we won't need the dLen1 any more
	}

	const f64 dLen2 = DLength(vRotLog2);
	// if the flipped 2nd rotation vector is closer to the first rotation vector,
	// then flip the second vector
	if ((vRotLog1 | vRotLog2) < ((dLen2 - (gf_PI / 2)) * dLen2) && dLen2 > 0.000001f)
	{
		// flip the second rotation also
		vRotLog2 *= (f32)(1.0f - (gf_PI / dLen2));
	}
}

JointState CControllerPQLog::GetOPS(f32 key, Quat& quat, Vec3& pos, Diag33& scl) const
{
	const QuatTNS& pq = DecodeKey(key);
	quat = pq.q;
	pos = pq.t;
	scl = pq.s;
	return eJS_Position | eJS_Orientation | eJS_Scale;
}

JointState CControllerPQLog::GetOP(f32 key, Quat& quat, Vec3& pos) const
{
	const QuatTNS& pq = DecodeKey(key);
	quat = pq.q;
	pos = pq.t;
	return eJS_Position | eJS_Orientation;
}

JointState CControllerPQLog::GetO(f32 key, Quat& quat) const
{
	const QuatTNS& pq = DecodeKey(key);
	quat = pq.q;
	return eJS_Orientation;
}
JointState CControllerPQLog::GetP(f32 key, Vec3& pos) const
{
	const QuatTNS& pq = DecodeKey(key);
	pos = pq.t;
	return eJS_Position;
}

JointState CControllerPQLog::GetS(f32 key, Diag33& scl) const
{
	const QuatTNS& pq = DecodeKey(key);
	scl = pq.s;
	return eJS_Scale;
}

int32 CControllerPQLog::GetO_numKey() const
{
	return m_arrKeys.size();
}

int32 CControllerPQLog::GetP_numKey() const
{
	return m_arrKeys.size();
}

size_t CControllerPQLog::GetRotationKeysNum() const
{
	return 0;
}

size_t CControllerPQLog::GetPositionKeysNum() const
{
	return 0;
}

size_t CControllerPQLog::GetScaleKeysNum() const
{
	return 0;
}

size_t CControllerPQLog::SizeOfController() const
{
	const size_t s0 = sizeof(*this);
	const size_t s1 = m_arrKeys.get_alloc_size();
	const size_t s2 = m_arrTimes.get_alloc_size();
	return s0 + s1 + s2;
}

size_t CControllerPQLog::ApproximateSizeOfThis() const
{
	return SizeOfController();
}
