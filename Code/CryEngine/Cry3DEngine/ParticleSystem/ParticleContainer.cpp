// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  Created:     24/09/2014 by Filipe amim
//  Description:
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ParticleContainer.h"

CRY_PFX2_DBG

namespace
{

static std::set<void*> g_allocMems;

void* ParticleAlloc(size_t sz)
{
	void* ptr = CryModuleMemalign(sz, CRY_PFX2_PARTICLES_ALIGNMENT);
	memset(ptr, 0, sz);
	return ptr;
}

void ParticleFree(void* ptr)
{
	CryModuleMemalignFree(ptr);
}

}

namespace pfx2
{

CParticleContainer::CParticleContainer()
{
	Clear();
}

CParticleContainer::CParticleContainer(const CParticleContainer& copy)
{
	Clear();
}

CParticleContainer::~CParticleContainer()
{
	Clear();
}

void CParticleContainer::Resize(size_t newSize)
{
	CRY_PFX2_PROFILE_DETAIL;

	if (newSize < m_maxParticles)
		return;

	const size_t newMaxParticles = CRY_PFX2_PARTICLESGROUP_UPPER(newSize + (newSize >> 1)) + 1;

	auto prevBuffers = m_pData;
	for (auto type : EParticleDataType::indices())
	{
		const size_t stride = type.info().typeSize();
		if (m_useData[type] && newMaxParticles > 0)
		{
			void* pNew = ParticleAlloc(newMaxParticles * stride);
			if (m_pData[type])
			{
				memcpy(pNew, m_pData[type], m_lastId * stride);
				ParticleFree(m_pData[type]);
			}
			m_pData[type] = pNew;
		}
		else
		{
			if (m_pData[type])
				ParticleFree(m_pData[type]);
			m_pData[type] = 0;
		}
	}

	m_maxParticles = newMaxParticles;
}

void CParticleContainer::ResetUsedData()
{
	m_useData.fill(false);
}

void CParticleContainer::AddParticleData(EParticleDataType type)
{
	CRY_PFX2_PROFILE_DETAIL;

	const size_t allocSize = m_maxParticles * type.info().typeSize();
	uint dim = type.info().dimension;
	for (uint i = 0; i < dim; ++i)
	{
		m_useData[type + i] = true;
		if (!m_pData[type + i])
			m_pData[type + i] = ParticleAlloc(allocSize);
	}
}

void CParticleContainer::Trim()
{
	for (auto type : EParticleDataType::indices())
	{
		if (!m_useData[type] && m_pData[type] != 0)
		{
			ParticleFree(m_pData[type]);
			m_pData[type] = 0;
		}
	}
}

void CParticleContainer::Clear()
{
	CRY_PFX2_PROFILE_DETAIL;

	for (auto i : EParticleDataType::indices())
	{
		if (m_pData[i] != 0)
			ParticleFree(m_pData[i]);
		m_pData[i] = 0;
		m_useData[i] = false;
	}
	m_maxParticles = CRY_PFX2_PARTICLESGROUP_STRIDE;
	m_lastId = 0;
	m_firstSpawnId = 0;
	m_lastSpawnId = 0;
	m_nextSpawnId = 0;
}

template<typename TData, typename FnCopy>
ILINE void SwapToEndRemoveTpl(TParticleId lastParticleId, const TParticleId* toRemove, size_t toRemoveCount, TData* pData, size_t stride, FnCopy fnCopy)
{
	const uint finalSize = lastParticleId - toRemoveCount;
	uint end = lastParticleId - 1;
	uint i = 0;
	uint j = toRemoveCount - 1;
	for (; i < toRemoveCount && toRemove[i] < finalSize; ++i, --end)
	{
		for (; end == toRemove[j]; --j, --end)
			;
		fnCopy(pData + stride * toRemove[i], pData + stride * end, stride * sizeof(TData));
	}
}

void SwapToEndRemove(TParticleId lastParticleId, const TParticleId* toRemove, size_t toRemoveCount, void* pData, size_t stride)
{
	auto copyFn = [](uint8* pDest, uint8* pSrc, uint stride)
	{
		memcpy(pDest, pSrc, stride);
	};
	SwapToEndRemoveTpl(
	  lastParticleId, toRemove, toRemoveCount,
	  reinterpret_cast<uint8*>(pData), stride, copyFn);
}

void SwapToEndRemoveStride1(TParticleId lastParticleId, const TParticleId* toRemove, size_t toRemoveCount, void* pData)
{
	auto copyFn = [](uint8* pDest, uint8* pSrc, uint stride)
	{
		*pDest = *pSrc;
	};
	SwapToEndRemoveTpl(
	  lastParticleId, toRemove, toRemoveCount,
	  reinterpret_cast<uint8*>(pData), 1, copyFn);
}

void SwapToEndRemoveStride4(TParticleId lastParticleId, const TParticleId* toRemove, size_t toRemoveCount, void* pData)
{
	auto copyFn = [](uint32* pDest, uint32* pSrc, uint stride)
	{
		*pDest = *pSrc;
	};
	SwapToEndRemoveTpl(
	  lastParticleId, toRemove, toRemoveCount,
	  reinterpret_cast<uint32*>(pData), 1, copyFn);
}

void SwapToEndRemoveStride8(TParticleId lastParticleId, const TParticleId* toRemove, size_t toRemoveCount, void* pData)
{
	auto copyFn = [](uint64* pDest, uint64* pSrc, uint stride)
	{
		*pDest = *pSrc;
	};
	SwapToEndRemoveTpl(
	  lastParticleId, toRemove, toRemoveCount,
	  reinterpret_cast<uint64*>(pData), 1, copyFn);
}

void CParticleContainer::AddParticle()
{
	SSpawnEntry entry = { 1 };
	AddRemoveParticles({&entry, 1}, {}, {});
}

void CParticleContainer::AddRemoveParticles(TConstArray<SSpawnEntry> spawnEntries, TVarArray<TParticleId> toRemove, TVarArray<TParticleId> swapIds)
{
	CRY_PFX2_PROFILE_DETAIL;

	size_t newSize = m_lastId - toRemove.size();
	for (const auto& spawnEntry : spawnEntries)
		newSize += spawnEntry.m_count;
	newSize = CRY_PFX2_PARTICLESGROUP_UPPER(newSize) + CRY_PFX2_PARTICLESGROUP_STRIDE + 1;
	Resize(newSize);

	if (!toRemove.empty())
	{
		if (!swapIds.empty())
			MakeSwapIds(toRemove, swapIds);
		RemoveParticles(toRemove);
	}

	if (!spawnEntries.empty())
		AddParticles(spawnEntries);
	else
		m_firstSpawnId = m_lastSpawnId = m_lastId;
}

void CParticleContainer::AddParticles(TConstArray<SSpawnEntry> spawnEntries)
{
	CRY_PFX2_PROFILE_DETAIL;

	m_firstSpawnId = m_lastSpawnId = CRY_PFX2_PARTICLESGROUP_UPPER(m_lastId) + 1;
	uint32 currentId = m_firstSpawnId;
	for (const auto& spawnEntry : spawnEntries)
	{
		const size_t toAddCount = spawnEntry.m_count;

		if (HasData(EPDT_ParentId))
		{
			TParticleId* pParentIds = GetData<TParticleId>(EPDT_ParentId);
			for (uint32 i = currentId; i < currentId + toAddCount; ++i)
				pParentIds[i] = spawnEntry.m_parentId;
		}

		if (HasData(EPDT_SpawnId))
		{
			uint32* pSpawnIds = GetData<TParticleId>(EPDT_SpawnId);
			for (uint32 i = currentId; i < currentId + toAddCount; ++i)
				pSpawnIds[i] = m_nextSpawnId++;
		}
		else
		{
			m_nextSpawnId += toAddCount;
		}

		if (HasData(EPDT_NormalAge))
		{
			// Store newborn ages
			float age = spawnEntry.m_ageBegin;
			float* pNormalAges = GetData<float>(EPDT_NormalAge);
			for (uint32 i = currentId; i < currentId + toAddCount; ++i, age += spawnEntry.m_ageIncrement)
				pNormalAges[i] = age;
		}

		if (HasData(EPDT_SpawnFraction))
		{
			float fraction = spawnEntry.m_fractionBegin;
			float* pSpawnFractions = GetData<float>(EPDT_SpawnFraction);
			for (uint32 i = currentId; i < currentId + toAddCount; ++i, fraction += spawnEntry.m_fractionIncrement)
				pSpawnFractions[i] = min(fraction, 1.0f);
		}

		currentId += toAddCount;
		m_lastSpawnId += toAddCount;
	}
}

void CParticleContainer::RemoveParticles(TConstArray<TParticleId> toRemove)
{
	CRY_PFX2_PROFILE_DETAIL;

	const TParticleId lastParticleId = GetLastParticleId();
	const uint toRemoveCount = toRemove.size();

	for (auto dataTypeId : EParticleDataType::indices())
	{
		const uint stride = dataTypeId.info().typeSize();
		void* pData = m_pData[dataTypeId];
		if (!m_useData[dataTypeId])
			continue;
		switch (stride)
		{
		case 1:
			SwapToEndRemoveStride1(lastParticleId, &toRemove[0], toRemoveCount, m_pData[dataTypeId]);
			break;
		case 4:
			SwapToEndRemoveStride4(lastParticleId, &toRemove[0], toRemoveCount, m_pData[dataTypeId]);
			break;
		case 8:
			SwapToEndRemoveStride8(lastParticleId, &toRemove[0], toRemoveCount, m_pData[dataTypeId]);
			break;
		default:
			SwapToEndRemove(lastParticleId, &toRemove[0], toRemoveCount, m_pData[dataTypeId], stride);
		}
	}

	m_lastId -= toRemoveCount;
}

void CParticleContainer::MakeSwapIds(TVarArray<TParticleId> toRemove, TVarArray<TParticleId> swapIds)
{
	CRY_PFX2_PROFILE_DETAIL;

	const TParticleId lastParticleId = GetLastParticleId();
	const uint toRemoveCount = toRemove.size();
	const uint finalSize = lastParticleId - toRemoveCount;
	CRY_PFX2_ASSERT(uint(swapIds.size()) >= lastParticleId);    // swapIds not big enough

	for (size_t j = 0; j < lastParticleId; ++j)
		swapIds[j] = j;

	SwapToEndRemoveStride4(lastParticleId, &toRemove[0], toRemoveCount, swapIds.data());
	for (uint i = finalSize; i < lastParticleId; ++i)
		swapIds[i] = gInvalidId;

	for (uint i = 0; i < finalSize; ++i)
	{
		TParticleId v0 = swapIds[i];
		TParticleId v1 = swapIds[v0];
		swapIds[i] = v1;
		swapIds[v0] = i;
	}
}

void CParticleContainer::ResetSpawnedParticles()
{
	CRY_PFX2_PROFILE_DETAIL;

	CRY_PFX2_ASSERT(m_firstSpawnId >= m_lastId);

	const uint numSpawn = GetNumSpawnedParticles();
	const uint gapSize = m_firstSpawnId - m_lastId;
	const uint movingId = m_lastSpawnId - min(gapSize, numSpawn);
	if (gapSize != 0)
	{
		for (auto dataTypeId : EParticleDataType::indices())
		{
			const size_t stride = dataTypeId.info().typeSize();
			byte* pBytes = reinterpret_cast<byte*>(m_pData[dataTypeId]);
			if (!pBytes)
				continue;
			memcpy(pBytes + m_lastId * stride, pBytes + movingId * stride, gapSize * stride);
		}
	}

	m_lastId = m_lastSpawnId - gapSize;
	m_firstSpawnId = m_lastSpawnId = m_lastId;
}

void CParticleContainer::RemoveNewBornFlags()
{
	void* pBegin = m_pData[EPDT_State];
	void* pCursor = pBegin;

	const uint8 mask = ~ESB_NewBorn;
	const uint32 flag32 = (mask << 24) | (mask << 16) | (mask << 8) | mask;
#ifdef CRY_PFX2_USE_SSE
	const u32v4 flag128 = convert<u32v4>(flag32);
	u32v4* pBegin128 = static_cast<u32v4*>(pCursor);
	u32v4* pEnd128 = static_cast<u32v4*>(pBegin) + m_lastId / sizeof(u32v4);
	for (; pBegin128 != pEnd128; ++pBegin128)
		*pBegin128 &= flag128;
	pCursor = pEnd128;
#endif

	uint32* pBegin32 = static_cast<uint32*>(pCursor);
	uint32* pEnd32 = static_cast<uint32*>(pBegin) + m_lastId / sizeof(uint32);
	for (; pBegin32 != pEnd32; ++pBegin32)
		*pBegin32 &= flag32;
	pCursor = pEnd32;

	uint8* pBegin8 = static_cast<uint8*>(pCursor);
	uint8* pEnd8 = static_cast<uint8*>(pBegin) + m_lastId / sizeof(uint8);
	for (; pBegin8 != pEnd8; ++pBegin8)
		*pBegin8 &= mask;
}

}
