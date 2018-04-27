// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#if !defined(AFX_XCONSOLEVARIABLE_H__AB510BA3_4D53_4C45_A2A0_EA15BABE0C34__INCLUDED_)
#define AFX_XCONSOLEVARIABLE_H__AB510BA3_4D53_4C45_A2A0_EA15BABE0C34__INCLUDED_

#if _MSC_VER > 1000
	#pragma once
#endif // _MSC_VER > 1000

#include <CrySystem/ISystem.h>
#include <CrySystem/CVarOverride.h>
#include <CryCore/BitFiddling.h>
#include <CryCore/SFunctor.h>

class CXConsole;

inline int64 TextToInt64(const char* s, int64 nCurrent, bool bBitfield)
{
	int64 nValue = 0;
	if (s)
	{
		char* e;
		if (bBitfield)
		{
			// Bit manipulation.
			if (*s == '^')
			// Bit number
#if defined(_MSC_VER)
				nValue = 1LL << _strtoi64(++s, &e, 10);
#else
				nValue = 1LL << strtoll(++s, &e, 10);
#endif
			else
			// Full number
#if defined(_MSC_VER)
				nValue = _strtoi64(s, &e, 10);
#else
				nValue = strtoll(s, &e, 10);
#endif
			// Check letter codes.
			for (; (*e >= 'a' && *e <= 'z') || (*e >= 'A' && *e <= 'Z'); e++)
				nValue |= AlphaBit64(*e);

			if (*e == '+')
				nValue = nCurrent | nValue;
			else if (*e == '-')
				nValue = nCurrent & ~nValue;
			else if (*e == '^')
				nValue = nCurrent ^ nValue;
		}
		else
#if defined(_MSC_VER)
			nValue = _strtoi64(s, &e, 10);
#else
			nValue = strtoll(s, &e, 10);
#endif
	}
	return nValue;
}

inline int TextToInt(const char* s, int nCurrent, bool bBitfield)
{
	return (int)TextToInt64(s, nCurrent, bBitfield);
}

class CXConsoleVariableBase : public ICVar
{
public:
	//! constructor
	//! \param pConsole must not be 0
	CXConsoleVariableBase(CXConsole* pConsole, const char* sName, int nFlags, const char* help);
	//! destructor
	virtual ~CXConsoleVariableBase();

	// interface ICVar --------------------------------------------------------------------------------------

	virtual void            ClearFlags(int flags) override;
	virtual int             GetFlags() const override;
	virtual int             SetFlags(int flags) override;
	virtual const char*     GetName() const override;
	virtual const char*     GetHelp() override;
	virtual void            Release() override;
	virtual void            ForceSet(const char* s) override;
	virtual void            SetOnChangeCallback(ConsoleVarFunc pChangeFunc) override;
	virtual uint64          AddOnChangeFunctor(const SFunctor& pChangeFunctor) override;
	virtual bool            RemoveOnChangeFunctor(const uint64 nElement) override;
	virtual uint64          GetNumberOfOnChangeFunctors() const override;
	virtual const SFunctor& GetOnChangeFunctor(uint64 nFunctorIndex) const override;
	virtual ConsoleVarFunc  GetOnChangeCallback() const override;

	virtual int             GetRealIVal() const override { return GetIVal(); }
	virtual bool            IsConstCVar() const override { return (m_nFlags & VF_CONST_CVAR) != 0; }
#if defined(DEDICATED_SERVER)
	virtual void            SetDataProbeString(const char* pDataProbeString) override
	{
		CRY_ASSERT(m_pDataProbeString == NULL);
		m_pDataProbeString = new char[strlen(pDataProbeString) + 1];
		strcpy(m_pDataProbeString, pDataProbeString);
	}
#endif
	virtual const char* GetDataProbeString() const override
	{
#if defined(DEDICATED_SERVER)
		if (m_pDataProbeString)
		{
			return m_pDataProbeString;
		}
#endif
		return GetOwnDataProbeString();
	}

protected: // ------------------------------------------------------------------------------------------

	virtual const char* GetOwnDataProbeString() const
	{
		return GetString();
	}

	void CallOnChangeFunctions();

	char*                 m_szName;                 // if VF_COPYNAME then this data need to be deleteed, otherwise it's pointer to .dll/.exe

	char*                 m_psHelp;                 // pointer to the help string, might be 0
#if defined(DEDICATED_SERVER)
	char*                 m_pDataProbeString;       // value client is required to have for data probes
#endif
	int                   m_nFlags;                 // e.g. VF_CHEAT, ...

	std::vector<SFunctor> m_cpChangeFunctors;
	ConsoleVarFunc        m_pChangeFunc;            // Callback function that is called when this variable changes.


