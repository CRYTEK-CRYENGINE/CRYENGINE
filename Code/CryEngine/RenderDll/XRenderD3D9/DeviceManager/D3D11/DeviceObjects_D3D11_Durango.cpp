// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

//=============================================================================

// Note: temporary solution, this should be removed as soon as the device
// layer for Durango is available
void* CDeviceObjectFactory::GetBackingStorage(D3DBuffer* buffer)
{
#if BUFFER_ENABLE_DIRECT_ACCESS
	void* base_ptr;
	UINT size = sizeof(base_ptr);
	HRESULT hr = buffer->GetPrivateData(BufferPointerGuid, &size, &base_ptr);
	if (hr == S_OK)
	{
		return base_ptr;
	}
	return NULL;
#endif
	return NULL;
}

void CDeviceObjectFactory::FreeBackingStorage(void* base_ptr)
{
	CRY_PROFILE_FUNCTION(PROFILE_RENDERER);
#if BUFFER_ENABLE_DIRECT_ACCESS
	HRESULT hr = D3DFreeGraphicsMemory(base_ptr);
	assert(hr == S_OK);
#endif
}

//=============================================================================

#if CRY_PLATFORM_DURANGO && BUFFER_ENABLE_DIRECT_ACCESS
// Note: temporary solution, this should be removed as soon as the device
// layer for Durango is available
const GUID BufferPointerGuid =
{
	0x2fa8ba27, 0xff17, 0x4b3c,
	{
		0xbd,     0xd2,   0xa2,  0x2c,0x1a, 0x22, 0x15, 0xbf
	}
};
#endif

////////////////////////////////////////////////////////////////////////////
// Fence API

HRESULT CDeviceObjectFactory::CreateFence(DeviceFenceHandle& query)
{
	HRESULT hr = S_FALSE;
	query = reinterpret_cast<DeviceFenceHandle>(new UINT64);
	hr = query ? S_OK : S_FALSE;
	if (!FAILED(hr))
	{
		hr = IssueFence(query);
	}
	return hr;
}

HRESULT CDeviceObjectFactory::ReleaseFence(DeviceFenceHandle query)
{
	HRESULT hr = S_FALSE;
	delete reinterpret_cast<UINT64*>(query);
	hr = S_OK;
	return hr;
}

HRESULT CDeviceObjectFactory::IssueFence(DeviceFenceHandle query)
{
	HRESULT hr = S_FALSE;
	UINT64* handle = reinterpret_cast<UINT64*>(query);
	if (handle)
	{
		*handle = gcpRendD3D->GetPerformanceDeviceContext().InsertFence();
		gcpRendD3D->GetDeviceContext().Flush();
		hr = S_OK;
	}
	return hr;
}

HRESULT CDeviceObjectFactory::SyncFence(DeviceFenceHandle query, bool block, bool flush)
{
	HRESULT hr = S_FALSE;
	UINT64* handle = reinterpret_cast<UINT64*>(query);
	if (handle)
	{
		do
		{
			if (gcpRendD3D->GetPerformanceDevice().IsFencePending(*handle) == false)
			{
				hr = S_OK;
			}
		}
		while (hr == S_FALSE && block);
	}
	return hr;
}

//=============================================================================

static bool IsBlockCompressed(DXGI_FORMAT fmt)
{
	switch (fmt)
	{
	case DXGI_FORMAT_BC1_TYPELESS:
		return true;
	case DXGI_FORMAT_BC1_UNORM:
		return true;
	case DXGI_FORMAT_BC1_UNORM_SRGB:
		return true;
	case DXGI_FORMAT_BC2_TYPELESS:
		return true;
	case DXGI_FORMAT_BC2_UNORM:
		return true;
	case DXGI_FORMAT_BC2_UNORM_SRGB:
		return true;
	case DXGI_FORMAT_BC3_TYPELESS:
		return true;
	case DXGI_FORMAT_BC3_UNORM:
		return true;
	case DXGI_FORMAT_BC3_UNORM_SRGB:
		return true;
	case DXGI_FORMAT_BC4_TYPELESS:
		return true;
	case DXGI_FORMAT_BC4_UNORM:
		return true;
	case DXGI_FORMAT_BC4_SNORM:
		return true;
	case DXGI_FORMAT_BC5_TYPELESS:
		return true;
	case DXGI_FORMAT_BC5_UNORM:
		return true;
	case DXGI_FORMAT_BC5_SNORM:
		return true;
	case DXGI_FORMAT_BC6H_TYPELESS:
		return true;
	case DXGI_FORMAT_BC6H_UF16:
		return true;
	case DXGI_FORMAT_BC6H_SF16:
		return true;
	case DXGI_FORMAT_BC7_TYPELESS:
		return true;
	case DXGI_FORMAT_BC7_UNORM:
		return true;
	case DXGI_FORMAT_BC7_UNORM_SRGB:
		return true;
	default:
		return false;
	}
}

#if ENABLE_STATOSCOPE
#include <CrySystem/Profilers/IStatoscope.h>

class CDurangoGPUMemoryManagerDataGroup : public IStatoscopeDataGroup
{
public:
	explicit CDurangoGPUMemoryManagerDataGroup(CDurangoGPUMemoryManager* pMM)
		: m_pMM(pMM)
	{
	}

	IStatoscopeDataGroup::SDescription GetDescription() const
	{
		return SDescription(
			'B',
			"gcmm",
			"['/GCMM/' "
			"(float capacity MB) "
			"(float inUse MB) "
			"(float available MB) "
			"(int inUseBlocks) "
			"(int freeBlocks) "
			"(int pinnedBlocks) "
			"(int movingBlocks) "
			"(float largestFreeBlock MB) "
			"(float smallestFreeBlock MB) "
			"(float aveFreeBlock MB)]");
	}

	void Write(IStatoscopeFrameRecord& fr)
	{
		IDefragAllocatorStats stats = m_pMM->GetStats();

		fr.AddValue(stats.nCapacity / (1024.0f * 1024.0f));
		fr.AddValue(stats.nInUseSize / (1024.0f * 1024.0f));
		fr.AddValue((stats.nCapacity - stats.nInUseSize) / (1024.0f * 1024.0f));
		fr.AddValue((int)stats.nInUseBlocks);
		fr.AddValue((int)stats.nFreeBlocks);
		fr.AddValue((int)stats.nPinnedBlocks);
		fr.AddValue((int)stats.nMovingBlocks);
		fr.AddValue(stats.nLargestFreeBlockSize / (1024.0f * 1024.0f));
		fr.AddValue(stats.nSmallestFreeBlockSize / (1024.0f * 1024.0f));
		fr.AddValue(stats.nMeanFreeBlockSize / (1024.0f * 1024.0f));
	}

private:
	CDurangoGPUMemoryManager* m_pMM;
};
#endif

CDurangoGPUMemoryManager::CDurangoGPUMemoryManager()
	: m_pAllocator(NULL)
	, m_pCPUAddr(NULL)
{
}

CDurangoGPUMemoryManager::~CDurangoGPUMemoryManager()
{
	DeInit();
}

