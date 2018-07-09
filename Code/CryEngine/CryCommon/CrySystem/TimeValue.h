// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

class CTimeValue
{
public:
	static const int64 TIMEVALUE_PRECISION = 100000; //!< One second.

public:
	void GetMemoryUsage(class ICrySizer* pSizer) const { /*nothing*/ }

	ILINE CTimeValue()
	{
		m_lValue = 0;
	}

	ILINE CTimeValue(const float fSeconds)
	{
		SetSeconds(fSeconds);
	}

	ILINE CTimeValue(const double fSeconds)
	{
		SetSeconds(fSeconds);
	}

	//! \param inllValue Positive negative, absolute or relative in 1 second= TIMEVALUE_PRECISION units.
	ILINE CTimeValue(const int64 inllValue)
	{
		m_lValue = inllValue;
	}

	//! Copy constructor.
	ILINE CTimeValue(const CTimeValue& inValue)
	{
		m_lValue = inValue.m_lValue;
	}

	ILINE ~CTimeValue() {}

	//! Assignment operator.
	//! \param inRhs Right side.
	ILINE CTimeValue& operator=(const CTimeValue& inRhs)
	{
		m_lValue = inRhs.m_lValue;
		return *this;
	};

	//! Use only for relative value, absolute values suffer a lot from precision loss.
	ILINE float GetSeconds() const
	{
		return m_lValue * (1.f / TIMEVALUE_PRECISION);
	}

	//! Get relative time difference in seconds.
	//! Call this on the endTime object: endTime.GetDifferenceInSeconds( startTime );
	ILINE float GetDifferenceInSeconds(const CTimeValue& startTime) const
	{
		return (m_lValue - startTime.m_lValue) * (1.f / TIMEVALUE_PRECISION);
	}

	ILINE void SetSeconds(const float infSec)
	{
		m_lValue = (int64)(infSec * TIMEVALUE_PRECISION);
	}

	ILINE void SetSeconds(const double infSec)
	{
		m_lValue = (int64)(infSec * TIMEVALUE_PRECISION);
	}

	ILINE void SetSeconds(const int64 indwSec)
	{
		m_lValue = indwSec * TIMEVALUE_PRECISION;
	}

	ILINE void SetMilliSeconds(const int64 indwMilliSec)
	{
		m_lValue = indwMilliSec * (TIMEVALUE_PRECISION / 1000);
	}

	//! Use only for relative value, absolute values suffer a lot from precision loss.
	ILINE float GetMilliSeconds() const
	{
		return m_lValue * (1000.f / TIMEVALUE_PRECISION);
	}

	ILINE int64 GetMilliSecondsAsInt64() const
	{
		return m_lValue * 1000 / TIMEVALUE_PRECISION;
	}

	ILINE int64 GetMicroSecondsAsInt64() const
	{
		return m_lValue * (1000 * 1000) / TIMEVALUE_PRECISION;
	}

	ILINE int64 GetValue() const
	{
		return m_lValue;
	}

	ILINE void SetValue(int64 val)
	{
		m_lValue = val;
	}

	ILINE void AddValueThreadSafe(int64 val)
	{
		CryInterlockedAdd(&m_lValue, val);
	}

	//! Useful for periodic events (e.g. water wave, blinking).
	//! Changing TimePeriod can results in heavy changes in the returned value.
	//! \return [0..1]
	float GetPeriodicFraction(const CTimeValue TimePeriod) const
	{
		// todo: change float implement to int64 for more precision
		float fAbs = GetSeconds() / TimePeriod.GetSeconds();
		return fAbs - (int)(fAbs);
	}

	// math operations

	//! Binary subtraction.
	ILINE CTimeValue operator-(const CTimeValue& inRhs) const { CTimeValue ret; ret.m_lValue = m_lValue - inRhs.m_lValue; return ret; };

	//! Binary addition.
	ILINE CTimeValue operator+(const CTimeValue& inRhs) const { CTimeValue ret; ret.m_lValue = m_lValue + inRhs.m_lValue; return ret;  };

	//! Sign inversion.
	ILINE CTimeValue  operator-() const                   { CTimeValue ret; ret.m_lValue = -m_lValue; return ret; };

	ILINE CTimeValue& operator+=(const CTimeValue& inRhs) { m_lValue += inRhs.m_lValue; return *this; }
	ILINE CTimeValue& operator-=(const CTimeValue& inRhs) { m_lValue -= inRhs.m_lValue; return *this; }

	ILINE CTimeValue& operator/=(int inRhs)               { m_lValue /= inRhs; return *this; }

	// comparison -----------------------

	ILINE bool operator<(const CTimeValue& inRhs) const  { return m_lValue < inRhs.m_lValue; };
	ILINE bool operator>(const CTimeValue& inRhs) const  { return m_lValue > inRhs.m_lValue; };
	ILINE bool operator>=(const CTimeValue& inRhs) const { return m_lValue >= inRhs.m_lValue; };
	ILINE bool operator<=(const CTimeValue& inRhs) const { return m_lValue <= inRhs.m_lValue; };
	ILINE bool operator==(const CTimeValue& inRhs) const { return m_lValue == inRhs.m_lValue; };
	ILINE bool operator!=(const CTimeValue& inRhs) const { return m_lValue != inRhs.m_lValue; };

	AUTO_STRUCT_INFO;

	void GetMemoryStatistics(class ICrySizer* pSizer) const { /*nothing*/ }

private:
	int64 m_lValue;     //!< Absolute or relative value in 1/TIMEVALUE_PRECISION, might be negative.

	friend class CTimer;
};
