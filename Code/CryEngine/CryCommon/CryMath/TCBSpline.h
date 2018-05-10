// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryMath/ISplines.h>

//! \cond INTERNAL
namespace spline
{
static float calc_ease(float t, float a, float b);

//////////////////////////////////////////////////////////////////////////
// TCBSpline class
//////////////////////////////////////////////////////////////////////////

template<class T>
struct TCBSplineKey : public SplineKey<T>
{
	// Key controls.
	float tens;           //!< Key tension value.
	float cont;           //!< Key continuity value.
	float bias;           //!< Key bias value.
	float easeto;         //!< Key ease to value.
	float easefrom;       //!< Key ease from value.

	TCBSplineKey()
		: tens(0), cont(0), bias(0), easeto(0), easefrom(0) {}

	static int tangent_passes()
	{
		return 2;
	}
	void compute_tangents(const TCBSplineKey* prev, const TCBSplineKey* next, int pass, int num_keys)
	{
		if (pass == 0)
		{
			// compute all middle tangents
			if (prev && next)
				compMiddleDeriv(prev, next);
		}
		else
		{
			// compute first and last tangents
			if (!prev)
				compFirstDeriv(next, num_keys);
			else if (!next)
				compLastDeriv(prev, num_keys);
		}
	}

	void interpolate(const TCBSplineKey& key2, float u, T& val)
	{
		u = calc_ease(u, easefrom, key2.easeto);
		SplineKey<T>::interpolate(key2, u, val);
	}

private:
	void compMiddleDeriv(const TCBSplineKey* prev, const TCBSplineKey* next);
	void compFirstDeriv(const TCBSplineKey* next, int num_keys);
	void compLastDeriv(const TCBSplineKey* prev, int num_keys);
};

/****************************************************************************
**                        TCBSpline class implementation									   **
****************************************************************************/
template<class T, class Key = TCBSplineKey<T>>
class TCBSpline : public TSpline<Key>
{
};

//////////////////////////////////////////////////////////////////////////
template<class T>
void TCBSplineKey<T >::compMiddleDeriv(const TCBSplineKey* prev, const TCBSplineKey* next)
{
	assert(prev && next);
	float dsA, dsB, ddA, ddB;
	float A, B, cont1, cont2;

	dsA = 0;
	ddA = 0;

	// dsAdjust,ddAdjust apply speed correction when continuity is 0.
	// Middle key.
	float fDiv = next->time - prev->time;
	if (fDiv != 0.f)
	{
		float dt = 2.0f / fDiv;
		dsA = dt * (this->time - prev->time);
		ddA = dt * (next->time - this->time);
	}

	T ds0 = this->ds;
	T dd0 = this->dd;

	float c = (float)fabs(this->cont);
	float sa = dsA + c * (1.0f - dsA);
	float da = ddA + c * (1.0f - ddA);

	A = 0.5f * (1.0f - this->tens) * (1.0f + this->bias);
	B = 0.5f * (1.0f - this->tens) * (1.0f - this->bias);
	cont1 = (1.0f - this->cont);
	cont2 = (1.0f + this->cont);
	//dsA = dsA * A * cont1;
	//dsB = dsA * B * cont2;
	//ddA = ddA * A * cont2;
	//ddB = ddA * B * cont1;
	dsA = sa * A * cont1;
	dsB = sa * B * cont2;
	ddA = da * A * cont2;
	ddB = da * B * cont1;

	this->ds = dsA * (this->value - prev->value) + dsB * (next->value - this->value);
	this->dd = ddA * (this->value - prev->value) + ddB * (next->value - this->value);

	switch (this->flags.inTangentType)
	{
	case ETangentType::Step:
	case ETangentType::Zero:
		Zero(this->ds);
		break;
	case ETangentType::Linear:
		this->ds = this->value - prev->value;
		break;
	case ETangentType::Custom:
		this->ds = ds0;
		break;
	}
	switch (this->flags.outTangentType)
	{
	case ETangentType::Step:
	case ETangentType::Zero:
		Zero(this->dd);
		break;
	case ETangentType::Linear:
		this->dd = next->value - this->value;
		break;
	case ETangentType::Custom:
		this->dd = dd0;
		break;
	}
}

template<class T>
void TCBSplineKey<T >::compFirstDeriv(const TCBSplineKey* next, int num_keys)
{
	if (this->flags.inTangentType != ETangentType::Custom)
		Zero(this->ds);

	if (this->flags.outTangentType != ETangentType::Custom)
	{
		if (next)
		{
			if (num_keys == 2)
				this->dd = (1.0f - this->tens) * (next->value - this->value);
			else
				this->dd = 1.5f * (1.0f - this->tens) * (next->value - this->value - next->ds);
		}
		else
			Zero(this->dd);
	}
}

template<class T>
void TCBSplineKey<T >::compLastDeriv(const TCBSplineKey* prev, int num_keys)
{
	if (this->flags.inTangentType != ETangentType::Custom)
	{
		if (prev)
		{
			if (num_keys == 2)
				this->ds = (1.0f - this->tens) * (this->value - prev->value);
			else
				this->ds = -1.5f * (1.0f - this->tens) * (prev->value - this->value + prev->dd);
		}
		else
			Zero(this->ds);
	}

	if (this->flags.outTangentType != ETangentType::Custom)
		Zero(this->dd);
}

inline float calc_ease(float t, float a, float b)
{
	float k;
	float s = a + b;

	if (t == 0.0f || t == 1.0f) return t;
	if (s == 0.0f) return t;
	if (s > 1.0f)
	{
		k = 1.0f / s;
		a *= k;
		b *= k;
	}
	k = 1.0f / (2.0f - a - b);
	if (t < a)
	{
		return ((k / a) * t * t);
	}
	else
	{
		if (t < 1.0f - b)
		{
			return (k * (2.0f * t - a));
		}
		else
		{
			t = 1.0f - t;
			return (1.0f - (k / b) * t * t);
		}
	}
}

/****************************************************************************
**                    TCBAngleAxisSpline class implementation							 **
****************************************************************************/
///////////////////////////////////////////////////////////////////////////////
//
// TCBAngleAxisSpline takes as input relative Angle-Axis values.
// Interpolated result is returned as Normalized quaternion.
//
//////////////////////////////////////////////////////////////////////////

//! Quaternion interpolation for angles > 2PI.
ILINE static Quat CreateSquadRev(
  f32 angle,          // angle of rotation
  const Vec3& axis,   // the axis of rotation
  const Quat& p,      // start quaternion
  const Quat& a,      // start tangent quaternion
  const Quat& b,      // end tangent quaternion
  const Quat& q,      // end quaternion
  f32 t)              // Time parameter, in range [0,1]
{
	f32 s, v;
	f32 omega = 0.5f * angle;
	f32 nrevs = 0;
	Quat r, pp, qq;

	if (omega < (gf_PI - 0.00001f))
	{
		return Quat::CreateSquad(p, a, b, q, t);
	}

	while (omega > (gf_PI - 0.00001f))
	{
		omega -= gf_PI;
		nrevs += 1.0f;
	}
	if (omega < 0) omega = 0;
	s = t * angle / gf_PI;    // 2t(omega+Npi)/pi

	if (s < 1.0f)
	{
		pp = p * Quat(0.0f, axis);//pp = p.Orthog( axis );
		r = Quat::CreateSquad(p, a, pp, pp, s);   // in first 90 degrees.
	}
	else
	{
		v = s + 1.0f - 2.0f * (nrevs + (omega / gf_PI));
		if (v <= 0.0f)
		{
			// middle part, on great circle(p,q).
			while (s >= 2.0f)
				s -= 2.0f;
			pp = p * Quat(0.0f, axis);//pp = p.Orthog(axis);
			r = Quat::CreateSlerp(p, pp, s);
		}
		else
		{
			// in last 90 degrees.
			qq = -q* Quat(0.0f, axis);
			r = Quat::CreateSquad(qq, qq, b, q, v);
		}
	}
	return r;
}

//! TCB spline key used in quaternion spline with angle axis as input.
struct TCBAngAxisKey : public TCBSplineKey<Quat>
{
	Vec3  axis;
	float angle;