IDefragAllocatorStats CDurangoGPUMemoryManager::GetStats()
{
	CryAutoLock<CryCriticalSectionNonRecursive> lock(m_lock);

	return m_pAllocator->GetStats();
}

bool CDurangoGPUMemoryManager::Init(size_t size, size_t bankSize, size_t reserveSize, uint32 xgMemType, bool allowAdditionalBanks)
{
	CryAutoLock<CryCriticalSectionNonRecursive> lock(m_lock);

#ifndef _RELEASE
	if (m_pAllocator)
	{
		__debugbreak();
	}
#endif

	m_banks.reserve(size / bankSize);
	m_bankShift = IntegerLog2(bankSize);
	m_memType = xgMemType;
	m_allowAdditionalBanks = allowAdditionalBanks;

	m_pAllocator = CryGetIMemoryManager()->CreateDefragAllocator();

	IDefragAllocator::Policy pol;
	pol.maxSegments = m_banks.capacity();
	pol.pDefragPolicy = this;
	pol.maxAllocs = 65535;
	m_pAllocator->Init(bankSize, AllocAlign, pol);

	reserveSize = max(reserveSize, bankSize);

	size_t numReserveBanks = reserveSize / bankSize;
	m_banks.resize(numReserveBanks);

	for (size_t i = 0; i < numReserveBanks; ++i)
	{
		Bank& bank = m_banks[i];

		if (i > 0)
		{
			m_pAllocator->AppendSegment(bankSize);
		}

#if CAPTURE_REPLAY_LOG
	#if defined(MEMREPLAY_INSTRUMENT_TEXTUREPOOL)
		MEMREPLAY_SCOPE(EMemReplayAllocClass::C_UserPointer, EMemReplayUserPointerClass::C_CryMalloc);
	#else
		MEMREPLAY_HIDE_BANKALLOC();
	#endif
#endif
		HRESULT hr = D3DAllocateGraphicsMemory(1ull << m_bankShift, 64 * 1024, 0, m_memType, &bank.pBase);

#ifndef _RELEASE
		if (FAILED(hr))
		{
			__debugbreak();
		}
#endif

		D3D11_BUFFER_DESC desc = { 0 };
		desc.ByteWidth = 1ull << m_bankShift;
		desc.Usage = D3D11_USAGE_DEFAULT;
		hr = gcpRendD3D->GetPerformanceDevice().CreatePlacementBuffer(&desc, bank.pBase, &bank.pBuffer);

#ifndef _RELEASE
		if (FAILED(hr))
		{
			__debugbreak();
		}
#endif

#if CAPTURE_REPLAY_LOG
		char name[64];
		sprintf(name, "Texture pool bank %i", (int)i);
		CryGetIMemReplay()->RegisterFixedAddressRange(bank.pBase, 1ull << m_bankShift, name);
#endif
	}

	m_tickFence = 0ull;

#if ENABLE_STATOSCOPE
	if (gEnv->pStatoscope)
		gEnv->pStatoscope->RegisterDataGroup(new CDurangoGPUMemoryManagerDataGroup(this));
#endif

	return true;
}

void CDurangoGPUMemoryManager::DeInit()
{
	CryAutoLock<CryCriticalSectionNonRecursive> lock(m_lock);

	if (m_pAllocator)
	{
		m_pAllocator->Release(true);
		m_pAllocator = NULL;
	}

	{
#if CAPTURE_REPLAY_LOG
	#if defined(MEMREPLAY_INSTRUMENT_TEXTUREPOOL)
		MEMREPLAY_SCOPE(EMemReplayAllocClass::C_UserPointer, EMemReplayUserPointerClass::C_CryMalloc);
	#else
		MEMREPLAY_HIDE_BANKALLOC();
	#endif
#endif

		for (size_t i = 0, c = m_banks.size(); i != c; ++i)
		{
			m_banks[i].pBuffer->Release();
			D3DFreeGraphicsMemory(m_banks[i].pBase);
		}
	}
	m_banks.clear();
}

size_t CDurangoGPUMemoryManager::GetPoolSize() const
{
	return m_banks.size() * (1ull << m_bankShift);
}

size_t CDurangoGPUMemoryManager::GetPoolAllocated() const
{
	return m_pAllocator->GetAllocated();
}

void CDurangoGPUMemoryManager::RT_Tick()
{
	FUNCTION_PROFILER_RENDERER();

	{
		CryAutoLock<CryCriticalSectionNonRecursive> lock(m_lock);
		TickFrees_Locked();
		TickUnpins_Locked();
	}

	if (CRendererCVars::CV_r_texturesstreampooldefragmentation)
	{
		CollectGarbage(CRendererCVars::CV_r_texturesstreampooldefragmentationmaxmoves, CRenderer::CV_r_texturesstreampooldefragmentationmaxamount);
	}
}

CDurangoGPUMemoryManager::AllocateResult CDurangoGPUMemoryManager::AllocatePinned(size_t amount, size_t align)
{
#ifndef _RELEASE
	if (align > BankAlign)
	{
		__debugbreak();
	}
	if (align > AllocAlign)
	{
		__debugbreak();
	}
	if (amount > (1ull << m_bankShift))
	{
		__debugbreak();
	}
	if (amount == 0)
	{
		__debugbreak();
	}
#endif

	CryAutoLock<CryCriticalSectionNonRecursive> lock(m_lock);

	TickFrees_Locked();

	AllocateResult ret;

#if defined(MEMREPLAY_INSTRUMENT_TEXTUREPOOL)
	MEMREPLAY_SCOPE(EMemReplayAllocClass::C_UserPointer, EMemReplayUserPointerClass::C_CryMalloc);
#endif

	IDefragAllocator::AllocatePinnedResult apr = m_pAllocator->AllocateAlignedPinned(amount, align, "");
	if (apr.hdl == IDefragAllocator::InvalidHdl)
	{
		// Can we add a segment?
		if (m_allowAdditionalBanks && (m_banks.size() < m_banks.capacity()))
		{
			Bank newBank;
			HRESULT hr;

			{
				PREFAST_SUPPRESS_WARNING(6246)
					MEMREPLAY_HIDE_BANKALLOC();
				hr = D3DAllocateGraphicsMemory(1ull << m_bankShift, 64 * 1024, 0, m_memType, &newBank.pBase);
			}

			if (!FAILED(hr))
			{
				D3D11_BUFFER_DESC desc = { 0 };
				desc.ByteWidth = 1ull << m_bankShift;
				desc.Usage = D3D11_USAGE_DEFAULT;
				hr = gcpRendD3D->GetPerformanceDevice().CreatePlacementBuffer(&desc, newBank.pBase, &newBank.pBuffer);
#ifndef _RELEASE
				if (FAILED(hr))
					__debugbreak();
#endif

#if CAPTURE_REPLAY_LOG
				char name[64];
				sprintf(name, "Texture pool bank %i", (int)m_banks.size());
				CryGetIMemReplay()->RegisterFixedAddressRange(newBank.pBase, 1ull << m_bankShift, name);
#endif

				m_banks.push_back(newBank);
				m_pAllocator->AppendSegment(1ull << m_bankShift);

				apr = m_pAllocator->AllocateAlignedPinned(amount, align, "");
			}
		}
	}

	void* pBaseAddress = NULL;

	if (apr.hdl != IDefragAllocator::InvalidHdl)
	{
		UINT_PTR offset = apr.offs;
		UINT_PTR bankRelOffset = offset & ((1ull << m_bankShift) - 1);
		Bank& bank = m_banks[offset >> m_bankShift];

		pBaseAddress = (char*)bank.pBase + bankRelOffset;
		ret.hdl = SGPUMemHdl(apr.hdl);
		ret.baseAddress = pBaseAddress;
	}
	else if (m_allowAdditionalBanks)
	{
#if !defined(MEMREPLAY_INSTRUMENT_TEXTUREPOOL)
		MEMREPLAY_HIDE_BANKALLOC();
#endif

		// Try and just allocate memory from D3D directly.
		void* pBase = NULL;
		HRESULT hr = D3DAllocateGraphicsMemory(Align(amount, MinD3DAlignment), max(align, (size_t)MinD3DAlignment), 0, m_memType, &pBase);
		if (!FAILED(hr))
		{
			ret.hdl = SGPUMemHdl(pBaseAddress);
			ret.baseAddress = pBaseAddress;
		}
	}

#if defined(MEMREPLAY_INSTRUMENT_TEXTUREPOOL)
	if (pBaseAddress)
		MEMREPLAY_SCOPE_ALLOC(pBaseAddress, amount, align);
#endif

	return ret;
}

