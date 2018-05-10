// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// #SchematycTODO : Split into separate files / move to GameHunt?

#pragma once

#include <CrySerialization/Enum.h>
#include <CrySerialization/IArchiveHost.h>
#include <CrySerialization/StringList.h>
#include <CrySerialization/TypeID.h>
#include <CrySerialization/Decorators/Resources.h>

#include "CrySchematyc/ICore.h"
#include "CrySchematyc/SerializationUtils/ISerializationContext.h"
#include "CrySchematyc/Script/IScriptGraph.h"

namespace Schematyc
{
namespace SerializationUtils
{
template<typename TYPE> class CArchiveBlockDecorator
{
public:

	inline CArchiveBlockDecorator(TYPE& value, const char* szName = "", const char* szLabel = nullptr)
		: m_value(value)
		, m_szName(szName)
		, m_szLabel(szLabel)
	{}

	void Serialize(Serialization::IArchive& archive)
	{
		archive(m_value, m_szName, m_szLabel);
	}

private:

	TYPE&       m_value;
	const char* m_szName;
	const char* m_szLabel;
};

template<typename TYPE> inline CArchiveBlockDecorator<TYPE> ArchiveBlock(TYPE& value, const char* szName = "", const char* szLabel = nullptr)
{
	return CArchiveBlockDecorator<TYPE>(value, szName, szLabel);
}
} // SerializationUtils

typedef std::function<void (Serialization::IArchive&, Serialization::StringList&, int&)> DynamicStringListGenerator;

class CDynamicStringListValue
{
	friend bool Serialize(Serialization::IArchive& archive, CDynamicStringListValue& value, const char* szName, const char* szLabel);

public:

	inline CDynamicStringListValue(const DynamicStringListGenerator& stringListGenerator, const char* szValue = "")
		: m_stringListGenerator(stringListGenerator)
		, m_value(szValue)
	{}

	inline const char* c_str() const
	{
		return m_value.c_str();
	}

private:

	inline bool Serialize(Serialization::IArchive& archive, const char* szName, const char* szLabel)
	{
		if (archive.isEdit())
		{
			Serialization::StringList stringList;
			int index = Serialization::StringList::npos;
			if (m_stringListGenerator)
			{
				m_stringListGenerator(archive, stringList, index);
			}
			const char* szWarning = nullptr;
			if (!m_value.empty())
			{
				index = stringList.find(m_value.c_str());
				if (index == Serialization::StringList::npos)
				{
					index = stringList.size();
					stringList.push_back(m_value.c_str());
					szWarning = "Current value is out of range!";
				}
			}
			Serialization::StringListValue value(stringList, index);
			archive(value, szName, szLabel);
			if (archive.isInput())
			{
				m_value = value.index() != Serialization::StringList::npos ? value.c_str() : "";
			}
			if (szWarning)
			{
				archive.warning(value, szWarning);
			}
		}
		else
		{
			archive(m_value, szName);
		}
		return true;
	}

	DynamicStringListGenerator m_stringListGenerator;
	string                     m_value;
};

inline bool Serialize(Serialization::IArchive& archive, CDynamicStringListValue& value, const char* szName, const char* szLabel)
{
	return value.Serialize(archive, szName, szLabel);
}

typedef std::vector<string> StringVector;

class CDynamicStringListValueList
{
	friend bool Serialize(Serialization::IArchive& archive, CDynamicStringListValueList& value, const char* szName, const char* szLabel);

public:

	inline CDynamicStringListValueList(const DynamicStringListGenerator& stringListGenerator)
		: m_stringListGenerator(stringListGenerator)
	{}

	const StringVector& GetActiveValues() const
	{
		return m_activeValues;
	}

private:

	struct SValue
	{
		inline SValue()
			: bActive(false)
		{}

		SValue(bool _bActive, const char* _szName)
			: bActive(_bActive)
			, name(_szName)
		{}

		void Serialize(Serialization::IArchive& archive)
		{
			if (archive.openBlock("value", "^"))
			{
				archive(bActive, "active", "^");
				archive(name, "name", "!^");
				archive.closeBlock();
			}
		}

		bool   bActive;
		string name;
	};

	typedef std::vector<SValue> ValueVector;

	inline bool Serialize(Serialization::IArchive& archive, const char* szName, const char* szLabel)
	{
		if (archive.isEdit())
		{
			Serialization::StringList stringList;
			int index = Serialization::StringList::npos;
			if (m_stringListGenerator)
			{
				m_stringListGenerator(archive, stringList, index);
			}

			m_values.clear();
			m_values.reserve(stringList.size());
			for (Serialization::StringList::value_type& stringListValue : stringList)
			{
				m_values.push_back(SValue(false, stringListValue.c_str()));
			}

			StringVector missingValues;
			for (string& activeValue : m_activeValues)
			{
				bool bFoundValue = false;
				for (SValue& value : m_values)
				{
					if (activeValue == value.name)
					{
						value.bActive = true;
						bFoundValue = true;
						break;
					}
				}
				if (!bFoundValue)
				{
					missingValues.push_back(activeValue);
				}
			}

			for (string& missingValue : missingValues)
			{
				stl::push_back_unique(m_activeValues, missingValue);
				archive.warning(m_values, "Missing value: %s", missingValue.c_str());
				m_values.push_back(SValue(true, missingValue));
			}

			archive(m_values, szName, szLabel);

			if (archive.isInput())
			{
				m_activeValues.clear();
				for (SValue& value : m_values)
				{
					if (value.bActive)
					{
						m_activeValues.push_back(value.name);
					}
				}
			}
			else if (archive.isOutput())
			{
				archive(m_activeValues, "activeValues");
			}
		}
		else
		{
			archive(m_activeValues, "activeValues");
		}
		return true;
	}

	DynamicStringListGenerator m_stringListGenerator;
	ValueVector                m_values;
	StringVector               m_activeValues;
};

inline bool Serialize(Serialization::IArchive& archive, CDynamicStringListValueList& value, const char* szName, const char* szLabel)
{
	return value.Serialize(archive, szName, szLabel);
}

template<typename TYPE> class CCopySerializer
{
public:

	inline CCopySerializer(const TYPE& value)
		: m_value(value)
	{}

	inline void Serialize(Serialization::IArchive& archive)
	{
		const_cast<IScriptGraphNode&>(m_value).Copy(archive);
	}

private:

	const TYPE& m_value;
};

template<typename TYPE> CCopySerializer<TYPE> CopySerialize(const TYPE& value)
{
	return CCopySerializer<TYPE>(value);
}

template<typename TYPE> class CPasteSerializer
{
public:

	inline CPasteSerializer(TYPE& value)
		: m_value(value)
	{}

	inline void Serialize(Serialization::IArchive& archive)
	{
		m_value.Paste(archive);
	}

private:

	TYPE& m_value;
};

template<typename TYPE> CPasteSerializer<TYPE> PasteSerialize(TYPE& value)
{
	return CPasteSerializer<TYPE>(value);
}
} // Schematyc
