// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySerialization/IArchive.h>

#include <CryMath/Cry_Vector2.h>
#include <CryMath/Cry_Vector3.h>
#include <CryMath/Cry_Vector4.h>
#include <CryMath/Cry_Quat.h>
#include <CryMath/Cry_Matrix33.h>
#include <CryMath/Cry_Matrix34.h>
#include <CryMath/Cry_Matrix44.h>
#include <CryMath/Cry_Geo.h>
#include <CrySerialization/Gap.h>
#include <CrySerialization/Decorators/Range.h>

namespace Serialization
{
	template<typename T>
	struct SSerializeVec2
	{
		Vec2_tpl<T>& value;
		SSerializeVec2(Vec2_tpl<T>& v) : value(v) {}
		void Serialize(Serialization::IArchive& ar)
		{
			ar(value.x, "x", "x");
			ar(value.y, "y", "y");
		}
	};
	template<typename T>
	struct SSerializeVec3
	{
		Vec3_tpl<T>& value;
		SSerializeVec3(Vec3_tpl<T>& v) : value(v) {}
		void Serialize(Serialization::IArchive& ar)
		{
			ar(value.x, "x", "x");
			ar(value.y, "y", "y");
			ar(value.z, "z", "z");
		}
	};
	template<typename T>
	struct SSerializeVec4
	{
		Vec4_tpl<T>& value;
		SSerializeVec4(Vec4_tpl<T>& v) : value(v) {}
		void Serialize(Serialization::IArchive& ar)
		{
			ar(value.x, "x", "x");
			ar(value.y, "y", "y");
			ar(value.z, "z", "z");
			ar(value.w, "w", "w");
		}
	};

#ifdef CRY_HARDWARE_VECTOR4
	template<typename T>
	struct SSerializeVec4H
	{
		Vec4H<T>& value;
		SSerializeVec4H(Vec4H<T>& v) : value(v) {}
		void Serialize(Serialization::IArchive& ar)
		{
			ar(value.x, "x", "x");
			ar(value.y, "y", "y");
			ar(value.z, "z", "z");
			ar(value.w, "w", "w");
		}
	};
#endif

	template<typename T>
	struct SSerializeQuat
	{
		Quat_tpl<T>& value;
		SSerializeQuat(Quat_tpl<T>& v) : value(v) {}
		void Serialize(Serialization::IArchive& ar)
		{
			ar(value.v.x, "x", "x");
			ar(value.v.y, "y", "y");
			ar(value.v.z, "z", "z");
			ar(value.w, "w", "w");
		}
	};
}

template<typename T>
bool Serialize(Serialization::IArchive& ar, Vec2_tpl<T>& value, const char* name, const char* label)
{
	if (!ar.isEdit() && !ar.caps(Serialization::IArchive::XML_VERSION_1))
	{
		return ar(Serialization::SStruct(Serialization::SSerializeVec2<T>(value)), name, label);
	}
	else
	{
		typedef T(&Array)[2];
		return ar((Array)value, name, label);
	}
}

template<typename T>
bool Serialize(Serialization::IArchive& ar, Vec3_tpl<T>& value, const char* name, const char* label)
{
	if (!ar.isEdit() && !ar.caps(Serialization::IArchive::XML_VERSION_1))
	{
		return ar(Serialization::SStruct(Serialization::SSerializeVec3<T>(value)), name, label);
	}
	else
	{
		typedef T (& Array)[3];
		return ar((Array)value, name, label);
	}
}

template<typename T>
inline bool Serialize(Serialization::IArchive& ar, struct Vec4_tpl<T>& value, const char* name, const char* label)
{
	if (!ar.isEdit() && !ar.caps(Serialization::IArchive::XML_VERSION_1))
	{
		return ar(Serialization::SStruct(Serialization::SSerializeVec4<T>(value)), name, label);
	}
	else
	{
		typedef T (& Array)[4];
		return ar((Array)value, name, label);
	}
}