void CDurangoGPUMemoryManager::Free(SGPUMemHdl hdl, UINT64 fence)
{
	CryAutoLock<CryCriticalSectionNonRecursive> lock(m_lock);

	PendingFree pf;
	pf.hdl = hdl;
	if (fence == ~0ull)
	{
		SRenderThread* pRT = gcpRendD3D->m_pRT;
		if (pRT->IsRenderThread() && !pRT->IsRenderLoadingThread())
		{
			fence = gcpRendD3D->GetPerformanceDeviceContext().InsertFence(D3D11_INSERT_FENCE_NO_KICKOFF);
		}
	}

	// Ensure that the object won't try and move whilst waiting for free
	if (!hdl.IsFixed())
		m_pAllocator->Pin(hdl.GetHandle());

	pf.fence = fence;
	m_pendingFrees.push_back(pf);
}

void CDurangoGPUMemoryManager::FreeUnused(SGPUMemHdl hdl)
{
	CryAutoLock<CryCriticalSectionNonRecursive> lock(m_lock);

#if defined(MEMREPLAY_INSTRUMENT_TEXTUREPOOL)
	MEMREPLAY_SCOPE(EMemReplayAllocClass::C_UserPointer, EMemReplayUserPointerClass::C_CryMalloc);
#endif

	void* pBaseAddress = NULL;

	if (!hdl.IsFixed())
	{
		IDefragAllocator::Hdl daHdl = hdl.GetHandle();
		RemovePendingUnpin_Locked(daHdl);

#if defined(MEMREPLAY_INSTRUMENT_TEXTUREPOOL)
		UINT_PTR offset = m_pAllocator->WeakPin(daHdl);
		UINT_PTR bankRelOffset = offset & ((1ull << m_bankShift) - 1);
		Bank& bank = m_banks[offset >> m_bankShift];

		pBaseAddress = (char*)bank.pBase + bankRelOffset;
#endif

		m_pAllocator->Free(daHdl);
	}
	else
	{
#if !defined(MEMREPLAY_INSTRUMENT_TEXTUREPOOL)
		MEMREPLAY_HIDE_BANKALLOC();
#endif

		pBaseAddress = hdl.GetFixedAddress();
		D3DFreeGraphicsMemory(pBaseAddress);
	}

#if defined(MEMREPLAY_INSTRUMENT_TEXTUREPOOL)
	MEMREPLAY_SCOPE_FREE(pBaseAddress);
#endif
}

void CDurangoGPUMemoryManager::BindContext(SGPUMemHdl hdl, CDeviceTexture* pDevTex)
{
	if (!hdl.IsFixed())
		m_pAllocator->ChangeContext(hdl.GetHandle(), pDevTex);
}

void* CDurangoGPUMemoryManager::WeakPin(SGPUMemHdl hdl)
{
	if (!hdl.IsFixed())
	{
		UINT_PTR offset = m_pAllocator->WeakPin(hdl.GetHandle());

		UINT_PTR bankRelOffset = offset & ((1ull << m_bankShift) - 1);
		Bank& bank = m_banks[offset >> m_bankShift];

		return (char*)bank.pBase + bankRelOffset;
	}
	else
	{
		return hdl.GetFixedAddress();
	}
}

void* CDurangoGPUMemoryManager::Pin(SGPUMemHdl hdl)
{
	if (!hdl.IsFixed())
	{
		UINT_PTR offset = m_pAllocator->Pin(hdl.GetHandle());

		UINT_PTR bankRelOffset = offset & ((1ull << m_bankShift) - 1);
		Bank& bank = m_banks[offset >> m_bankShift];

		return (char*)bank.pBase + bankRelOffset;
	}
	else
	{
		return hdl.GetFixedAddress();
	}
}

void CDurangoGPUMemoryManager::Unpin(SGPUMemHdl hdl)
{
	if (!hdl.IsFixed())
		m_pAllocator->Unpin(hdl.GetHandle());
}

void CDurangoGPUMemoryManager::GpuPin(SGPUMemHdl hdl)
{
	if (!hdl.IsFixed())
		m_pAllocator->Pin(hdl.GetHandle());
}

void CDurangoGPUMemoryManager::GpuUnpin(SGPUMemHdl hdl, ID3DXboxPerformanceContext* pCtx)
{
	if (!hdl.IsFixed())
	{
		UINT64 fence = gcpRendD3D->GetPerformanceDeviceContext().InsertFence(D3D11_INSERT_FENCE_NO_KICKOFF);
		PushPendingUnpin(hdl, fence);
	}
}

void CDurangoGPUMemoryManager::GpuUnpin(SGPUMemHdl hdl, ID3D11DmaEngineContextX* pCtx)
{
	if (!hdl.IsFixed())
	{
		UINT64 fence = pCtx->InsertFence(D3D11_INSERT_FENCE_NO_KICKOFF);
		PushPendingUnpin(hdl, fence);
	}
}

uint32 CDurangoGPUMemoryManager::BeginCopy(void* pContext, UINT_PTR dstOffset, UINT_PTR srcOffset, UINT_PTR size, IDefragAllocatorCopyNotification* pCompleteNotification)
{
	// Assume called from CollectGarbage

	ASSERT_IS_RENDER_THREAD(gcpRendD3D->m_pRT);

	uint32 nIdx = ~0U;
	uint32 nLastIdx = m_nLastCopyIdx;

	for (uint32 i = 0; i < MaxCopies; ++i)
	{
		uint32 nTestIdx = (nLastIdx + i) % MaxCopies;
		if (m_copies[nTestIdx].pNotification == NULL)
		{
			nIdx = nTestIdx;
			break;
		}
	}

	if (nIdx != ~0U)
	{
		new (&m_copies[nIdx]) InflightCopy(srcOffset, dstOffset, size, pCompleteNotification);

		CopyDesc descr;
		descr.src = srcOffset;
		descr.dst = dstOffset;
		descr.size = size;
		descr.idx = nIdx;
		descr.fence = &m_copies[nIdx].copyFence;
		QueueCopy(descr);

		m_nLastCopyIdx = nIdx;

		return nIdx + 1;
	}

	return 0;
}

