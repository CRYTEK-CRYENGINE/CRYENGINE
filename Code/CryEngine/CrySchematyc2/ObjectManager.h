// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryMemory/CrySizer.h>
#include <CryMemory/PoolAllocator.h>

#include <CrySchematyc2/IObjectManager.h>

#include "Object.h"
#include "Lib.h"

// #SchematycTODO : Finish implementing object pool and look-up system!!!

namespace Schematyc2
{
	// Object manager.
	////////////////////////////////////////////////////////////////////////////////////////////////////
	class CObjectManager : public IObjectManager
	{
	public:

		CObjectManager();

		~CObjectManager();

		// IObjectManager
		virtual IObject* CreateObject(const SObjectParams& params) override;
		virtual void DestroyObject(IObject* pObject) override;
		virtual IObject* GetObjectById(const ObjectId& objectId) override;
		//virtual IObject* GetObjectByEntityId(const ExplicitEntityId& entityId) override;
		virtual void VisitObjects(const IObjectVisitor& visitor) override;
		virtual void VisitObjects(const IObjectConstVisitor& visitor) const override;
		virtual void SendSignal(const ObjectId& objectId, const SGUID& signalGUID, const TVariantConstArray& inputs = TVariantConstArray()) override;
		virtual void SendSignal(const ExplicitEntityId& entityId, const SGUID& signalGUID, const TVariantConstArray& inputs = TVariantConstArray()) override;
		virtual void BroadcastSignal(const SGUID& signalGUID, const TVariantConstArray& inputs = TVariantConstArray()) override;
		// ~IObjectManager

	private:

		static const uint32 POOL_SIZE						= 256;
		static const uint32 MAX_POOL_COUNT			= 256;
		static const uint32 POOL_INDEX_SHIFT		= 0;
		static const uint32 POOL_INDEX_MASK			= 0x000000ff;
		static const uint32 BUCKET_INDEX_SHIFT	= 8;
		static const uint32 BUCKET_INDEX_MASK		= 0x0000ff00;
		static const uint32 SALT_SHIFT					= 16;
		static const uint32 SALT_MASK						= 0x00ff0000;

		class CPool
		{
		public:

			CPool();

			bool Allocate(void*& ptr, uint32& iBucket, uint32& salt);
			void Free(uint32 iBucket);
			void* Get(uint32 iBucket, uint32 salt);

		private:

			struct SBucket
			{
				union
				{
					uint32	iNextFreeBucket;
					uint8		storage[sizeof(CObject)];
				};

				uint32	salt;
			};

			SBucket	m_buckets[POOL_SIZE];
			uint32	m_iFirstFreeBucket;
		};

		DECLARE_SHARED_POINTERS(CPool)

		typedef stl::TPoolAllocator<CObject>	TObjectAllocator;
		typedef VectorMap<ObjectId, CObject*>	TObjectMap;
		typedef std::vector<CPoolPtr>					TPoolVector;

		bool CreateObject(const SObjectParams& params, CObject*& pObject, ObjectId& objectId);
		void DestroyObject(const ObjectId& objectId);
		CObject* GetObjectImpl(const ObjectId& objectId);

		ObjectId            m_nextObjectId;
		TObjectAllocator    m_objectAllocator;
		TObjectMap          m_objects;
		TPoolVector         m_pools;
		bool                m_bDestroying;
	};
}
