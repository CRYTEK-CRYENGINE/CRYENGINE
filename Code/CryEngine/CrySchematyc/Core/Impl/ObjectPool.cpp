// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ObjectPool.h"

#include <CrySchematyc/Runtime/IRuntimeClass.h>

#include "Core.h"
#include "Object.h"
#include "Runtime/RuntimeRegistry.h"

namespace Schematyc
{
bool CObjectPool::CreateObject(const Schematyc::SObjectParams& params, IObject*& pObjectOut)
{
	CRuntimeClassConstPtr pClass = CCore::GetInstance().GetRuntimeRegistryImpl().GetClassImpl(params.classGUID);
	if (pClass)
	{
		if (m_freeSlots.empty() && !AllocateSlots(m_slots.size() + 1))
		{
			return false;
		}

		const uint32 slodIdx = m_freeSlots.back();
		m_freeSlots.pop_back();
		SSlot& slot = m_slots[slodIdx];

		CObjectPtr pObject = std::make_shared<CObject>(*params.pEntity, slot.objectId, params.pCustomData);
		if (pObject->Init(params.classGUID, params.pProperties))
		{
			slot.pObject = pObject;
			pObjectOut = pObject.get();
			return true;
		}
		else
		{
			m_freeSlots.push_back(slodIdx);
		}
	}
	return false;
}

IObject* CObjectPool::GetObject(ObjectId objectId)
{
	uint32 slotIdx;
	uint32 salt;
	if (ExpandObjectId(objectId, slotIdx, salt))
	{
		return m_slots[slotIdx].pObject.get();
	}
	return nullptr;
}

void CObjectPool::DestroyObject(ObjectId objectId)
{
	uint32 slotIdx;
	uint32 salt;
	if (ExpandObjectId(objectId, slotIdx, salt))
	{
		SSlot& slot = m_slots[slotIdx];
		slot.pObject.reset();
		slot.objectId = CreateObjectId(slotIdx, IncrementSalt(salt));
		m_freeSlots.push_back(slotIdx);
	}
}

void CObjectPool::SendSignal(ObjectId objectId, const SObjectSignal& signal)
{
	IObject* pObject = GetObject(objectId);
	if (pObject)
	{
		pObject->ProcessSignal(signal);
	}
}

void CObjectPool::BroadcastSignal(const SObjectSignal& signal)
{
	for (SSlot& slot : m_slots)
	{
		if (slot.pObject)
		{
			slot.pObject->ProcessSignal(signal);
		}
	}
}

bool CObjectPool::AllocateSlots(uint32 slotCount)
{
	const uint32 prevSlotCount = m_slots.size();
	if (slotCount > prevSlotCount)
	{
		const uint32 maxSlotCount = 0xffff;
		if (slotCount > maxSlotCount)
		{
			return false;
		}

		const uint32 minSlotCount = 100;
		slotCount = max(max(slotCount, min(prevSlotCount * 2, maxSlotCount)), minSlotCount);

		m_slots.resize(slotCount);
		m_freeSlots.reserve(slotCount);

		for (uint32 slotIdx = prevSlotCount; slotIdx < slotCount; ++slotIdx)
		{
			m_slots[slotIdx].objectId = CreateObjectId(slotIdx, 0);
			m_freeSlots.push_back(slotIdx);
		}
	}
	return true;
}

ObjectId CObjectPool::CreateObjectId(uint32 slotIdx, uint32 salt) const
{
	return static_cast<ObjectId>((slotIdx << 16) | salt);
}

bool CObjectPool::ExpandObjectId(ObjectId objectId, uint32& slotIdx, uint32& salt) const
{
	const uint32 value = static_cast<uint32>(objectId);
	slotIdx = value >> 16;
	salt = value & 0xffff;
	return (slotIdx < m_slots.size()) && (m_slots[slotIdx].objectId == objectId);
}

uint32 CObjectPool::IncrementSalt(uint32 salt) const
{
	const uint32 maxSalt = 0x7fff;
	if (++salt > maxSalt)
	{
		salt = 0;
	}
	return salt;
}
} // Schematyc
