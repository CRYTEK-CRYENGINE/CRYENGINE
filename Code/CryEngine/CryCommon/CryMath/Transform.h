// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySerialization/Forward.h>
#include <CrySerialization/Math.h>

#include "Rotation.h"

namespace CryTransform
{

class CTransform
{
public:

	inline CTransform()
		: m_translation(ZERO)
		, m_scale(1.0f, 1.0f, 1.0f)
	{}

	inline CTransform(const Vec3& translation, const CRotation& rotation, const Vec3& scale)
		: m_translation(translation)
		, m_rotation(rotation)
		, m_scale(scale)
	{}

	explicit inline CTransform(const QuatT& transform, const Vec3& scale = Vec3(1.0f, 1.0f, 1.0f))
		: m_translation(transform.t)
		, m_rotation(transform.q)
		, m_scale(scale)
	{}

	explicit inline CTransform(const Matrix34& transform)
		: m_translation(transform.GetTranslation())
		, m_rotation(Matrix33(transform))
		, m_scale(transform.GetScale())
	{}

	inline void SetTranslation(const Vec3& translation)
	{
		m_translation = translation;
	}

	inline Vec3 GetTranslation() const
	{
		return m_translation;
	}

	inline void SetScale(const Vec3& scale)
	{
		m_scale = scale;
	}

	inline Vec3 GetScale() const
	{
		return m_scale;
	}

	inline void SetRotation(const CRotation& rotation)
	{
		m_rotation = rotation;
	}

	inline CRotation GetRotation() const
	{
		return m_rotation;
	}

	inline QuatT ToQuatT() const
	{
		return QuatT(m_rotation.ToQuat(), m_translation);
	}

	inline Matrix34 ToMatrix34() const
	{
		return Matrix34::Create(m_scale, m_rotation.ToQuat(), m_translation);
	}

	inline void Serialize(Serialization::IArchive& archive)
	{
		archive(Serialization::SPosition(m_translation), "translation", "Translation");
		archive(m_rotation, "rotation", "Rotation");
		archive(Serialization::SUniformScale(m_scale), "scale", "Scale");
	}

	inline void operator=(const QuatT& transform)
	{
		m_translation = transform.t;
		m_rotation = transform.q;
		m_scale.Set(m_scale.x, m_scale.y, m_scale.z);
	}

	inline void operator=(const Matrix34& transform)
	{
		m_translation = transform.GetTranslation();
		m_rotation = Matrix33(transform);
		m_scale = transform.GetScale();
	}

	inline bool operator==(const CTransform& rhs) const
	{
		return m_translation == rhs.m_translation && m_rotation == rhs.m_rotation && m_scale == rhs.m_scale;
	}

	inline bool operator!=(const CTransform& rhs) const
	{
		return !(*this == rhs);
	}

	static inline void ReflectType(Schematyc::CTypeDesc<CTransform>& type)
	{
		type.SetGUID("a19ff444-7d11-49ea-80ea-5b629c44f588"_cry_guid);
		type.SetLabel("Transform");
		type.SetDescription("Transform");
		type.AddMember(&CTransform::m_translation, 't', "translation", "Translation", "Translation", Vec3(ZERO));
		type.AddMember(&CTransform::m_rotation, 'r', "rotation", "Rotation", "Rotation", CRotation());
		type.AddMember(&CTransform::m_scale, 's', "scale", "Scale", "Scale", Vec3(1.0f, 1.0f, 1.0f));
	}

private:

	Vec3      m_translation;
	Vec3      m_scale;
	CRotation m_rotation;
};

inline CTransform Multiply(const CTransform& a, const CTransform& b)
{
	const Vec3& sa = a.GetScale();
	const Vec3& sb = b.GetScale();
	return CTransform(a.ToQuatT() * b.ToQuatT(), Vec3(sa.x * sb.x, sa.y * sb.y, sa.z * sb.z));
}

typedef std::shared_ptr<CTransform> CTransformPtr;

} // CryTransform