	CXConsole*            m_pConsole;               // used for the callback OnBeforeVarChange()

	friend class CXConsole;
	static int            m_sys_cvar_logging;
};

//////////////////////////////////////////////////////////////////////////
class CXConsoleVariableString : public CXConsoleVariableBase
{
public:
	// constructor
	CXConsoleVariableString(CXConsole* pConsole, const char* sName, const char* szDefault, int nFlags, const char* help)
		: CXConsoleVariableBase(pConsole, sName, nFlags, help)
		, m_sValue(GetCVarOverride(sName, szDefault))
	{
	}

	// interface ICVar --------------------------------------------------------------------------------------

	virtual int         GetIVal() const override   { return atoi(m_sValue); }
	virtual int64       GetI64Val() const override { return _atoi64(m_sValue); }
	virtual float       GetFVal() const override   { return (float)atof(m_sValue); }
	virtual const char* GetString() const override { return m_sValue; }
	virtual void        Set(const char* s) override
	{
		if (!s)
			return;

		if ((m_sValue == s) && (m_nFlags & VF_ALWAYSONCHANGE) == 0)
			return;

		if (!m_allowedValues.empty())
		{
			if (std::find_if(m_allowedValues.cbegin(), m_allowedValues.cend(),
				[s](const string& allowedValue) { return stricmp(allowedValue.c_str(), s) == 0; }) == m_allowedValues.cend())
			{
				if (m_sys_cvar_logging > 0)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "'%s' is not a valid value of '%s'", s, GetName());
				}
				if (std::find_if(m_allowedValues.cbegin(), m_allowedValues.cend(),
					[this](const string& allowedValue) { return stricmp(allowedValue.c_str(), m_sValue.c_str()) == 0; }) != m_allowedValues.cend())
				{
					s = m_sValue.c_str();
				}
				else
				{
					s = m_allowedValues[0];
				}
			}
		}

		if (m_pConsole->OnBeforeVarChange(this, s))
		{
			m_nFlags |= VF_MODIFIED;
			{
				m_sValue = s;
			}

			CallOnChangeFunctions();

			m_pConsole->OnAfterVarChange(this);
		}
	}

	virtual void Set(const float f) override
	{
		stack_string s;
		s.Format("%g", f);

		if ((m_sValue == s.c_str()) && (m_nFlags & VF_ALWAYSONCHANGE) == 0)
			return;

		m_nFlags |= VF_MODIFIED;
		Set(s.c_str());
	}

	virtual void Set(const int i) override
	{
		stack_string s;
		s.Format("%d", i);

		if ((m_sValue == s.c_str()) && (m_nFlags & VF_ALWAYSONCHANGE) == 0)
			return;

		m_nFlags |= VF_MODIFIED;
		Set(s.c_str());
	}

	virtual void SetMinValue(int min) override { CRY_ASSERT_MESSAGE(false, "Trying to set a minimum value on a string CVar."); }
	virtual void SetMinValue(float min) override { CRY_ASSERT_MESSAGE(false, "Trying to set a minimum value on a string CVar."); }
	virtual void SetMaxValue(int max) override { CRY_ASSERT_MESSAGE(false, "Trying to set a maximum value on a string CVar."); }
	virtual void SetMaxValue(float max) override { CRY_ASSERT_MESSAGE(false, "Trying to set a maximum value on a string CVar."); }

	virtual void SetAllowedValues(std::initializer_list<int> values) override { CRY_ASSERT_MESSAGE(false, "Trying to set allowed int values on a string CVar."); }
	virtual void SetAllowedValues(std::initializer_list<float> values) override { CRY_ASSERT_MESSAGE(false, "Trying to set allowed float values on a string CVar."); }
	virtual void SetAllowedValues(std::initializer_list<string> values) override
	{
		m_allowedValues = values;
		Set(m_sValue.c_str());
	}

	virtual int  GetType() override                                     { return CVAR_STRING; }

	virtual void GetMemoryUsage(class ICrySizer* pSizer) const override { pSizer->AddObject(this, sizeof(*this)); }
private:           // --------------------------------------------------------------------------------------------
	string m_sValue; //!<

	std::vector<string> m_allowedValues;
};

class CXConsoleVariableInt : public CXConsoleVariableBase
{
public:
	// constructor
	CXConsoleVariableInt(CXConsole* pConsole, const char* sName, const int iDefault, int nFlags, const char* help)
		: CXConsoleVariableBase(pConsole, sName, nFlags, help)
		, m_iValue(GetCVarOverride(sName, iDefault))
		, m_minValue(std::numeric_limits<int>::lowest())
		, m_maxValue(std::numeric_limits<int>::max())
	{
	}

	// interface ICVar --------------------------------------------------------------------------------------

