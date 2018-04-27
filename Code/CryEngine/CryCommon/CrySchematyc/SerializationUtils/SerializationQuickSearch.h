// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySerialization/StringList.h>
#include <CrySerialization/Decorators/Resources.h>
#include <CrySerialization/Decorators/ResourceSelector.h>

#include "CrySchematyc/Editor/IQuickSearchOptions.h"
#include "CrySchematyc/Utils/Assert.h"

namespace Schematyc
{
namespace SerializationUtils
{
namespace Private
{

class CStringListQuickSearchOptions : public Serialization::ICustomResourceParams, public IQuickSearchOptions
{
public:

	inline CStringListQuickSearchOptions(const char* szHeader, const char* szDelimiter)
		: m_header(szHeader)
		, m_delimiter(szDelimiter)
	{}

	inline CStringListQuickSearchOptions(const Serialization::StringList& names, const char* szHeader, const char* szDelimiter)
		: m_names(names)
		, m_header(szHeader)
		, m_delimiter(szDelimiter)
	{}

	// IQuickSearchOptions

	virtual uint32 GetCount() const override
	{
		return m_names.size();
	}

	virtual const char* GetLabel(uint32 optionIdx) const override
	{
		return GetName(optionIdx);
	}

	virtual const char* GetDescription(uint32 optionIdx) const override
	{
		return "";
	}

	virtual const char* GetHeader() const override
	{
		return m_header.c_str();
	}

	virtual const char* GetDelimiter() const override
	{
		return m_delimiter.c_str();
	}

	// ~IQuickSearchOptions

	inline const char* GetName(uint32 optionIdx) const
	{
		return optionIdx < (uint32)m_names.size() ? m_names[optionIdx] : "";
	}

protected:

	Serialization::StringList m_names;
	string                    m_header;
	string                    m_delimiter;
};

struct SStringListQuickSearchDecorator
{
	inline SStringListQuickSearchDecorator(Serialization::StringListValue& _value, const char* _szHeader, const char* _szDelimiter)
		: value(_value)
		, szHeader(_szHeader)
		, szDelimiter(_szDelimiter)
	{}

	Serialization::StringListValue& value;
	const char*                     szHeader;
	const char*                     szDelimiter;
};

template<typename TYPE> class CQuickSearchOptions : public CStringListQuickSearchOptions
{
private:

	struct SOption
	{
		inline SOption(const TYPE& _value, const char* _szLabel, const char* _szDescription)
			: value(_value)
			, label(_szLabel)
			, description(_szDescription)
		{}

		TYPE   value;
		string label;
		string description;
	};

	typedef std::vector<SOption> Options;

public:

	inline CQuickSearchOptions(const char* szHeader, const char* szDelimiter)
		: CStringListQuickSearchOptions(szHeader, szDelimiter)
	{}

	// IQuickSearchOptions

	virtual const char* GetLabel(uint32 optionIdx) const
	{
		return optionIdx < m_options.size() ? m_options[optionIdx].label.c_str() : "";
	}

	virtual const char* GetDescription(uint32 optionIdx) const
	{
		return optionIdx < m_options.size() ? m_options[optionIdx].description.c_str() : "";
	}

	// ~IQuickSearchOptions

	inline void AddOption(const char* szName, const TYPE& value, const char* szLabel, const char* szDescription)
	{
		SCHEMATYC_CORE_ASSERT(szName);
		if (szName)
		{
			m_names.push_back(szName);
			m_options.push_back(SOption(value, szLabel ? szLabel : szName, szDescription));
		}
	}

	inline bool FromString(const char* szName, TYPE& value) const
	{
		const int pos = m_names.find(szName);
		if (pos != Serialization::StringList::npos)
		{
			value = m_options[pos].value;
			return true;
		}
		return false;
	}

	inline const char* ToString(const TYPE& value) const
	{
		for (uint32 idx = 0, count = m_names.size(); idx < count; ++idx)
		{
			if (m_options[idx].value == value)
			{
				return m_names[idx].c_str();
			}
		}
		return "";
	}

private:

	Options m_options;
};

} // Private

// Scoped configuration for mapping names to values and displaying as quick-search trees.
template<typename TYPE> class CScopedQuickSearchConfig
{
private:

	typedef Private::CQuickSearchOptions<TYPE> Options;

	DECLARE_SHARED_POINTERS(Options)

public:

	inline CScopedQuickSearchConfig(Serialization::IArchive& archive, const char* szHeader, const char* szDelimiter = nullptr)
		: m_context(archive, static_cast<CScopedQuickSearchConfig*>(nullptr))
		, m_pOptions(std::make_shared<Options>(szHeader, szDelimiter))
	{
		m_context.set(static_cast<CScopedQuickSearchConfig*>(this));
	}

	inline void AddOption(const char* szName, const TYPE& value, const char* szLabel = nullptr, const char* szDescription = nullptr)
	{
		m_pOptions->AddOption(szName, value, szLabel, szDescription);
	}

	inline const OptionsPtr& GetOptions()
	{
		return m_pOptions;
	}

private:

	Serialization::SContext m_context;
	OptionsPtr              m_pOptions;
};

namespace Private
{

class CStringListStaticQuickSearchOptions : public Serialization::ICustomResourceParams, public IQuickSearchOptions
{
public:

	inline CStringListStaticQuickSearchOptions(const Serialization::StringListStatic& names, const char* szHeader, const char* szDelimiter)
		: m_names(names)
		, m_header(szHeader)
		, m_delimiter(szDelimiter)
	{}

