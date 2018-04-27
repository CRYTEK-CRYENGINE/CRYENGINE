// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc2/Prerequisites.h"

#include "CrySchematyc2/Utils/StringUtils.h"

namespace Schematyc2
{
	class CStringOArchive : public Serialization::IArchive
	{
	public:

		inline CStringOArchive(const CharArrayView& output)
			: Serialization::IArchive(Serialization::IArchive::OUTPUT)
			, m_output(output)
			, m_pos(0)
		{}

		// Serialization::IArchive

		virtual bool operator () (bool& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			PrintHeader(szName, szLabel);
			char stringBuffer[StringUtils::s_boolStringBufferSize] = "";
			Append(StringUtils::BoolToString(value, stringBuffer));
			Append(", ");
			return true;
		}

		virtual bool operator () (int8& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			PrintHeader(szName, szLabel);
			char stringBuffer[StringUtils::s_int8StringBufferSize] = "";
			Append(StringUtils::Int8ToString(value, stringBuffer));
			Append(", ");
			return true;
		}

		virtual bool operator () (uint8& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			PrintHeader(szName, szLabel);
			char stringBuffer[StringUtils::s_uint8StringBufferSize] = "";
			Append(StringUtils::UInt8ToString(value, stringBuffer));
			Append(", ");
			return true;
		}

		virtual bool operator () (int32& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			PrintHeader(szName, szLabel);
			char stringBuffer[StringUtils::s_int32StringBufferSize] = "";
			Append(StringUtils::Int32ToString(value, stringBuffer));
			Append(", ");
			return true;
		}

		virtual bool operator () (uint32& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			PrintHeader(szName, szLabel);
			char stringBuffer[StringUtils::s_uint32StringBufferSize] = "";
			Append(StringUtils::UInt32ToString(value, stringBuffer));
			Append(", ");
			return true;
		}

		virtual bool operator () (int64& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			PrintHeader(szName, szLabel);
			char stringBuffer[StringUtils::s_int64StringBufferSize] = "";
			Append(StringUtils::Int64ToString(value, stringBuffer));
			Append(", ");
			return true;
		}

		virtual bool operator () (uint64& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			PrintHeader(szName, szLabel);
			char stringBuffer[StringUtils::s_uint64StringBufferSize] = "";
			Append(StringUtils::UInt64ToString(value, stringBuffer));
			Append(", ");
			return true;
		}

		virtual bool operator () (float& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			PrintHeader(szName, szLabel);
			char stringBuffer[StringUtils::s_floatStringBufferSize] = "";
			Append(StringUtils::FloatToString(value, stringBuffer));
			Append(", ");
			return true;
		}

		virtual bool operator () (Serialization::IString& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			PrintHeader(szName, szLabel);
			Append(value.get());
			Append(", ");
			return true;
		}

		virtual bool operator () (const Serialization::SStruct& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			PrintHeader(szName, szLabel);
			Append("{ ");
			value(*this);
			TrimRight();
			Append(" }, ");
			return true;
		}

		virtual bool operator () (Serialization::IContainer& value, const char* szName = "", const char* szLabel = nullptr) override
		{
			if(value.size())
			{
				PrintHeader(szName, szLabel);
				Append("[ ");
				do
				{
					value(*this, "", nullptr);
				} while(value.next());
				TrimRight();
				Append(" ], ");
			}
			return true;
		}

		using Serialization::IArchive::operator ();

		// ~Serialization::IArchive

		inline void TrimRight()
		{
			if(m_pos != StringUtils::s_invalidPos)
			{
				m_pos -= StringUtils::TrimRight(m_output, ", ", m_pos);
			}
		}

	private:

		inline void PrintHeader(const char* szName, const char* szLabel)
		{
			if(szLabel && szLabel[0])
			{
				Append(szLabel);
				Append(" = ");
			}
			else if(szName[0])
			{
				Append(szName);
				Append(" = ");
			}
		}

		inline void Append(const char* szInput)
		{
			if(m_pos != StringUtils::s_invalidPos)
			{
				const int32	appendLength = StringUtils::Append(szInput, m_output, m_pos);
				if(appendLength)
				{
					m_pos += appendLength;
				}
				else
				{
					m_pos = StringUtils::s_invalidPos;
				}
			}
		}

		CharArrayView m_output;
		int32         m_pos;
	};
}