	TCBAngAxisKey()
		: axis(0, 0, 0), angle(0) {}

	void compute_tangents(const TCBAngAxisKey* prev, const TCBAngAxisKey* next, int pass, int num_keys)
	{
		Quat qp, qm;
		float fp, fn;

		if (prev)
		{
			if (angle > gf_PI2)
			{
				Vec3 a = axis;
				qm = Quat(0, Quat::log(Quat(0, a.x, a.y, a.z)));
			}
			else
			{
				qm = prev->value;
				if ((qm | value) < 0.0f)
					qm = -qm;
				qm = Quat::LnDif(qm, value);
			}
		}

		if (next)
		{
			if (next->angle > gf_PI2)
			{
				Vec3 a = next->axis;
				qp = Quat(0, Quat::log(Quat(0, a.x, a.y, a.z)));
			}
			else
			{
				Quat qnext = next->value;
				if ((qnext | value) < 0.0f)
					qnext = -qnext;
				qp = Quat::LnDif(value, qnext);
			}
		}

		if (!prev)
			qm = qp;
		if (!next)
			qp = qm;

		float c = (float)fabs(cont);

		fp = fn = 1.0f;
		if (prev && next)
		{
			float dt = 2.0f / (next->time - prev->time);
			fp = dt * (time - prev->time);
			fn = dt * (next->time - time);
			fp += c - c * fp;
			fn += c - c * fn;
		}

		float tm, cm, cp, bm, bp, tmcm, tmcp, ksm, ksp, kdm, kdp;

		cm = 1.0f - cont;
		tm = 0.5f * (1.0f - tens);
		cp = 2.0f - cm;
		bm = 1.0f - bias;
		bp = 2.0f - bm;
		tmcm = tm * cm;
		tmcp = tm * cp;
		ksm = 1.0f - tmcm * bp * fp;
		ksp = -tmcp * bm * fp;
		kdm = tmcp * bp * fn;
		kdp = tmcm * bm * fn - 1.0f;

		const Vec3 va = 0.5f * (kdm * qm.v + kdp * qp.v);
		const Vec3 vb = 0.5f * (ksm * qm.v + ksp * qp.v);

		const Quat qa = Quat::exp(va);
		const Quat qb = Quat::exp(vb);

		ds = value * qb;
		dd = value * qa;
	}

	void interpolate(const TCBAngAxisKey& key2, float u, Quat& val)
	{
		u = calc_ease(u, easefrom, key2.easeto);
		val = CreateSquadRev(key2.angle, key2.axis, this->value, this->dd, key2.ds, key2.value, u);
		val.Normalize();
	}
};

//////////////////////////////////////////////////////////////////////////
typedef TCBSpline<Quat, TCBAngAxisKey> TCBAngleAxisSpline;

}; // namespace spline
//! \endcond