// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// Created by: Michael Kopietz
// Modified: -
//
//---------------------------------------------------------------------------

#ifndef __CCRYPOOLMEMORY__
#define __CCRYPOOLMEMORY__

namespace NCryPoolAlloc
{

class CMemoryDynamic
{
	size_t m_Size;
	uint8* m_pData;

protected:
	ILINE CMemoryDynamic() :
		m_Size(0),
		m_pData(0){}

public:
	ILINE void InitMem(const size_t S, uint8* pData)
	{
		m_Size = S;
		m_pData = pData;
		CPA_ASSERT(S);
		CPA_ASSERT(pData);
	}

	ILINE size_t       MemSize() const { return m_Size; }
	ILINE uint8*       Data()          { return m_pData; }
	ILINE const uint8* Data() const    { return m_pData; }
};

template<size_t TSize>
class CMemoryStatic
{
	uint8 m_Data[TSize];

protected:
	ILINE CMemoryStatic()
	{
	}
public:
	ILINE void InitMem(const size_t S, uint8* pData)
	{
	}
	ILINE size_t       MemSize() const { return TSize; }
	ILINE uint8*       Data()          { return m_Data; }
	ILINE const uint8* Data() const    { return m_Data; }
};

}

#endif
