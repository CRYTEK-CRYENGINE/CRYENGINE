// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _AISIGNAL_H_
#define _AISIGNAL_H_

#if _MSC_VER > 1000
	#pragma once
#endif

#include <CryAISystem/IAISystem.h>
#include <CryNetwork/ISerialize.h>
#include <CryMemory/PoolAllocator.h>

struct AISignalExtraData : public IAISignalExtraData
{
public:
	static void CleanupPool();

public:
	AISignalExtraData();
	AISignalExtraData(const AISignalExtraData& other) : IAISignalExtraData(other), sObjectName() { SetObjectName(other.sObjectName); }
	virtual ~AISignalExtraData();

	AISignalExtraData& operator=(const AISignalExtraData& other);
	virtual void       Serialize(TSerialize ser);

	inline void*       operator new(size_t size)
	{
		return m_signalExtraDataAlloc.Allocate();
	}

	inline void operator delete(void* p)
	{
		return m_signalExtraDataAlloc.Deallocate(p);
	}

	virtual const char* GetObjectName() const { return sObjectName ? sObjectName : ""; }
	virtual void        SetObjectName(const char* objectName);

	// To/from script table
	virtual void ToScriptTable(SmartScriptTable& table) const;
	virtual void FromScriptTable(const SmartScriptTable& table);

private:
	char* sObjectName;

	typedef stl::PoolAllocator<sizeof(IAISignalExtraData) + sizeof(void*),
	                           stl::PoolAllocatorSynchronizationSinglethreaded> SignalExtraDataAlloc;
	static SignalExtraDataAlloc m_signalExtraDataAlloc;
};

#endif