	virtual int         GetIVal() const override   { return m_iValue; }
	virtual int64       GetI64Val() const override { return m_iValue; }
	virtual float       GetFVal() const override   { return (float)GetIVal(); }
	virtual const char* GetString() const override
	{
		static char szReturnString[256];

		cry_sprintf(szReturnString, "%d", GetIVal());
		return szReturnString;
	}
	virtual void Set(const char* s) override
	{
		int nValue = TextToInt(s, m_iValue, (m_nFlags & VF_BITFIELD) != 0);

		Set(nValue);
	}
	virtual void Set(const float f) override
	{
		Set((int)f);
	}
	virtual void Set(const int i) override
	{
		if (i == m_iValue && (m_nFlags & VF_ALWAYSONCHANGE) == 0)
			return;

		stack_string s;
		s.Format("%d", i);

		if (m_pConsole->OnBeforeVarChange(this, s.c_str()))
		{
			m_nFlags |= VF_MODIFIED;
			SetInternal(i);

			CallOnChangeFunctions();

			m_pConsole->OnAfterVarChange(this);
		}
	}

	virtual void SetMinValue(int min) override
	{
		CRY_ASSERT_MESSAGE((m_nFlags & VF_BITFIELD) == 0, "Trying to set a minimum value on a bitfield");
		CRY_ASSERT_MESSAGE(m_allowedValues.empty(), "Trying to set a minimum value after SetAllowedValues() was used, the minimum will be ignored");
		m_minValue = min;
		SetInternal(m_iValue);
	}
	virtual void SetMinValue(float min) override { SetMinValue(static_cast<int>(min)); }
	virtual void SetMaxValue(int max) override
	{
		CRY_ASSERT_MESSAGE((m_nFlags & VF_BITFIELD) == 0, "Trying to set a maximum value on a bitfield");
		CRY_ASSERT_MESSAGE(m_allowedValues.empty(), "Trying to set a maximum value after SetAllowedValues() was used, the maximum will be ignored");
		m_maxValue = max;
		SetInternal(m_iValue);
	}
	virtual void SetMaxValue(float max) override { SetMaxValue(static_cast<int>(max)); }

	virtual void SetAllowedValues(std::initializer_list<int> values) override
	{
		m_allowedValues = values;
		SetInternal(m_iValue);
	}
	virtual void SetAllowedValues(std::initializer_list<float> values) override { CRY_ASSERT_MESSAGE(false, "Trying to set allowed float values on an int CVar."); }
	virtual void SetAllowedValues(std::initializer_list<string> values) override { CRY_ASSERT_MESSAGE(false, "Trying to set allowed string values on an int CVar."); }
	
	virtual int  GetType() override                                     { return CVAR_INT; }

	virtual void GetMemoryUsage(class ICrySizer* pSizer) const override { pSizer->AddObject(this, sizeof(*this)); }
protected: // --------------------------------------------------------------------------------------------

	void SetInternal(int value);

	int m_iValue;                                   //!<

	int m_minValue;
	int m_maxValue;
	std::vector<int> m_allowedValues;
};

class CXConsoleVariableInt64 : public CXConsoleVariableBase
{
public:
	// constructor
	CXConsoleVariableInt64(CXConsole* pConsole, const char* sName, const int64 iDefault, int nFlags, const char* help)
		: CXConsoleVariableBase(pConsole, sName, nFlags, help)
		, m_iValue(GetCVarOverride(sName, iDefault))
		, m_minValue(std::numeric_limits<int64>::lowest())
		, m_maxValue(std::numeric_limits<int64>::max())
	{
	}

	// interface ICVar --------------------------------------------------------------------------------------

	virtual int         GetIVal() const override   { return (int)m_iValue; }
	virtual int64       GetI64Val() const override { return m_iValue; }
	virtual float       GetFVal() const override   { return (float)GetIVal(); }
	virtual const char* GetString() const override
	{
		static char szReturnString[256];
		cry_sprintf(szReturnString, "%" PRIi64, GetI64Val());
		return szReturnString;
	}
	virtual void Set(const char* s) override
	{
		int64 nValue = TextToInt64(s, m_iValue, (m_nFlags & VF_BITFIELD) != 0);

		Set(nValue);
	}
	virtual void Set(const float f) override
	{
		Set((int)f);
	}
	virtual void Set(const int i) override
	{
		Set((int64)i);
	}
	virtual void Set(const int64 i)
	{
		if (i == m_iValue && (m_nFlags & VF_ALWAYSONCHANGE) == 0)
			return;

		stack_string s;
		s.Format("%" PRIi64, i);

		if (m_pConsole->OnBeforeVarChange(this, s.c_str()))
		{
			m_nFlags |= VF_MODIFIED;
			SetInternal(i);

			CallOnChangeFunctions();

			m_pConsole->OnAfterVarChange(this);
		}
	}
	virtual int  GetType() override                                     { return CVAR_INT; }

