// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryMath/TCBSpline.h>

namespace spline
{

//////////////////////////////////////////////////////////////////////////
/**	TCB spline key extended for tangent unify/break.
 */
template<class T>
struct TCBSplineKeyEx : public TCBSplineKey<T>
{
	float theta_from_dd_to_ds;
	float scale_from_dd_to_ds;

	void ComputeThetaAndScale() { assert(0); }
	void SetOutTangentFromIn()  { assert(0); }
	void SetInTangentFromOut()  { assert(0); }

	TCBSplineKeyEx() : theta_from_dd_to_ds(gf_PI), scale_from_dd_to_ds(1.0f) {}
};

template<>
inline void TCBSplineKeyEx<float >::ComputeThetaAndScale()
{
	scale_from_dd_to_ds = (easeto + 1.0f) / (easefrom + 1.0f);
	float out = atan_tpl(dd);
	float in = atan_tpl(ds);
	theta_from_dd_to_ds = in + gf_PI - out;
}

template<>
inline void TCBSplineKeyEx<float >::SetOutTangentFromIn()
{
	assert((flags & SPLINE_KEY_TANGENT_ALL_MASK) == SPLINE_KEY_TANGENT_UNIFIED);
	easefrom = (easeto + 1.0f) / scale_from_dd_to_ds - 1.0f;
	if (easefrom > 1)
		easefrom = 1.0f;
	else if (easefrom < 0)
		easefrom = 0;
	float in = atan_tpl(ds);
	dd = tan_tpl(in + gf_PI - theta_from_dd_to_ds);
}

template<>
inline void TCBSplineKeyEx<float >::SetInTangentFromOut()
{
	assert((flags & SPLINE_KEY_TANGENT_ALL_MASK) == SPLINE_KEY_TANGENT_UNIFIED);
	easeto = scale_from_dd_to_ds * (easefrom + 1.0f) - 1.0f;
	if (easeto > 1)
		easeto = 1.0f;
	else if (easeto < 0)
		easeto = 0;
	float out = atan_tpl(dd);
	ds = tan_tpl(out + theta_from_dd_to_ds - gf_PI);
}

//////////////////////////////////////////////////////////////////////////
template<class T>
class TrackSplineInterpolator : public CSplineKeyInterpolator<TCBSplineKeyEx<T>>
{
	typedef CSplineKeyInterpolator<TCBSplineKeyEx<T>> base;

public:
	virtual void SetKeyFlags(int k, int flags)
	{
		if (k >= 0 && k < this->num_keys())
		{
			if ((this->key(k).flags & SPLINE_KEY_TANGENT_ALL_MASK) != SPLINE_KEY_TANGENT_UNIFIED
			    && (flags & SPLINE_KEY_TANGENT_ALL_MASK) == SPLINE_KEY_TANGENT_UNIFIED)
				this->key(k).ComputeThetaAndScale();
		}
		base::SetKeyFlags(k, flags);
	}
	virtual void SetKeyInTangent(int k, ISplineInterpolator::ValueType tin)
	{
		if (k >= 0 && k < this->num_keys())
		{
			base::FromValueType(tin, this->key(k).ds);
			if ((this->key(k).flags & SPLINE_KEY_TANGENT_ALL_MASK) == SPLINE_KEY_TANGENT_UNIFIED)
				this->key(k).SetOutTangentFromIn();
			this->SetModified(true);
		}
	}
	virtual void SetKeyOutTangent(int k, ISplineInterpolator::ValueType tout)
	{
		if (k >= 0 && k < this->num_keys())
		{
			base::FromValueType(tout, this->key(k).dd);
			if ((this->key(k).flags & SPLINE_KEY_TANGENT_ALL_MASK) == SPLINE_KEY_TANGENT_UNIFIED)
				this->key(k).SetInTangentFromOut();
			this->SetModified(true);
		}
	}
};

template<>
class TrackSplineInterpolator<Quat> : public CBaseSplineInterpolator<TCBAngleAxisSpline>
{
};

}; // namespace spline
