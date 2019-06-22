// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "../ParticleCommon.h"
#include "../ParticleSpline.h"
#include <CrySerialization/Enum.h>
#include <CrySerialization/Decorators/Range.h>

namespace pfx2
{

struct SEnable
{
public:
	SEnable() : m_value(true) {}
	ILINE             operator bool() const { return m_value; }
	ILINE void        Set(bool value)       { m_value = value; }
private:
	ILINE friend bool Serialize(Serialization::IArchive& ar, SEnable& val, cstr name, cstr label);
	bool m_value;
};

///////////////////////////////////////////////////////////////////////////////
// Traits

template<typename T>
ILINE T RemoveNegZero(T val)
{
	return val;
}

template<>
ILINE float RemoveNegZero(float val)
{
	return val == -0.0f ? 0.0f : val;
}

template<typename T, typename S = T>
struct TNumber
{
	typedef T TType;
	typedef S TStore;

	static T HardMin()        { return std::numeric_limits<T>::lowest(); }
	static T HardMax()        { return std::numeric_limits<T>::max(); }
	static T SoftMin()        { return std::numeric_limits<T>::is_signed ? max(T(-1000), HardMin()) : HardMin(); }
	static T SoftMax()        { return min(T(+1000), HardMax()); }
	static T MinStep()        { return std::numeric_limits<T>::is_integer ? T(1) : std::numeric_limits<T>::epsilon(); }
	static T Default()        { return T(0); }
	static T NonDefault()     { return MinStep(); }
	static S To(T val)        { return RemoveNegZero(val); } // write to value
	static T From(S val)      { return RemoveNegZero(val); } // read from value
	static bool HideDefault() { return false; }
	static cstr DefaultName() { return "None"; }
	static cstr ValueName()   { return "value"; }
	static cstr ModsName()    { return "modifiers"; }
};

template<typename TTraits>
struct TValue: TTraits
{
	typedef typename TTraits::TType  T;
	typedef typename TTraits::TStore S;

	TValue()            { Set(TTraits::Default()); }
	TValue(T value)     { Set(value); }

	void Set(T value)   { m_value = TTraits::To(clamp(value, TTraits::HardMin(), TTraits::HardMax())); }

	operator S() const  { return m_value; }
	S Get() const       { return m_value; }
	S operator+() const { return m_value; }

private:

	friend bool Serialize(Serialization::IArchive& ar, TValue& val, cstr name, cstr label)
	{
		return val.Serialize(ar, name, label);
	}
	bool Serialize(Serialization::IArchive& ar, cstr name, cstr label);
	static Serialization::RangeDecorator<T> Range(T& value)
	{
		return Serialization::Range(value, TTraits::HardMin(), TTraits::HardMax(), TTraits::SoftMin(), TTraits::SoftMax());
	}

	S m_value;
};

template<typename T, int iMin>
struct THardMin: TNumber<T>
{
	static T HardMin() { return T(iMin); }
	static T SoftMin() { return HardMin(); }
};

template<typename T>
struct TPositive: TNumber<T>
{
	static T HardMin() { return std::numeric_limits<T>::min(); }
	static T SoftMin() { return HardMin(); }
};

template<typename T, int iMin, int iMax>
struct THardLimits: THardMin<T, iMin>
{
	static T HardMax() { return T(iMax); }
	static T SoftMax() { return HardMax(); }
};

template<typename T>
struct THideDefault: TNumber<T>
{
	static bool HideDefault() { return true; }
};

template<typename T>
struct TDefaultMin: THideDefault<T>
{
	static T Default()    { return TNumber<T>::HardMin(); }
	static T NonDefault() { return max(Default() + TNumber<T>::MinStep(), T(0)); }
};

template<typename T>
struct TDefaultMax: THideDefault<T>
{
	static T Default()    { return TNumber<T>::HardMax(); }
	static T NonDefault() { return min(Default() - TNumber<T>::MinStep(), T(0)); }
};

template<typename Base>
struct TDefaultInf: Base
{
	typedef typename Base::TType T;
	static T HardMax()        { return std::numeric_limits<T>::infinity(); }
	static T Default()        { return HardMax(); }
	static bool HideDefault() { return true; }
	static cstr DefaultName() { return "Infinity"; }
};

template<typename Base, int iSoftMax>
struct TSoftLimits: Base
{
	typedef typename Base::TType T;
	static T SoftMin() { return std::numeric_limits<T>::is_signed ? max(T(-iSoftMax), Base::HardMin()) : Base::HardMin(); }
	static T SoftMax() { return min(T(+iSoftMax), Base::HardMax()); }
};

typedef TValue<TNumber<float>>                  SFloat;
typedef TValue<THardMin<float, 0>>              UFloat;

typedef TValue<TDefaultInf<THardMin<float, 0>>> UInfFloat;
typedef TValue<TDefaultInf<TPositive<float>>>   PInfFloat;

typedef TValue<THardLimits<float, -1, 1>>       SUnitFloat;
typedef TValue<THardLimits<float, 0, 1>>        UUnitFloat;

typedef TValue<THardMin<uint, 1>>               PosInt;

typedef TValue<THardLimits<uint, 0, 255>>       UByte;
typedef TValue<THardLimits<uint, 1, 256>>       UBytePos;

typedef TValue<TSoftLimits<SFloat, 10>>         SFloat10;
typedef TValue<TSoftLimits<UFloat, 10>>         UFloat10;
typedef TValue<TSoftLimits<UFloat, 100>>        UFloat100;


template<typename Base, int iTo, int iFrom>
struct ConvertScale: Base
{
	typedef typename Base::TType T;
	static T To(T val)	 { return Base::To(val * T(iTo) / T(iFrom)); }
	static T From(T val) { return Base::From(val) * T(iFrom) / T(iTo) ; }
};

template<typename Base>
struct TDegrees: Base
{
	typedef typename Base::TType T;
	static T To(T val)	 { return TNumber<T>::To(DEG2RAD(val)); }
	static T From(T val) { return TNumber<T>::From(RAD2DEG(val)); }
};

typedef TValue<TDegrees<TNumber<float>>>                SAngle;
typedef TValue<TDegrees<THardMin<float, 0>>>            UAngle;
typedef TValue<TDegrees<THardLimits<float, -360, 360>>> SAngle360;
typedef TValue<TDegrees<THardLimits<float, 0, 180>>>    UAngle180;

struct TColor: TNumber<Vec3, UCol>
{
	static Vec3 HardMin() { return Vec3(0); }
	static Vec3 HardMax() { return Vec3(1); }
	static Vec3 SoftMin() { return HardMin(); }
	static Vec3 SoftMax() { return HardMax(); }

	static UCol To(const Vec3& val)
	{
		UCol result;
		result.r = float_to_ufrac8(val.x);
		result.g = float_to_ufrac8(val.y);
		result.b = float_to_ufrac8(val.z);
		result.a = 0xff;
		return result;
	}
	static Vec3 From(UCol val)
	{
		return Vec3(
			ufrac8_to_float(val.r),
			ufrac8_to_float(val.g),
			ufrac8_to_float(val.b));
	}
	static cstr ValueName()   { return "Color"; }
	static cstr ModsName()    { return "Modifiers"; }
};

typedef TValue<TColor> UColor;

}

#include "ParamTraitsImpl.h"
