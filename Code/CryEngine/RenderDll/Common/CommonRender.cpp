// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

// NameTable internal variable.
CNameTableR* CCryNameR::ms_table;

// Resource manager internal variables.
ResourceClassMap CBaseResource::m_sResources;
CryRWLock CBaseResource::s_cResLock;

bool CBaseResource::IsValid() const
{
	CryAutoReadLock<CryRWLock> lock(s_cResLock);

	const SResourceContainer* pContainer = GetResourcesForClass(m_ClassName);
	if (!pContainer)
		return false;

	const ResourceClassMapItor itRM = m_sResources.find(m_ClassName);
	if (itRM == m_sResources.end())
		return false;
	if (itRM->second != pContainer)
		return false;

	const ResourcesMapItor itRL = itRM->second->m_RMap.find(m_NameCRC);
	if (itRL == itRM->second->m_RMap.end())
		return false;
	if (itRL->second != this)
		return false;

	return true;
}

SResourceContainer* CBaseResource::GetResourcesForClass(const CCryNameTSCRC& className)
{
	ResourceClassMapItor itRM = m_sResources.find(className);
	if (itRM == m_sResources.end())
		return NULL;
	return itRM->second;
}

CBaseResource* CBaseResource::GetResource(const CCryNameTSCRC& className, int nID, bool bAddRef)
{
	FUNCTION_PROFILER_RENDER_FLAT
	CryAutoReadLock<CryRWLock> lock(s_cResLock);

	SResourceContainer* pRL = GetResourcesForClass(className);
	if (!pRL)
		return NULL;

	int nIndex = RListIndexFromId(nID);

	//assert(pRL->m_RList.size() > nID);
	if (nIndex >= (int)pRL->m_RList.size() || nIndex < 0)
		return NULL;
	CBaseResource* pBR = pRL->m_RList[nIndex];
	if (pBR)
	{
		if (bAddRef)
			pBR->AddRef();
		return pBR;
	}
	return NULL;
}

CBaseResource* CBaseResource::GetResource(const CCryNameTSCRC& className, const CCryNameTSCRC& Name, bool bAddRef)
{
	FUNCTION_PROFILER_RENDER_FLAT
	CryAutoReadLock<CryRWLock> lock(s_cResLock);

	SResourceContainer* pRL = GetResourcesForClass(className);
	if (!pRL)
		return NULL;

	CBaseResource* pBR = NULL;
	ResourcesMapItor itRL = pRL->m_RMap.find(Name);
	if (itRL != pRL->m_RMap.end())
	{
		pBR = itRL->second;
		if (bAddRef)
			pBR->AddRef();
		return pBR;
	}
	return NULL;
}

bool CBaseResource::Register(const CCryNameTSCRC& className, const CCryNameTSCRC& Name)
{
	CryAutoWriteLock<CryRWLock> lock(s_cResLock);

	SResourceContainer* pRL = GetResourcesForClass(className);
	if (!pRL)
	{
		pRL = new SResourceContainer;
		m_sResources.insert(ResourceClassMapItor::value_type(className, pRL));
	}

	assert(pRL);
	if (!pRL)
		return false;

	ResourcesMapItor itRL = pRL->m_RMap.find(Name);
	if (itRL != pRL->m_RMap.end())
		return false;
	pRL->m_RMap.insert(ResourcesMapItor::value_type(Name, this));
	int nIndex;
	if (pRL->m_AvailableIDs.size())
	{
		ResourceIds::iterator it = pRL->m_AvailableIDs.end() - 1;
		nIndex = RListIndexFromId(*it);
		pRL->m_AvailableIDs.erase(it);
		assert(nIndex < (int)pRL->m_RList.size());
		pRL->m_RList[nIndex] = this;
	}
	else
	{
		nIndex = pRL->m_RList.size();
		pRL->m_RList.push_back(this);
	}
	m_nID = IdFromRListIndex(nIndex);
	m_NameCRC = Name;
	m_ClassName = className;
	m_nRefCount = 1;

	return true;
}

