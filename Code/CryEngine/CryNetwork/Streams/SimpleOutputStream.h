// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __SIMPLEOUTPUTSTREAM_H__
#define __SIMPLEOUTPUTSTREAM_H__

#pragma once

#include <CryNetwork/INetwork.h>
#include "SimpleStreamDefs.h"

class CSimpleOutputStream
{
public:
	CSimpleOutputStream(size_t numRecords);
	virtual ~CSimpleOutputStream();

	void Sync();
	void Put(const char* key, const char* value);
	void Put(const char* key, float value);
	void Put(const char* key, int value);
	void Put(const char* key, uint64 value);
	void Put(const char* key, int64 value);
	void Put(const char* key, EntityId value);
	void Put(const char* key, Vec2 value);
	void Put(const char* key, Vec3 value);
	void Put(const char* key, Ang3 value) { Put(key, Vec3(value)); }
	void Put(const char* key, Quat value);
	void Put(const char* key, SNetObjectID value);

	void GetMemoryStatistics(ICrySizer* pSizer)
	{
		SIZER_COMPONENT_NAME(pSizer, "CSimpleOutputStream");

		pSizer->Add(*this);
		if (m_records)
			pSizer->Add(m_records, m_numRecords);
	}

	void Put(const SStreamRecord& record);

private:
	virtual void Flush(const SStreamRecord* pRecords, size_t numRecords) = 0;

	size_t         m_numRecords;
	size_t         m_curRecord;
	SStreamRecord* m_records;
};

#endif
