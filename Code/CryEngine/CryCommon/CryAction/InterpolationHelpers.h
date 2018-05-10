// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Helper classes with catchup for smoother synchronization,

   -------------------------------------------------------------------------
   History:
   - 7:12:2006 : Created by Stas Spivakov

*************************************************************************/

#ifndef __INTERPOLATIONHELPERS_H__
#define __INTERPOLATIONHELPERS_H__

#pragma once

template<class T>
struct InterpolateHelper
{
	typedef const T& SetType;
	typedef const T& GetType;
	typedef float    DeltaType;
	static DeltaType Delta(const T& a, const T& b)
	{
		return b - a;
	}
	static DeltaType Interpolate(T& a, const T& b, float dist, float change)
	{
		float t = change / dist;
		a = a + (b - a) * t;
		return change;
	}
	static T GetDefault()
	{
		return T();
	}
	static DeltaType GetDefaultTeleport()
	{
		return DeltaType();
	}
	static DeltaType GetDefaultSpeed()
	{
		return DeltaType();
	}
};

template<class T, class H = InterpolateHelper<T>>
class InterpolatedValue_tpl
{
public:
	typedef typename H::SetType   SetType;
	typedef typename H::GetType   GetType;
	typedef typename H::DeltaType DeltaType;

	InterpolatedValue_tpl(SetType x = H::GetDefault(), DeltaType teleportd = H::GetDefaultTeleport(), DeltaType stepspeed = H::GetDefaultSpeed()) :
		m_teleportd(teleportd), m_stepspeed(stepspeed)
	{
		Set(m_value = x);
	}
	void Set(SetType x)
	{
		m_interpolating = false;
		m_interpolated = x;
	}
	void SetGoal(SetType x)
	{
		m_interpolating = true;
		m_value = x;
	}
	GetType   Get() const             { return m_interpolated; }
	GetType   GetGoal() const         { return m_value; }
	bool      IsInterpolating() const { return m_interpolating; }
	DeltaType Update(DeltaType delta)
	{
		if (!m_interpolating)
			return DeltaType(0);
		DeltaType dist = H::Delta(m_interpolated, m_value);
		DeltaType change = m_stepspeed * delta;
		if (dist < change || dist > m_teleportd)
		{
			Set(GetGoal());
			return dist;
		}
		return H::Interpolate(m_interpolated, m_value, dist, change);
	}
protected:
	T         m_value;
	T         m_interpolated;

	bool      m_interpolating;
	DeltaType m_teleportd;
	DeltaType m_stepspeed;
	int       m_policy;
};

//QUAT - normalized quaternion interpolation
struct InterpolatedQuatHelper
{
	struct NormalizedQuat
	{
		NormalizedQuat(const Quat& q)
		{
			m_quat = q.GetNormalized();
		}
		operator Quat(){ return m_quat; }
		operator const Quat&() const { return m_quat; }
		Quat m_quat;
	};

	typedef Quat                  T;
	typedef const NormalizedQuat& SetType;
	typedef const T&              GetType;
	typedef float                 DeltaType;
	static DeltaType Delta(const T& a, const T& b)
	{
		float cosine = std::min(1.0f, a | b);
		return 2.0f * fabs_tpl(acos_tpl(cosine));
	}
	static float Interpolate(T& a, const T& b, float dist, float change)
	{
		float t = std::min(1.0f, change / dist);
		a = Quat::CreateSlerp(a, b, t).GetNormalized();
		return t * dist;
	}
	static T GetDefault()
	{
		return Quat::CreateIdentity();
	}
	static DeltaType GetDefaultTeleport()
	{
		return gf_PI * 0.25f;
	}
	static DeltaType GetDefaultSpeed()
	{
		return 1.0f;
	}
};

typedef InterpolatedValue_tpl<Quat, InterpolatedQuatHelper> InterpolatedQuat;

//VEC3

struct InterpolatedVec3Helper : public InterpolateHelper<Vec3>
{
	static float Delta(const Vec3& a, const Vec3& b)
	{
		return (b - a).len();
	}
	static Vec3 GetDefault()
	{
		return Vec3(ZERO);
	}
	static float GetDefaultTeleport()
	{
		return 0.25f;
	}
	static float GetDefaultSpeed()
	{
		return 1.0f;
	}
};

typedef InterpolatedValue_tpl<Vec3, InterpolatedVec3Helper> InterpolatedVec3;

//float

struct InterpolatedFloatHelper : public InterpolateHelper<float>
{
	typedef float SetType;
	typedef float GetType;
	typedef float DeltaType;
	static DeltaType Delta(float a, float b)
	{
		return b - a;
	}
	static DeltaType Interpolate(float& a, float b, float dist, float change)
	{
		float t = std::min(1.0f, change / dist);
		a = a + (b - a) * t;
		return t * dist;
	}
};

typedef InterpolatedValue_tpl<float, InterpolatedFloatHelper> InterpoltedFloat;

#endif /*__INTERPOLATIONHELPERS_H__*/