	// IQuickSearchOptions

	virtual uint32 GetCount() const override
	{
		return m_names.size();
	}

	virtual const char* GetLabel(uint32 optionIdx) const override
	{
		return GetName(optionIdx);
	}

	virtual const char* GetDescription(uint32 optionIdx) const override
	{
		return "";
	}

	virtual const char* GetHeader() const override
	{
		return m_header.c_str();
	}

	virtual const char* GetDelimiter() const override
	{
		return m_delimiter.c_str();
	}

	// ~IQuickSearchOptions

	inline const char* GetName(uint32 optionIdx) const
	{
		return optionIdx < (uint32)m_names.size() ? m_names[optionIdx] : "";
	}

	//protected:

	const Serialization::StringListStatic& m_names;
	string m_header;
	string m_delimiter;
};

struct SStringListStaticQuickSearchDecorator
{
	inline SStringListStaticQuickSearchDecorator(Serialization::StringListStaticValue& _value, const char* _szHeader, const char* _szDelimiter)
		: value(_value)
		, szHeader(_szHeader)
		, szDelimiter(_szDelimiter)
	{}

	Serialization::StringListStaticValue& value;
	const char*                           szHeader;
	const char*                           szDelimiter;
};

inline bool Serialize(Serialization::IArchive& archive, SStringListStaticQuickSearchDecorator& value, const char* szName, const char* szLabel)
{
	if (archive.isEdit())
	{
		Serialization::ResourceSelector<Serialization::StringListStaticValue> resourceSelector(value.value, "StringListStaticSearch", std::make_shared<CStringListStaticQuickSearchOptions>(value.value.stringList(), value.szHeader, value.szDelimiter));
		return archive(resourceSelector, szName, szLabel);
	}
	else
	{
		return archive(value.value, szName, szLabel);
	}
}

inline bool Serialize(Serialization::IArchive& archive, SStringListQuickSearchDecorator& value, const char* szName, const char* szLabel)
{
	if (archive.isEdit())
	{
		Serialization::ResourceSelector<Serialization::StringListValue> resourceSelector(value.value, "StringListSearch", std::make_shared<CStringListQuickSearchOptions>(value.value.stringList(), value.szHeader, value.szDelimiter));
		return archive(resourceSelector, szName, szLabel);
	}
	else
	{
		return archive(value.value, szName, szLabel);
	}
}

template<typename TYPE> struct SQuickSearchDecorator
{
	inline SQuickSearchDecorator(TYPE& _value)
		: value(_value)
	{}

	TYPE& value;
};

template<typename TYPE> class CQuickSearchAdapter
{
private:

	typedef CQuickSearchOptions<TYPE> Options;

public:

	inline CQuickSearchAdapter(TYPE& value, const Options& options)
		: m_value(value)
		, m_options(options)
	{}

	inline void        Serialize(Serialization::IArchive& archive) {}

	inline const char* c_str() const
	{
		return m_options.ToString(m_value);
	}

	inline void operator=(const char* szName)
	{
		m_options.FromString(szName, m_value);
	}

private:

	TYPE&          m_value;
	const Options& m_options;
};

template<typename TYPE> inline bool Serialize(Serialization::IArchive& archive, SQuickSearchDecorator<TYPE>& value, const char* szName, const char* szLabel)
{
	typedef CScopedQuickSearchConfig<TYPE> Config;

	if (archive.isEdit())
	{
		Config* pConfig = archive.context<Config>();
		if (pConfig)
		{
			CQuickSearchAdapter<TYPE> adapter(value.value, *pConfig->GetOptions());
			Serialization::ResourceSelector<CQuickSearchAdapter<TYPE>> resourceSelector(adapter, "StringListSearch", pConfig->GetOptions());
			return archive(resourceSelector, szName, szLabel);
		}
		return false;
	}
	else
	{
		return archive(value.value, szName, szLabel);
	}
}

} // Private

// Decorator for displaying static string lists as quick-search trees.
inline Private::SStringListStaticQuickSearchDecorator QuickSearch(Serialization::StringListStaticValue& value, const char* szHeader = nullptr, const char* szDelimiter = nullptr)
{
	return Private::SStringListStaticQuickSearchDecorator(value, szHeader, szDelimiter);
}

// Decorator for displaying string lists as quick-search trees.
inline Private::SStringListQuickSearchDecorator QuickSearch(Serialization::StringListValue& value, const char* szHeader = nullptr, const char* szDelimiter = nullptr)
{
	return Private::SStringListQuickSearchDecorator(value, szHeader, szDelimiter);
}

// Decorator for displaying quick-search trees.
template<typename TYPE> inline Private::SQuickSearchDecorator<TYPE> QuickSearch(TYPE& value)
{
	return Private::SQuickSearchDecorator<TYPE>(value);
}

// Wrapper type for serializing nested values as quick-search trees.
template<typename TYPE> class SQuickSearchTypeWrapper
{
public:

	inline SQuickSearchTypeWrapper()
		: value()
	{}

	inline SQuickSearchTypeWrapper(const TYPE& _value)
		: value(_value)
	{}

	TYPE value;
};

template<typename TYPE> inline bool Serialize(Serialization::IArchive& archive, SQuickSearchTypeWrapper<TYPE>& value, const char* szName, const char* szLabel)
{
	return archive(SerializationUtils::QuickSearch(value.value), szName, szLabel);
}

} // SerializationUtils
} // Schematyc
