// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

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

struct ConvertIdentity
{
	template<typename T>
	static T To(T val)
	{
		return RemoveNegZero(val);
	}
	template<typename T>
	static T From(T val)
	{
		return RemoveNegZero(val);
	}
};

template<int iTo, int iFrom>
struct ConvertScale
{
	template<typename T>
	static T To(T val)
	{
		return RemoveNegZero(val) * T(iTo) / T(iFrom);
	}
	template<typename T>
	static T From(T val)
	{
		return RemoveNegZero(val) * T(iFrom) / T(iTo);
	}
};

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

template<typename T, typename TLimits, typename TConvert = ConvertIdentity>
struct TValue
{
public:
	typedef T TType;

	TValue() { Set(TConvert::To(TType())); }
	TValue(TType var) { Set(var); }
	void Set(TType value)
	{
		if (TLimits::isHardMin)
			value = max(value, HardMin());
		if (TLimits::isHardMax)
			value = min(value, HardMax());
		m_value = value;
	}

	operator TType() const    { return m_value; }
	TType        Get() const  { return m_value; }
	TType        Base() const { return m_value; }
	TType        Min() const  { return m_value; }
	TType        Max() const  { return m_value; }

	static TType HardMin()    { return TLimits::isHardMin ? TType(TLimits::minValue) : std::numeric_limits<TType>::lowest(); }
	static TType HardMax()    { return TLimits::isHardMax ? TType(TLimits::maxValue) : std::numeric_limits<TType>::max(); }
	static TType SoftMin()    { return TType(TLimits::minValue); }
	static TType SoftMax()    { return TType(TLimits::maxValue); }

private:

	friend bool Serialize(Serialization::IArchive& ar, TValue& val, const char* name, const char* label)
	{
		TType v = TConvert::From(val.m_value);
		bool res = ar(
		  Serialization::Range(v, HardMin(), HardMax()),
		  name, label);
		if (ar.isInput())
		{
			if (TLimits::isHardMin)
				v = max(v, HardMin());
			if (TLimits::isHardMax)
				v = min(v, HardMax());
		}
		val.m_value = TConvert::To(v);
		return res;
	}

	TType m_value;
};

// Specify value limits
template<int L, int H>
struct TLimits
{
	static const int minValue = L;
	static const int maxValue = H;
};

template<int L, int H>
struct THardLimits : TLimits<L, H>
{
	static const bool isHardMin = true;
	static const bool isHardMax = true;
};

template<int L, int H>
struct THardMin : TLimits<L, H>
{
	static const bool isHardMin = true;
	static const bool isHardMax = false;
};

template<int L, int H>
struct TSoftLimits : TLimits<L, H>
{
	static const bool isHardMin = false;
	static const bool isHardMax = false;
};

template<int H>
struct SSoftLimit : TSoftLimits<-H, H> {};

template<int H>
struct USoftLimit : TLimits<0, H>
{
	static const bool isHardMin = true;
	static const bool isHardMax = false;
};

typedef TValue<float, SSoftLimit<1>>      SFloat;
typedef TValue<float, SSoftLimit<10>>     SFloat10;
typedef TValue<float, USoftLimit<1>>      UFloat;
typedef TValue<float, USoftLimit<10>>     UFloat10;
typedef TValue<float, USoftLimit<100>>    UFloat100;

typedef TValue<float, THardLimits<-1, 1>> SUnitFloat;
typedef TValue<float, THardLimits<0, 1>>  UUnitFloat;

typedef TValue<uint, THardLimits<0, 255>> UByte;
typedef TValue<uint, THardLimits<1, 256>> UBytePos;

struct ConvertDegrees
{
	static float From(float val) { return RemoveNegZero(RAD2DEG(val)); }
	static float To(float val)   { return RemoveNegZero(DEG2RAD(val)); }
};
typedef TValue<float, SSoftLimit<360>, ConvertDegrees>        SAngle;
typedef TValue<float, USoftLimit<360>, ConvertDegrees>        UAngle;
typedef TValue<float, THardLimits<-360, 360>, ConvertDegrees> SAngle360;
typedef TValue<float, THardLimits<0, 180>, ConvertDegrees>    UAngle180;

struct ConvertInfinite
{
	static float From(float val)
	{
		return val < std::numeric_limits<float>::max() ? val : 0.0f;
	}
	static float To(float val)
	{ 
		return val != 0.0f ? val : gInfinity; 
	}
};
typedef TValue<float, USoftLimit<100>, ConvertInfinite>       UFloatInf;

}

#include "ParamTraitsImpl.h"

#endif // PARAMTRAITS_H
