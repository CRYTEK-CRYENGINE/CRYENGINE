// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "TimeOfDayConstants.h"
#include <CryMath/Bezier.h>

class CBezierSpline
{
public:
	CBezierSpline();

	void              Init(float fDefaultValue);
	float             Evaluate(float t) const;

	void              SetKeys(const SBezierKey* keysArray, unsigned int keysArraySize) { m_keys.resize(keysArraySize); memcpy(&m_keys[0], keysArray, keysArraySize * sizeof(SBezierKey)); }
	void              GetKeys(SBezierKey* keys) const                                  { memcpy(keys, &m_keys[0], m_keys.size() * sizeof(SBezierKey)); }

	void              InsertKey(SAnimTime time, float value);
	void              UpdateKeyForTime(float fTime, float value);

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
		bool operator()(SAnimTime l, const TKeyContainer::value_type& r) const                        { return l < r.m_time; }
		bool operator()(const TKeyContainer::value_type& l, SAnimTime r) const                        { return l.m_time < r; }
	};
};

//////////////////////////////////////////////////////////////////////////
class CTimeOfDayVariable
{
public:
	CTimeOfDayVariable();

	void                          Init(const char* group, const char* displayName, const char* name, ITimeOfDay::ETimeOfDayParamID nParamId, ITimeOfDay::EVariableType type, float defVal0, float defVal1, float defVal2);
	void                          Update(float time);

	Vec3                          GetInterpolatedAt(float t) const;

	ITimeOfDay::ETimeOfDayParamID GetId() const                  { return m_id; }
	ITimeOfDay::EVariableType     GetType() const                { return m_type; }
	const char*                   GetName() const                { return m_name; }
	const char*                   GetDisplayName() const         { return m_displayName; }
	const char*                   GetGroupName() const           { return m_group; }
	Vec3                          GetValue() const               { return m_value; }
	void                          SetValue(const Vec3& newValue) { m_value = newValue; }

	float                         GetMinValue() const            { return m_minValue; }
	float                         GetMaxValue() const            { return m_maxValue; }

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
	bool   UpdateSplineKeyForTime(int nSpline, float fTime, float newKey);

	void   Serialize(Serialization::IArchive& ar);

	void   SetValuesFrom(const CTimeOfDayVariable& var);
private:
	ITimeOfDay::ETimeOfDayParamID m_id;
	ITimeOfDay::EVariableType     m_type;

	const char*                   m_name;       // Variable name.
	const char*                   m_displayName;// Variable user readable name.
	const char*                   m_group;      // Group name.

	float                         m_minValue;
	float                         m_maxValue;

	Vec3                          m_value;
	CBezierSpline                 m_spline[Vec3::component_count]; //spline for each component in m_value
};

class CTimeOfDayVariables : public ITimeOfDay::IVariables
{
public:
	CTimeOfDayVariables();

	virtual void              Reset() override;
	virtual int               GetVariableCount() override;
	virtual bool              GetVariableInfo(int nIndex, ITimeOfDay::SVariableInfo& varInfo) override;
	virtual bool              InterpolateVarInRange(int nIndex, float fMin, float fMax, unsigned int nCount, Vec3* resultArray) const override;
	virtual uint              GetSplineKeysCount(int nIndex, int nSpline) const override;
	virtual bool              GetSplineKeysForVar(int nIndex, int nSpline, SBezierKey* keysArray, unsigned int keysArraySize) const override;
	virtual bool              SetSplineKeysForVar(int nIndex, int nSpline, const SBezierKey* keysArray, unsigned int keysArraySize) override;
	virtual bool              UpdateSplineKeyForVar(int nIndex, int nSpline, float fTime, float newValue) override;

	const CTimeOfDayVariable* GetVar(ITimeOfDay::ETimeOfDayParamID id) const { return &m_vars[id]; }
	CTimeOfDayVariable*       GetVar(ITimeOfDay::ETimeOfDayParamID id)       { return &m_vars[id]; }
	CTimeOfDayVariable*       GetVar(const char* varName);
	void                      Update(float t);
	bool                      InterpolateVarInRange(ITimeOfDay::ETimeOfDayParamID id, float fMin, float fMax, unsigned int nCount, Vec3* resultArray) const;

private:
	void AddVar(const char* group, const char* displayName, const char* name, ITimeOfDay::ETimeOfDayParamID nParamId, ITimeOfDay::EVariableType type, float defVal0, float defVal1, float defVal2);
	CTimeOfDayVariable m_vars[ITimeOfDay::PARAM_TOTAL];
};

class CEnvironmentPreset : public ITimeOfDay::IPreset
{
public:
	virtual ITimeOfDay::IVariables& GetVariables() override;
	virtual ITimeOfDay::IConstants& GetConstants() override;
	virtual void                    Reset() override;

	const CTimeOfDayVariable*       GetVar(ITimeOfDay::ETimeOfDayParamID id) const { return m_variables.GetVar(id); }
	CTimeOfDayVariable*             GetVar(ITimeOfDay::ETimeOfDayParamID id)       { return m_variables.GetVar(id); }
	CTimeOfDayVariable*             GetVar(const char* varName)                    { return m_variables.GetVar(varName); }
	void                            Update(float normalizedTime);

	void                            Serialize(Serialization::IArchive& ar);
	void                            ReadVariablesFromXML(Serialization::IArchive& ar, CTimeOfDayVariables& variables, const bool bConvertLegacyVersion);

	static const float              GetAnimTimeSecondsIn24h();

private:
	CTimeOfDayVariables m_variables;
	STimeOfDayConstants m_constants;
};
