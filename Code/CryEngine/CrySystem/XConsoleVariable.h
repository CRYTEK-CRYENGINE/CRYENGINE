// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySystem/ISystem.h>
#include <CrySystem/CVarOverride.h>
#include <CryCore/BaseTypes.h>
#include <CryCore/BitFiddling.h>

#include <cstdlib>

class CXConsoleVariableBase : public ICVar
{
public:
	//! constructor
	//! \param pConsole must not be 0
	CXConsoleVariableBase(IConsole* pConsole, const string& name, int flags, const char* szHelp)
		: m_name(name)
		, m_helpMessage(szHelp)
		, m_flags(flags)
		, m_pConsole(pConsole)
	{
		CRY_ASSERT_MESSAGE(m_pConsole != nullptr, "Console must always exist");
	}
	//! destructor
	virtual ~CXConsoleVariableBase() {}

	// interface ICVar --------------------------------------------------------------------------------------

	virtual void ClearFlags(int flags) override
	{
		m_flags &= ~flags;
	}
	virtual int GetFlags() const override
	{
		return m_flags;
	}
	virtual int SetFlags(int flags) override
	{
		m_flags = flags;
		return m_flags;
	}
	virtual const char* GetName() const override
	{
		return m_name;
	}
	virtual const char* GetHelp() const override
	{
		return m_helpMessage;
	}
	virtual void ForceSet(const char* szValue) override
	{
		const int excludeFlags = (VF_CHEAT | VF_READONLY | VF_NET_SYNCED);
		const int oldFlags = (m_flags & excludeFlags);
		m_flags &= ~(excludeFlags);
		SetFromString(szValue);

		m_flags |= oldFlags;
	}
	virtual uint64 AddOnChange(SmallFunction<void()> changeFunctor) override
	{
		uint64 functorId = m_changeFunctorIdTotal;
		m_onChangeCallbacks.emplace_back(SCallback{ std::move(changeFunctor), functorId });
		++m_changeFunctorIdTotal;
		return functorId;
	}
	virtual bool RemoveOnChangeFunctor(const uint64 id) override
	{
		bool erased = stl::find_and_erase_if(m_onChangeCallbacks, [id](const SCallback& callback)
		{
			return callback.id == id;
		});
		return erased;
	}

	virtual uint64 GetNumberOfOnChangeFunctors() const override
	{
		return m_onChangeCallbacks.size();
	}
	virtual const SmallFunction<void()>& GetOnChangeFunctor(uint64 id) const override
	{
		CRY_ASSERT_MESSAGE(id < m_changeFunctorIdTotal, "[CXConsoleVariableBase::GetOnChangeFunctor] Functor id out of range");

		const auto iter = std::find_if(m_onChangeCallbacks.begin(), m_onChangeCallbacks.end(), [id](const SCallback& callback)
		{
			return callback.id == id;
		});

		CRY_ASSERT(iter != m_onChangeCallbacks.end());
		return iter->function;
	}

	virtual int  GetRealIVal() const override { return GetIVal(); }
	virtual bool IsConstCVar() const override { return (m_flags & VF_CONST_CVAR) != 0; }
#if defined(DEDICATED_SERVER)
	virtual void SetDataProbeString(const char* pDataProbeString) override
	{
		CRY_ASSERT(m_dataProbeString->IsEmpty());
		m_dataProbeString = pDataProbeString;
	}
#endif
	virtual const char* GetDataProbeString() const override
	{
#if defined(DEDICATED_SERVER)
		return m_dataProbeString;
#else
		return GetOwnDataProbeString();
#endif
	}

	bool IsOwnedByConsole() const override { return true; }

protected: // ------------------------------------------------------------------------------------------

	virtual const char* GetOwnDataProbeString() const
	{
		return GetString();
	}

	void CallOnChangeFunctions()
	{
		for (SCallback& callback : m_onChangeCallbacks)
		{
			callback.function();
		}
	}