void CDurangoGPUMemoryManager::Relocate(uint32 userMoveId, void* pContext, UINT_PTR newOffset, UINT_PTR oldOffset, UINT_PTR size)
{
	// Assume called from CollectGarbage

	InflightCopy& copy = m_copies[userMoveId - 1];

	CDeviceTexture* pDevTex = reinterpret_cast<CDeviceTexture*>(pContext);
	char* pOldTexBase = GetPhysicalAddress(oldOffset);
	char* pTexBase = GetPhysicalAddress(newOffset);

	Relocate_Int(pDevTex, pOldTexBase, pTexBase, size);

	copy.inUseFence = m_tickFence;
	copy.cooling = true;
}

void CDurangoGPUMemoryManager::CancelCopy(uint32 userMoveId, void* pContext, bool bSync)
{
	if (bSync) __debugbreak(); // TODO

	InflightCopy& copy = m_copies[userMoveId - 1];
	copy.cancelled = true;
}

void CDurangoGPUMemoryManager::SyncCopy(void* pContext, UINT_PTR dstOffset, UINT_PTR srcOffset, UINT_PTR size)
{
	CDeviceTexture* pDevTex = reinterpret_cast<CDeviceTexture*>(pContext);
	char* pOldTexBase = GetPhysicalAddress(srcOffset);
	char* pTexBase = GetPhysicalAddress(dstOffset);

	memmove(pTexBase, pOldTexBase, size);

	Relocate_Int(pDevTex, pOldTexBase, pTexBase, size);
}

uint32 CDurangoGPUMemoryManager::Hash(UINT_PTR offset, UINT_PTR size)
{
	char* p = GetPhysicalAddress(offset);
	return CCrc32::Compute(p, size, 0);
}

void CDurangoGPUMemoryManager::TickFrees_Locked()
{
	UINT64 currentFence = ~0ull;
	SRenderThread* pRT = gcpRendD3D->m_pRT;
	const bool isRenderThread = pRT->IsRenderThread() && !pRT->IsRenderLoadingThread();

	size_t writeIdx = 0;
	size_t numPending = m_pendingFrees.size();
	for (size_t readIdx = 0; readIdx < numPending; ++readIdx)
	{
		PendingFree& pf = m_pendingFrees[readIdx];
		if (pf.fence == ~0ull && isRenderThread)
		{
			if (currentFence == ~0ull)
			{
				currentFence = gcpRendD3D->GetPerformanceDeviceContext().InsertFence(D3D11_INSERT_FENCE_NO_KICKOFF);
			}
			pf.fence = currentFence;
		}

		if (pf.fence != ~0ull && !gcpRendD3D->GetPerformanceDevice().IsFencePending(pf.fence))
		{
#if defined(MEMREPLAY_INSTRUMENT_TEXTUREPOOL)
			MEMREPLAY_SCOPE(EMemReplayAllocClass::C_UserPointer, EMemReplayUserPointerClass::C_CryMalloc);
#endif

			void* pBaseAddress = NULL;

			if (!pf.hdl.IsFixed())
			{
				IDefragAllocator::Hdl daHdl = pf.hdl.GetHandle();
				RemovePendingUnpin_Locked(daHdl);

#if defined(MEMREPLAY_INSTRUMENT_TEXTUREPOOL)
				UINT_PTR offset = m_pAllocator->WeakPin(daHdl);
				UINT_PTR bankRelOffset = offset & ((1ull << m_bankShift) - 1);
				Bank& bank = m_banks[offset >> m_bankShift];

				pBaseAddress = (char*)bank.pBase + bankRelOffset;
#endif

				m_pAllocator->Free(daHdl);
			}
			else
			{
#if !defined(MEMREPLAY_INSTRUMENT_TEXTUREPOOL)
				MEMREPLAY_HIDE_BANKALLOC();
#endif

				pBaseAddress = pf.hdl.GetFixedAddress();
				D3DFreeGraphicsMemory(pBaseAddress);
			}

#if defined(MEMREPLAY_INSTRUMENT_TEXTUREPOOL)
			MEMREPLAY_SCOPE_FREE(pBaseAddress);
#endif
		}
		else
		{
			m_pendingFrees[writeIdx++] = pf;
		}
	}
	m_pendingFrees.resize(writeIdx);
}

void CDurangoGPUMemoryManager::TickUnpins_Locked()
{
	size_t writeIdx = 0;
	size_t numPending = m_pendingUnpins.size();

	std::sort(m_pendingUnpins.begin(), m_pendingUnpins.end());

	for (size_t readIdx = 0; readIdx < numPending;)
	{
		// This should be a span of unpin requests for a single block. Find the end of the span.

		SGPUMemHdl hdl = m_pendingUnpins[readIdx].hdl;

		size_t readEnd = readIdx + 1;
		while (readEnd < numPending && m_pendingUnpins[readEnd].hdl == hdl)
			++readEnd;

		// Consume unpins if their fences are complete.

		size_t numComplete = 0;
		for (size_t testIdx = readIdx; testIdx < readEnd; ++testIdx)
		{
			PendingFree& pf = m_pendingUnpins[testIdx];

			if (!gcpRendD3D->GetPerformanceDevice().IsFencePending(pf.fence))
			{
				++numComplete;
			}
			else
			{
				m_pendingUnpins[writeIdx++] = pf;
			}
		}

		if (numComplete == (readEnd - readIdx))
		{
			// All unpin requests were complete. Release the real pin.
			m_pAllocator->Unpin(hdl.GetHandle());
		}

		// Advance to the next span.
		readIdx = readEnd;
	}

	m_pendingUnpins.resize(writeIdx);
}

void CDurangoGPUMemoryManager::RemovePendingUnpin_Locked(IDefragAllocator::Hdl hdl)
{
	using std::swap;

	for (size_t i = 0, c = m_pendingUnpins.size(); i < c; )
	{
		if (m_pendingUnpins[i].hdl.GetHandle() == hdl)
		{
			m_pendingUnpins[i] = m_pendingUnpins.back();
			m_pendingUnpins.pop_back();
			--c;
		}
		else
		{
			++i;
		}
	}
}

void CDurangoGPUMemoryManager::PushPendingUnpin(SGPUMemHdl hdl, UINT64 fence)
{
	PendingFree pf;
	pf.hdl = hdl;
	pf.fence = fence;

	CryAutoLock<CryCriticalSectionNonRecursive> lock(m_lock);

	// All queued unpins for a particular block share the same underlying pin - as the
	// pin count space is so constrained on the actual allocator.
	for (size_t i = 0, c = m_pendingUnpins.size(); i < c; ++i)
	{
		if (m_pendingUnpins[i].hdl.GetHandle() == hdl.GetHandle())
		{
			m_pAllocator->Unpin(hdl.GetHandle());
			break;
		}
	}

	m_pendingUnpins.push_back(pf);
}

