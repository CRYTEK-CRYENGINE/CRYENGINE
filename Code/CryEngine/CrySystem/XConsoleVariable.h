// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#if !defined(AFX_XCONSOLEVARIABLE_H__AB510BA3_4D53_4C45_A2A0_EA15BABE0C34__INCLUDED_)
#define AFX_XCONSOLEVARIABLE_H__AB510BA3_4D53_4C45_A2A0_EA15BABE0C34__INCLUDED_

#if _MSC_VER > 1000
	#pragma once
#endif // _MSC_VER > 1000

#include <CrySystem/ISystem.h>
#include <CryCore/BitFiddling.h>
#include <CryCore/SFunctor.h>

struct IScriptSystem;
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
};

//////////////////////////////////////////////////////////////////////////
class CXConsoleVariableString : public CXConsoleVariableBase
{
public:
	// constructor
	CXConsoleVariableString(CXConsole* pConsole, const char* sName, const char* szDefault, int nFlags, const char* help)
		: CXConsoleVariableBase(pConsole, sName, nFlags, help)
	{
		m_sValue = szDefault;
	}

	// interface ICVar --------------------------------------------------------------------------------------

	virtual int         GetIVal() const   { return atoi(m_sValue); }
	virtual int64       GetI64Val() const { return _atoi64(m_sValue); }
	virtual float       GetFVal() const   { return (float)atof(m_sValue); }
	virtual const char* GetString() const { return m_sValue; }
	virtual void        Set(const char* s)
	{
		if (!s)
			return;

		if ((m_sValue == s) && (m_nFlags & VF_ALWAYSONCHANGE) == 0)
			return;

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

	virtual void Set(const float f)
	{
		stack_string s;
		s.Format("%g", f);

		if ((m_sValue == s.c_str()) && (m_nFlags & VF_ALWAYSONCHANGE) == 0)
			return;

		m_nFlags |= VF_MODIFIED;
		Set(s.c_str());
	}

	virtual void Set(const int i)
	{
		stack_string s;
		s.Format("%d", i);

		if ((m_sValue == s.c_str()) && (m_nFlags & VF_ALWAYSONCHANGE) == 0)
			return;

		m_nFlags |= VF_MODIFIED;
		Set(s.c_str());
	}
	virtual int  GetType()                                     { return CVAR_STRING; }

	virtual void GetMemoryUsage(class ICrySizer* pSizer) const { pSizer->AddObject(this, sizeof(*this)); }
private:           // --------------------------------------------------------------------------------------------
	string m_sValue; //!<
};

class CXConsoleVariableInt : public CXConsoleVariableBase
{
public:
	// constructor
	CXConsoleVariableInt(CXConsole* pConsole, const char* sName, const int iDefault, int nFlags, const char* help)
		: CXConsoleVariableBase(pConsole, sName, nFlags, help), m_iValue(iDefault)
	{
	}

	// interface ICVar --------------------------------------------------------------------------------------

	virtual int         GetIVal() const   { return m_iValue; }
	virtual int64       GetI64Val() const { return m_iValue; }
	virtual float       GetFVal() const   { return (float)GetIVal(); }
	virtual const char* GetString() const
	{
		static char szReturnString[256];

		cry_sprintf(szReturnString, "%d", GetIVal());
		return szReturnString;
	}
	virtual void Set(const char* s)
	{
		int nValue = TextToInt(s, m_iValue, (m_nFlags & VF_BITFIELD) != 0);

		Set(nValue);
	}
	virtual void Set(const float f)
	{
		Set((int)f);
	}
	virtual void Set(const int i)
	{
		if (i == m_iValue && (m_nFlags & VF_ALWAYSONCHANGE) == 0)
			return;

		stack_string s;
		s.Format("%d", i);

		if (m_pConsole->OnBeforeVarChange(this, s.c_str()))
		{
			m_nFlags |= VF_MODIFIED;
			m_iValue = i;

			CallOnChangeFunctions();

			m_pConsole->OnAfterVarChange(this);
		}
	}
	virtual int  GetType()                                     { return CVAR_INT; }

