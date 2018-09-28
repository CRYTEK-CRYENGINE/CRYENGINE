// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _environment_preset_h_
#define _environment_preset_h_
#pragma once

#include <CryMath/Bezier.h>

// PERSONAL NOTE: Looks like the 'time' here is actual time, not a pseudo-time value e.g. 'wind strength' and so on.
class CBezierSpline
{
public:
	CBezierSpline();
	~CBezierSpline();

	void              Init(float fDefaultValue);
	float             Evaluate(const CTimeValue& t) const;

	void              SetKeys(const SBezierKey* keysArray, unsigned int keysArraySize) { m_keys.resize(keysArraySize); std::copy(keysArray, keysArray + keysArraySize, &m_keys[0]); }
	void              GetKeys(SBezierKey* keys) const                                  { std::copy(&m_keys[0], &m_keys[0] + m_keys.size(), keys); }

	void              InsertKey(const CTimeValue& time, float value);
	void              UpdateKeyForTime(const CTimeValue& fTime, float value);

	void              Resize(size_t nSize)        { m_keys.resize(nSize); }

	size_t            GetKeyCount() const         { return m_keys.size(); }
	const SBezierKey& GetKey(size_t nIndex) const { return m_keys[nIndex]; }
	SBezierKey&       GetKey(size_t nIndex)       { return m_keys[nIndex]; }

	void              Serialize(Serialization::IArchive& ar);
private:
	typedef std::vector<SBezierKey> TKeyContainer;
	TKeyContainer m_keys;

	struct SCompKeyTime
	{
		bool operator()(const TKeyContainer::value_type& l, const TKeyContainer::value_type& r) const { return l.m_time < r.m_time; }
		bool operator()(const CTimeValue& l, const TKeyContainer::value_type& r) const                { return l < r.m_time; }
		bool operator()(const TKeyContainer::value_type& l, const CTimeValue& r) const                { return l.m_time < r; }
	};
};

//////////////////////////////////////////////////////////////////////////
class CTimeOfDayVariable
{
public:
	CTimeOfDayVariable();
	~CTimeOfDayVariable();

	void                          Init(const char* group, const char* displayName, const char* name, ITimeOfDay::ETimeOfDayParamID nParamId, ITimeOfDay::EVariableType type, float defVal0, float defVal1, float defVal2);
	void                          Update(const CTimeValue& time);

	Vec3                          GetInterpolatedAt(const CTimeValue& t) const;

	ITimeOfDay::ETimeOfDayParamID GetId() const          { return m_id; }
	ITimeOfDay::EVariableType     GetType() const        { return m_type; }
	const char*                   GetName() const        { return m_name; }
	const char*                   GetDisplayName() const { return m_displayName; }
	const char*                   GetGroupName() const   { return m_group; }
	const Vec3                    GetValue() const       { return m_value; }

	float                         GetMinValue() const    { return m_minValue; }
	float                         GetMaxValue() const    { return m_maxValue; }

	const CBezierSpline*          GetSpline(int nIndex) const
	{
		if (nIndex >= 0 && nIndex < Vec3::component_count)
			return &m_spline[nIndex];
		else
			return NULL;
	}

	CBezierSpline* GetSpline(int nIndex)
	{
		if (nIndex >= 0 && nIndex < Vec3::component_count)
			return &m_spline[nIndex];
		else
			return NULL;
	}

	size_t GetSplineKeyCount(int nSpline) const;
	bool   GetSplineKeys(int nSpline, SBezierKey* keysArray, unsigned int keysArraySize) const;
	bool   SetSplineKeys(int nSpline, const SBezierKey* keysArray, unsigned int keysArraySize);
	bool   UpdateSplineKeyForTime(int nSpline, const CTimeValue& fTime, float newKey);

	void   Serialize(Serialization::IArchive& ar);
private:
	ITimeOfDay::ETimeOfDayParamID m_id;
	ITimeOfDay::EVariableType     m_type;

	const char*                   m_name;        // Variable name.
	const char*                   m_displayName; // Variable user readable name.
	const char*                   m_group;       // Group name.

	float                         m_minValue;
	float                         m_maxValue;

	Vec3                          m_value;
	CBezierSpline                 m_spline[Vec3::component_count]; //spline for each component in m_value
};

//////////////////////////////////////////////////////////////////////////
class CEnvironmentPreset
{
public:
	CEnvironmentPreset();
	~CEnvironmentPreset();

	void                      ResetVariables();
	void                      Update(const CTimeValue& t);

	const CTimeOfDayVariable* GetVar(ITimeOfDay::ETimeOfDayParamID id) const { return &m_vars[id]; }
	CTimeOfDayVariable*       GetVar(ITimeOfDay::ETimeOfDayParamID id)       { return &m_vars[id]; }
	CTimeOfDayVariable*       GetVar(const char* varName);
	bool                      InterpolateVarInRange(ITimeOfDay::ETimeOfDayParamID id, const CTimeValue& fMin, const CTimeValue& fMax, unsigned int nCount, Vec3* resultArray) const;

	void                      Serialize(Serialization::IArchive& ar);

	static int                GetAnimTimeSecondsIn24h();
private:
	void                      AddVar(const char* group, const char* displayName, const char* name, ITimeOfDay::ETimeOfDayParamID nParamId, ITimeOfDay::EVariableType type, float defVal0, float defVal1, float defVal2);

	CTimeOfDayVariable m_vars[ITimeOfDay::PARAM_TOTAL];
};

#endif //_environment_preset_h_