	const string m_name;
	string m_helpMessage;
#if defined(DEDICATED_SERVER)
	string m_dataProbeString;                      // value client is required to have for data probes
#endif
	int   m_flags;                                 // e.g. VF_CHEAT, ...

	struct SCallback
	{
		SmallFunction<void()> function;
		uint64                id;
	};
	std::vector<SCallback> m_onChangeCallbacks;
	uint64                 m_changeFunctorIdTotal = 0;

	IConsole*              m_pConsole;              // used for the callback OnBeforeVarChange()
};

template<class T>
class CXNumericConsoleVariable : public CXConsoleVariableBase
{
	using value_type = T;
	using base_type = typename std::remove_reference<value_type>::type;
	static_assert(std::is_arithmetic<base_type>::value, "base_type has to be arithmetic");

public:
	CXNumericConsoleVariable(IConsole* const pConsole, const string& name, value_type var, int flags, const char* szHelp, bool ownedByConsole)
		: CXConsoleVariableBase(pConsole, name, flags, szHelp)
		, m_value(var) // Assign here first, in case value_type is a reference
		, m_maxValue(std::numeric_limits<base_type>::max())
		, m_minValue(std::numeric_limits<base_type>::lowest())
	{
		m_value = GetCVarOverride(name, m_value);
	}

	virtual int GetIVal() const override
	{
		return static_cast<int>(m_value);
	}
	virtual int64 GetI64Val() const override
	{
		return static_cast<int64>(m_value);
	}
	virtual float GetFVal() const override
	{
		return static_cast<float>(m_value);
	}
	virtual const char* GetString() const override
	{
		static char szReturnString[256];

		constexpr ECVarType type = GetTypeInternal();
		if (type == ECVarType::Int)
			cry_sprintf(szReturnString, "%d", m_value);
		else if (type == ECVarType::Int64)
			cry_sprintf(szReturnString, "%lli", m_value);
		else if (type == ECVarType::Float)
			cry_sprintf(szReturnString, "%g", m_value);

		return szReturnString;
	}

	virtual void Set(const char* szValue) override
	{
		CRY_ASSERT_MESSAGE(GetType() == ECVarType::String, "Wrong Set() function called. Use SetFromString() if you intended to set a numeric cvar with a string. %s", GetName());
		SetInternal<base_type>(szValue);
	}
	virtual void Set(float value) override
	{
		CRY_ASSERT_MESSAGE(GetType() == ECVarType::Float, "Wrong Set() function called. %s", GetName());
		SetInternal(static_cast<base_type>(value));
	}
	virtual void Set(int value) override
	{
		CRY_ASSERT_MESSAGE(GetType() == ECVarType::Int, "Wrong Set() function called. %s", GetName());
		SetInternal(static_cast<base_type>(value));
	}
	virtual void Set(int64 value) override
	{
		CRY_ASSERT_MESSAGE(GetType() == ECVarType::Int64, "Wrong Set() function called. %s", GetName());
		SetInternal(static_cast<base_type>(value));
	}

	virtual void SetFromString(const char* szValue) override
	{
		constexpr ECVarType type = GetTypeInternal();
		if (type == ECVarType::Int)
			Set(TextToInt(szValue, GetIVal(), (GetFlags() | VF_BITFIELD) != 0));
		else if (type == ECVarType::Int64)
			Set(TextToInt64(szValue, GetIVal(), (GetFlags() | VF_BITFIELD) != 0));
		else if (type == ECVarType::Float)
			Set(static_cast<float>(std::atof(szValue)));
	}