void CDurangoGPUMemoryManager::CollectGarbage(size_t maxMoves, size_t maxAmount)
{
	ASSERT_IS_RENDER_THREAD(gcpRendD3D->m_pRT);

	m_tickFence = gcpRendD3D->GetPerformanceDeviceContext().InsertFence();

	CompleteMoves();
	m_pAllocator->DefragmentTick(maxMoves, maxAmount);

	PerformCopies();
}

void CDurangoGPUMemoryManager::CompleteMoves()
{
	// Move completion occurs in two stages:
	//  1. Wait for copy to complete, meanwhile draw calls can occur with the old address
	//  2. Once copy is complete, update texture to point to new offset - further draw calls will
	//     be correct
	//  3. Wait for draw calls with the old address to complete, then notify the allocator the move
	//     is done

	uint64 lastCopyFence = ~0ull, lastCopyFenceCompleted = 0;
	uint64 lastInUseFence = ~0ull, lastInUseFenceCompleted = 0;

	for (uint32 i = 0; i < MaxCopies; ++i)
	{
		InflightCopy& copy = m_copies[i];

		if (copy.pNotification)
		{
			bool bDone = false;

			if (copy.copying)
			{
				if (copy.copyFence != lastCopyFence)
				{
					lastCopyFenceCompleted = !gcpRendD3D->GetPerformanceDevice().IsFencePending(copy.copyFence);
					lastCopyFence = copy.copyFence;
				}

				if (lastCopyFenceCompleted)
				{
					copy.copying = false;
					copy.pNotification->bDstIsValid = true;
				}
			}
			else if (copy.cooling)
			{
				if (copy.inUseFence != lastInUseFence)
				{
					lastInUseFenceCompleted = !gcpRendD3D->GetPerformanceDevice().IsFencePending(copy.inUseFence);
					lastInUseFence = copy.inUseFence;
				}

				if (lastInUseFenceCompleted)
				{
					copy.cooling = false;
					copy.pNotification->bSrcIsUnneeded = true;
					bDone = true;
				}
			}
			else if (copy.cancelled)
			{
				copy.pNotification->bSrcIsUnneeded = true;
				bDone = true;
			}

			if (bDone)
			{
				copy.pNotification = NULL;
			}
		}
	}
}

void CDurangoGPUMemoryManager::PerformCopies()
{
	if (!m_queuedCopies.empty())
	{
		size_t numQueued = m_queuedCopies.size();
		ScheduleCopies(&m_queuedCopies[0], numQueued);

		for (size_t i = 0; i < numQueued; ++i)
		{
			CopyDesc& descr = m_queuedCopies[i];
			IF(descr.idx == ~0u, 0)
				continue;
			InflightCopy& copy = m_copies[descr.idx];
			if (descr.copied)
				copy.pNotification->bDstIsValid = true;
			else
				copy.copying = true;
		}

		m_queuedCopies.clear();
	}
}

void CDurangoGPUMemoryManager::QueueCopy(const CopyDesc& copy)
{
	m_queuedCopies.push_back(copy);
}

void CDurangoGPUMemoryManager::Relocate_Int(CDeviceTexture* pDevTex, char* pOldTexBase, char* pTexBase, UINT_PTR size)
{
//	pDevTex->Unbind();

	const SDeviceTextureDesc* pDesc = pDevTex->GetTDesc();

#ifndef _RELEASE
	if (reinterpret_cast<UINT_PTR>(pOldTexBase) & (pDesc->layout.BaseAlignmentBytes - 1))
		__debugbreak();
	if (reinterpret_cast<UINT_PTR>(pTexBase) & (pDesc->layout.BaseAlignmentBytes - 1))
		__debugbreak();
#endif

	ID3D11Texture2D* pD3DTex = NULL;
	HRESULT hr = gcpRendD3D->GetPerformanceDevice().CreatePlacementTexture2D(&pDesc->d3dDesc, pDesc->xgTileMode, 0, pTexBase, &pD3DTex);
#ifndef _RELEASE
	if (FAILED(hr))
		__debugbreak();
#endif

	pDevTex->ReplaceTexture(pD3DTex);

#if defined(MEMREPLAY_INSTRUMENT_TEXTUREPOOL)
	MEMREPLAY_SCOPE(EMemReplayAllocClass::C_UserPointer, EMemReplayUserPointerClass::C_CryMalloc);
	MEMREPLAY_SCOPE_REALLOC(pOldTexBase, pTexBase, size, AllocAlign);
#endif
}

char* CDurangoGPUMemoryManager::GetPhysicalAddress(UINT_PTR offs)
{
	UINT_PTR bankRelMask = (1ull << m_bankShift) - 1;
	UINT_PTR bank = offs >> m_bankShift;
	return (char*)m_banks[bank].pBase + (offs & bankRelMask);
}

void CDurangoGPUMemoryManager::ScheduleCopies(CopyDesc* descriptions, size_t ncopies)
{
	if (!descriptions || ncopies == 0)
		return;

	bool bFallback = true;

	uint64 fence = ~0ull;

	if (CRenderer::CV_r_texturesstreampooldefragmentation == 2)
	{
		UINT_PTR bankRelMask = (1ull << m_bankShift) - 1;

		for (size_t i = 0; i < ncopies; ++i)
		{
			CopyDesc &copy = descriptions[i];

			UINT_PTR srcBank = copy.src >> m_bankShift;
			UINT_PTR dstBank = copy.dst >> m_bankShift;
			UINT_PTR srcBankOffs = copy.src & bankRelMask;
			UINT_PTR dstBankOffs = copy.dst & bankRelMask;

			D3D11_BOX srcBox;
			srcBox.left = (UINT)srcBankOffs;
			srcBox.top = 0;
			srcBox.front = 0;
			srcBox.right = (UINT)(srcBankOffs + copy.size);
			srcBox.bottom = 1;
			srcBox.back = 1;
			gcpRendD3D->GetDeviceContext().CopySubresourceRegion(
				m_banks[dstBank].pBuffer,
				0,
				(UINT)dstBankOffs,
				0,
				0,
				m_banks[srcBank].pBuffer,
				0,
				&srcBox);
		}

		fence = gcpRendD3D->GetPerformanceDeviceContext().InsertFence();

		for (size_t i = 0; i < ncopies; ++i)
		{
			CopyDesc &copy = descriptions[i];

			if (copy.fence)
				*copy.fence = fence;
			copy.copied = false;
		}

		bFallback = false;
	}

	if (bFallback)
	{
		fence = gcpRendD3D->GetPerformanceDeviceContext().InsertFence();

		for (size_t i = 0; i < ncopies; ++i)
		{
			CopyDesc &copy = descriptions[i];
			void* pDst = GetPhysicalAddress(copy.dst);
			memcpy(pDst, GetPhysicalAddress(copy.src), copy.size);

#if defined(TEXTURES_IN_CACHED_MEM)
			D3DFlushCpuCache(pDst, copy.size);
#endif

			if (copy.fence)
				*copy.fence = fence;
			copy.copied = true;
		}
	}
}

//=============================================================================

