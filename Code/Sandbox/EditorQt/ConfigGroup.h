// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace Config
{
// Abstract configurable variable
struct IConfigVar
{
public:
	enum EType
	{
		eType_BOOL,
		eType_INT,
		eType_FLOAT,
		eType_STRING,
	};

	enum EFlags
	{
		eFlag_NoUI      = 1 << 0,
		eFlag_NoCVar    = 1 << 1,
		eFlag_DoNotSave = 1 << 2,
	};

	IConfigVar(const char* szName, const char* szDescription, EType varType, uint8 flags)
		: m_name(szName)
		, m_description(szDescription)
		, m_type(varType)
		, m_flags(flags)
		, m_ptr(NULL)
	{};

	ILINE EType GetType() const
	{
		return m_type;
	}

	ILINE const string& GetName() const
	{
		return m_name;
	}

	ILINE const string& GetDescription() const
	{
		return m_description;
	}

	ILINE bool IsFlagSet(EFlags flag) const
	{
		return 0 != (m_flags & flag);
	}

	virtual void Get(void* outPtr) const = 0;
	virtual void Set(const void* ptr) = 0;
	virtual bool IsDefault() const = 0;
	virtual void GetDefault(void* outPtr) const = 0;
	virtual void Reset() = 0;

	static EType TranslateType(const bool&)   { return eType_BOOL; }
	static EType TranslateType(const int&)    { return eType_INT; }
	static EType TranslateType(const float&)  { return eType_FLOAT; }
	static EType TranslateType(const string&) { return eType_STRING; }

protected:
	EType  m_type;
	uint8  m_flags;
	string m_name;
	string m_description;
	void*  m_ptr;
	ICVar* m_pCVar;
};

// Typed wrapper for config variable
template<class T>
class TConfigVar : public IConfigVar
{
private:
	T m_default;

public:
	TConfigVar(const char* szName, const char* szDescription, uint8 flags, T& ptr, const T& defaultValue)
		: IConfigVar(szName, szDescription, IConfigVar::TranslateType(ptr), flags)
		, m_default(defaultValue)
	{
		m_ptr = &ptr;

		// reset to default value on initializations
		ptr = defaultValue;
	}

	virtual void Get(void* outPtr) const
	{
		*reinterpret_cast<T*>(outPtr) = *reinterpret_cast<const T*>(m_ptr);
	}

	virtual void Set(const void* ptr)
	{
		*reinterpret_cast<T*>(m_ptr) = *reinterpret_cast<const T*>(ptr);
	}

	virtual void Reset()
	{
		*reinterpret_cast<T*>(m_ptr) = m_default;
	}

	virtual void GetDefault(void* outPtr) const
	{
		*reinterpret_cast<T*>(outPtr) = m_default;
	}

	virtual bool IsDefault() const
	{
		return *reinterpret_cast<const T*>(m_ptr) == m_default;
	}
};

// Group of configuration variables with optional mapping to CVars
class CConfigGroup
{
private:
	typedef std::vector<IConfigVar*> TConfigVariables;
	TConfigVariables m_vars;

	typedef std::vector<ICVar*> TConsoleVariables;
	TConsoleVariables m_consoleVars;

public:
	CConfigGroup();
	virtual ~CConfigGroup();

	void              AddVar(IConfigVar* var);
	uint32            GetVarCount();
	IConfigVar*       GetVar(const char* szName);
	IConfigVar*       GetVar(uint index);
	const IConfigVar* GetVar(const char* szName) const;
	const IConfigVar* GetVar(uint index) const;

	void              SaveToXML(XmlNodeRef node);
	void              LoadFromXML(XmlNodeRef node);

	template<class T>
	void AddVar(const char* szName, const char* szDescription, T& var, const T& defaultValue, uint8 flags = 0)
	{
		AddVar(new TConfigVar<T>(szName, szDescription, flags, var, defaultValue));
	}
};

};

