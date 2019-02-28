// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CryFactoryRegistryImpl.h"
#include "../System.h"

#include <CryExtension/ICryUnknown.h>
#include <CryExtension/RegFactoryNode.h>

#include <algorithm>

CCryFactoryRegistryImpl CCryFactoryRegistryImpl::s_registry;

CCryFactoryRegistryImpl::CCryFactoryRegistryImpl()
	: m_guard()
	, m_byCName()
	, m_byCID()
	, m_byIID()
	, m_callbacks()
{
}

CCryFactoryRegistryImpl::~CCryFactoryRegistryImpl()
{
}

CCryFactoryRegistryImpl& CCryFactoryRegistryImpl::Access()
{
	return s_registry;
}

ICryFactory* CCryFactoryRegistryImpl::GetFactory(const char* cname) const
{
	AUTO_READLOCK(m_guard);

	if (!cname)
		return 0;

	const FactoryByCName search(cname);
	FactoriesByCNameConstIt it = std::lower_bound(m_byCName.begin(), m_byCName.end(), search);
	return it != m_byCName.end() && !(search < *it) ? (*it).m_ptr : 0;
}

ICryFactory* CCryFactoryRegistryImpl::GetFactory(const CryClassID& cid) const
{
	AUTO_READLOCK(m_guard);

	const FactoryByCID search(cid);
	FactoriesByCIDConstIt it = std::lower_bound(m_byCID.begin(), m_byCID.end(), search);
	return it != m_byCID.end() && !(search < *it) ? (*it).m_ptr : 0;
}

void CCryFactoryRegistryImpl::IterateFactories(const CryInterfaceID& iid, ICryFactory** pFactories, size_t& numFactories) const
{
	AUTO_READLOCK(m_guard);

	typedef std::pair<FactoriesByIIDConstIt, FactoriesByIIDConstIt> SearchResult;
	SearchResult res = std::equal_range(m_byIID.begin(), m_byIID.end(), FactoryByIID(iid, 0), LessPredFactoryByIIDOnly());

	const size_t numFactoriesFound = std::distance(res.first, res.second);
	if (pFactories)
	{
		numFactories = min(numFactories, numFactoriesFound);
		FactoriesByIIDConstIt it = res.first;
		for (size_t i = 0; i < numFactories; ++i, ++it)
			pFactories[i] = (*it).m_ptr;
	}
	else
		numFactories = numFactoriesFound;
}

void CCryFactoryRegistryImpl::RegisterCallback(ICryFactoryRegistryCallback* pCallback)
{
	if (!pCallback)
		return;

	{
		AUTO_MODIFYLOCK(m_guard);

		Callbacks::iterator it = std::lower_bound(m_callbacks.begin(), m_callbacks.end(), pCallback);
		if (it == m_callbacks.end() || pCallback < *it)
			m_callbacks.insert(it, pCallback);
		else
			assert(0 && "CCryFactoryRegistryImpl::RegisterCallback() -- pCallback already registered!");
	}
	{
		AUTO_READLOCK(m_guard);

		typedef std::pair<FactoriesByIIDConstIt, FactoriesByIIDConstIt> SearchResult;
		SearchResult res = std::equal_range(m_byIID.begin(), m_byIID.end(), FactoryByIID(cryiidof<ICryUnknown>(), 0), LessPredFactoryByIIDOnly());

		for (; res.first != res.second; ++res.first)
			pCallback->OnNotifyFactoryRegistered((*res.first).m_ptr);
	}
}

void CCryFactoryRegistryImpl::UnregisterCallback(ICryFactoryRegistryCallback* pCallback)
{
	if (!pCallback)
		return;

	AUTO_MODIFYLOCK(m_guard);

	Callbacks::iterator it = std::lower_bound(m_callbacks.begin(), m_callbacks.end(), pCallback);
	if (it != m_callbacks.end() && !(pCallback < *it))
		m_callbacks.erase(it);
}

bool CCryFactoryRegistryImpl::GetInsertionPos(ICryFactory* pFactory, FactoriesByCNameIt& itPosForCName, FactoriesByCIDIt& itPosForCID)
{
	assert(pFactory);

	struct FatalError
	{
		static void Report(ICryFactory* pKnownFactory, ICryFactory* pNewFactory)
		{
			char err[1024];
			cry_sprintf(err, "Conflicting factories...\n"
			                 "Factory (0x%p): ClassID = %s, ClassName = \"%s\"\n"
			                 "Factory (0x%p): ClassID = %s, ClassName = \"%s\"",
			            pKnownFactory, pKnownFactory ? pKnownFactory->GetClassID().ToString().c_str() : "$unknown$", pKnownFactory ? pKnownFactory->GetName() : "$unknown$",
			            pNewFactory, pNewFactory ? pNewFactory->GetClassID().ToString().c_str() : "$unknown$", pNewFactory ? pNewFactory->GetName() : "$unknown$");

#if CRY_PLATFORM_ORBIS || CRY_PLATFORM_APPLE || CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID
			printf("\n!!! Fatal error !!!\n");
			printf("%s", err);
			printf("\n");
#elif CRY_PLATFORM_WINDOWS
			OutputDebugStringA("\n!!! Fatal error !!!\n");
			OutputDebugStringA(err);
			OutputDebugStringA("\n");
			CryMessageBox(err, "!!! Fatal error !!!", eMB_Error);
#endif

			assert(0);
			exit(0);
		}
	};

	FactoryByCName searchByCName(pFactory);
	FactoriesByCNameIt itForCName = std::lower_bound(m_byCName.begin(), m_byCName.end(), searchByCName);
	if (itForCName != m_byCName.end() && !(searchByCName < *itForCName))
		FatalError::Report((*itForCName).m_ptr, pFactory);

	FactoryByCID searchByCID(pFactory);
	FactoriesByCIDIt itForCID = std::lower_bound(m_byCID.begin(), m_byCID.end(), searchByCID);
	if (itForCID != m_byCID.end() && !(searchByCID < *itForCID))
		FatalError::Report((*itForCID).m_ptr, pFactory);

	itPosForCName = itForCName;
	itPosForCID = itForCID;

	return true;
}

