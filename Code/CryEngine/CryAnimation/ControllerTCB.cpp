// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "ControllerTCB.h"

#include "CharacterManager.h"

JointState CControllerTCB::GetOPS(f32 time, Quat& quat, Vec3& pos, Diag33& scl) const
{
	return
	  CControllerTCB::GetO(time, quat) |
	  CControllerTCB::GetP(time, pos) |
	  CControllerTCB::GetS(time, scl);
}

JointState CControllerTCB::GetOP(f32 time, Quat& quat, Vec3& pos) const
{
	return
	  CControllerTCB::GetO(time, quat) |
	  CControllerTCB::GetP(time, pos);
}

JointState CControllerTCB::GetO(f32 key, Quat& rot) const
{
	if (m_active & eJS_Orientation)
	{
		rot = static_cast<spline::TCBAngleAxisSpline>(m_rotTrack).interpolate(key);
		rot.Invert();
	}
	return m_active & eJS_Orientation;
}

JointState CControllerTCB::GetP(f32 key, Vec3& pos) const
{
	if (m_active & eJS_Position)
	{
		static_cast<spline::TCBSpline<Vec3>>(m_posTrack).interpolate(key, pos);
		pos /= 100.0f; // Position controller from Max must be scaled 100 times down.
	}
	return m_active & eJS_Position;
}

JointState CControllerTCB::GetS(f32 key, Diag33& scl) const
{
	if (m_active & eJS_Scale)
	{
		Vec3 out;
		static_cast<spline::TCBSpline<Vec3>>(m_sclTrack).interpolate(key, out);
		scl = Diag33(out);
	}
	return m_active & eJS_Scale;
}