	virtual void SetMaxValue(int max) override
	{
		CRY_ASSERT_MESSAGE(GetTypeInternal() == ECVarType::Int, "Wrong type used (%s)", GetName());
		CRY_ASSERT_MESSAGE((m_flags & VF_BITFIELD) == 0, "Trying to set a minimum value on a bitfield (%s)", GetName());
		CRY_ASSERT_MESSAGE(m_allowedValues.empty(), "SetMaxValue() called after SetAllowedValues(), the maximum will be ignored (%s)", GetName());

		m_maxValue = static_cast<base_type>(max);
		SetInternal(m_value);
	}
	virtual void SetMaxValue(int64 max) override
	{
		CRY_ASSERT_MESSAGE(GetTypeInternal() == ECVarType::Int64, "Wrong type used (%s)", GetName());
		CRY_ASSERT_MESSAGE((m_flags & VF_BITFIELD) == 0, "Trying to set a minimum value on a bitfield (%s)", GetName());
		CRY_ASSERT_MESSAGE(m_allowedValues.empty(), "SetMaxValue() called after SetAllowedValues(), the maximum will be ignored (%s)", GetName());

		m_maxValue = static_cast<base_type>(max);
		SetInternal(m_value);
	}
	virtual void SetMaxValue(float max) override
	{
		CRY_ASSERT_MESSAGE(GetTypeInternal() == ECVarType::Float, "Wrong type used (%s)", GetName());
		CRY_ASSERT_MESSAGE((m_flags & VF_BITFIELD) == 0, "Trying to set a minimum value on a bitfield (%s)", GetName());
		CRY_ASSERT_MESSAGE(m_allowedValues.empty(), "SetMaxValue() called after SetAllowedValues(), the maximum will be ignored (%s)", GetName());

		m_maxValue = static_cast<base_type>(max);
		SetInternal(m_value);
	}
	virtual void SetMinValue(int min) override
	{
		CRY_ASSERT_MESSAGE(GetTypeInternal() == ECVarType::Int, "Wrong type used (%s)", GetName());
		CRY_ASSERT_MESSAGE((m_flags & VF_BITFIELD) == 0, "Trying to set a minimum value on a bitfield (%s)", GetName());
		CRY_ASSERT_MESSAGE(m_allowedValues.empty(), "SetMinValue() called after SetAllowedValues(), the minimum will be ignored (%s)", GetName());

		m_minValue = static_cast<base_type>(min);
		SetInternal(m_value);
	}
	virtual void SetMinValue(int64 min) override
	{
		CRY_ASSERT_MESSAGE(GetTypeInternal() == ECVarType::Int64, "Wrong type used (%s)", GetName());
		CRY_ASSERT_MESSAGE((m_flags & VF_BITFIELD) == 0, "Trying to set a minimum value on a bitfield (%s)", GetName());
		CRY_ASSERT_MESSAGE(m_allowedValues.empty(), "SetMinValue() called after SetAllowedValues(), the minimum will be ignored (%s)", GetName());

		m_minValue = static_cast<base_type>(min);
		SetInternal(m_value);
	}
	virtual void SetMinValue(float min) override
	{
		CRY_ASSERT_MESSAGE(GetTypeInternal() == ECVarType::Float, "Wrong type used (%s)", GetName());
		CRY_ASSERT_MESSAGE((m_flags & VF_BITFIELD) == 0, "Trying to set a minimum value on a bitfield (%s)", GetName());
		CRY_ASSERT_MESSAGE(m_allowedValues.empty(), "SetMinValue() called after SetAllowedValues(), the minimum will be ignored (%s)", GetName());

		m_minValue = static_cast<base_type>(min);
		SetInternal(m_value);
	}

	virtual void SetAllowedValues(std::initializer_list<int> values) override
	{
		CRY_ASSERT_MESSAGE(GetTypeInternal() == ECVarType::Int, "Wrong type used");
		SetAllowedValuesInternal(values);
	}
	virtual void SetAllowedValues(std::initializer_list<int64> values) override
	{
		CRY_ASSERT_MESSAGE(GetTypeInternal() == ECVarType::Int64, "Wrong type used");
		SetAllowedValuesInternal(values);
	}
	virtual void SetAllowedValues(std::initializer_list<float> values) override
	{
		CRY_ASSERT_MESSAGE(GetTypeInternal() == ECVarType::Float, "Wrong type used");
		SetAllowedValuesInternal(values);
	}
	virtual void SetAllowedValues(std::initializer_list<string> values) override
	{
		CRY_ASSERT_MESSAGE(GetTypeInternal() == ECVarType::String, "Wrong type used");
	}