bool CBaseResource::UnRegister()
{
	if (IsValid())
	{
		CryAutoWriteLock<CryRWLock> lock(s_cResLock);

		SResourceContainer* pContainer = GetResourcesForClass(m_ClassName);
		assert(pContainer);
		if (pContainer)
		{
			pContainer->m_RMap.erase(m_NameCRC);
			pContainer->m_RList[RListIndexFromId(m_nID)] = NULL;
			pContainer->m_AvailableIDs.push_back(m_nID);
		}

		return true;
	}

	return false;
}

void CBaseResource::UnregisterAndDelete()
{
	UnRegister();
	if (!m_bDeleted)
	{
		m_bDeleted = true;
		CRY_ASSERT(gRenDev != nullptr);
		if(gRenDev)
			gRenDev->ScheduleResourceForDelete(this);
	}
}

//=================================================================

SResourceContainer::~SResourceContainer()
{
	for (ResourcesMapItor it = m_RMap.begin(); it != m_RMap.end(); )
	{
		CBaseResource* pRes = it->second;
		if (pRes && CRenderer::CV_r_printmemoryleaks)
			iLog->Log("Warning: ~SResourceContainer: Resource %d was not deleted (%d)", pRes->GetID(), pRes->GetRefCounter());
		++it; // advance "it" here because the safe release below usually invalidates "it" (calls m_RMap.erase(it))
		SAFE_RELEASE(pRes);
	}
	m_RMap.clear();
	m_RList.clear();
	m_AvailableIDs.clear();
}

void CBaseResource::ShutDown()
{
	if (CRenderer::CV_r_releaseallresourcesonexit)
	{
		ResourceClassMapItor it;
		for (it = m_sResources.begin(); it != m_sResources.end(); it++)
		{
			SResourceContainer* pCN = it->second;
			SAFE_DELETE(pCN);
		}
		m_sResources.clear();
	}
}

//=================================================================

static const inline size_t NoAlign(size_t nSize) { return nSize; }

void SResourceBinding::AddInvalidateCallback(void* pCallbackOwner, SResourceBindPoint bindPoint, const SResourceBinding::InvalidateCallbackFunction& callback) threadsafe const
{
	switch (type)
	{
		case EResourceType::ConstantBuffer:                                                                       break;
		case EResourceType::Texture:        pTexture->AddInvalidateCallback(pCallbackOwner, bindPoint, callback); break;
		case EResourceType::Buffer:         pBuffer ->AddInvalidateCallback(pCallbackOwner, bindPoint, callback); break;
		case EResourceType::Sampler:                                                                              break;
		default:                            CRY_ASSERT(false);
	}
}

void SResourceBinding::RemoveInvalidateCallback(void* pCallbackOwner, SResourceBindPoint bindPoint) threadsafe const
{
	switch (type)
	{
		case EResourceType::ConstantBuffer:                                                                       break;
		case EResourceType::Texture:        pTexture->RemoveInvalidateCallbacks(pCallbackOwner, bindPoint);       break;
		case EResourceType::Buffer:         pBuffer ->RemoveInvalidateCallbacks(pCallbackOwner, bindPoint);       break;
		case EResourceType::Sampler:                                                                              break;
		default:                            CRY_ASSERT(false);
	}
}

size_t CResourceBindingInvalidator::CountInvalidateCallbacks() threadsafe
{
	m_invalidationLock.RLock();
	size_t count = m_invalidateCallbacks.size();
	m_invalidationLock.RUnlock();

	return count;
}