	virtual void SetMinValue(int64 min)
	{
		CRY_ASSERT_MESSAGE((m_nFlags & VF_BITFIELD) == 0, "Trying to set a minimum value on a bitfield");
		CRY_ASSERT_MESSAGE(m_allowedValues.empty(), "Trying to set a minimum value after SetAllowedValues() was used, the minimum will be ignored");
		m_minValue = min;
		SetInternal(m_iValue);
	}
	virtual void SetMinValue(int min) override { SetMinValue(static_cast<int64>(min)); }
	virtual void SetMinValue(float min) override { SetMinValue(static_cast<int64>(min)); }
	virtual void SetMaxValue(int64 max)
	{
		CRY_ASSERT_MESSAGE((m_nFlags & VF_BITFIELD) == 0, "Trying to set a maximum value on a bitfield");
		CRY_ASSERT_MESSAGE(m_allowedValues.empty(), "Trying to set a maximum value after SetAllowedValues() was used, the maximum will be ignored");
		m_maxValue = max;
		SetInternal(m_iValue);
	}
	virtual void SetMaxValue(int max) override { SetMaxValue(static_cast<int64>(max)); }
	virtual void SetMaxValue(float max) override { SetMaxValue(static_cast<int64>(max)); }

	virtual void SetAllowedValues(std::initializer_list<int> values) override
	{
		if (values.size() > 0)
		{
			m_allowedValues.resize(values.size());
			std::transform(values.begin(), values.end(), m_allowedValues.begin(), [](int i) { return static_cast<int64>(i); });
			SetInternal(m_iValue);
		}
		else
		{
			m_allowedValues.clear();
		}
	}
	virtual void SetAllowedValues(std::initializer_list<float> values) override { CRY_ASSERT_MESSAGE(false, "Trying to set allowed float values on an int CVar."); }
	virtual void SetAllowedValues(std::initializer_list<string> values) override { CRY_ASSERT_MESSAGE(false, "Trying to set allowed string values on an int CVar."); }

	virtual void GetMemoryUsage(class ICrySizer* pSizer) const override { pSizer->AddObject(this, sizeof(*this)); }
protected: // --------------------------------------------------------------------------------------------

	void SetInternal(int64 value);

	int64 m_iValue;                                   //!<

	int64 m_minValue;
	int64 m_maxValue;

	std::vector<int64> m_allowedValues;
};

//////////////////////////////////////////////////////////////////////////
class CXConsoleVariableFloat : public CXConsoleVariableBase
{
public:
	// constructor
	CXConsoleVariableFloat(CXConsole* pConsole, const char* sName, const float fDefault, int nFlags, const char* help)
		: CXConsoleVariableBase(pConsole, sName, nFlags, help)
		, m_fValue(GetCVarOverride(sName, fDefault))
		, m_minValue(std::numeric_limits<float>::lowest())
		, m_maxValue(std::numeric_limits<float>::max())
	{
	}

	// interface ICVar --------------------------------------------------------------------------------------

	virtual int         GetIVal() const override   { return (int)m_fValue; }
	virtual int64       GetI64Val() const override { return (int64)m_fValue; }
	virtual float       GetFVal() const override   { return m_fValue; }
	virtual const char* GetString() const override
	{
		static char szReturnString[256];

		cry_sprintf(szReturnString, "%g", m_fValue);    // %g -> "2.01",   %f -> "2.01000"
		return szReturnString;
	}
	virtual void Set(const char* s) override
	{
		float fValue = 0;
		if (s)
			fValue = (float)atof(s);

		if (fValue == m_fValue && (m_nFlags & VF_ALWAYSONCHANGE) == 0)
			return;

		if (m_pConsole->OnBeforeVarChange(this, s))
		{
			m_nFlags |= VF_MODIFIED;
			SetInternal(fValue);

			CallOnChangeFunctions();

			m_pConsole->OnAfterVarChange(this);
		}
	}
	virtual void Set(const float f) override
	{
		if (f == m_fValue && (m_nFlags & VF_ALWAYSONCHANGE) == 0)
			return;

		stack_string s;
		s.Format("%g", f);

		if (m_pConsole->OnBeforeVarChange(this, s.c_str()))
		{
			m_nFlags |= VF_MODIFIED;
			SetInternal(f);

			CallOnChangeFunctions();

			m_pConsole->OnAfterVarChange(this);
		}
	}
	virtual void Set(const int i) override
	{
		if ((float)i == m_fValue && (m_nFlags & VF_ALWAYSONCHANGE) == 0)
			return;

		char sTemp[128];
		cry_sprintf(sTemp, "%d", i);

		if (m_pConsole->OnBeforeVarChange(this, sTemp))
		{
			m_nFlags |= VF_MODIFIED;
			SetInternal(static_cast<float>(i));
			CallOnChangeFunctions();
			m_pConsole->OnAfterVarChange(this);
		}
	}

