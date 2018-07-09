// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryExtension/CryGUID.h>

namespace CryGraph {

typedef struct SPinId
{
	enum : uint16 { Invalid = 0xffff };

	SPinId()
		: m_id(Invalid)
	{}

	SPinId(uint16 id)
		: m_id(id)
	{}

	SPinId operator=(uint16 id)
	{
		m_id = id;
		return *this;
	}

	bool IsValid() const
	{
		return m_id != Invalid;
	}

	operator uint16() const
	{
		return m_id;
	}

private:
	uint16 m_id;
} PinId;

typedef struct SPinIndex
{
	enum : uint16 { Invalid = 0xffff };

	SPinIndex()
		: m_index(Invalid)
	{}

	SPinIndex(uint16 index)
		: m_index(index)
	{}

	SPinIndex operator=(uint16 index)
	{
		m_index = index;
		return *this;
	}

	bool IsValid() const
	{
		return m_index != Invalid;
	}

	operator uint16() const
	{
		return m_index;
	}

private:
	uint16 m_index;
} PinIndex;

enum class EPinType
{
	Unset = 0,
	DataInput,
	DataOutput,
};

class CBaseNode
{
};

typedef CryGUID DataType;

}