void CResourceBindingInvalidator::AddInvalidateCallback(void* listener, const SResourceBindPoint bindPoint, const SResourceBinding::InvalidateCallbackFunction& callback) threadsafe
{
#if !CRY_PLATFORM_ORBIS || defined(__GXX_RTTI)
	CRY_ASSERT(callback.target<SResourceBinding::InvalidateCallbackSignature*>() != nullptr);
#endif

	m_invalidationLock.WLock();

	// Use try_emplace once we support c++17
	auto map_it = m_invalidateCallbacks.find(listener);
	if (map_it == m_invalidateCallbacks.end())
	{
		map_it = m_invalidateCallbacks.emplace(std::piecewise_construct,
			std::forward_as_tuple(listener),
			std::forward_as_tuple(std::vector<SInvalidateCallback>{})).first;
	}

	auto &slot = map_it->second;
	auto bindpoint_it = std::lower_bound(slot.begin(), slot.end(), bindPoint);
	if (bindpoint_it == slot.end() || bindpoint_it->bindpoint != bindPoint)
		bindpoint_it = slot.emplace(bindpoint_it, bindPoint);
	bindpoint_it->callback = callback;

	++bindpoint_it->refCount;

	m_invalidationLock.WUnlock();

	// We only allow one callback function per listener
#if !CRY_PLATFORM_ORBIS || defined(__GXX_RTTI)
	CRY_ASSERT(*callback.target<SResourceBinding::InvalidateCallbackSignature*>() == *bindpoint_it->callback.target<SResourceBinding::InvalidateCallbackSignature*>());
#endif
}

void CResourceBindingInvalidator::RemoveInvalidateCallbacks(void* listener, const SResourceBindPoint bindPoint) threadsafe
{
	const bool bBatchRemoval = (bindPoint == SResourceBindPoint());
	m_invalidationLock.WLock();

	const auto it = m_invalidateCallbacks.find(listener);
	if (it != m_invalidateCallbacks.end())
	{
		// remove all callback of this listener
		if (bBatchRemoval)
		{
			m_invalidateCallbacks.erase(it);
		}
		// remove one callback of this listener
		else
		{
			auto &slot = it->second;
			auto bindpoint_it = std::lower_bound(slot.begin(), slot.end(), bindPoint);
			if (bindpoint_it != slot.end() && bindpoint_it->bindpoint == bindPoint &&
				--bindpoint_it->refCount <= 0)
			{
				slot.erase(bindpoint_it);
				if (slot.size() == 0)
					m_invalidateCallbacks.erase(it);
			}
		}
	}

	m_invalidationLock.WUnlock();
}

void CResourceBindingInvalidator::InvalidateDeviceResource(CGpuBuffer* pBuffer, uint32 dirtyFlags) threadsafe
{
//	pBuffer->AddRef();
	InvalidateDeviceResource(UResourceReference(pBuffer), dirtyFlags);
	CRY_ASSERT_MESSAGE(!(dirtyFlags & eResourceDestroyed) || (CountInvalidateCallbacks() == 0), "CGpuBuffer %s is destroyd but the invalidation callbacks haven't been removed!", "Unknown" /*pBuffer->GetName()*/);
//	pBuffer->Release();
}

void CResourceBindingInvalidator::InvalidateDeviceResource(CTexture* pTexture, uint32 dirtyFlags) threadsafe
{
	pTexture->AddRef();
	InvalidateDeviceResource(UResourceReference(pTexture), dirtyFlags);
	CRY_ASSERT_MESSAGE(!(dirtyFlags & eResourceDestroyed) || (CountInvalidateCallbacks() == 0), "CTexture %s is destroyd but the invalidation callbacks haven't been removed!", pTexture->GetName());
	pTexture->Release();
}

// Until we support c++14...
template <class Iterator>
std::reverse_iterator<Iterator> make_reverse_iterator(Iterator i)
{
	return std::reverse_iterator<Iterator>(i);
}

void CResourceBindingInvalidator::InvalidateDeviceResource(UResourceReference pResource, uint32 dirtyFlags) threadsafe
{
	m_invalidationLock.WLock();

	for (auto it = m_invalidateCallbacks.begin(); it != m_invalidateCallbacks.end();)
	{
		bool erase_slot = false;

		auto &slot = it->second;
		for (auto slot_it = slot.rbegin(); slot_it != slot.rend(); )
		{
			// Should we keep the callback? true/false
			if (!slot_it->callback(it->first, slot_it->bindpoint, pResource, dirtyFlags))
			{
				slot_it = ::make_reverse_iterator(slot.erase(std::next(slot_it).base()));
				if (slot.size() == 0)
					erase_slot = true;
			}
			else
				slot_it = std::next(slot_it);
		}

		if (!erase_slot)
			it = std::next(it);
		else
			it = m_invalidateCallbacks.erase(it);
	}

	m_invalidationLock.WUnlock();
}