CDurangoGPURingMemAllocator::CDurangoGPURingMemAllocator()
	: m_pContext(NULL)
	, m_pCPUAddr(NULL)
	, m_allocateHead(InvalidBlockId)
	, m_allocateDepth(0)
	, m_freeHead(InvalidBlockId)
{
}

bool CDurangoGPURingMemAllocator::Init(ID3D11DmaEngineContextX* pContext, uint32 size)
{
#ifndef _RELEASE
	if (m_pContext)
	{
		__debugbreak();
	}
#endif

	HRESULT hr = D3DAllocateGraphicsMemory(
		size,
		BaseAlignment,
		0,
		D3D11_GRAPHICS_MEMORY_ACCESS_CPU_CACHE_COHERENT,
		(void**)&m_pCPUAddr);

	if (FAILED(hr))
	{
		return false;
	}

	m_pContext = pContext;
	m_capacity = size;

	m_blocks.resize(MaxBlocks);
	m_freeBlocks.resize(MaxBlocks);
	std::iota(m_freeBlocks.begin(), m_freeBlocks.end(), 0);

	TBlockId startSentinalId = AllocateBlockId();
	TBlockId rootBlockId = AllocateBlockId();
	TBlockId endSentinalId = AllocateBlockId();

	Block& startSentinal = m_blocks[startSentinalId];
	startSentinal.offset = 0;
	startSentinal.fence = 0;
	startSentinal.next = rootBlockId;

	Block& endSentinal = m_blocks[endSentinalId];
	endSentinal.offset = size;
	endSentinal.fence = 0;
	endSentinal.next = startSentinalId;

	Block& block = m_blocks[rootBlockId];
	block.offset = 0;
	block.next = endSentinalId;
	block.fence = 0;

	m_freeHead = rootBlockId;
	m_startSentinalId = startSentinalId;
	m_endSentinalId = endSentinalId;

	return true;
}

void* CDurangoGPURingMemAllocator::BeginAllocate(uint32 size, uint32 align, TAllocateContext& contextOut)
{
	// For all blocks, starting from the head block, that alias the requested size + align, wait
	// for their fences to complete then merge/split to form the new allocation block

	// If the requested size runs off the end of the heap, reset to the start of the heap (all blocks
	// at the end will have been synced on the way)

	uint32 required = size + align; // Naive align

	if (required > m_capacity)
	{
#ifndef _RELEASE
		__debugbreak();
#endif
		return NULL;
	}

	if (align > BaseAlignment)
	{
#ifndef _RELEASE
		__debugbreak();
#endif
		return NULL;
	}

	Block* pBlocks = &m_blocks[0];

	uint32 acquired = 0;
	TBlockId baseBlockId = m_freeHead;
	Block* pBaseBlock = &pBlocks[baseBlockId];

	for (TBlockId blockId = baseBlockId; acquired < required; blockId = pBlocks[blockId].next)
	{
		Block* pBlock = &pBlocks[blockId];

		if (blockId == m_endSentinalId)
		{
			// We've hit the end of the range. Reset to the start.

			Block& startSentinal = m_blocks[m_startSentinalId];

			baseBlockId = blockId = startSentinal.next;
			pBaseBlock = pBlock = &pBlocks[baseBlockId];
			acquired = 0;
		}

#ifndef _RELEASE
		// If we hit this, we've underrun during a recursive allocate - is the buffer undersized?
		if (baseBlockId == m_allocateHead)
		{
			__debugbreak();
		}
#endif

		if (pBlock->fence)
		{
			while (gcpRendD3D->GetPerformanceDevice().IsFencePending(pBlock->fence))
			{
				// Stall :(
				CrySleep(1);
			}

			pBlock->fence = 0;
		}

		TBlockId nextBlockId = pBlock->next;
		Block& nextBlock = pBlocks[nextBlockId];
		uint32 blockSize = nextBlock.offset - pBlock->offset;

		// Now that this block is free, merge it with the base block.
		if (blockId != baseBlockId)
		{
			pBaseBlock->next = nextBlockId;
			ReleaseBlockId(blockId);
		}

		acquired += blockSize;
	}

	// The base block is now at least large enough for the allocation. Split to size.

	if (acquired > required)
	{
		TBlockId remainderBlockId = AllocateBlockId();

		// If we failed to find a block, we'll just return an oversized block. Should be rare
		// enough to not matter
		if (remainderBlockId != InvalidBlockId)
		{
			Block& remainderBlock = pBlocks[remainderBlockId];

			remainderBlock.offset = pBaseBlock->offset + required;
			remainderBlock.fence = 0;
			remainderBlock.next = pBaseBlock->next;
			pBaseBlock->next = remainderBlockId;
		}
	}

	if ((++m_allocateDepth) == 1)
	{
		m_allocateHead = baseBlockId;
	}

	m_freeHead = pBaseBlock->next;

	contextOut = baseBlockId;
	return m_pCPUAddr + Align(pBaseBlock->offset, align);
}

void CDurangoGPURingMemAllocator::EndAllocate(TAllocateContext context, uint64 freeFence)
{
	Block& block = m_blocks[context];
	block.fence = freeFence;

	if ((--m_allocateDepth) == 0)
	{
		m_allocateHead = InvalidBlockId;
	}
}

IDefragAllocatorStats CDeviceObjectFactory::GetTexturePoolStats()
{
	return m_texturePool.GetStats();
}

//=============================================================================

bool CDeviceObjectFactory::InPlaceConstructable(const D3D11_TEXTURE2D_DESC& Desc, uint32 eFlags)
{
	return
		(Desc.Usage == D3D11_USAGE_DEFAULT) &&
		(Desc.BindFlags == D3D11_BIND_SHADER_RESOURCE) &&
		(Desc.CPUAccessFlags == 0) &&
		!(Desc.MiscFlags & D3D11X_RESOURCE_MISC_ESRAM_RESIDENT);
}