	virtual ECVarType GetType() const override
	{
		return GetTypeInternal();
	}

	virtual void GetMemoryUsage(class ICrySizer* pSizer) const override;

protected:
	static constexpr ECVarType GetTypeInternal()
	{
		return std::is_integral<base_type>::value
		       ? (sizeof(base_type) == sizeof(int) ? ECVarType::Int : ECVarType::Int64)
		       : ECVarType::Float;
	}

	static int64 TextToInt64(const char* szText, int64 currentValue, bool isBitfield)
	{
		int64 value = 0;
		if (szText != nullptr)
		{
			char* pEnd;
			if (isBitfield)
			{
				// Bit manipulation.
				if (*szText == '^')
					value = 1LL << std::strtoll(++szText, &pEnd, 10); // Bit number
				else
					value = std::strtoll(szText, &pEnd, 10); // Full number

				// Check letter codes.
				while ((*pEnd >= 'a' && *pEnd <= 'z') || (*pEnd >= 'A' && *pEnd <= 'Z'))
				{
					value |= AlphaBit64(*pEnd);
					++pEnd;
				}

				if (*pEnd == '+')
					value = currentValue | value;
				else if (*pEnd == '-')
					value = currentValue & ~value;
				else if (*pEnd == '^')
					value = currentValue ^ value;
			}
			else
			{
				value = std::strtoll(szText, &pEnd, 10);
			}
		}

		return value;
	}
	static int TextToInt(const char* szText, int currentValue, bool isBitfield)
	{
		return static_cast<int>(TextToInt64(szText, currentValue, isBitfield));
	}

	template<class U>
	typename std::enable_if<std::is_same<base_type, U>::value>::type SetAllowedValuesInternal(std::initializer_list<U> values)
	{
		CRY_ASSERT_MESSAGE(values.size() == 0 || (m_maxValue == std::numeric_limits<base_type>::max() && m_minValue == std::numeric_limits<base_type>::lowest()),
		                   "SetAllowedValues() called after SetMinValue() and/or SetMinValue(), minimum and maximum will be ignored (%s)", GetName());
		m_allowedValues = values;
	}
	template<class U>
	typename std::enable_if<!std::is_same<base_type, U>::value>::type SetAllowedValuesInternal(std::initializer_list<U> values)
	{
		CRY_ASSERT_MESSAGE(values.size() == 0 || (m_maxValue == std::numeric_limits<base_type>::max() && m_minValue == std::numeric_limits<base_type>::lowest()),
		                   "SetAllowedValues() called after SetMinValue() and/or SetMinValue(), minimum and maximum will be ignored (%s)", GetName());

		if (values.size() > 0)
		{
			m_allowedValues.resize(values.size());
			std::transform(values.begin(), values.end(), m_allowedValues.begin(), [](U val) { return static_cast<base_type>(val); });
		}
		else
		{
			m_allowedValues.clear();
		}
	}

	template<class U = base_type>
	typename std::enable_if<std::is_same<U, int>::value>::type SetInternal(const char* szValue)
	{
		static_assert(std::is_same<U, base_type>::value, "You're not supposed to pass in a different template parameter");
		const int value = TextToInt(szValue, m_value, (m_flags & VF_BITFIELD) != 0);
		SetInternal(value);
	}
	template<class U = base_type>
	typename std::enable_if<std::is_same<U, int64>::value>::type SetInternal(const char* szValue)
	{
		static_assert(std::is_same<U, base_type>::value, "You're not supposed to pass in a different template parameter");
		const int64 value = TextToInt64(szValue, m_value, (m_flags & VF_BITFIELD) != 0);
		SetInternal(value);
	}
	template<class U = base_type>
	typename std::enable_if<std::is_same<U, float>::value>::type SetInternal(const char* szValue)
	{
		static_assert(std::is_same<U, base_type>::value, "You're not supposed to pass in a different template parameter");
		const float value = static_cast<float>(std::atof(szValue));
		SetInternal(value);
	}