#ifdef CRY_TYPE_SIMD4
template<typename T>
inline bool Serialize(Serialization::IArchive& ar, Vec4H<T>& value, const char* name, const char* label)
{
	if (!ar.isEdit() && !ar.caps(Serialization::IArchive::XML_VERSION_1))
	{
		return ar(Serialization::SStruct(Serialization::SSerializeVec4H<T>(value)), name, label);
	}
	else
	{
		typedef T (& Array)[4];
		return ar((Array)value, name, label);
	}
}
#endif

template<typename T>
bool Serialize(Serialization::IArchive& ar, Quat_tpl<T>& value, const char* name, const char* label)
{
	if (ar.isEdit())
	{
		typedef T (& Array)[3];
		Vec3_tpl<T> v = Vec3_tpl<T>(RAD2DEG(Ang3_tpl<T>::GetAnglesXYZ(Matrix33_tpl<T>(value))));
		bool result = ar((Array)v, name, label);
		if (ar.isInput())
			value = Quat_tpl<T>(Ang3_tpl<T>(DEG2RAD(v)));
		return result;
	}
	else if (!ar.caps(Serialization::IArchive::XML_VERSION_1))
	{
		return ar(Serialization::SStruct(Serialization::SSerializeQuat<T>(value)), name, label);
	}
	else
	{
		typedef T (& Array)[4];
		return ar((Array)value, name, label);
	}
}

template<typename T>
struct SerializableQuatT : QuatT_tpl<T>
{
	void Serialize(Serialization::IArchive& ar)
	{
		ar(this->q, "q", "Quaternion");
		ar(this->t, "t", "Translation");
	}
};

template<typename T>
bool Serialize(Serialization::IArchive& ar, struct QuatT_tpl<T>& value, const char* name, const char* label)
{
	return Serialize(ar, static_cast<SerializableQuatT<T>&>(value), name, label);
}

struct SerializableAABB : AABB
{
	void Serialize(Serialization::IArchive& ar)
	{
		ar(this->min, "min", "Min");
		ar(this->max, "max", "Max");
	}
};

inline bool Serialize(Serialization::IArchive& ar, struct AABB& value, const char* name, const char* label)
{
	return Serialize(ar, static_cast<SerializableAABB&>(value), name, label);
}

template<typename T>
bool Serialize(Serialization::IArchive& ar, Matrix33_tpl<T>& value, const char* name, const char* label)
{
	typedef T(&Array)[9];
	return ar((Array)value, name, label);
}

template<typename T>
bool Serialize(Serialization::IArchive& ar, Matrix34_tpl<T>& value, const char* name, const char* label)
{
	typedef T (& Array)[12];
	return ar((Array)value, name, label);
}

#ifdef CRY_TYPE_SIMD4
template<typename T>
bool Serialize(Serialization::IArchive& ar, Matrix34H<T>& value, const char* name, const char* label)
{
	typedef T (& Array)[12];
	return ar((Array)value, name, label);
}
#endif

template<typename T>
bool Serialize(Serialization::IArchive& ar, Matrix44_tpl<T>& value, const char* name, const char* label)
{
	typedef T(&Array)[16];
	return ar((Array)value, name, label);
}


#ifdef CRY_TYPE_SIMD4
template<typename T>
bool Serialize(Serialization::IArchive& ar, Matrix44H<T>& value, const char* name, const char* label)
{
	typedef T(&Array)[16];
	return ar((Array)value, name, label);
}
#endif

//////////////////////////////////////////////////////////////////////////

namespace Serialization
{
struct SRotation
{
	explicit SRotation(Quat& q)
		: quat(q)
	{}

	void Serialize(Serialization::IArchive& ar)
	{
		if (ar.isEdit())
		{
			Vec3 v = Vec3(RAD2DEG(Ang3::GetAnglesXYZ(Matrix33(quat))));
			ar(Serialization::MinMaxRange(v.x, 1.0f), "x", "^");
			ar(Serialization::MinMaxRange(v.y, 1.0f), "y", "^");
			ar(Serialization::MinMaxRange(v.z, 1.0f), "z", "^");
			if (ar.isInput())
				quat = Quat(Ang3(DEG2RAD(v)));

			ar(Serialization::SGap(), "gap", "^");
		}
		else
		{
			typedef float(&Array)[4];
			ar((Array)quat, "rotation", "Rotation");
		}
	}

