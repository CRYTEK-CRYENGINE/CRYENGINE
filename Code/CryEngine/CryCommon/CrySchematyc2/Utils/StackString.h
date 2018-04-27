// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySerialization/Forward.h>
#include <CrySerialization/IArchive.h>
#include <CrySerialization/STL.h>

#include "CrySchematyc2/Utils/IString.h"

namespace Schematyc2
{
class CStackString : public IString, public stack_string
{
public:

	inline CStackString() {}

	inline CStackString(const char* szValue)
		: stack_string(szValue)
	{}

	inline CStackString(const stack_string& rhs)
		: stack_string(rhs)
	{}

	inline CStackString(const CStackString& rhs)
		: stack_string(rhs)
	{}

	// IStringWrapper

	virtual const char* c_str() const override
	{
		return stack_string::c_str();
	}

	virtual uint32 length() const override
	{
		return stack_string::length();
	}

	virtual bool empty() const override
	{
		return stack_string::empty();
	}

	virtual void clear() override
	{
		stack_string::clear();
	}

	virtual void assign(const char* szInput) override
	{
		stack_string::assign(szInput);
	}

	virtual void assign(const char* szInput, uint32 offset, uint32 length) override
	{
		stack_string::assign(szInput, offset, length);
	}

	virtual void append(const char* szInput) override
	{
		stack_string::append(szInput ? szInput : "");
	}

	virtual void append(const char* szInput, uint32 offset, uint32 length) override
	{
		stack_string::append(szInput ? szInput : "", offset, length);
	}

	virtual void insert(uint32 offset, const char* szInput) override
	{
		stack_string::insert(offset, szInput);
	}

	virtual void insert(uint32 offset, const char* szInput, uint32 length) override
	{
		stack_string::insert(offset, szInput, length);
	}

	virtual void Format(const char* szFormat, ...) override
	{
		va_list va_args;
		va_start(va_args, szFormat);

		stack_string::FormatV(szFormat, va_args);

		va_end(va_args);
	}

	virtual void TrimLeft(const char* szChars) override
	{
		stack_string::TrimLeft(szChars);
	}

	virtual void TrimRight(const char* szChars) override
	{
		stack_string::TrimRight(szChars);
	}

	// ~IStringWrapper

	inline CStackString& operator=(const char* szValue)
	{
		stack_string::operator=(szValue);
		return *this;
	}
};

namespace SerializationUtils
{
class CStackStringWrapper : public Serialization::IString
{
public:

	inline CStackStringWrapper(CStackString& value)
		: m_value(value)
	{}

	// Serialization::IString

	virtual void set(const char* szValue) override
	{
		m_value = szValue;
	}

	virtual const char* get() const override
	{
		return m_value.c_str();
	}

	virtual const void* handle() const override
	{
		return &m_value;
	}

	virtual Serialization::TypeID type() const override
	{
		return Serialization::TypeID::get<CStackString>();
	}

	// Serialization::IString

private:

	CStackString& m_value;
};
}

inline bool Serialize(Serialization::IArchive& archive, CStackString& value, const char* szName, const char* szLabel)
{
	SerializationUtils::CStackStringWrapper wrapper(value);
	return archive(static_cast<Serialization::IString&>(wrapper), szName, szLabel);
}
} // Schematyc
