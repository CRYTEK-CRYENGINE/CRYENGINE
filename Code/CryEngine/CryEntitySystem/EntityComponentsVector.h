// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryExtension/ICryFactoryRegistry.h>

// Implemented in a similar way to CListenerSet

// Forward decl.
template<typename T>
class CEntityComponentsIterator;

//! SEntityComponentRecord takes over ownership of the component and stores additional data.
//! Components will be shutdown and cleaned up.
struct SEntityComponentRecord
{
	int                               proxyType;            //!< Proxy id associated with this component (Only when component correspond to know proxy, -1 overwise)
	int                               eventPriority;        //!< Event dispatch priority
	uint16                            creationOrder;        //!< Determines when an order was constructed, i.e. 0 = first component, 1 = second. Not necessarily sequential.
	EntityEventMask                   registeredEventsMask; //!< Bitmask of the EEntityEvent values
	CryInterfaceID                    typeId;               //!< Interface IDD for the registered component.
	std::shared_ptr<IEntityComponent> pComponent;           //!< Pointer to the owned component, Only the entity owns the component life time

	SEntityComponentRecord() = default;
	SEntityComponentRecord(const SEntityComponentRecord&) = delete;
	SEntityComponentRecord& operator=(const SEntityComponentRecord&) = delete;
	SEntityComponentRecord(SEntityComponentRecord&& other)
		: pComponent(std::move(other.pComponent))
		, typeId(other.typeId)
		, registeredEventsMask(other.registeredEventsMask)
		, creationOrder(other.creationOrder)
		, eventPriority(other.eventPriority)
		, proxyType(other.proxyType) 
	{
		other.pComponent.reset();
	}
	SEntityComponentRecord& operator=(SEntityComponentRecord&&) = default;
	~SEntityComponentRecord();

	IEntityComponent* GetComponent() const { return pComponent.get(); }
	//! Check whether the component has been removed
	bool IsValid() const { return pComponent != nullptr; }
	int GetProxyType() const { return proxyType; }
	int GetEventPriority() const { return eventPriority; }
	uint16 GetCreationOrder() const { return creationOrder; }

	// Mark the component was removed
	// This is done since we are not always able to remove the record from storage immediately
	void Invalidate()
	{
		Shutdown();
		pComponent.reset();
	}

	void Shutdown();
};

//! SMinimalEntityComponent is used to store component without taking ownership of them.
//! On delete the component won't be shutdown or deleted, just the entry in the vector will be remove.
//! This struct should be used if components should be stored temporarily.
struct SMinimalEntityComponentRecord
{
	SMinimalEntityComponentRecord() = default;
	SMinimalEntityComponentRecord(const SMinimalEntityComponentRecord&) = delete;
	SMinimalEntityComponentRecord& operator=(const SMinimalEntityComponentRecord&) = delete;
	SMinimalEntityComponentRecord(SMinimalEntityComponentRecord&& other)
		: pComponent(other.pComponent)
	{
		other.pComponent = nullptr;
	}
	SMinimalEntityComponentRecord& operator=(SMinimalEntityComponentRecord&&) = default;

	IEntityComponent* pComponent;

	IEntityComponent* GetComponent() const { return pComponent; }
	// Check whether the component has been removed
	bool IsValid() const { return pComponent != nullptr; }
	int GetProxyType() const { return pComponent != nullptr ? pComponent->GetProxyType() : ENTITY_PROXY_LAST; }
	int GetEventPriority() const { return pComponent != nullptr ? pComponent->GetEventPriority() : 0; }
	uint16 GetCreationOrder() const { return GetEventPriority(); }

	void Invalidate()
	{
		pComponent = nullptr;
	}
};

enum class EComponentIterationResult : uint8
{
	Continue,
	Break
};