	Quat& quat;
};

struct SPosition
{
	explicit SPosition(Vec3& v)
		: vec(v)
	{}

	void Serialize(Serialization::IArchive& ar)
	{
		if (ar.isEdit())
		{
			ar(Serialization::MinMaxRange(vec.x), "x", "^");
			ar(Serialization::MinMaxRange(vec.y), "y", "^");
			ar(Serialization::MinMaxRange(vec.z), "z", "^");
			ar(Serialization::SGap(), "gap", "^");
		}
		else
		{
			typedef float(*Array)[3];
			ar(*((Array)& vec.x), "position", "Position");
		}
	}

	Vec3& vec;
};

struct SScale
{
	explicit SScale(Vec3& v)
		: vec(v)
	{}

	void Serialize(Serialization::IArchive& ar)
	{
		if (ar.isEdit())
		{
			ar(Serialization::MinMaxRange(vec.x), "x", "^");
			ar(Serialization::MinMaxRange(vec.y), "y", "^");
			ar(Serialization::MinMaxRange(vec.z), "z", "^");
		}
		else
		{
			typedef float(*Array)[3];
			ar(*((Array)& vec.x), "scale", "Scale");
		}
	}

	Vec3& vec;
};

struct SUniformScale
{
	explicit SUniformScale(Vec3& v)
		: vec(v)
		, uniform(false)
	{}

	void Serialize(Serialization::IArchive& ar)
	{
	}

	Vec3& vec;
	bool uniform;
};

inline bool Serialize(Serialization::IArchive& ar, SUniformScale& c, const char* name, const char* label)
{
	if (!ar.isEdit())
	{
		typedef float(*Array)[3];
		return ar(*((Array)& c.vec.x), name, label);
	}
	else if(ar.openBlock(name, label))
	{
		bool result = true;

		Vec3 vec = c.vec;
		ar(Serialization::MinMaxRange(vec.x), "x", "^");
		ar(Serialization::MinMaxRange(vec.y), "y", "^");
		ar(Serialization::MinMaxRange(vec.z), "z", "^");
		result = ar(Serialization::SStruct(c), "scale", "^");

		if (ar.isInput() && c.uniform)
		{
			if (!strcmp(ar.getModifiedRowName(), "x"))
			{
				float multiplier = c.vec.x != 0 ? vec.x / c.vec.x : 0;
				c.vec.x = vec.x;
				c.vec.y *= multiplier;
				c.vec.z *= multiplier;
			}
			else if (!strcmp(ar.getModifiedRowName(), "y"))
			{
				float multiplier = c.vec.y != 0 ? vec.y / c.vec.y : 0;
				c.vec.x *= multiplier;
				c.vec.y = vec.y;
				c.vec.z *= multiplier;
			}
			else if (!strcmp(ar.getModifiedRowName(), "z"))
			{
				float multiplier = c.vec.z != 0 ? vec.z / c.vec.z : 0;
				c.vec.x *= multiplier;
				c.vec.y *= multiplier;
				c.vec.z = vec.z;
			}
		}
		else
		{
			c.vec.x = vec.x;
			c.vec.y = vec.y;
			c.vec.z = vec.z;
		}
		ar.closeBlock();
		return result;
	}
	return false;
}

template<class T>
bool SerializeRadiansAsDeg(Serialization::IArchive& ar, T& radians, const char* name, const char* label)
{
	if (ar.isEdit()) 
	{
		float degrees = RAD2DEG(radians);
		const float oldDegrees = degrees;
		if (!ar(degrees, name, label))
			return false;
		if (oldDegrees != degrees)
			radians = DEG2RAD(degrees);
		return true;
	}
	else
		return ar(radians, name, label);
}

template<class T>
bool Serialize(Serialization::IArchive& ar, Serialization::SRadiansAsDeg<T>& value, const char* name, const char* label)
{
	return SerializeRadiansAsDeg(ar, *value.radians, name, label);
}

template<class T>
bool Serialize(Serialization::IArchive& ar, Serialization::SRadiansWithRangeAsDeg<T>& value, const char* name, const char* label)
{
	if (!SerializeRadiansAsDeg(ar, *value.radians, name, label))
		return false;

	if (ar.isInput())
	{
		*value.radians = clamp_tpl(*value.radians, value.hardMin, value.hardMax);
	}
	return true;
}

template<class T>
bool Serialize(Serialization::IArchive& ar, Serialization::SRadianAng3AsDeg<T>& value, const char* name, const char* label)
{
	if (ar.isEdit())
	{
		Ang3 degrees(RAD2DEG(value.ang3->x), RAD2DEG(value.ang3->y), RAD2DEG(value.ang3->z));
		Ang3 oldDegrees = degrees;
		if (!ar(degrees, name, label))
			return false;
		if (oldDegrees != degrees)
			*value.ang3 = Ang3(DEG2RAD(degrees.x), DEG2RAD(degrees.y), DEG2RAD(degrees.z));
		return true;
	}
	else
		return ar(*value.ang3, name, label);
}

}

