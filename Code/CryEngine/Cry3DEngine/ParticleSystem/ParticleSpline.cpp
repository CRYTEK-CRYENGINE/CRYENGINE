// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  Created:     05/03/2015 by Filipe amim
//  Description:
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ParticleCommon.h"
#include "ParticleMath.h"
#include "ParticleSpline.h"
#include <CrySerialization/STL.h>


namespace pfx2
{


	CParticleSpline::CParticleSpline()
		: m_valueRange(1.0f)
	{
		MakeFlatLine(1.0f);
	}


	void CParticleSpline::MakeLinear(float v0, float v1)
	{
		Resize(2);
		m_keys[0].time     = 0.0f;
		m_keys[1].time     = 1.0f;
		m_keys[0].value    = v0;
		m_keys[1].value    = v1;
		m_keys[0].timeMult = 1.0f;
		m_keys[0].coeff0   = m_keys[0].coeff1 = 0.0f;

		m_valueRange = Range(Range::EMPTY) | v0 | v1;
	}




	bool CParticleSpline::FromString(cstr str)
	{
		CSourceSpline spline;
		if (!spline.FromString(str))
			return false;
		FromSpline(spline);
		return true;
	}

	bool CParticleSpline::Serialize(Serialization::IArchive& ar, const char* name, const char* label)
	{
		if (!ar.isEdit() && ar.isOutput())
		{
			// On output, simplify identity spline
			if (m_valueRange == 1.0f)
			{
				string str;
				return !ar(str, name, label);
			}
		}
		return ISplineEvaluator::Serialize(ar, name, label);
	}

	void CParticleSpline::FromSpline(const ISplineEvaluator& source)
	{
		const size_t nKeys    = source.GetKeyCount();
		const size_t subNKeys = nKeys == 0 ? 0 : nKeys - 1;

		KeyType key;
		if (nKeys >= 2)
		{
			Resize(nKeys);

			m_valueRange = Range(Range::EMPTY);

			for (size_t i = 0; i < nKeys; ++i)
			{
				source.GetKey(i, key);
				m_keys[i].time  = key.time;
				m_keys[i].value = key.value[0];
				m_valueRange   |= m_keys[i].value;
			}
			for (size_t i = 0; i < subNKeys; ++i)
			{
				const float t0 = m_keys[i].time;
				const float t1 = max(t0 + FLT_EPSILON, m_keys[i + 1].time);
				const float v0 = m_keys[i].value;
				const float v1 = m_keys[i + 1].value;
				source.GetKey(i, key);
				const float s0 = key.dd[0];
				source.GetKey(i + 1, key);
				const float s1 = key.ds[0];
				const float dv = v1 - v0;

				m_keys[i].timeMult = 1.0f / (t1 - t0);
				m_keys[i].coeff0   = s0 - dv;
				m_keys[i].coeff1   = dv - s1;

				// Compute curve min/max
				// v = v0 + s0 t + (-3v0 +3v1 -2s0 -s1) tt + (+2v0 -2v1 +s0 +s1) ttt
				// v' = s0 + (-6v0 +6v1 -4s0 -2s1) t + (+6v0 -6v1 +3s0 +3s1) tt

				float roots[2];
				int nRoots = solve_quadratic(
				    /* tt term */ 6.0f * (v0 - v1) + 3.0f * (s0 + s1),
				    /* t  term */ 6.0f * (v1 - v0) - 4.0f * s0 - 2.0f * s1,
				    /* 1  term */ s0,
					roots);

				while (nRoots-- > 0)
				{
					const float t = roots[nRoots];
					if (t > 0.0f && t < 1.0f)
					{
						const float v = Interpolate(::Lerp(t0, t1, t));
						m_valueRange |= v;
					}
				}
			}
		}
		else
		{
			MakeFlatLine((nKeys == 1) ? (source.GetKey(0, key), key.value[0]) : 1.0f);
		}
	}



	void CParticleSpline::Resize(size_t size)
	{
		m_keys.resize(size);
	}

	void CParticleSpline::GetKey(int i, KeyType& key) const
	{
		key.time     = m_keys[i].time;
		key.value[0] = m_keys[i].value;

		// Compute slopes.
		key.ds[0] = key.dd[0] = 0.0f;
		key.flags = 0;
		if (i > 0)
		{
			// In slope.
			key.ds[0]               = EndSlope(i - 1);
			key.flags.inTangentType = spline::ETangentType::Custom;
		}
		else
			//	key.flags.inTangentType = spline::ETangentType::Auto;
			key.flags.inTangentType = spline::ETangentType::Smooth;

		if (i < GetKeyCount() - 1)
		{
			// Out slope.
			key.dd[0]                = StartSlope(i);
			key.flags.outTangentType = spline::ETangentType::Custom;
		}
		else
			//	key.flags.outTangentType = spline::ETangentType::Auto;
			key.flags.outTangentType = spline::ETangentType::Smooth;
	}



	float CParticleSpline::DefaultSlope(size_t keyIdx) const
	{
		const size_t numKeys = GetNumKeys();
		const bool isBorder  = keyIdx == 0 || keyIdx >= numKeys - 1;
		if (isBorder)
			return 0.0f;
		else
			return minmag(m_keys[keyIdx].value - m_keys[keyIdx - 1].value, m_keys[keyIdx + 1].value - m_keys[keyIdx].value);
	}



	float CParticleSpline::StartSlope(size_t keyIdx) const
	{
		return m_keys[keyIdx + 1].value - m_keys[keyIdx].value + m_keys[keyIdx].coeff0;
	}



	float CParticleSpline::EndSlope(size_t keyIdx) const
	{
		return m_keys[keyIdx + 1].value - m_keys[keyIdx].value - m_keys[keyIdx].coeff1;
	}




}