	void SetInternal(base_type value)
	{
		constexpr ECVarType type = GetTypeInternal();
		if (!m_allowedValues.empty())
		{
			if (std::find(m_allowedValues.cbegin(), m_allowedValues.cend(), value) == m_allowedValues.cend())
			{
				if (ICVar* pCVarLogging = gEnv->pConsole->GetCVar("sys_cvar_logging"))
				{
					if (pCVarLogging->GetIVal() > 0)
					{
						if (type == ECVarType::Int)
							CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[CVARS] '%i' is not a valid value of '%s'", value, GetName());
						else if (type == ECVarType::Int64)
							CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[CVARS] '%lli' is not a valid value of '%s'", value, GetName());
						else if (type == ECVarType::Float)
							CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[CVARS] '%g' is not a valid value of '%s'", value, GetName());
					}
				}
				return;
			}
		}
		else
		{
			if (value > m_maxValue || value < m_minValue)
			{
				if (ICVar* pCVarLogging = gEnv->pConsole->GetCVar("sys_cvar_logging"))
				{
					if (pCVarLogging->GetIVal() > 0)
					{
						if (type == ECVarType::Int)
							CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[CVARS] '%i' is not in the allowed range of '%s' (%i-%i)", value, GetName(), m_minValue, m_maxValue);
						else if (type == ECVarType::Int64)
							CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[CVARS] '%lli' is not in the allowed range of '%s' (%lli-%lli)", value, GetName(), m_minValue, m_maxValue);
						else if (type == ECVarType::Float)
							CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[CVARS] '%g' is not in the allowed range of '%s' (%g-%g)", value, GetName(), m_minValue, m_maxValue);
					}
				}
				value = clamp_tpl(value, m_minValue, m_maxValue);
			}
		}

		if (value == m_value && (m_flags & VF_ALWAYSONCHANGE) == 0)
			return;

		if (m_pConsole->OnBeforeVarChange(this, GetString()))
		{
			if (ICVar* pCVarLogging = gEnv->pConsole->GetCVar("sys_cvar_logging"))
			{
				if (pCVarLogging->GetIVal() >= 2)
				{
					if (type == ECVarType::Int)
						CryLog("[CVARS] '%s' set to %d (was %d)", GetName(), value, m_value);
					else if (type == ECVarType::Int64)
						CryLog("[CVARS] '%s' set to %lli (was %lli)", GetName(), value, m_value);
					else if (type == ECVarType::Float)
						CryLog("[CVARS] '%s' set to %g (was %g)", GetName(), value, m_value);
				}
			}

			m_flags |= VF_MODIFIED;
			m_value = value;

			CallOnChangeFunctions();

			m_pConsole->OnAfterVarChange(this);
		}
	}

	value_type             m_value;

	base_type              m_maxValue;
	base_type              m_minValue;
	std::vector<base_type> m_allowedValues;
};

template<class T>
class CXStringConsoleVariable : public CXConsoleVariableBase
{
	using value_type = T;
	using base_type = typename std::remove_reference<value_type>::type;
	static_assert(std::is_same<base_type, string>::value, "base_type has to be string");

public:
	CXStringConsoleVariable(IConsole* const pConsole, const string& name, typename base_type::const_pointer defaultValue, int flags, const char* szHelp, bool ownedByConsole)
		: CXConsoleVariableBase(pConsole, name, flags, szHelp)
		, m_value(GetCVarOverride(name, defaultValue))
		, m_pUserBuffer(nullptr)
	{
		static_assert(!std::is_reference<value_type>::value, "This constructor can only be used for non-reference types.");
	}
	CXStringConsoleVariable(IConsole* const pConsole, const string& name, typename base_type::const_pointer& userBuffer, typename base_type::const_pointer defaultValue, int nFlags, const char* szHelp, bool ownedByConsole)
		: CXConsoleVariableBase(pConsole, name, nFlags, szHelp)
		, m_value(GetCVarOverride(name, defaultValue))
		, m_pUserBuffer(&userBuffer)
	{
		static_assert(std::is_reference<value_type>::value, "This constructor can only be used for reference types.");
		*m_pUserBuffer = m_value.c_str();
	}

