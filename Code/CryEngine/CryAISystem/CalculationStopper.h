// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   -------------------------------------------------------------------------
   File name:   CalculationStopper.h
   $Id$
   Description: Temp file holding code extracted from CAISystem.h/cpp

   -------------------------------------------------------------------------
   History:

 *********************************************************************/

#ifndef _CALCULATIONSHOPPER_H_
#define _CALCULATIONSHOPPER_H_

//#define CALIBRATE_STOPPER
//#define DEBUG_STOPPER
//#define STOPPER_CAN_USE_COUNTER

/// Used to determine when a calculation should stop. Normally this would be after a certain time (dt). However,
/// if m_useCounter has been set then the times are internally converted to calls to ShouldCalculationStop using
/// callRate, which should have been estimated previously
class CCalculationStopper
{
public:
	/// name is used during calibration. fCalculationTime is the amount of time (in seconds).
	/// fCallsPerSecond is for when we are running in "counter" mode - the time
	/// gets converted into a number of calls to ShouldCalculationStop
	CCalculationStopper(const char* szName, float fCalculationTime, float fCallsPerSecond);
	bool  ShouldCalculationStop() const;
	float GetSecondsRemaining() const;

#ifdef STOPPER_CAN_USE_COUNTER
	static bool m_useCounter;
#endif

private:
	CTimeValue m_endTime;

#ifdef STOPPER_CAN_USE_COUNTER
	mutable unsigned m_stopCounter; // if > 0 use this - stop when it's 0
	float            m_fCallsPerSecond;
#endif // STOPPER_CAN_USE_COUNTER

#ifdef DEBUG_STOPPER
	static bool m_neverStop; // for debugging
#endif

#ifdef CALIBRATE_STOPPER
public:
	mutable unsigned m_calls;
	float            m_dt;
	string           m_name;
	/// the pair is calls and time (seconds)
	typedef std::map<string, std::pair<unsigned, float>> TMapCallRate;
	static TMapCallRate m_mapCallRate;
#endif
};

#endif