	virtual void SetMinValue(int min) override { SetMinValue(static_cast<float>(min)); }
	virtual void SetMinValue(float min) override
	{
		CRY_ASSERT_MESSAGE((m_nFlags & VF_BITFIELD) == 0, "Trying to set a minimum value on a bitfield");
		CRY_ASSERT_MESSAGE(m_allowedValues.empty(), "Trying to set a minimum value after SetAllowedValues() was used, the minimum will be ignored");
		m_minValue = min;
		SetInternal(m_fValue);
	}
	virtual void SetMaxValue(int max) override { SetMinValue(static_cast<float>(max)); }
	virtual void SetMaxValue(float max) override
	{
		CRY_ASSERT_MESSAGE((m_nFlags & VF_BITFIELD) == 0, "Trying to set a maximum value on a bitfield");
		CRY_ASSERT_MESSAGE(m_allowedValues.empty(), "Trying to set a maximum value after SetAllowedValues() was used, the maximum will be ignored");
		m_maxValue = max;
		SetInternal(m_fValue);
	}

	virtual void SetAllowedValues(std::initializer_list<int> values) override { CRY_ASSERT_MESSAGE(false, "Trying to set allowed int values on a float CVar."); }
	virtual void SetAllowedValues(std::initializer_list<float> values) override
	{
		m_allowedValues = values;
		SetInternal(m_fValue);
	}
	virtual void SetAllowedValues(std::initializer_list<string> values) override { CRY_ASSERT_MESSAGE(false, "Trying to set allowed int values on a float CVar."); }

	virtual int  GetType() override                                     { return CVAR_FLOAT; }

	virtual void GetMemoryUsage(class ICrySizer* pSizer) const override { pSizer->AddObject(this, sizeof(*this)); }

protected:

	virtual const char* GetOwnDataProbeString() const override
	{
		static char szReturnString[8];

		cry_sprintf(szReturnString, "%.1g", m_fValue);
		return szReturnString;
	}

private: // --------------------------------------------------------------------------------------------

	void SetInternal(float value);

	float m_fValue;                                   //!<

	float m_minValue;
	float m_maxValue;

	std::vector<float> m_allowedValues;
};

class CXConsoleVariableIntRef : public CXConsoleVariableBase
{
public:
	//! constructor
	//!\param pVar must not be 0
	CXConsoleVariableIntRef(CXConsole* pConsole, const char* sName, int32* pVar, int nFlags, const char* help)
		: CXConsoleVariableBase(pConsole, sName, nFlags, help)
		, m_iValue(*pVar)
		, m_minValue(std::numeric_limits<int>::lowest())
		, m_maxValue(std::numeric_limits<int>::max())
	{
		assert(pVar);
		*pVar = GetCVarOverride(sName, m_iValue);
	}

	// interface ICVar --------------------------------------------------------------------------------------

	virtual int         GetIVal() const override   { return m_iValue; }
	virtual int64       GetI64Val() const override { return m_iValue; }
	virtual float       GetFVal() const override   { return (float)m_iValue; }
	virtual const char* GetString() const override
	{
		static char szReturnString[256];

		cry_sprintf(szReturnString, "%d", m_iValue);
		return szReturnString;
	}
	virtual void Set(const char* s) override
	{
		int nValue = TextToInt(s, m_iValue, (m_nFlags & VF_BITFIELD) != 0);
		if (nValue == m_iValue && (m_nFlags & VF_ALWAYSONCHANGE) == 0)
			return;

		if (m_pConsole->OnBeforeVarChange(this, s))
		{
			m_nFlags |= VF_MODIFIED;
			SetInternal(nValue);

			CallOnChangeFunctions();
			m_pConsole->OnAfterVarChange(this);
		}
	}
	virtual void Set(const float f) override
	{
		if ((int)f == m_iValue && (m_nFlags & VF_ALWAYSONCHANGE) == 0)
			return;

		char sTemp[128];
		cry_sprintf(sTemp, "%g", f);

		if (m_pConsole->OnBeforeVarChange(this, sTemp))
		{
			m_nFlags |= VF_MODIFIED;
			SetInternal(static_cast<int>(f));
			CallOnChangeFunctions();
			m_pConsole->OnAfterVarChange(this);
		}
	}
	virtual void Set(const int i) override
	{
		if (i == m_iValue && (m_nFlags & VF_ALWAYSONCHANGE) == 0)
			return;

		char sTemp[128];
		cry_sprintf(sTemp, "%d", i);

		if (m_pConsole->OnBeforeVarChange(this, sTemp))
		{
			m_nFlags |= VF_MODIFIED;
			SetInternal(i);
			CallOnChangeFunctions();
			m_pConsole->OnAfterVarChange(this);
		}
	}