	virtual int GetIVal() const override
	{
		return atoi(m_value.c_str());
	}
	virtual int64 GetI64Val() const override
	{
		return _atoi64(m_value.c_str());
	}
	virtual float GetFVal() const override
	{
		return static_cast<float>(atof(m_value.c_str()));
	}
	virtual const char* GetString() const override
	{
		return m_value.c_str();
	}

	virtual void Set(const char* szValue) override
	{
		if (szValue == nullptr)
			return;

		if (m_value == szValue && (m_flags & VF_ALWAYSONCHANGE) == 0)
			return;

		if (!m_allowedValues.empty())
		{
			if (std::find_if(m_allowedValues.cbegin(), m_allowedValues.cend(),
			                 [szValue](const string& allowedValue) { return stricmp(allowedValue.c_str(), szValue) == 0; }) == m_allowedValues.cend())
			{
				if (ICVar* pCVarLogging = gEnv->pConsole->GetCVar("sys_cvar_logging"))
				{
					if (pCVarLogging->GetIVal() > 0)
						CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[CVARS] '%s' is not a valid value of '%s'", szValue, GetName());
				}
				return;
			}
		}

		if (m_pConsole->OnBeforeVarChange(this, szValue))
		{
			if (ICVar* pCVarLogging = gEnv->pConsole->GetCVar("sys_cvar_logging"))
			{
				if (pCVarLogging->GetIVal() >= 2)
					CryLog("[CVARS] '%s' set to %s (was %s)", GetName(), szValue, m_value.c_str());
			}

			m_flags |= VF_MODIFIED;
			m_value = szValue;
			if (m_pUserBuffer != nullptr)
				*m_pUserBuffer = m_value.c_str();

			CallOnChangeFunctions();

			m_pConsole->OnAfterVarChange(this);
		}
	}
	virtual void Set(float value) override
	{
		CRY_ASSERT_MESSAGE(GetType() == ECVarType::String, "Wrong Set() function called. %s", GetName());

		stack_string temp;
		temp.Format("%g", value);
		Set(temp.c_str());
	}
	virtual void Set(int value) override
	{
		CRY_ASSERT_MESSAGE(GetType() == ECVarType::String, "Wrong Set() function called. %s", GetName());

		stack_string temp;
		temp.Format("%d", value);
		Set(temp.c_str());
	}
	virtual void Set(int64 value) override
	{
		CRY_ASSERT_MESSAGE(GetType() == ECVarType::String, "Wrong Set() function called. %s", GetName());

		stack_string temp;
		temp.Format("%lld", value);
		Set(temp.c_str());
	}

	virtual void SetFromString(const char* szValue) override
	{
		Set(szValue);
	}

	virtual void      SetMinValue(int min) override                                   { CRY_ASSERT_MESSAGE(false, "Trying to set a minimum value on a string CVar."); }
	virtual void      SetMinValue(int64 min) override                                 { CRY_ASSERT_MESSAGE(false, "Trying to set a minimum value on a string CVar."); }
	virtual void      SetMinValue(float min) override                                 { CRY_ASSERT_MESSAGE(false, "Trying to set a minimum value on a string CVar."); }
	virtual void      SetMaxValue(int max) override                                   { CRY_ASSERT_MESSAGE(false, "Trying to set a maximum value on a string CVar."); }
	virtual void      SetMaxValue(int64 max) override                                 { CRY_ASSERT_MESSAGE(false, "Trying to set a maximum value on a string CVar."); }
	virtual void      SetMaxValue(float max) override                                 { CRY_ASSERT_MESSAGE(false, "Trying to set a maximum value on a string CVar."); }

