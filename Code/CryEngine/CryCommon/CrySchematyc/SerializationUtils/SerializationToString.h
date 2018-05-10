// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySerialization/Enum.h>
#include <CrySerialization/Forward.h>
#include <CrySerialization/IArchive.h>

#include "CrySchematyc/Utils/IString.h"

namespace Schematyc
{
namespace SerializationUtils
{
namespace Private
{
class CStringOArchive : public Serialization::IArchive
{
public:

	inline CStringOArchive(IString& output)
		: Serialization::IArchive(Serialization::IArchive::OUTPUT)
		, m_output(output)
	{}

	// Serialization::IArchive

	virtual bool operator()(bool& value, const char* szName = "", const char* szLabel = nullptr) override
	{
		PrintHeader(szName, szLabel);
		m_output.append(value ? "1" : "0");
		m_output.append(", ");
		return true;
	}

	virtual bool operator()(int8& value, const char* szName = "", const char* szLabel = nullptr) override
	{
		PrintHeader(szName, szLabel);
		{
			char temp[8] = "";
			itoa(value, temp, 10);
			m_output.append(temp);
		}
		m_output.append(", ");
		return true;
	}

	virtual bool operator()(uint8& value, const char* szName = "", const char* szLabel = nullptr) override
	{
		PrintHeader(szName, szLabel);
		{
			char temp[8] = "";
			ltoa(value, temp, 10);
			m_output.append(temp);
		}
		m_output.append(", ");
		return true;
	}

	virtual bool operator()(int32& value, const char* szName = "", const char* szLabel = nullptr) override
	{
		PrintHeader(szName, szLabel);
		{
			char temp[16] = "";
			itoa(value, temp, 10);
			m_output.append(temp);
		}
		m_output.append(", ");
		return true;
	}

	virtual bool operator()(uint32& value, const char* szName = "", const char* szLabel = nullptr) override
	{
		PrintHeader(szName, szLabel);
		{
			char temp[16] = "";
			ltoa(value, temp, 10);
			m_output.append(temp);
		}
		m_output.append(", ");
		return true;
	}

	virtual bool operator()(int64& value, const char* szName = "", const char* szLabel = nullptr) override
	{
		PrintHeader(szName, szLabel);
		{
			char temp[32] = "";
			cry_sprintf(temp, "%llu", static_cast<unsigned long long>(value)); // #SchematycTODO : Make sure %llu works on Windows platforms!!!
			m_output.append(temp);
		}
		m_output.append(", ");
		return true;
	}

	virtual bool operator()(uint64& value, const char* szName = "", const char* szLabel = nullptr) override
	{
		PrintHeader(szName, szLabel);
		{
			char temp[32] = "";
			cry_sprintf(temp, "%lld", static_cast<unsigned long long>(value)); // #SchematycTODO : Make sure %lld works on Windows platforms!!!
			m_output.append(temp);
		}
		m_output.append(", ");
		return true;
	}

	virtual bool operator()(float& value, const char* szName = "", const char* szLabel = nullptr) override
	{
		PrintHeader(szName, szLabel);
		{
			char temp[64] = "";
			cry_sprintf(temp, "%.8f", value);
			m_output.append(temp);
		}
		m_output.append(", ");
		return true;
	}

	virtual bool operator()(Serialization::IString& value, const char* szName = "", const char* szLabel = nullptr) override
	{
		PrintHeader(szName, szLabel);
		m_output.append(value.get());
		m_output.append(", ");
		return true;
	}

	virtual bool operator()(const Serialization::SStruct& value, const char* szName = "", const char* szLabel = nullptr) override
	{
		PrintHeader(szName, szLabel);
		m_output.append("{ ");
		value(*this);
		m_output.TrimRight(", ");
		m_output.append(" }, ");
		return true;
	}

	virtual bool operator()(Serialization::IContainer& value, const char* szName = "", const char* szLabel = nullptr) override
	{
		if (value.size())
		{
			PrintHeader(szName, szLabel);
			m_output.append("[ ");
			do
			{
				value(*this, "", nullptr);
			}
			while (value.next());
			m_output.TrimRight(", ");
			m_output.append(" ], ");
		}
		return true;
	}

	using Serialization::IArchive::operator();

	// ~Serialization::IArchive

private:

	inline void PrintHeader(const char* szName, const char* szLabel)
	{
		if (szLabel && szLabel[0])
		{
			m_output.append(szLabel);
			m_output.append(" = ");
		}
		else if (szName[0])
		{
			m_output.append(szName);
			m_output.append(" = ");
		}
	}

	IString& m_output;
};

template<typename TYPE, bool TYPE_IS_ENUM = std::is_enum<TYPE>::value> struct SToStringHelper;

template<typename TYPE> struct SToStringHelper<TYPE, true>
{
	inline bool operator()(IString& output, const TYPE& input) const
	{
		const Serialization::EnumDescription& enumDescription = Serialization::getEnumDescription<TYPE>();
		output.assign(enumDescription.labelByIndex(enumDescription.indexByValue(static_cast<int>(input))));
		return true;
	}
};

template<typename TYPE> struct SToStringHelper<TYPE, false>
{
	inline bool operator()(IString& output, const TYPE& input) const
	{
		CStringOArchive archive(output);
		const bool bResult = archive(input);
		output.TrimRight(", ");
		return bResult;
	}
};
}   // Private

template<typename TYPE> bool ToString(IString& output, const TYPE& input)
{
	return Private::SToStringHelper<TYPE>()(output, input);
}
} // SerializationUtils
} // Schematyc