	virtual void SetMinValue(int min) override
	{
		CRY_ASSERT_MESSAGE((m_nFlags & VF_BITFIELD) == 0, "Trying to set a minimum value on a bitfield");
		CRY_ASSERT_MESSAGE(m_allowedValues.empty(), "Trying to set a minimum value after SetAllowedValues() was used, the minimum will be ignored");
		m_minValue = min;
		SetInternal(m_iValue);
	}
	virtual void SetMinValue(float min) override { SetMinValue(static_cast<int>(min)); }
	virtual void SetMaxValue(int max) override
	{
		CRY_ASSERT_MESSAGE((m_nFlags & VF_BITFIELD) == 0, "Trying to set a maximum value on a bitfield");
		CRY_ASSERT_MESSAGE(m_allowedValues.empty(), "Trying to set a maximum value after SetAllowedValues() was used, the maximum will be ignored");
		m_maxValue = max;
		SetInternal(m_iValue);
	}
	virtual void SetMaxValue(float max) override { SetMaxValue(static_cast<int>(max)); }

	virtual void SetAllowedValues(std::initializer_list<int> values) override
	{
		m_allowedValues = values;
		SetInternal(m_iValue);
	}
	virtual void SetAllowedValues(std::initializer_list<float> values) override { CRY_ASSERT_MESSAGE(false, "Trying to set allowed float values on an int CVar."); }
	virtual void SetAllowedValues(std::initializer_list<string> values) override { CRY_ASSERT_MESSAGE(false, "Trying to set allowed string values on an int CVar."); }

	virtual int  GetType() override                                     { return CVAR_INT; }

	virtual void GetMemoryUsage(class ICrySizer* pSizer) const override { pSizer->AddObject(this, sizeof(*this)); }
private: // --------------------------------------------------------------------------------------------

	void SetInternal(int value);

	int& m_iValue;                                  //!<

	int m_minValue;
	int m_maxValue;

	std::vector<int> m_allowedValues;
};

class CXConsoleVariableFloatRef : public CXConsoleVariableBase
{
public:
	//! constructor
	//!\param pVar must not be 0
	CXConsoleVariableFloatRef(CXConsole* pConsole, const char* sName, float* pVar, int nFlags, const char* help)
		: CXConsoleVariableBase(pConsole, sName, nFlags, help)
		, m_fValue(*pVar)
		, m_minValue(std::numeric_limits<float>::lowest())
		, m_maxValue(std::numeric_limits<float>::max())
	{
		assert(pVar);
		*pVar = GetCVarOverride(sName, m_fValue);
	}

	// interface ICVar --------------------------------------------------------------------------------------

	virtual int         GetIVal() const override   { return (int)m_fValue; }
	virtual int64       GetI64Val() const override { return (int64)m_fValue; }
	virtual float       GetFVal() const override   { return m_fValue; }
	virtual const char* GetString() const override
	{
		static char szReturnString[256];

		cry_sprintf(szReturnString, "%g", m_fValue);
		return szReturnString;
	}
	virtual void Set(const char* s) override
	{
		float fValue = 0;
		if (s)
			fValue = (float)atof(s);
		if (fValue == m_fValue && (m_nFlags & VF_ALWAYSONCHANGE) == 0)
			return;

		if (m_pConsole->OnBeforeVarChange(this, s))
		{
			m_nFlags |= VF_MODIFIED;
			SetInternal(fValue);

			CallOnChangeFunctions();
			m_pConsole->OnAfterVarChange(this);
		}
	}
	virtual void Set(const float f) override
	{
		if (f == m_fValue && (m_nFlags & VF_ALWAYSONCHANGE) == 0)
			return;

		char sTemp[128];
		cry_sprintf(sTemp, "%g", f);

		if (m_pConsole->OnBeforeVarChange(this, sTemp))
		{
			m_nFlags |= VF_MODIFIED;
			SetInternal(f);
			CallOnChangeFunctions();
			m_pConsole->OnAfterVarChange(this);
		}
	}
	virtual void Set(const int i) override
	{
		if ((float)i == m_fValue && (m_nFlags & VF_ALWAYSONCHANGE) == 0)
			return;

		char sTemp[128];
		cry_sprintf(sTemp, "%d", i);

		if (m_pConsole->OnBeforeVarChange(this, sTemp))
		{
			m_nFlags |= VF_MODIFIED;
			SetInternal(static_cast<float>(i));
			CallOnChangeFunctions();
			m_pConsole->OnAfterVarChange(this);
		}
	}