	virtual void      SetAllowedValues(std::initializer_list<int> values) override    { CRY_ASSERT_MESSAGE(false, "Trying to set allowed int values on a string CVar."); }
	virtual void      SetAllowedValues(std::initializer_list<int64> values) override  { CRY_ASSERT_MESSAGE(false, "Trying to set allowed int values on a string CVar."); }
	virtual void      SetAllowedValues(std::initializer_list<float> values) override  { CRY_ASSERT_MESSAGE(false, "Trying to set allowed float values on a string CVar."); }
	virtual void      SetAllowedValues(std::initializer_list<string> values) override { m_allowedValues = values; }

	virtual ECVarType GetType() const override                                        { return ECVarType::String; }

	virtual void GetMemoryUsage(class ICrySizer* pSizer) const override;

private:
	base_type m_value;
	typename base_type::const_pointer* const m_pUserBuffer;

	std::vector<base_type>                   m_allowedValues;
};

using CXConsoleVariableInt = CXNumericConsoleVariable<int>;
using CXConsoleVariableIntRef = CXNumericConsoleVariable<int&>;
using CXConsoleVariableFloat = CXNumericConsoleVariable<float>;
using CXConsoleVariableFloatRef = CXNumericConsoleVariable<float&>;
using CXConsoleVariableInt64 = CXNumericConsoleVariable<int64>;
using CXConsoleVariableString = CXStringConsoleVariable<string>;
using CXConsoleVariableStringRef = CXStringConsoleVariable<string&>;

#include <CryMemory/CrySizer.h>

template<class T>
void CXNumericConsoleVariable<T >::GetMemoryUsage(class ICrySizer* pSizer) const
{
	pSizer->AddObject(this, sizeof(*this));
}

template<class T>
void CXStringConsoleVariable<T >::GetMemoryUsage(class ICrySizer* pSizer) const
{
	pSizer->AddObject(this, sizeof(*this));
}

// works like CXConsoleVariableInt but when changing it sets other console variables
// getting the value returns the last value it was set to - if that is still what was applied
// to the cvars can be tested with GetRealIVal()
class CXConsoleVariableCVarGroup : public CXConsoleVariableInt, public ILoadConfigurationEntrySink
{
public:
	CXConsoleVariableCVarGroup(IConsole* pConsole, const string& name, const char* szFileName, int flags);
	~CXConsoleVariableCVarGroup();

	// Returns:
	//   part of the help string - useful to log out detailed description without additional help text
	string GetDetailedInfo() const;

	// interface ICVar -----------------------------------------------------------------------------------

	virtual const char* GetHelp() const override;

	virtual int         GetRealIVal() const override;

	virtual void        DebugLog(const int expectedValue, const ICVar::EConsoleLogMode mode) const override;

	virtual void        Set(int i) override;

	// ConsoleVarFunc ------------------------------------------------------------------------------------

	static void OnCVarChangeFunc(ICVar* pVar);

	// interface ILoadConfigurationEntrySink -------------------------------------------------------------

	virtual void OnLoadConfigurationEntry(const char* szKey, const char* szValue, const char* szGroup) override;
	virtual void OnLoadConfigurationEntry_End() override;

	virtual void GetMemoryUsage(class ICrySizer* pSizer) const override
	{
		pSizer->AddObject(this, sizeof(*this));
		pSizer->AddObject(m_defaultValue);
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
	string             m_defaultValue;                           // used by OnLoadConfigurationEntry_End()

	void        ApplyCVars(const SCVarGroup& group, const SCVarGroup* pExclude = 0);

	const char* GetHelpInternal();

	// Arguments:
	//   sKey - must exist, at least in default
	//   pSpec - can be 0
	string GetValueSpec(const string& key, const int* pSpec = 0) const;

	// should only be used by TestCVars()
	// Returns:
	//   true=all console variables match the state (excluding default state), false otherwise
	bool _TestCVars(const SCVarGroup& group, const ICVar::EConsoleLogMode mode, const SCVarGroup* pExclude = 0) const;

	// Arguments:
	//   pGroup - can be 0 to test if the default state is set
	// Returns:
	//   true=all console variables match the state (including default state), false otherwise
	bool TestCVars(const SCVarGroup* pGroup, const ICVar::EConsoleLogMode mode = ICVar::eCLM_Off) const;
};