	virtual void GetMemoryUsage(class ICrySizer* pSizer) const { pSizer->AddObject(this, sizeof(*this)); }
protected: // --------------------------------------------------------------------------------------------

	int m_iValue;                                   //!<
};

class CXConsoleVariableInt64 : public CXConsoleVariableBase
{
public:
	// constructor
	CXConsoleVariableInt64(CXConsole* pConsole, const char* sName, const int64 iDefault, int nFlags, const char* help)
		: CXConsoleVariableBase(pConsole, sName, nFlags, help), m_iValue(iDefault)
	{
	}

	// interface ICVar --------------------------------------------------------------------------------------

	virtual int         GetIVal() const   { return (int)m_iValue; }
	virtual int64       GetI64Val() const { return m_iValue; }
	virtual float       GetFVal() const   { return (float)GetIVal(); }
	virtual const char* GetString() const
	{
		static char szReturnString[256];
		cry_sprintf(szReturnString, "%" PRIi64, GetI64Val());
		return szReturnString;
	}
	virtual void Set(const char* s)
	{
		int64 nValue = TextToInt64(s, m_iValue, (m_nFlags & VF_BITFIELD) != 0);

		Set(nValue);
	}
	virtual void Set(const float f)
	{
		Set((int)f);
	}
	virtual void Set(const int i)
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
			m_iValue = i;

			CallOnChangeFunctions();

			m_pConsole->OnAfterVarChange(this);
		}
	}
	virtual int  GetType()                                     { return CVAR_INT; }

	virtual void GetMemoryUsage(class ICrySizer* pSizer) const { pSizer->AddObject(this, sizeof(*this)); }
protected: // --------------------------------------------------------------------------------------------

	int64 m_iValue;                                   //!<
};

//////////////////////////////////////////////////////////////////////////
class CXConsoleVariableFloat : public CXConsoleVariableBase
{
public:
	// constructor
	CXConsoleVariableFloat(CXConsole* pConsole, const char* sName, const float fDefault, int nFlags, const char* help)
		: CXConsoleVariableBase(pConsole, sName, nFlags, help), m_fValue(fDefault)
	{
	}

	// interface ICVar --------------------------------------------------------------------------------------

	virtual int         GetIVal() const   { return (int)m_fValue; }
	virtual int64       GetI64Val() const { return (int64)m_fValue; }
	virtual float       GetFVal() const   { return m_fValue; }
	virtual const char* GetString() const
	{
		static char szReturnString[256];

		cry_sprintf(szReturnString, "%g", m_fValue);    // %g -> "2.01",   %f -> "2.01000"
		return szReturnString;
	}
	virtual void Set(const char* s)
	{
		float fValue = 0;
		if (s)
			fValue = (float)atof(s);

		if (fValue == m_fValue && (m_nFlags & VF_ALWAYSONCHANGE) == 0)
			return;

		if (m_pConsole->OnBeforeVarChange(this, s))
		{
			m_nFlags |= VF_MODIFIED;
			m_fValue = fValue;

			CallOnChangeFunctions();

			m_pConsole->OnAfterVarChange(this);
		}
	}
	virtual void Set(const float f)
	{
		if (f == m_fValue && (m_nFlags & VF_ALWAYSONCHANGE) == 0)
			return;

		stack_string s;
		s.Format("%g", f);

		if (m_pConsole->OnBeforeVarChange(this, s.c_str()))
		{
			m_nFlags |= VF_MODIFIED;
			m_fValue = f;

			CallOnChangeFunctions();

			m_pConsole->OnAfterVarChange(this);
		}
	}
	virtual void Set(const int i)
	{
		if ((float)i == m_fValue && (m_nFlags & VF_ALWAYSONCHANGE) == 0)
			return;

		char sTemp[128];
		cry_sprintf(sTemp, "%d", i);

		if (m_pConsole->OnBeforeVarChange(this, sTemp))
		{
			m_nFlags |= VF_MODIFIED;
			m_fValue = (float)i;
			CallOnChangeFunctions();
			m_pConsole->OnAfterVarChange(this);
		}
	}
	virtual int  GetType()                                     { return CVAR_FLOAT; }

