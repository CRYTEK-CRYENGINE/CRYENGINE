// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace Schematyc
{
struct IString
{
	virtual ~IString() {}

	virtual const char* c_str() const = 0;
	virtual uint32      length() const = 0;
	virtual bool        empty() const = 0;

	virtual void        clear() = 0;
	virtual void        assign(const char* szInput) = 0;
	virtual void        assign(const char* szInput, uint32 offset, uint32 length) = 0;
	virtual void        append(const char* szInput) = 0;
	virtual void        append(const char* szInput, uint32 offset, uint32 length) = 0;
	virtual void        insert(uint32 offset, const char* szInput) = 0;
	virtual void        insert(uint32 offset, const char* szInput, uint32 length) = 0;

	virtual void        Format(const char* szFormat, ...) = 0;
	virtual void        TrimLeft(const char* szChars) = 0;
	virtual void        TrimRight(const char* szChars) = 0;
};
} // Schematyc
