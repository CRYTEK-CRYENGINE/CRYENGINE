// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  Created:     04/03/2015 by Filipe amim
//  Description:
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#ifndef PARAMTRAITS_H
#define PARAMTRAITS_H

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
	ILINE friend bool Serialize(Serialization::IArchive& ar, SEnable& val, const char* name, const char* label);
	bool m_value;
};

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

struct BaseTraits
{
	template<typename T> static T HardMin()   { return std::numeric_limits<T>::lowest(); }
	template<typename T> static T HardMax()   { return std::numeric_limits<T>::max(); }
	template<typename T> static T Default()   { return T(); }
	template<typename T> static T Enabled()   { return T(); }
	template<typename T> static T To(T val)   { return RemoveNegZero(val); } // write to value
	template<typename T> static T From(T val) { return RemoveNegZero(val); } // read from value
	static bool HideDefault()                 { return false;  }
	static cstr DefaultName()                 { return "None";  }
};

template<typename T, typename TTraits = BaseTraits>
struct TValue
{
public:
	typedef T TType;

	TValue() { Set(Default()); }
	TValue(T var) { Set(var); }

	void Set(T value)
	{
		m_value = TTraits::To(crymath::clamp(value, HardMin(), HardMax()));
	}

	operator T() const        { return m_value; }
	T Get() const             { return m_value; }
	T operator+() const       { return m_value; }

	static T HardMin()        { return TTraits::template HardMin<T>(); }
	static T HardMax()        { return TTraits::template HardMax<T>(); }
	static T Default()        { return TTraits::template Default<T>(); }
	static T Enabled()        { return TTraits::template Enabled<T>(); }

private:

	friend bool Serialize(Serialization::IArchive& ar, TValue& val, const char* name, const char* label)
	{
		return val.Serialize(ar, name, label);
	}

	bool Serialize(Serialization::IArchive& ar, const char* name, const char* label);

	T m_value;
};

template<int iMin, typename Base = BaseTraits>
struct THardMin: Base
{
	template<typename T> static T HardMin() { return T(iMin); }
};

template<int iMin, int iMax, typename Base = BaseTraits>
struct THardLimits: THardMin<iMin, Base>
{
	template<typename T> static T HardMax() { return T(iMax); }
};

template<typename Base = BaseTraits>
struct TDefaultMin: Base
{
	template<typename T> static T Default() { return Base::template HardMin<T>(); }
	static bool HideDefault()               { return true; }
};

template<typename Base = BaseTraits>
struct TDefaultMax: Base
{
	template<typename T> static T Default() { return Base::template HardMax<T>(); }
	static bool HideDefault()               { return true; }
};

template<typename Base = BaseTraits>
struct TDefaultZero: Base
{
	template<typename T> static T Enabled() { return T(1); }
	static bool HideDefault()               { return true; }
};

template<typename Base = BaseTraits>
struct TDefaultInf: Base
{
	template<typename T> static T HardMax() { return std::numeric_limits<T>::infinity(); }
	template<typename T> static T Default() { return HardMax<T>(); }
	static bool HideDefault()               { return true; }
	static cstr DefaultName()               { return "Infinity"; }
};

template<typename Base = BaseTraits>
struct TPositive: Base
{
	template<typename T> static T HardMin() { return std::numeric_limits<T>::min(); }
};

typedef TValue<float>                             SFloat;
typedef TValue<float, THardMin<0>>                UFloat;

typedef TValue<float, THardMin<0, TDefaultInf<>>> UInfFloat;
typedef TValue<float, TPositive<TDefaultInf<>>>   PInfFloat;

typedef TValue<float, THardLimits<-1, 1>>         SUnitFloat;
typedef TValue<float, THardLimits<0, 1>>          UUnitFloat;

typedef TValue<uint, THardLimits<0, 255>>         UByte;
typedef TValue<uint, THardLimits<1, 256>>         UBytePos;

template<int iTo, int iFrom, typename Base = BaseTraits>
struct ConvertScale: Base
{
	template<typename T> static T To(T val)	  { return Base::To(val * T(iTo) / T(iFrom)); }
	template<typename T> static T From(T val) { return Base::From(val) * T(iFrom) / T(iTo) ; }
};

struct ConvertDegrees: BaseTraits
{
	template<typename T> static T To(T val)	  { return BaseTraits::To(DEG2RAD(val)); }
	template<typename T> static T From(T val) { return BaseTraits::From(RAD2DEG(val)); }
};

typedef TValue<float, ConvertDegrees>                         SAngle;
typedef TValue<float, THardMin<0, ConvertDegrees>>            UAngle;
typedef TValue<float, THardLimits<-360, 360, ConvertDegrees>> SAngle360;
typedef TValue<float, THardLimits<0, 180, ConvertDegrees>>    UAngle180;

// Legacy
typedef SFloat SFloat10;
typedef UFloat UFloat10;
typedef UFloat UFloat100;

}

#include "ParamTraitsImpl.h"

#endif // PARAMTRAITS_H