//! Main component collection class used in conjunction with CEntityComponentsIterator.
template<typename TRecord>
class CEntityComponentsVector
{
public:
	using TRecordStorage = std::vector<TRecord>;
	using iterator = typename TRecordStorage::iterator;
	using const_iterator = typename TRecordStorage::const_iterator;

private:
	//! Collection of unique component records.
	//! This vector can only be changed on the main thread!
	//! Access from any other thread requires use of the lock at all times.
	TRecordStorage             m_vector;
	uint16                     m_activeScopes : 14;     //!< Counts current iteration scopes (cleanup cannot occur unless this is 0).
	uint16                     m_cleanupRequired : 1;   //!< Indicates NULL elements in listener.
	uint16                     m_sortingValid : 1;      //!< Indicates that sorting order of the elements maybe not be valid.
	mutable CryCriticalSection m_lock;
	
public:
	//! \note No default constructor in favor of forcing users to provide an expected capacity.
	CEntityComponentsVector() : m_activeScopes(0), m_cleanupRequired(false), m_sortingValid(true) {}
	~CEntityComponentsVector()
	{
		CRY_ASSERT_MESSAGE(m_activeScopes == 0, "Entity components vector was destroyed while scopes were active, most likely on another thread!");
		CRY_ASSERT(!m_cleanupRequired);
	};

	//! Sorted insertion based on the event priority
	//! \param componentRecord Component record which should be inserted into the collection
	const TRecord& SortedEmplace(TRecord&& componentRecord)
	{
		CRY_ASSERT_MESSAGE(gEnv->mMainThreadId == CryGetCurrentThreadId(), "Components vector can only be added to by the main thread!");
		CRY_ASSERT(m_sortingValid || m_activeScopes > 0);

		CryAutoCriticalSection lock(m_lock);
		if (m_activeScopes == 0)
		{
			const_iterator upperBoundIt = std::upper_bound(m_vector.begin(), m_vector.end(), componentRecord, [](const TRecord& lhs, const TRecord& rhs) -> bool
			{
				return lhs.GetEventPriority() < rhs.GetEventPriority();
			});

			return *m_vector.emplace(upperBoundIt, std::move(componentRecord));
		}
		else
		{
			m_vector.emplace_back(std::move(componentRecord));

			m_sortingValid = false;
			return m_vector.back();
		}
	}

	iterator FindExistingComponent(IEntityComponent* pExistingComponent)
	{
		CRY_ASSERT_MESSAGE(gEnv->mMainThreadId == CryGetCurrentThreadId(), "Existing component can only be queried by the main thread, or use FindComponent!");
		return std::find_if(m_vector.begin(), m_vector.end(), [pExistingComponent](const TRecord& record) -> bool
		{
			return record.GetComponent() == pExistingComponent;
		});;
	}

	const_iterator GetEnd() const { return m_vector.end(); }

	void ReSortComponent(iterator it)
	{
		CRY_ASSERT_MESSAGE(gEnv->mMainThreadId == CryGetCurrentThreadId(), "Existing component can only be re-sorted by the main thread!");
		CRY_ASSERT_MESSAGE(m_activeScopes == 0, "Re-sorting an existing component record while iteration is in progress is not supported!");
		CRY_ASSERT(!m_cleanupRequired);
		CRY_ASSERT(m_sortingValid);

		TRecord componentRecord;
		{
			CryAutoCriticalSection lock(m_lock);
			componentRecord = std::move(*it);
			m_vector.erase(it);
		}

		SortedEmplace(std::move(componentRecord));
	}

	//! Removes a component from the collection.
	void Remove(IEntityComponent* pComponent)
	{
		CRY_ASSERT_MESSAGE(gEnv->mMainThreadId == CryGetCurrentThreadId(), "Existing component can only be removed by the main thread!");
		
		iterator endIter = m_vector.end();
		iterator it = std::find_if(m_vector.begin(), endIter, [pComponent](const TRecord& record) -> bool
		{
			return record.GetComponent() == pComponent;
		});

		if (it != endIter)
		{
			TRecord tempComponentRecord = std::move(*it);

			CryAutoCriticalSection lock(m_lock);
			// If no iteration in progress
			if (m_activeScopes == 0)
			{
				m_vector.erase(it);

				CRY_ASSERT(!m_cleanupRequired);
				CRY_ASSERT(m_sortingValid);
			}
			else  // Iteration(s) in progress, cannot re-order vector
			{
				m_cleanupRequired = true;

				CRY_ASSERT(!it->IsValid());
			}
		}
	}

