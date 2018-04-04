// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

// #SchematycTODO : Move serialization utils to separate header?

#pragma once

#include "Angle.h"

namespace CryTransform
{

class CRotation
{
private:

	enum class EFormat : uint32
	{
		Angles,
		Quat
	};

public:

	inline CRotation()
		: m_format(EFormat::Angles)
		, m_value(0.0f, 0.0f, 0.0f, 0.0f)
	{}

	explicit inline CRotation(const CAngles3& angles)
		: m_format(EFormat::Angles)
		, m_value(0.0f, angles.x.ToRadians(), angles.y.ToRadians(), angles.z.ToRadians())
	{}

	explicit inline CRotation(const Quat& rotation)
		: m_format(EFormat::Quat)
		, m_value(rotation)
	{}

	explicit inline CRotation(const Matrix33& rotation)
		: m_format(EFormat::Quat)
		, m_value(rotation)
	{}

	inline CRotation(const CRotation& rhs)
		: m_format(rhs.m_format)
		, m_value(rhs.m_value)
	{}

	inline CAngles3 ToAngles() const
	{
		if (m_format == EFormat::Quat)
		{
			Ang3 angles(m_value);

			return CAngles3(CAngle::FromRadians(angles.x), CAngle::FromRadians(angles.y), CAngle::FromRadians(angles.z));
		}
		else
		{
			return CAngles3(CAngle::FromRadians(m_value.v.x), CAngle::FromRadians(m_value.v.y), CAngle::FromRadians(m_value.v.z));
		}
	}

	inline Quat ToQuat() const
	{
		if (m_format == EFormat::Angles)
		{
			return Quat(Ang3(m_value.v.x, m_value.v.y, m_value.v.z));
		}
		else
		{
			return m_value;
		}
	}

	inline Matrix33 ToMatrix33() const
	{
		return Matrix33::CreateRotationXYZ(ToAngles().ToAng3());
	}

	inline void operator=(const CAngles3& angles)
	{
		m_format = EFormat::Angles;
		m_value = Quat(0.0f, angles.x.ToRadians(), angles.y.ToRadians(), angles.z.ToRadians());
	}

	inline void operator=(const Quat& rotation)
	{
		m_format = EFormat::Quat;
		m_value = rotation;
	}

	inline void operator=(const Matrix33& rotation)
	{
		m_format = EFormat::Quat;
		m_value = Quat(rotation);
	}

	inline bool operator==(const CRotation& rhs) const
	{
		return m_format == rhs.m_format && m_value == rhs.m_value;
	}

	static inline void ReflectType(Schematyc::CTypeDesc<CRotation>& desc)
	{
		desc.SetGUID("991960c1-3b34-45cc-ac3c-f3f0e55ed7ef"_cry_guid);
		desc.SetLabel("Rotation");
		desc.SetDescription("Quaternion rotation");
	}

private:

	EFormat m_format;
	Quat    m_value;
};

inline bool Serialize(Serialization::IArchive& archive, CRotation& value, const char* szName, const char* szLabel)
{
	CAngles3 angles = archive.isOutput() ? value.ToAngles() : CAngles3();

	typedef float(&Array)[3];
	if ((archive.isEdit() && archive((Array)angles, szName, szLabel)) || archive(angles, szName, szLabel))
	{
		if (archive.isInput())
		{
			value = angles;
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