//////////////////////////////////////////////////////////////////////////

namespace Serialization
{

template<class T>
bool SerializeRadiusAsDiameter(Serialization::IArchive& ar, T* radius, const char* name, const char* label)
{
	if (ar.isEdit())
	{
		T diameter = *radius * T(2);
		T oldDiameter = diameter;
		if (!ar(diameter, name, label))
			return false;
		if (oldDiameter != diameter)
			*radius = diameter / T(2);
		return true;
	}
	else
		return ar(*radius, name, label);
}

template<class T>
bool Serialize(Serialization::IArchive& ar, Serialization::SRadiusAsDiameter<T>& value, const char* name, const char* label)
{
	return SerializeRadiusAsDiameter(ar, value.radius, name, label);
}

template<class T>
bool Serialize(Serialization::IArchive& ar, Serialization::SRadiusWithRangeAsDiameter<T>& value, const char* name, const char* label)
{
	if (!SerializeRadiusAsDiameter(ar, value.radius, name, label))
		return false;

	if (ar.isInput())
	{
		if (*value.radius < value.hardMin)
			*value.radius = value.hardMin;
		if (*value.radius > value.hardMax)
			*value.radius = value.hardMax;
	}
	return true;
}
}

//////////////////////////////////////////////////////////////////////////

template<typename T>
bool Serialize(Serialization::IArchive& ar, Ang3_tpl<T>& value, const char* name, const char* label)
{
	typedef T (& Array)[3];
	return ar((Array)value, name, label);
}

//////////////////////////////////////////////////////////////////////////

namespace Serialization
{

template<class T>
bool Serialize(Serialization::IArchive& ar, Serialization::QuatAsAng3<T>& value, const char* name, const char* label)
{
	if (ar.isEdit())
	{
		Ang3 ang3(*value.quat);
		Ang3 oldAng3 = ang3;
		if (!ar(Serialization::RadiansAsDeg(ang3), name, label))
			return false;
		if (ang3 != oldAng3)
			*value.quat = Quat(ang3);
		return true;
	}
	else
		return ar(*value.quat, name, label);
}

template<class T>
bool Serialize(Serialization::IArchive& ar, Serialization::QuatTAsVec3Ang3<T>& value, const char* name, const char* label)
{
	if (ar.isEdit())
	{
		if (!ar.openBlock(name, label))
			return false;

		ar(QuatAsAng3<T>((value.trans)->q), "rot", "Rotation");
		ar.doc("Euler Angles in degrees");
		ar((value.trans)->t, "t", "Translation");
		ar.closeBlock();
		return true;
	}
	else
	{
		return ar(*(value.trans), name, label);
	}
}

}