HRESULT CDeviceObjectFactory::BeginTileFromLinear2D(CDeviceTexture* pDst, const STileRequest* pSubresources, size_t nSubresources, UINT64& fenceOut)
{
	if (!pDst->IsInPool())
	{
#ifndef _RELEASE
		__debugbreak();
#endif

		return E_INVALIDARG;
	}

	ID3D11Texture2D* pDstTex = pDst->Get2DTexture();

	D3D11_TEXTURE2D_DESC dstDesc;
	pDstTex->GetDesc(&dstDesc);

	bool isBlockCompressed = IsBlockCompressed(dstDesc.Format);

	CDeviceTexturePin pin(pDst);

	// TODO Use the layout and test just the surface we care about?
	char* pDstAddr = (char*)pin.GetBaseAddress();
	char* pDstAddrEnd = pDstAddr + pDst->GetDeviceSize();

	CryAutoLock<CryCriticalSection> dmaLock(m_dma1Lock);
	ID3D11DmaEngineContextX* pDMA = m_pDMA1;

	ID3D11Texture2D* pTileSrcs[g_nD3D10MaxSupportedSubres] = { 0 };
	CDurangoGPURingMemAllocator::TAllocateContext allocCtxs[g_nD3D10MaxSupportedSubres] = { 0 };
	size_t nAllocCtxs = 0;

	for (size_t nSRI = 0; nSRI < nSubresources; ++nSRI)
	{
		bool bSrcInGPUMemory = pSubresources[nSRI].bSrcInGPUMemory;
		int nDstSubResource = pSubresources[nSRI].nDstSubResource;
		const void* pLinSurfaceSrc = pSubresources[nSRI].pLinSurfaceSrc;

		int nDstMip = nDstSubResource % dstDesc.MipLevels;
		int nDstSlice = nDstSubResource / dstDesc.MipLevels;

		D3D11_TEXTURE2D_DESC subResDesc;
		ZeroStruct(subResDesc);
		subResDesc.Width = isBlockCompressed
			? Align(max(1, (int)dstDesc.Width >> nDstMip), 4)
			: max(1, (int)dstDesc.Width >> nDstMip);
		subResDesc.Height = isBlockCompressed
			? Align(max(1, (int)dstDesc.Height >> nDstMip), 4)
			: max(1, (int)dstDesc.Height >> nDstMip);
		subResDesc.MipLevels = 1;
		subResDesc.ArraySize = 1;
		subResDesc.Format = dstDesc.Format;
		subResDesc.SampleDesc.Count = dstDesc.SampleDesc.Count;
		subResDesc.SampleDesc.Quality = dstDesc.SampleDesc.Quality;
		subResDesc.Usage = D3D11_USAGE_DEFAULT;

		const SDeviceTextureDesc* pDTD = Find2DResourceLayout(subResDesc, 0, eTM_LinearPadded);
		const XG_RESOURCE_LAYOUT* pLyt = &pDTD->layout;

		// If the src data aliases the destination, or is only in cpu addressable memory, we need
		// some temporary storage from the ring allocator

		void* pRingAllocBase = NULL;
		void* pGPUSrcAddr = NULL;

		if (!bSrcInGPUMemory || (pDstAddr <= pLinSurfaceSrc && pLinSurfaceSrc < pDstAddrEnd))
		{
			pRingAllocBase = m_textureStagingRing.BeginAllocate((uint32)pLyt->SizeBytes, (uint32)pLyt->BaseAlignmentBytes, allocCtxs[nAllocCtxs++]);
			if (!pRingAllocBase)
			{
#ifndef _RELEASE
				__debugbreak();
#endif

				return E_OUTOFMEMORY;
			}

			pGPUSrcAddr = pRingAllocBase;
		}
		else
		{
			pGPUSrcAddr = (void*)pLinSurfaceSrc;
		}

		HRESULT hr = S_OK;

		// Create a resource around the gpu memory (be it linear src or ring alloc)

		hr = gcpRendD3D->GetPerformanceDevice().CreatePlacementTexture2D(&subResDesc, XG_TILE_MODE_LINEAR, 0, pGPUSrcAddr, &pTileSrcs[nSRI]);
		if (FAILED(hr))
		{
#ifndef _RELEASE
			__debugbreak();
#endif

			return hr;
		}

		if (pRingAllocBase)
		{
			// We need to move the linear data into the ring buffer - either use the move engine if the src is in
			// GPU memory, or memcpy

			if (bSrcInGPUMemory)
			{
#ifndef _RELEASE
				if (((UINT_PTR)pLinSurfaceSrc) & (pLyt->BaseAlignmentBytes - 1))
				{
					__debugbreak();
				}
#endif

				ID3D11Texture2D* pLinSrc = NULL;
				hr = gcpRendD3D->GetPerformanceDevice().CreatePlacementTexture2D(&subResDesc, XG_TILE_MODE_LINEAR, 0, (void*)pLinSurfaceSrc, &pLinSrc);
				if (FAILED(hr))
				{
#ifndef _RELEASE
					__debugbreak();
#endif
					return hr;
				}

				pDMA->CopySubresourceRegion(pTileSrcs[nSRI], 0, 0, 0, 0, pLinSrc, 0, NULL, D3D11_COPY_NO_OVERWRITE);

				pLinSrc->Release();
			}
			else
			{
				// Src data is in only-CPU addressable memory, so just copy it into the ring buffer
				size_t nToCopy = pLyt->Plane[0].MipLayout[0].Slice2DSizeBytes;
				memcpy(pRingAllocBase, pLinSurfaceSrc, nToCopy);

				D3DFlushCpuCache(pRingAllocBase, nToCopy);
				//D3DFlushCpuCache((void*)pLinSurfaceSrc, nToCopy);
			}
		}
	}

	for (size_t nSRI = 0; nSRI < nSubresources; ++nSRI)
	{
		const STileRequest& sr = pSubresources[nSRI];
		pDMA->CopySubresourceRegion(
			pDstTex, sr.nDstSubResource, 0, 0, 0,
			pTileSrcs[nSRI], 0,
			NULL, D3D11_COPY_NO_OVERWRITE);
		pTileSrcs[nSRI]->Release();
	}

	UINT64 fence = pDMA->InsertFence(D3D11_INSERT_FENCE_NO_KICKOFF);
	pDMA->Submit();

	for (size_t i = 0; i < nAllocCtxs; ++i)
	{
		m_textureStagingRing.EndAllocate(allocCtxs[i], fence);
	}

	fenceOut = fence;
	return S_OK;
}