void CCryFactoryRegistryImpl::RegisterFactories(const SRegFactoryNode* pFactories)
{
	size_t numFactoriesToAdd = 0;
	size_t numInterfacesSupported = 0;
	{
		const SRegFactoryNode* p = pFactories;
		while (p)
		{
			ICryFactory* pFactory = p->m_pFactory;
			assert(pFactory);
			if (pFactory)
			{
				const CryInterfaceID* pIIDs = 0;
				size_t numIIDs = 0;
				pFactory->ClassSupports(pIIDs, numIIDs);

				numInterfacesSupported += numIIDs;
				++numFactoriesToAdd;
			}

			p = p->m_pNext;
		}
	}

	{
		AUTO_MODIFYLOCK(m_guard);

		m_byCName.reserve(m_byCName.size() + numFactoriesToAdd);
		m_byCID.reserve(m_byCID.size() + numFactoriesToAdd);
		m_byIID.reserve(m_byIID.size() + numInterfacesSupported);

		size_t numFactoriesAdded = 0;
		const SRegFactoryNode* p = pFactories;
		while (p)
		{
			ICryFactory* pFactory = p->m_pFactory;
			if (pFactory)
			{
				FactoriesByCNameIt itPosForCName;
				FactoriesByCIDIt itPosForCID;
				if (GetInsertionPos(pFactory, itPosForCName, itPosForCID))
				{
					m_byCName.insert(itPosForCName, FactoryByCName(pFactory));
					m_byCID.insert(itPosForCID, FactoryByCID(pFactory));

					const CryInterfaceID* pIIDs = 0;
					size_t numIIDs = 0;
					pFactory->ClassSupports(pIIDs, numIIDs);

					for (size_t i = 0; i < numIIDs; ++i)
					{
						const FactoryByIID newFactory(pIIDs[i], pFactory);
						m_byIID.push_back(newFactory);
					}

					for (size_t i = 0, s = m_callbacks.size(); i < s; ++i)
						m_callbacks[i]->OnNotifyFactoryRegistered(pFactory);

					++numFactoriesAdded;
				}
			}

			p = p->m_pNext;
		}

		if (numFactoriesAdded)
			std::sort(m_byIID.begin(), m_byIID.end());
	}
}

void CCryFactoryRegistryImpl::UnregisterFactories(const SRegFactoryNode* pFactories)
{
	AUTO_MODIFYLOCK(m_guard);

	const SRegFactoryNode* p = pFactories;
	while (p)
	{
		ICryFactory* pFactory = p->m_pFactory;
		UnregisterFactoryInternal(pFactory);
		p = p->m_pNext;
	}
}

void CCryFactoryRegistryImpl::UnregisterFactory(ICryFactory* const pFactory)
{
	AUTO_MODIFYLOCK(m_guard);

	UnregisterFactoryInternal(pFactory);
}

void CCryFactoryRegistryImpl::UnregisterFactoryInternal(ICryFactory* const pFactory)
{
	if (pFactory)
	{
		FactoryByCName searchByCName(pFactory);
		FactoriesByCNameIt itForCName = std::lower_bound(m_byCName.begin(), m_byCName.end(), searchByCName);
		if (itForCName != m_byCName.end() && !(searchByCName < *itForCName))
		{
			assert((*itForCName).m_ptr == pFactory);
			if ((*itForCName).m_ptr == pFactory)
				m_byCName.erase(itForCName);
		}

		FactoryByCID searchByCID(pFactory);
		FactoriesByCIDIt itForCID = std::lower_bound(m_byCID.begin(), m_byCID.end(), searchByCID);
		if (itForCID != m_byCID.end() && !(searchByCID < *itForCID))
		{
			assert((*itForCID).m_ptr == pFactory);
			if ((*itForCID).m_ptr == pFactory)
				m_byCID.erase(itForCID);
		}

		const CryInterfaceID* pIIDs = 0;
		size_t numIIDs = 0;
		pFactory->ClassSupports(pIIDs, numIIDs);

		for (size_t i = 0; i < numIIDs; ++i)
		{
			FactoryByIID searchByIID(pIIDs[i], pFactory);
			FactoriesByIIDIt itForIID = std::lower_bound(m_byIID.begin(), m_byIID.end(), searchByIID);
			if (itForIID != m_byIID.end() && !(searchByIID < *itForIID))
				m_byIID.erase(itForIID);
		}

		for (size_t i = 0, s = m_callbacks.size(); i < s; ++i)
			m_callbacks[i]->OnNotifyFactoryUnregistered(pFactory);
	}
}

ICryFactoryRegistry* CSystem::GetCryFactoryRegistry() const
{
	return &CCryFactoryRegistryImpl::Access();
}