	//! Removes all components from the collection
	void Clear()
	{
		CRY_ASSERT_MESSAGE(gEnv->mMainThreadId == CryGetCurrentThreadId(), "Components vector can only be cleared by the main thread!");
		CRY_ASSERT(!m_cleanupRequired);
		CRY_ASSERT(m_activeScopes == 0);

		// Delete the game object before all other components, resulting in it and its extensions being removed from the vector
		typename TRecordStorage::iterator legacyGameObjectComponentIt = std::find_if(m_vector.begin(), m_vector.end(), [](const TRecord& record) -> bool
		{
			return record.GetProxyType() == ENTITY_PROXY_USER;
		});

		if (legacyGameObjectComponentIt != m_vector.end())
		{
			legacyGameObjectComponentIt->Invalidate();
		}

		// Remove all entity components in a temporary vector, in case any components try to modify components in their destructor.
		TRecordStorage tempComponents = std::move(m_vector);
		m_sortingValid = true;

		// Sort components by creation order, to ensure that we destroy in reverse order of creation
		std::stable_sort(tempComponents.begin(), tempComponents.end(), [](const TRecord& p1, const TRecord& p2) -> bool
		{
			return p1.GetCreationOrder() < p2.GetCreationOrder();
		});

		// Now destroy in reverse order of creation
		for(typename TRecordStorage::reverse_iterator it = tempComponents.rbegin(), end = tempComponents.rend(); it != end; ++it)
		{
			it->Invalidate();
		}
	}

	//! Reserves space to help avoid runtime reallocation.
	void Reserve(size_t capacity)
	{
		CRY_ASSERT_MESSAGE(gEnv->mMainThreadId == CryGetCurrentThreadId(), "Components vector capacity can only be changed by the main thread!");
		if (m_vector.capacity() < capacity)
		{
			m_vector.reserve(capacity);
		}
	}

	size_t Size() const
	{
		return m_vector.size();
	}

	//! Iterate through all components, passing the record to the predicate.
	//! If the predicate returns EComponentIterationResult::Break, then iteration will be aborted.
	//! This function handles cases where m_vector is changed during iteration, and is safe to use from other threads.
	void ForEach(const std::function<EComponentIterationResult(const TRecord& record)>& func) const
	{
		const_cast<CEntityComponentsVector*>(this)->ForEach(func);
	}

	//! Iterate through all components, passing the record to the predicate.
	//! If the predicate returns EComponentIterationResult::Break, then iteration will be aborted.
	//! This function handles cases where m_vector is changed during iteration, and is safe to use from other threads.
	template<typename LambdaFunction>
	void ForEach(const LambdaFunction& func)
	{
		CRY_ASSERT_MESSAGE(m_activeScopes > 0 || (m_sortingValid && !m_cleanupRequired), "Non-recursive call to ForEach should not require cleanup or sorting!");

		CryAutoCriticalSection lock(m_lock);
		++m_activeScopes;

		// Iterate only until current count, ignore new elements added during the loop
		for (size_t index = 0; index < m_vector.size(); ++index)
		{
			TRecord& rec = m_vector[index];
			if (rec.GetComponent())
			{
				// Unlock while the predicate is executing, many executions can be quite slow, leaving other threads potentially stalled
				m_lock.Unlock();
				if (func(rec) == EComponentIterationResult::Break)
				{
					m_lock.Lock();
					break;
				}
				else
				{
					m_lock.Lock();
				}
			}
		}

		// End the scope
		// Ensure at least one notification scope was started
		CRY_ASSERT(m_activeScopes > 0);

		// If this is the last notification, remove null elements
		--m_activeScopes;

		if (m_activeScopes == 0)
		{
			CRY_ASSERT(gEnv->mMainThreadId == CryGetCurrentThreadId() || (!m_cleanupRequired && m_sortingValid));
			if (m_cleanupRequired)
			{
				// Shuffles all elements != value to the front and returns the start of the removed elements.
				auto newEndIter = std::remove_if(m_vector.begin(), m_vector.end(), [](const TRecord& rec)
				{
					return rec.GetComponent() == nullptr;
				});

				// Delete the removed range at the back of the container (low-cost for vector).
				m_vector.erase(newEndIter, m_vector.end());

				m_cleanupRequired = false;
			}

			if (!m_sortingValid)
			{
				std::stable_sort(m_vector.begin(), m_vector.end(), [](const TRecord& p1, const TRecord& p2) -> bool
				{
					return p1.GetEventPriority() < p2.GetEventPriority();
				});

				m_sortingValid = true;
			}
		}
	}

