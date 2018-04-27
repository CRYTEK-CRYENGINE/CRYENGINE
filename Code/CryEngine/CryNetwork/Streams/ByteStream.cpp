// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ByteStream.h"

uint32 CByteStreamPacker::GetNextOfs(uint32 sz)
{
	uint32 out;
	switch (sz)
	{
	case 0:
		return m_data[eD_Size];
	case 1:
		out = GetNextOfs_Fixed(NTypelist::Int2Type<1>());
		break;
	case 2:
		out = GetNextOfs_Fixed(NTypelist::Int2Type<2>());
		break;
	case 3:
	case 4:
		out = GetNextOfs_Fixed(NTypelist::Int2Type<4>());
		break;
	default:
		out = m_data[eD_Size];
		m_data[eD_Size] += sz + (sz % 4 != 0) * (4 - sz % 4);
		break;
	}
	return out;
}

void CByteOutputStream::Grow(size_t sz)
{
	size_t oldCapacity = m_capacity;
	// exponentially grow, to get an amortized constant growth time
	do
		m_capacity *= 2;
	while (m_capacity < sz);
	m_buffer = (uint8*)m_pSA->Realloc(m_buffer, m_capacity);
	memset(m_buffer + oldCapacity, 0, m_capacity - oldCapacity);
}
