// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

// NameTable internal variable.
CNameTableR* CCryNameR::ms_table;

// Resource manager internal variables.
ResourceClassMap CBaseResource::m_sResources;
CryCriticalSection CBaseResource::s_cResLock;

bool CBaseResource::IsValid()
{
	AUTO_LOCK(s_cResLock); // Not thread safe without this

	SResourceContainer* pContainer = GetResourcesForClass(m_ClassName);
	if (!pContainer)
		return false;

	ResourceClassMapItor itRM = m_sResources.find(m_ClassName);

	if (itRM == m_sResources.end())
		return false;
	if (itRM->second != pContainer)
		return false;
	ResourcesMapItor itRL = itRM->second->m_RMap.find(m_NameCRC);
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
	  AUTO_LOCK(s_cResLock); // Not thread safe without this

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
	  AUTO_LOCK(s_cResLock); // Not thread safe without this

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
	AUTO_LOCK(s_cResLock); // Not thread safe without this

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
	AUTO_LOCK(s_cResLock); // Not thread safe without this

	if (IsValid())
	{
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
	if (gRenDev && gRenDev->m_pRT)
		gRenDev->m_pRT->RC_ReleaseBaseResource(this);
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
		case EResourceType::Buffer:         pBuffer->AddInvalidateCallback(pCallbackOwner, bindPoint, callback);  break;
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
		case EResourceType::Buffer:         pBuffer->RemoveInvalidateCallbacks(pCallbackOwner, bindPoint);        break;
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
	auto context = std::make_pair(listener, bindPoint);

#if !CRY_PLATFORM_ORBIS || defined(__GXX_RTTI)
	CRY_ASSERT(callback.target<SResourceBinding::InvalidateCallbackSignature*>() != nullptr);
#endif

	m_invalidationLock.WLock();
	auto insertResult = m_invalidateCallbacks.emplace(context, callback);
	m_invalidationLock.WUnlock();

	++insertResult.first->second.refCount;

	// We only allow one callback function per listener
#if !CRY_PLATFORM_ORBIS || defined(__GXX_RTTI)
	CRY_ASSERT(*callback.target<SResourceBinding::InvalidateCallbackSignature*>() == *insertResult.first->second.callback.target<SResourceBinding::InvalidateCallbackSignature*>());
#endif
}

void CResourceBindingInvalidator::RemoveInvalidateCallbacks(void* listener, const SResourceBindPoint bindPoint) threadsafe
{
	typedef std::vector<SInvalidateRegistry::const_iterator, CryStack::CSingleBlockAllocator<SInvalidateRegistry::const_iterator>> StackErasureVector;
	const bool bBatchRemoval = (bindPoint == SResourceBindPoint());
	// If a bind-point is given the limit is 1, otherwise the limit is the maximum number of possible bind-points per listener (256 as in DX11s t0-t255)
	const size_t erasureLimit = bBatchRemoval ? 256ULL /*m_invalidateCallbacks.size()*/ : 1ULL;

	CryStackAllocatorWithSizeVector(SInvalidateRegistry::const_iterator, erasureLimit, erasureMem, NoAlign);
	StackErasureVector erasureList(erasureMem);
	erasureList.reserve(erasureLimit);

	{
		m_invalidationLock.RLock();

		// remove all callback of this listener
		if (bBatchRemoval)
		{
			for (auto it = m_invalidateCallbacks.begin(), end = m_invalidateCallbacks.end(); it != end;)
			{
				auto itCurrentCallback = it++;
				auto context = itCurrentCallback->first;

				bool bErase = (context.first == listener);
				if (bErase)
					erasureList.push_back(itCurrentCallback);
			}
		}
		// remove one callback of this listener
		else
		{
			auto context = std::make_pair(listener, bindPoint);
			auto itCurrentCallback = m_invalidateCallbacks.find(context);

			bool bErase = (itCurrentCallback != m_invalidateCallbacks.end()) && (--itCurrentCallback->second.refCount <= 0);
			if (bErase)
				erasureList.push_back(itCurrentCallback);
		}

		m_invalidationLock.RUnlock();
	}

	if (erasureList.size())
	{
		m_invalidationLock.WLock();
		if (m_invalidateCallbacks.size() == erasureList.size())
			m_invalidateCallbacks.clear();
		else for (auto eraseIt = erasureList.begin(), end = erasureList.end(); eraseIt != end; ++eraseIt)
			m_invalidateCallbacks.erase(*eraseIt);
		m_invalidationLock.WUnlock();
	}
}

void CResourceBindingInvalidator::InvalidateDeviceResource(UResourceReference pResource, uint32 dirtyFlags) threadsafe
{
	typedef std::vector<SInvalidateRegistry::const_iterator, CryStack::CSingleBlockAllocator<SInvalidateRegistry::const_iterator>> StackErasureVector;
	const size_t erasureLimit = m_invalidateCallbacks.size();

	CryStackAllocatorWithSizeVector(SInvalidateRegistry::const_iterator, erasureLimit, erasureMem, NoAlign);
	StackErasureVector erasureList(erasureMem);
	erasureList.reserve(erasureLimit);

	{
		m_invalidationLock.RLock();

		for (auto it = m_invalidateCallbacks.begin(), end = m_invalidateCallbacks.end(); it != end;)
		{
			auto itCurrentCallback = it++;
			auto context = itCurrentCallback->first;
			auto callback = itCurrentCallback->second;

			// Should we keep the callback? true/false
			bool bKeep = callback.callback(context.first, context.second, pResource, dirtyFlags);
			if (!bKeep)
				erasureList.push_back(itCurrentCallback);
		}

		m_invalidationLock.RUnlock();
	}

	if (erasureList.size())
	{
		m_invalidationLock.WLock();
		if (m_invalidateCallbacks.size() == erasureList.size())
			m_invalidateCallbacks.clear();
		else for (auto eraseIt = erasureList.begin(), end = erasureList.end(); eraseIt != end; ++eraseIt)
			m_invalidateCallbacks.erase(*eraseIt);
		m_invalidationLock.WUnlock();
	}
}