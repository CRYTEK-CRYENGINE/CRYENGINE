// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// #SchematycTODO : Move serialization utils to separate header?

#pragma once

#include "Angle.h"

namespace CryTransform
{

class CRotation
{
public:
	inline CRotation()
	{
		m_value.SetIdentity();
	}

	explicit inline CRotation(const CAngles3& angles)
	{
		m_value = Quat(angles.ToAng3());
	}

	explicit inline CRotation(const Quat& rotation)
		: m_value(rotation)
	{}

	explicit inline CRotation(const Matrix33& rotation)
		: m_value(rotation)
	{}

	inline CAngles3 ToAngles() const
	{
		Ang3 ang(m_value);

		return CAngles3(CAngle::FromRadians(ang.x), CAngle::FromRadians(ang.y), CAngle::FromRadians(ang.z));
	}

	inline Matrix34 ToMatrix() const
	{
		return Matrix34(m_value);
	}

	inline Quat ToQuat() const
	{
		return m_value;
	}

	inline Matrix33 ToMatrix33() const
	{
		return Matrix33::CreateRotationXYZ(ToAngles().ToAng3());
	}

	inline void operator=(const CAngles3& angles)
	{
		m_value.SetRotationXYZ(angles.ToAng3());
	}

	inline void operator=(const Quat& rotation)
	{
		m_value = rotation;
	}

	inline void operator=(const Matrix33& rotation)
	{
		m_value = Quat(rotation);
	}

	inline bool operator==(const CRotation& rhs) const
	{
		return m_value == rhs.m_value;
	}

	inline bool operator!=(const CRotation& rhs) const
	{
		return m_value != rhs.m_value;
	}

	static inline void ReflectType(Schematyc::CTypeDesc<CRotation>& desc)
	{
		desc.SetGUID("991960c1-3b34-45cc-ac3c-f3f0e55ed7ef"_cry_guid);
		desc.SetLabel("Rotation");
		desc.SetDescription("Quaternion rotation");
	}

private:
	Quat m_value;
};

inline bool Serialize(Serialization::IArchive& archive, CRotation& value, const char* szName, const char* szLabel)
{
	Vec3 degree = value.ToAngles().ToDegrees();

	typedef float(&Array)[3];
	if ((archive.isEdit() && archive((Array)degree, szName, szLabel)) || archive(degree, szName, szLabel))
	{
		if (archive.isInput())
		{
			value = CRotation(CAngles3(CAngle::FromDegrees(degree.x), CAngle::FromDegrees(degree.y), CAngle::FromDegrees(degree.z)));
		}
		return true;
	}
	return false;
}

inline CRotation Multiply(const CRotation& a, const CRotation& b)
{
	return CRotation(a.ToQuat() * b.ToQuat());
}

} // CryTransform
