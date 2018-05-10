// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ObjectManager.h"

namespace Schematyc2
{
	//////////////////////////////////////////////////////////////////////////
	CObjectManager::CObjectManager()
		: m_nextObjectId(1)
		, m_bDestroying(false)
	{
		m_objects.reserve(256);
	}

	//////////////////////////////////////////////////////////////////////////
	CObjectManager::~CObjectManager()
	{
		m_bDestroying = true;

		TObjectMap objects;
		objects.swap(m_objects);

		for(TObjectMap::iterator iObject = objects.begin(), iEndObject = objects.end(); iObject != iEndObject; ++ iObject)
		{
			static_cast<CObject*>(iObject->second)->~CObject();
		}
		m_objectAllocator.FreeMemory();
	}

	//////////////////////////////////////////////////////////////////////////
	IObject* CObjectManager::CreateObject(const SObjectParams& params)
	{
		CRY_ASSERT(params.pLibClass);
		CRY_ASSERT(!m_bDestroying);
		if(params.pLibClass && !m_bDestroying)
		{
			const ObjectId	objectId = m_nextObjectId ++;
			CObject*				pObject = new (m_objectAllocator.Allocate()) CObject(objectId, params);
			m_objects.insert(TObjectMap::value_type(objectId, pObject));
			return pObject;
		}
		return NULL;
	}

