// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "AnimEventList.h"

CAnimEventList::CAnimEventList()
{
}

CAnimEventList::~CAnimEventList()
{
}

uint32 CAnimEventList::GetCount() const
{
	return m_animEvents.size();
}

const CAnimEventData& CAnimEventList::GetByIndex(uint32 animEventIndex) const
{
	return m_animEvents[animEventIndex];
}

CAnimEventData& CAnimEventList::GetByIndex(uint32 animEventIndex)
{
	return m_animEvents[animEventIndex];
}

void CAnimEventList::Append(const CAnimEventData& animEvent)
{
	m_animEvents.push_back(animEvent);
}

void CAnimEventList::Remove(uint32 animEventIndex)
{
	m_animEvents.erase(animEventIndex);
}

void CAnimEventList::Clear()
{
	m_animEvents.clear();
}

size_t CAnimEventList::GetAllocSize() const
{
	size_t allocSize = 0;
	allocSize += m_animEvents.get_alloc_size();

	const uint32 animEventCount = m_animEvents.size();
	for (uint32 i = 0; i < animEventCount; ++i)
	{
		allocSize += m_animEvents[i].GetAllocSize();
	}

	return allocSize;
}

void CAnimEventList::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->AddObject(m_animEvents);
}