	virtual void GetMemoryUsage(class ICrySizer* pSizer) const { pSizer->AddObject(this, sizeof(*this)); }

protected:

	virtual const char* GetOwnDataProbeString() const
	{
		static char szReturnString[8];

		cry_sprintf(szReturnString, "%.1g", m_fValue);
		return szReturnString;
	}

private: // --------------------------------------------------------------------------------------------

	float m_fValue;                                   //!<
};

class CXConsoleVariableIntRef : public CXConsoleVariableBase
{
public:
	//! constructor
	//!\param pVar must not be 0
	CXConsoleVariableIntRef(CXConsole* pConsole, const char* sName, int32* pVar, int nFlags, const char* help)
		: CXConsoleVariableBase(pConsole, sName, nFlags, help), m_iValue(*pVar)
	{
		assert(pVar);
	}

	// interface ICVar --------------------------------------------------------------------------------------

	virtual int         GetIVal() const   { return m_iValue; }
	virtual int64       GetI64Val() const { return m_iValue; }
	virtual float       GetFVal() const   { return (float)m_iValue; }
	virtual const char* GetString() const
	{
		static char szReturnString[256];

		cry_sprintf(szReturnString, "%d", m_iValue);
		return szReturnString;
	}
	virtual void Set(const char* s)
	{
		int nValue = TextToInt(s, m_iValue, (m_nFlags & VF_BITFIELD) != 0);
		if (nValue == m_iValue && (m_nFlags & VF_ALWAYSONCHANGE) == 0)
			return;

		if (m_pConsole->OnBeforeVarChange(this, s))
		{
			m_nFlags |= VF_MODIFIED;
			m_iValue = nValue;

			CallOnChangeFunctions();
			m_pConsole->OnAfterVarChange(this);
		}
	}
	virtual void Set(const float f)
	{
		if ((int)f == m_iValue && (m_nFlags & VF_ALWAYSONCHANGE) == 0)
			return;

		char sTemp[128];
		cry_sprintf(sTemp, "%g", f);

		if (m_pConsole->OnBeforeVarChange(this, sTemp))
		{
			m_nFlags |= VF_MODIFIED;
			m_iValue = (int)f;
			CallOnChangeFunctions();
			m_pConsole->OnAfterVarChange(this);
		}
	}
	virtual void Set(const int i)
	{
		if (i == m_iValue && (m_nFlags & VF_ALWAYSONCHANGE) == 0)
			return;

		char sTemp[128];
		cry_sprintf(sTemp, "%d", i);

		if (m_pConsole->OnBeforeVarChange(this, sTemp))
		{
			m_nFlags |= VF_MODIFIED;
			m_iValue = i;
			CallOnChangeFunctions();
			m_pConsole->OnAfterVarChange(this);
		}
	}
	virtual int  GetType()                                     { return CVAR_INT; }

	virtual void GetMemoryUsage(class ICrySizer* pSizer) const { pSizer->AddObject(this, sizeof(*this)); }
private: // --------------------------------------------------------------------------------------------

	int& m_iValue;                                  //!<
};

class CXConsoleVariableFloatRef : public CXConsoleVariableBase
{
public:
	//! constructor
	//!\param pVar must not be 0
	CXConsoleVariableFloatRef(CXConsole* pConsole, const char* sName, float* pVar, int nFlags, const char* help)
		: CXConsoleVariableBase(pConsole, sName, nFlags, help), m_fValue(*pVar)
	{
		assert(pVar);
	}

	// interface ICVar --------------------------------------------------------------------------------------

