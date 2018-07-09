// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   ParticleUtils.h
//  Version:     v1.00
//  Created:     11/03/2010 by Corey (split out from other files).
//  Compilers:   Visual Studio.NET
//  Description: Splitting out some of the particle specific containers to here.
//							 Will be moved to a proper home eventually.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __particleutils_h__
#define __particleutils_h__
#pragma once

//////////////////////////////////////////////////////////////////////////
// Ref target that doesn't auto-delete. Handled manually
template<typename Counter> class _plain_reference_target
{
public:
	_plain_reference_target()
		: m_nRefCounter(0)
	{}

	~_plain_reference_target()
	{
		assert(m_nRefCounter == 0);
#ifdef _DEBUG
		m_nRefCounter = -1;
#endif
	}

	void AddRef()
	{
		assert(m_nRefCounter >= 0);
		++m_nRefCounter;
	}

	void Release()
	{
		--m_nRefCounter;
		assert(m_nRefCounter >= 0);
	}

	Counter NumRefs() const
	{
		assert(m_nRefCounter >= 0);
		return m_nRefCounter;
	}

protected:
	Counter m_nRefCounter;
};

extern ITimer* g_pParticleTimer;

ILINE ITimer* GetParticleTimer()
{
	return g_pParticleTimer;
}

//////////////////////////////////////////////////////////////////////////
// 3D helper functions

inline void RotateTo(Quat& qRot, Vec3 const& vNorm)
{
	qRot = Quat::CreateRotationV0V1(qRot.GetColumn1(), vNorm) * qRot;
}
inline void OrientTo(Quat& qRot, Vec3 const& vNorm)
{
	qRot = Quat::CreateRotationV0V1(qRot.GetColumn2(), vNorm) * qRot;
}
inline bool CheckNormalize(Vec3& vDest, const Vec3& vSource)
{
	float fLenSq = vSource.GetLengthSquared();
	if (fLenSq > FLT_MIN)
	{
		vDest = vSource * isqrt_tpl(fLenSq);
		return true;
	}
	return false;
}
inline bool CheckNormalize(Vec3& v)
{
	return CheckNormalize(v, v);
}

#endif // __particleutils_h__