HRESULT CDeviceObjectFactory::CreateInPlaceTexture2D(const D3D11_TEXTURE2D_DESC& Desc, uint32 eFlags, const STexturePayload* pPayload, CDeviceTexture*& pDevTexOut)
{
	FUNCTION_PROFILER_RENDERER();
	bool bDeferD3DConstruction = (eFlags & USAGE_STREAMING) && !(pPayload && pPayload->m_pSysMemSubresourceData);

	// Determine optimal layout, and size/alignment for texture
	const SDeviceTextureDesc* pDTD = Find2DResourceLayout(Desc, eFlags, eTM_Optimal);
	const XG_RESOURCE_LAYOUT* pLayout = &pDTD->layout;
	XG_TILE_MODE dstNativeTileMode = pLayout->Plane[0].MipLayout[0].TileMode;

#ifndef _RELEASE
	if (pLayout->Planes != 1)
	{
		__debugbreak();
	}
#endif

	// Try and allocate space for the texture.

	CDurangoGPUMemoryManager::AllocateResult ar = m_texturePool.AllocatePinned(pLayout->SizeBytes, pLayout->BaseAlignmentBytes);
	if (!ar.hdl.IsValid())
	{
		return E_OUTOFMEMORY;
	}

	HRESULT hr = S_OK;

	SGPUMemHdl texHdl = ar.hdl;
	void* pDstBaseAddress = ar.baseAddress;

	ID3D11Texture2D* pD3DTex = NULL;
	if (!bDeferD3DConstruction)
	{
		hr = gcpRendD3D->GetPerformanceDevice().CreatePlacementTexture2D(&Desc, dstNativeTileMode, 0, pDstBaseAddress, &pD3DTex);
		if (FAILED(hr))
		{
			m_texturePool.FreeUnused(texHdl);
			return hr;
		}
	}

	CDeviceTexture* pDeviceTexture = new CDeviceTexture();

	pDeviceTexture->m_pNativeResource = pD3DTex;
	pDeviceTexture->m_gpuHdl = texHdl;
	pDeviceTexture->m_nBaseAllocatedSize = pLayout->SizeBytes;
	pDeviceTexture->m_pLayout = pDTD;

	m_texturePool.BindContext(ar.hdl, pDeviceTexture);

	const SSubresourcePayload* pTIDs;
	if (pPayload && (pTIDs = pPayload->m_pSysMemSubresourceData))
	{
		ETEX_TileMode srcTileMode = pPayload->m_eSysMemTileMode;
		bool isBlockCompressed = IsBlockCompressed(Desc.Format);

		// If any of the sub resources are in a linear general format, we'll need a computer to tile on the CPU.
		bool bNeedsComputer = (srcTileMode == eTM_None) || (srcTileMode == eTM_LinearPadded && isBlockCompressed);

		XGTextureAddressComputer* pComputerRaw = NULL;
		if (bNeedsComputer)
		{
			XG_TEXTURE2D_DESC xgDesc;
			memcpy(&xgDesc, &Desc, sizeof(Desc));
			xgDesc.Pitch = 0;
			xgDesc.TileMode = dstNativeTileMode;

			hr = XGCreateTexture2DComputer(&xgDesc, &pComputerRaw);
			if (FAILED(hr))
			{
#ifndef _RELEASE
				__debugbreak();
#endif

				delete pDeviceTexture;
				return hr;
			}
		}

		_smart_ptr<XGTextureAddressComputer> pComputer(pComputerRaw);
		if (pComputer)
		{
			pComputer->Release();
		}

		for (int nSlice = 0; nSlice < Desc.ArraySize; ++nSlice)
		{
			for (int nMip = 0; nMip < Desc.MipLevels; ++nMip)
			{
				int nDstSubResIdx = D3D11CalcSubresource(nMip, nSlice, Desc.MipLevels);
				const void* pSrcDataAddress = pTIDs[nDstSubResIdx].m_pSysMem;
				const UINT nSubResourceSize = pTIDs[nDstSubResIdx].m_sSysMemAlignment.planeStride;

				if (srcTileMode == eTM_Optimal)
				{
					CRY_ASSERT(nSubResourceSize == pLayout->Plane[0].MipLayout[nMip].Slice2DSizeBytes);
					UINT64 nSubResourceLocation = 
						pLayout->Plane[0].MipLayout[nMip].OffsetBytes +
						pLayout->Plane[0].MipLayout[nMip].Slice2DSizeBytes * nSlice;

					memcpy(reinterpret_cast<byte*>(pDstBaseAddress) + nSubResourceLocation, pSrcDataAddress, nSubResourceSize);
				}
				else if (srcTileMode == eTM_LinearPadded && !isBlockCompressed)
				{
					// Data is in a format that the move engine can tile, wrap it in a placement texture
					// and move it.

					// OF FIXME - test to see if pSRSrc is in gpu memory

					STileRequest req;
					req.nDstSubResource = nDstSubResIdx;
					req.pLinSurfaceSrc = pSrcDataAddress;
					req.bSrcInGPUMemory = false;

					UINT64 fence = 0;
					if (!FAILED(BeginTileFromLinear2D(pDeviceTexture, &req, 1, fence)))
					{
						while (gcpRendD3D->GetPerformanceDevice().IsFencePending(fence))
						{
							CrySleep(0);
						}
					}
				}
				else if (srcTileMode == eTM_LinearPadded && isBlockCompressed)
				{
					// BlockCompressed - can't use the move engine, so tile on the CPU :(.
					// Layout-info is invalid in this case (see CTexture::TextureDataSize)

					const SDeviceTextureDesc* pDTD_Pad = Find2DResourceLayout(Desc, eFlags, eTM_LinearPadded);
					const XG_RESOURCE_LAYOUT* pLayout_Pad = &pDTD_Pad->layout;

					pComputer->CopyIntoSubresource(
						pDstBaseAddress,
						0,
						nDstSubResIdx,
						pSrcDataAddress,
						UINT32(pLayout_Pad->Plane[0].MipLayout[nMip].PitchBytes),
						UINT32(pLayout_Pad->Plane[0].MipLayout[nMip].Slice2DSizeBytes));
				}
				else if (srcTileMode == eTM_None)
				{
					// Linear general - can't use the move engine, so tile on the CPU :(.
					// Layout-info is valid in this case (see CTexture::TextureDataSize)

					pComputer->CopyIntoSubresource(
						pDstBaseAddress,
						0,
						nDstSubResIdx,
						pSrcDataAddress,
						pTIDs[nDstSubResIdx].m_sSysMemAlignment.rowStride,
						pTIDs[nDstSubResIdx].m_sSysMemAlignment.planeStride);
				}
				else
				{
#ifndef _RELEASE
					__debugbreak();
#endif
				}
			}
		}
	}

	m_texturePool.Unpin(texHdl);

	pDevTexOut = pDeviceTexture;
	return S_OK;
}

const SDeviceTextureDesc* CDeviceObjectFactory::Find2DResourceLayout(const D3D11_TEXTURE2D_DESC& Desc, uint32 eFlags, ETEX_TileMode tileMode)
{
	FUNCTION_PROFILER_RENDERER();

	SMinimisedTexture2DDesc minDesc;
	minDesc.width = Desc.Width;
	minDesc.height = Desc.Height;
	minDesc.mips = Desc.MipLevels;
	minDesc.arraySize = Desc.ArraySize;
	minDesc.format = (uint8)Desc.Format;
	minDesc.isCube = (Desc.MiscFlags & D3D11_RESOURCE_MISC_TEXTURECUBE) != 0;
	minDesc.tileMode = (uint8)tileMode;

	CryAutoLock<CryCriticalSectionNonRecursive> lock(m_layoutTableLock);
	TLayoutTableMap::iterator it = m_layoutTable.find(minDesc);
	if (it != m_layoutTable.end())
	{
		return &it->second;
	}

	XG_BIND_FLAG xgFlags = ConvertToXGBindFlags(eFlags);
	XG_TEXTURE2D_DESC xgDesc;
	memcpy(&xgDesc, &Desc, sizeof(Desc));
	xgDesc.Pitch = 0;

	switch (tileMode)
	{
	case eTM_None:
		xgDesc.TileMode = XG_TILE_MODE_LINEAR_GENERAL;
		xgDesc.Usage = XG_USAGE_STAGING;
		break;
	case eTM_LinearPadded:
		xgDesc.TileMode = XG_TILE_MODE_LINEAR;
		xgDesc.Usage = XG_USAGE_STAGING;
		break;
	case eTM_Optimal:
		xgDesc.TileMode = XGComputeOptimalTileMode(
			XG_RESOURCE_DIMENSION_TEXTURE2D,
			(XG_FORMAT)Desc.Format,
			Desc.Width,
			Desc.Height,
			Desc.ArraySize,
			1,
			xgFlags);
		break;
	}

	SDeviceTextureDesc ddesc;
	XGComputeTexture2DLayout(&xgDesc, &ddesc.layout);
	ddesc.xgTileMode = xgDesc.TileMode;
	ddesc.d3dDesc = Desc;

	it = m_layoutTable.insert(std::make_pair(minDesc, ddesc)).first;
	return &it->second;
}