	virtual int         GetIVal() const   { return (int)m_fValue; }
	virtual int64       GetI64Val() const { return (int64)m_fValue; }
	virtual float       GetFVal() const   { return m_fValue; }
	virtual const char* GetString() const
	{
		static char szReturnString[256];

		cry_sprintf(szReturnString, "%g", m_fValue);
		return szReturnString;
	}
	virtual void Set(const char* s)
	{
		float fValue = 0;
		if (s)
			fValue = (float)atof(s);
		if (fValue == m_fValue && (m_nFlags & VF_ALWAYSONCHANGE) == 0)
			return;

		if (m_pConsole->OnBeforeVarChange(this, s))
		{
			m_nFlags |= VF_MODIFIED;
			m_fValue = fValue;

			CallOnChangeFunctions();
			m_pConsole->OnAfterVarChange(this);
		}
	}
	virtual void Set(const float f)
	{
		if (f == m_fValue && (m_nFlags & VF_ALWAYSONCHANGE) == 0)
			return;

		char sTemp[128];
		cry_sprintf(sTemp, "%g", f);

		if (m_pConsole->OnBeforeVarChange(this, sTemp))
		{
			m_nFlags |= VF_MODIFIED;
			m_fValue = f;
			CallOnChangeFunctions();
			m_pConsole->OnAfterVarChange(this);
		}
	}
	virtual void Set(const int i)
	{
		if ((float)i == m_fValue && (m_nFlags & VF_ALWAYSONCHANGE) == 0)
			return;

		char sTemp[128];
		cry_sprintf(sTemp, "%d", i);

		if (m_pConsole->OnBeforeVarChange(this, sTemp))
		{
			m_nFlags |= VF_MODIFIED;
			m_fValue = (float)i;
			CallOnChangeFunctions();
			m_pConsole->OnAfterVarChange(this);
		}
	}
	virtual int  GetType()                                     { return CVAR_FLOAT; }

	virtual void GetMemoryUsage(class ICrySizer* pSizer) const { pSizer->AddObject(this, sizeof(*this)); }

protected:

	virtual const char* GetOwnDataProbeString() const
	{
		static char szReturnString[8];

		cry_sprintf(szReturnString, "%.1g", m_fValue);
		return szReturnString;
	}

private: // --------------------------------------------------------------------------------------------

	float& m_fValue;                                  //!<
};

class CXConsoleVariableStringRef : public CXConsoleVariableBase
{
public:
	//! constructor
	//!\param userBuf must not be 0
	CXConsoleVariableStringRef(CXConsole* pConsole, const char* sName, const char** userBuf, const char* defaultValue, int nFlags, const char* help)
		: CXConsoleVariableBase(pConsole, sName, nFlags, help), m_sValue(defaultValue), m_userPtr(*userBuf)
	{
		m_userPtr = m_sValue.c_str();
		assert(userBuf);
	}

	// interface ICVar --------------------------------------------------------------------------------------

	virtual int         GetIVal() const   { return atoi(m_sValue.c_str()); }
	virtual int64       GetI64Val() const { return _atoi64(m_sValue.c_str()); }
	virtual float       GetFVal() const   { return (float)atof(m_sValue.c_str()); }
	virtual const char* GetString() const
	{
		return m_sValue.c_str();
	}
	virtual void Set(const char* s)
	{
		if ((m_sValue == s) && (m_nFlags & VF_ALWAYSONCHANGE) == 0)
			return;

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
	virtual void Set(const float f)
	{
		stack_string s;
		s.Format("%g", f);
		Set(s.c_str());
	}
	virtual void Set(const int i)
	{
		stack_string s;
		s.Format("%d", i);
		Set(s.c_str());
	}
	virtual int  GetType()                                     { return CVAR_STRING; }

	virtual void GetMemoryUsage(class ICrySizer* pSizer) const { pSizer->AddObject(this, sizeof(*this)); }
private: // --------------------------------------------------------------------------------------------

	string       m_sValue;
	const char*& m_userPtr;                     //!<
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

	virtual const char* GetHelp();

	virtual int         GetRealIVal() const;

	virtual void        DebugLog(const int iExpectedValue, const ICVar::EConsoleLogMode mode) const;

	virtual void        Set(const int i);

	// ConsoleVarFunc ------------------------------------------------------------------------------------

	static void OnCVarChangeFunc(ICVar* pVar);

	// interface ILoadConfigurationEntrySink -------------------------------------------------------------

	virtual void OnLoadConfigurationEntry(const char* szKey, const char* szValue, const char* szGroup);
	virtual void OnLoadConfigurationEntry_End();

	virtual void GetMemoryUsage(class ICrySizer* pSizer) const
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