	//////////////////////////////////////////////////////////////////////////
	void CObjectManager::DestroyObject(IObject* pObject)
	{
		CRY_ASSERT(pObject != NULL);
		if(pObject != NULL)
		{
			TObjectMap::iterator it = m_objects.find(pObject->GetObjectId());
			if (it != m_objects.end())
			{
				m_objects.erase(it);
				pObject->~IObject();
				m_objectAllocator.Deallocate(static_cast<CObject*>(pObject));
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	IObject* CObjectManager::GetObjectById(const ObjectId& objectId)
	{
		TObjectMap::iterator	iObject = m_objects.find(objectId);
		return iObject != m_objects.end() ? iObject->second : NULL;
	}

	//////////////////////////////////////////////////////////////////////////
	void CObjectManager::VisitObjects(const IObjectVisitor& visitor)
	{
		CRY_ASSERT(visitor);
		if(visitor)
		{
			for(TObjectMap::iterator iObject = m_objects.begin(), iEndObject = m_objects.end(); iObject != iEndObject; ++ iObject)
			{
				if(visitor(*iObject->second) != EVisitStatus::Continue)
				{
					return;
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CObjectManager::VisitObjects(const IObjectConstVisitor& visitor) const
	{
		CRY_ASSERT(visitor);
		if(visitor)
		{
			for(TObjectMap::const_iterator iObject = m_objects.begin(), iEndObject = m_objects.end(); iObject != iEndObject; ++ iObject)
			{
				if(visitor(*iObject->second) != EVisitStatus::Continue)
				{
					return;
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CObjectManager::SendSignal(const ObjectId& objectId, const SGUID& signalGUID, const TVariantConstArray& inputs)
	{
		IObject*	pObject = GetObjectById(objectId);
		if(pObject != NULL)
		{
			pObject->ProcessSignal(signalGUID, inputs);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CObjectManager::SendSignal(const ExplicitEntityId& entityId, const SGUID& signalGUID, const TVariantConstArray& inputs)
	{
		for(TObjectMap::iterator iObject = m_objects.begin(), iEndObject = m_objects.end(); iObject != iEndObject; ++ iObject)
		{
			CObject&	object = *iObject->second;
			if(object.GetEntityId() == entityId)
			{
				object.ProcessSignal(signalGUID, inputs);
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CObjectManager::BroadcastSignal(const SGUID& signalGUID, const TVariantConstArray& inputs)
	{
		for(TObjectMap::iterator iObject = m_objects.begin(), iEndObject = m_objects.end(); iObject != iEndObject; ++ iObject)
		{
			iObject->second->ProcessSignal(signalGUID, inputs);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	CObjectManager::CPool::CPool()
		: m_iFirstFreeBucket(0)
	{
		for(uint32 iBucket = 0; iBucket < POOL_SIZE; ++ iBucket)
		{
			SBucket&	bucket = m_buckets[iBucket];
			bucket.iNextFreeBucket	= iBucket + 1;
			bucket.salt							= 0;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	bool CObjectManager::CPool::Allocate(void*& ptr, uint32& iBucket, uint32& salt)
	{
		if(m_iFirstFreeBucket < POOL_SIZE)
		{
			SBucket&	bucket = m_buckets[m_iFirstFreeBucket];
			ptr									= bucket.storage;
			iBucket							= m_iFirstFreeBucket;
			salt								= bucket.salt;
			m_iFirstFreeBucket	= bucket.iNextFreeBucket;
			return true;
		}
		return false;
	}

	//////////////////////////////////////////////////////////////////////////
	void CObjectManager::CPool::Free(uint32 iBucket)
	{
		CRY_ASSERT(iBucket < POOL_SIZE);
		if(iBucket < POOL_SIZE)
		{
			SBucket&	bucket = m_buckets[iBucket];
			bucket.iNextFreeBucket	= m_iFirstFreeBucket;
			m_iFirstFreeBucket			= iBucket;
			++ bucket.salt;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void* CObjectManager::CPool::Get(uint32 iBucket, uint32 salt)
	{
		CRY_ASSERT(iBucket < POOL_SIZE);
		if(iBucket < POOL_SIZE)
		{
			SBucket&	bucket = m_buckets[iBucket];
			if(bucket.salt == salt)
			{
				return bucket.storage;
			}
		}
		return NULL;
	}

	//////////////////////////////////////////////////////////////////////////
	bool CObjectManager::CreateObject(const SObjectParams& params, CObject*& pObject, ObjectId& objectId)
	{
		void*		ptr = NULL;
		uint32	iPool = 0;
		uint32	iBucket = POOL_SIZE;
		uint32	salt = 0;
		for(size_t poolCount = m_pools.size(); iPool < poolCount; ++ iPool)
		{
			if(m_pools[iPool]->Allocate(ptr, iBucket, salt))
			{
				break;
			}
		}
		if((ptr == NULL) && (iPool < MAX_POOL_COUNT))
		{
			CPoolPtr	pNewPool(new CPool());
			m_pools.push_back(pNewPool);
			pNewPool->Allocate(ptr, iBucket, salt);
		}
		CRY_ASSERT(ptr != NULL);
		if(ptr != NULL)
		{
			iPool		= (iPool << POOL_INDEX_SHIFT) & POOL_INDEX_MASK;
			iBucket	= (iBucket << BUCKET_INDEX_SHIFT) & BUCKET_INDEX_MASK;
			salt		= (salt << SALT_SHIFT) & SALT_MASK;
			const uint32	rawObjectId = iPool | iBucket | salt;
			objectId.SetValue(rawObjectId);
			pObject = new (ptr) CObject(objectId, params);
			return true;
		}
		return false;
	}

	//////////////////////////////////////////////////////////////////////////
	void CObjectManager::DestroyObject(const ObjectId& objectId)
	{
		const uint32	rawObjectId = objectId.GetValue();
		const uint32	iPool = (rawObjectId & POOL_INDEX_MASK) >> POOL_INDEX_SHIFT;
		const uint32	iBucket = (rawObjectId & BUCKET_INDEX_MASK) >> BUCKET_INDEX_SHIFT;
		const uint32	salt = (rawObjectId & SALT_MASK) >> SALT_SHIFT;
		CRY_ASSERT(iPool < m_pools.size());
		if(iPool < m_pools.size())
		{
			CPool&		pool = *m_pools[iPool];
			CObject*	pObject = static_cast<CObject*>(pool.Get(iBucket, salt));
			pObject->~CObject();
			pool.Free(iBucket);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	CObject* CObjectManager::GetObjectImpl(const ObjectId& objectId)
	{
		const uint32	rawObjectId = objectId.GetValue();
		const uint32	iPool = (rawObjectId & POOL_INDEX_MASK) >> POOL_INDEX_SHIFT;
		const uint32	iBucket = (rawObjectId & BUCKET_INDEX_MASK) >> BUCKET_INDEX_SHIFT;
		const uint32	salt = (rawObjectId & SALT_MASK) >> SALT_SHIFT;
		CRY_ASSERT(iPool < m_pools.size());
		if(iPool < m_pools.size())
		{
			return static_cast<CObject*>(m_pools[iPool]->Get(iBucket, salt));
		}
		return NULL;
	}
}