	virtual void SetMinValue(int min) override { SetMinValue(static_cast<float>(min)); }
	virtual void SetMinValue(float min) override
	{
		CRY_ASSERT_MESSAGE((m_nFlags & VF_BITFIELD) == 0, "Trying to set a minimum value on a bitfield");
		CRY_ASSERT_MESSAGE(m_allowedValues.empty(), "Trying to set a minimum value after SetAllowedValues() was used, the minimum will be ignored");
		m_minValue = min;
		SetInternal(m_fValue);
	}
	virtual void SetMaxValue(int max) override { SetMinValue(static_cast<float>(max)); }
	virtual void SetMaxValue(float max) override
	{
		CRY_ASSERT_MESSAGE((m_nFlags & VF_BITFIELD) == 0, "Trying to set a maximum value on a bitfield");
		CRY_ASSERT_MESSAGE(m_allowedValues.empty(), "Trying to set a minimum value after SetAllowedValues() was used, the maximum will be ignored");
		m_maxValue = max;
		SetInternal(m_fValue);
	}

	virtual void SetAllowedValues(std::initializer_list<int> values) override { CRY_ASSERT_MESSAGE(false, "Trying to set allowed int values on a float CVar."); }
	virtual void SetAllowedValues(std::initializer_list<float> values) override
	{
		m_allowedValues = values;
		SetInternal(m_fValue);
	}
	virtual void SetAllowedValues(std::initializer_list<string> values) override { CRY_ASSERT_MESSAGE(false, "Trying to set allowed string values on a float CVar."); }

	virtual int  GetType() override                                     { return CVAR_FLOAT; }

	virtual void GetMemoryUsage(class ICrySizer* pSizer) const override { pSizer->AddObject(this, sizeof(*this)); }

protected:

	virtual const char* GetOwnDataProbeString() const override
	{
		static char szReturnString[8];

		cry_sprintf(szReturnString, "%.1g", m_fValue);
		return szReturnString;
	}

private: // --------------------------------------------------------------------------------------------

	void SetInternal(float value);

	float& m_fValue;                                  //!<

	float m_minValue;
	float m_maxValue;

	std::vector<float> m_allowedValues;
};

class CXConsoleVariableStringRef : public CXConsoleVariableBase
{
public:
	//! constructor
	//!\param userBuf must not be 0
	CXConsoleVariableStringRef(CXConsole* pConsole, const char* sName, const char** userBuf, const char* defaultValue, int nFlags, const char* help)
		: CXConsoleVariableBase(pConsole, sName, nFlags, help)
		, m_sValue(GetCVarOverride(sName, defaultValue))
		, m_userPtr(*userBuf)
	{
		m_userPtr = m_sValue.c_str();
		assert(userBuf);
	}

	// interface ICVar --------------------------------------------------------------------------------------

	virtual int         GetIVal() const override   { return atoi(m_sValue.c_str()); }
	virtual int64       GetI64Val() const override { return _atoi64(m_sValue.c_str()); }
	virtual float       GetFVal() const override   { return (float)atof(m_sValue.c_str()); }
	virtual const char* GetString() const override
	{
		return m_sValue.c_str();
	}
	virtual void Set(const char* s) override
	{
		if ((m_sValue == s) && (m_nFlags & VF_ALWAYSONCHANGE) == 0)
			return;

		if (!m_allowedValues.empty())
		{
			if (std::find_if(m_allowedValues.cbegin(), m_allowedValues.cend(),
				[s](const string& allowedValue) { return stricmp(allowedValue.c_str(), s) == 0; }) == m_allowedValues.cend())
			{
				if (m_sys_cvar_logging > 0)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "'%s' is not a valid value of '%s'", s, GetName());
				}
				if (std::find_if(m_allowedValues.cbegin(), m_allowedValues.cend(),
					[this](const string& allowedValue) { return stricmp(allowedValue.c_str(), m_sValue.c_str()) == 0; }) != m_allowedValues.cend())
				{
					s = m_sValue.c_str();
				}
				else
				{
					s = m_allowedValues[0].c_str();
				}
			}
		}

		if (m_pConsole->OnBeforeVarChange(this, s))
		{
			m_nFlags |= VF_MODIFIED;
			{
				m_sValue = s;
				m_userPtr = m_sValue.c_str();
			}

			CallOnChangeFunctions();
			m_pConsole->OnAfterVarChange(this);
		}
	}
	virtual void Set(const float f) override
	{
		stack_string s;
		s.Format("%g", f);
		Set(s.c_str());
	}
	virtual void Set(const int i) override
	{
		stack_string s;
		s.Format("%d", i);
		Set(s.c_str());
	}
	
	virtual void SetMinValue(int min) override { CRY_ASSERT_MESSAGE(false, "Trying to set a minimum value on a string CVar."); }
	virtual void SetMinValue(float min) override { CRY_ASSERT_MESSAGE(false, "Trying to set a minimum value on a string CVar."); }
	virtual void SetMaxValue(int max) override { CRY_ASSERT_MESSAGE(false, "Trying to set a maximum value on a string CVar."); }
	virtual void SetMaxValue(float max) override { CRY_ASSERT_MESSAGE(false, "Trying to set a maximum value on a string CVar."); }

	virtual void SetAllowedValues(std::initializer_list<int> values) override { CRY_ASSERT_MESSAGE(false, "Trying to set allowed int values on a string CVar."); }
	virtual void SetAllowedValues(std::initializer_list<float> values) override { CRY_ASSERT_MESSAGE(false, "Trying to set allowed float values on a string CVar."); }
	virtual void SetAllowedValues(std::initializer_list<string> values) override
	{
		m_allowedValues = values;
		Set(m_sValue.c_str());
	}

	virtual int  GetType() override                                     { return CVAR_STRING; }

	virtual void GetMemoryUsage(class ICrySizer* pSizer) const override { pSizer->AddObject(this, sizeof(*this)); }
