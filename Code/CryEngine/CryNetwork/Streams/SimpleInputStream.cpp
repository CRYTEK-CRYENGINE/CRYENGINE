// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SimpleInputStream.h"

CSimpleInputStream::CSimpleInputStream(size_t size) : m_maxRecords(size), m_numRecords(0), m_curRecord(0), m_records(new SStreamRecord[size])
{
	m_eof = false;
}

CSimpleInputStream::~CSimpleInputStream()
{
	delete[] m_records;
}

const SStreamRecord* CSimpleInputStream::Next(bool peek)
{
	if (m_eof)
		return NULL;
	if (m_curRecord == m_numRecords)
	{
		m_numRecords = m_maxRecords;
		Underflow(m_records, m_numRecords);
		if (m_numRecords == 0)
		{
			m_eof = true;
			return NULL;
		}
		m_curRecord = 0;
	}
	if (peek)
		return m_records + m_curRecord;
	return m_records + (m_curRecord++);
}