	//! Iterate through all components, passing the record to the predicate.
	//! If the predicate returns EComponentIterationResult::Break, then iteration will be aborted.
	//! This function assumes that m_vector will never be changed during iteration!
	void NonRecursiveForEach(const std::function<EComponentIterationResult(const TRecord& record)>& func) const
	{
		const_cast<CEntityComponentsVector*>(this)->NonRecursiveForEach(func);
	}

	//! Iterate through all components, passing the record to the predicate.
	//! If the predicate returns EComponentIterationResult::Break, then iteration will be aborted.
	//! This function assumes that m_vector will never be changed during iteration!
	template<typename LambdaFunction>
	void NonRecursiveForEach(const LambdaFunction& func)
	{
		CryOptionalAutoLock<CryCriticalSection> lock(m_lock, gEnv->mMainThreadId != CryGetCurrentThreadId());

		for(TRecord& record : m_vector)
		{
			if (record.GetComponent())
			{
				if (func(record) == EComponentIterationResult::Break)
				{
					break;
				}
			}
		}
	}

	//! Iterates through all components, and returns the first component that matches the predicate
	//! The predicate is assumed to be thread-safe, as in does not modify the record or any memory within the component itself
	//! Additionally, the predicate is not allowed to call any components that may cause a recursive iteration, for example calling IEntity::GetComponent.
	std::shared_ptr<IEntityComponent> FindComponent(const std::function<bool(const TRecord& record)> func) const
	{
		CryOptionalAutoLock<CryCriticalSection> lock(m_lock, gEnv->mMainThreadId != CryGetCurrentThreadId());

		for (const TRecord& record : m_vector)
		{
			if (record.GetComponent())
			{
				if (func(record))
				{
					return record.pComponent;
				}
			}
		}

		return nullptr;
	}

	//! Iterates through all components, and adds the ones that match the predicate to the components array
	//! The predicate is assumed to be thread-safe, as in does not modify the record or any memory within the component itself
	//! Additionally, the predicate is not allowed to call any components that may cause a recursive iteration, for example calling IEntity::GetComponent.
	void FindComponents(const std::function<bool(const TRecord& record)> func, DynArray<IEntityComponent*>& components) const
	{
		CryOptionalAutoLock<CryCriticalSection> lock(m_lock, gEnv->mMainThreadId != CryGetCurrentThreadId());

		for (const TRecord& record : m_vector)
		{
			if (record.GetComponent())
			{
				if (func(record))
				{
					components.push_back(record.GetComponent());
				}
			}
		}
	}

	void GetMemoryStatistics(ICrySizer* pSizer) const
	{
		pSizer->AddContainer(m_vector);

		ForEach([pSizer](const TRecord& record) -> EComponentIterationResult
		{
			record.pComponent->GetMemoryUsage(pSizer);
			return EComponentIterationResult::Continue;
		});
	}
};