// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "ControllerPQLog.h"

#include "AnimationCompiler.h"
#include "AnimationLoader.h"
#include "AnimationManager.h"

#include <float.h>


inline double DLength( Vec3 &v ) 
{
	double dx=v.x*v.x;	
	double dy=v.y*v.y;	
	double dz=v.z*v.z;	
	return sqrt( dx + dy + dz );
}


// adjusts the rotation of these PQs: if necessary, flips them or one of them (effectively NOT changing the whole rotation,but
// changing the rotation angle to Pi-X and flipping the rotation axis simultaneously)
// this is needed for blending between animations represented by quaternions rotated by ~PI in quaternion space
// (and thus ~2*PI in real space)
static void AdjustLogRotations(Vec3& vRotLog1, Vec3& vRotLog2)
{
	double dLen1 = DLength(vRotLog1);
	if (dLen1 > gf_PI/2)
	{
		vRotLog1 *= (f32)(1.0f - gf_PI/dLen1); 
		// now the length of the first vector is actually gf_PI - dLen1,
		// and it's closer to the origin than its complementary
		// but we won't need the dLen1 any more
	}

	double dLen2 = DLength(vRotLog2);
	// if the flipped 2nd rotation vector is closer to the first rotation vector,
	// then flip the second vector
	if ((vRotLog1|vRotLog2) < (dLen2 - gf_PI/2) * dLen2)
	{
		// flip the second rotation also
		vRotLog2 *= (f32)(1.0f - gf_PI / dLen2);
	}
}



//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
//--------------------------------------------------------------------------

Status4 CControllerPQLog::GetOPS(f32 realtime, Quat& quat, Vec3& pos, Diag33& scale)
{
	QuatTNS pq =GetValue3(realtime);
	quat = pq.q;
	pos	 = pq.t;
	scale = pq.s;
	return Status4(1,1,1,0); 
}

Status4 CControllerPQLog::GetOP(f32 realtime, Quat& quat, Vec3& pos)
{
	QuatTNS pq = GetValue3(realtime);
	quat = pq.q;
	pos = pq.t;
	return Status4(1,1,0,0); 
}

uint32 CControllerPQLog::GetO(f32 realtime, Quat& quat)
{
	QuatTNS pq = GetValue3(realtime);
	quat = pq.q;
	return 1;
}
uint32 CControllerPQLog::GetP(f32 realtime, Vec3& pos)
{
	QuatTNS pq = GetValue3(realtime);
	pos = pq.t;
	return 1;
}
uint32 CControllerPQLog::GetS(f32 realtime, Diag33& scale)
{
	QuatTNS pq = GetValue3(realtime);
	scale = pq.s;
	return 1;
}



//////////////////////////////////////////////////////////////////////////
// retrieves the position and orientation (in the logarithmic space,
// i.e. instead of quaternion, its logarithm is returned)
// may be optimal for motion interpolation
QuatTNS CControllerPQLog::GetValue3(f32 fTime)
{
#ifdef DEFINE_PROFILER_FUNCTION
	DEFINE_PROFILER_FUNCTION();
#endif

	assert(!m_arrKeys.empty());

	const f32 keytimeStart = GetTimeStart();
	const f32 keytimeEnd = GetTimeEnd();

	if (fTime <= keytimeStart)
	{
		const PQLogS& value = m_arrKeys.front();
		return QuatTNS(!Quat::exp(value.rotationLog), value.position, value.scale);
	}

	if (fTime >= keytimeEnd)
	{
		const PQLogS& pq = m_arrKeys.back();
		return QuatTNS(!Quat::exp(pq.rotationLog), pq.position, pq.scale);
	}

	assert(m_arrKeys.size() > 1);

	int nPos = m_arrKeys.size() / 2;
	int nStep = m_arrKeys.size() / 4;

	// use binary search
	while (nStep)
	{
		if(fTime < m_arrTimes[nPos])
		{
			nPos = nPos - nStep;
		}
		else if(fTime > m_arrTimes[nPos])
		{
			nPos = nPos + nStep;
		}
		else
		{
			break;
		}
		nStep = nStep>>1;
	}

	// fine-tuning needed since time is not linear
	while (fTime > m_arrTimes[nPos])
	{
		nPos++;
	}

	while (fTime < m_arrTimes[nPos-1])
	{
		nPos--;
	}

	assert(nPos > 0 && (uint32)nPos < m_arrKeys.size());  

	f32 blendFactor;
	if (m_arrTimes[nPos] != m_arrTimes[nPos - 1])
	{
		blendFactor = (f32(fTime - m_arrTimes[nPos - 1])) / (m_arrTimes[nPos] - m_arrTimes[nPos - 1]);
	}
	else
	{
		blendFactor = 0;
	}

	PQLogS pKeys[2] = { m_arrKeys[nPos - 1], m_arrKeys[nPos] };
	AdjustLogRotations(pKeys[0].rotationLog, pKeys[1].rotationLog);

	PQLogS value;
	value.Blend(pKeys[0], pKeys[1], blendFactor);

	return QuatTNS(!Quat::exp(value.rotationLog), value.position, value.scale);
}




//////////////////////////////////////////////////////////////////////////
// retrieves the position and orientation (in the logarithmic space,
// i.e. instead of quaternion, its logarithm is returned)
// may be optimal for motion interpolation
QuatTNS CControllerPQLog::GetValueByKey(uint32 key) const
{
	const uint32 numKey = m_arrKeys.size();
	assert(numKey);
	if (key >= numKey)
	{
		const PQLogS& value = m_arrKeys[numKey - 1];
		return QuatTNS(!Quat::exp(value.rotationLog),value.position, value.scale);
	}
	const PQLogS& value = m_arrKeys[key];
	return QuatTNS(!Quat::exp(value.rotationLog), value.position, value.scale);
}



size_t CControllerPQLog::SizeOfThis() const
{
	return sizeof(*this) + m_arrKeys.size() * (sizeof(m_arrKeys[0]) + sizeof(m_arrTimes[0]));
}