private: // --------------------------------------------------------------------------------------------

	string       m_sValue;
	const char*& m_userPtr;                     //!<

	std::vector<string> m_allowedValues;
};

// works like CXConsoleVariableInt but when changing it sets other console variables
// getting the value returns the last value it was set to - if that is still what was applied
// to the cvars can be tested with GetRealIVal()
class CXConsoleVariableCVarGroup : public CXConsoleVariableInt, public ILoadConfigurationEntrySink
{
public:
	// constructor
	CXConsoleVariableCVarGroup(CXConsole* pConsole, const char* sName, const char* szFileName, int nFlags);

	// destructor
	~CXConsoleVariableCVarGroup();

	// Returns:
	//   part of the help string - useful to log out detailed description without additional help text
	string GetDetailedInfo() const;

	// interface ICVar -----------------------------------------------------------------------------------

	virtual const char* GetHelp() override;

	virtual int         GetRealIVal() const override;

	virtual void        DebugLog(const int iExpectedValue, const ICVar::EConsoleLogMode mode) const override;

	virtual void        Set(const int i) override;

	// ConsoleVarFunc ------------------------------------------------------------------------------------

	static void OnCVarChangeFunc(ICVar* pVar);

	// interface ILoadConfigurationEntrySink -------------------------------------------------------------

	virtual void OnLoadConfigurationEntry(const char* szKey, const char* szValue, const char* szGroup) override;
	virtual void OnLoadConfigurationEntry_End() override;

	virtual void GetMemoryUsage(class ICrySizer* pSizer) const override
	{
		pSizer->AddObject(this, sizeof(*this));
		pSizer->AddObject(m_sDefaultValue);
		pSizer->AddObject(m_CVarGroupStates);

	}
private: // --------------------------------------------------------------------------------------------

	struct SCVarGroup
	{
		std::map<string, string> m_KeyValuePair;                    // e.g. m_KeyValuePair["r_fullscreen"]="0"
		void                     GetMemoryUsage(class ICrySizer* pSizer) const
		{
			pSizer->AddObject(m_KeyValuePair);
		}
	};

	SCVarGroup         m_CVarGroupDefault;
	typedef std::map<int, SCVarGroup*> TCVarGroupStateMap;
	TCVarGroupStateMap m_CVarGroupStates;
	string             m_sDefaultValue;                           // used by OnLoadConfigurationEntry_End()

	void ApplyCVars(const SCVarGroup& rGroup, const SCVarGroup* pExclude = 0);

	// Arguments:
	//   sKey - must exist, at least in default
	//   pSpec - can be 0
	string GetValueSpec(const string& sKey, const int* pSpec = 0) const;

	// should only be used by TestCVars()
	// Returns:
	//   true=all console variables match the state (excluding default state), false otherwise
	bool _TestCVars(const SCVarGroup& rGroup, const ICVar::EConsoleLogMode mode, const SCVarGroup* pExclude = 0) const;

	// Arguments:
	//   pGroup - can be 0 to test if the default state is set
	// Returns:
	//   true=all console variables match the state (including default state), false otherwise
	bool TestCVars(const SCVarGroup* pGroup, const ICVar::EConsoleLogMode mode = ICVar::eCLM_Off) const;
};

#endif // !defined(AFX_XCONSOLEVARIABLE_H__AB510BA3_4D53_4C45_A2A0_EA15BABE0C34__INCLUDED_)
