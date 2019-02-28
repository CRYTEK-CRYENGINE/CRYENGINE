// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryMath/TCBSpline.h>

// CJoint animation.
struct CControllerTCB : public IController
{
	size_t SizeOfController() const
	{
		return sizeof(CControllerTCB) + m_posTrack.sizeofThis() + m_rotTrack.sizeofThis() + m_sclTrack.sizeofThis();
	}

	size_t ApproximateSizeOfThis() const
	{
		return SizeOfController();
	}

	JointState     GetOPS(f32 key, Quat& rot, Vec3& pos, Diag33& scl) const;
	JointState     GetOP(f32 key, Quat& rot, Vec3& pos) const;
	JointState     GetO(f32 key, Quat& rot) const;
	JointState     GetP(f32 key, Vec3& pos) const;
	JointState     GetS(f32 key, Diag33& scl) const;

	virtual size_t GetRotationKeysNum() const
	{
		return 0;
	}

	virtual size_t GetPositionKeysNum() const
	{
		return 0;
	}

	virtual size_t GetScaleKeysNum() const
	{
		return 0;
	}

	JointState                 m_active;
	spline::TCBSpline<Vec3>    m_posTrack;
	spline::TCBAngleAxisSpline m_rotTrack;
	spline::TCBSpline<Vec3>    m_sclTrack;

	CControllerTCB()
	{
		m_nControllerId = 0xaaaa5555;
	}

	void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(this, sizeof(*this));
		pSizer->AddObject(m_posTrack);
		pSizer->AddObject(m_rotTrack);
		pSizer->AddObject(m_sclTrack);
	}
};
