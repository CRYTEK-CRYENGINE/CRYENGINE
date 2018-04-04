#pragma once

#include <CryExtension/ICryFactoryRegistry.h>

// Implemented in a similar way to CListenerSet

// Forward decl.
template<typename T>
class CEntityComponentsIterator;

struct SEntityComponentRecord
{
	int                               proxyType;            //!< Proxy id associated with this component (Only when component correspond to know proxy, -1 overwise)
	int                               eventPriority;        //!< Event dispatch priority
	uint64                            registeredEventsMask; //!< Bitmask of the EEntityEvent values
	CryInterfaceID                    typeId;               //!< Interface IDD for the registered component.
	std::shared_ptr<IEntityComponent> pComponent;           //!< Pointer to the owned component, Only the entity owns the component life time

	// Mark the component was removed
	// This is done since we are not always able to remove the record from storage immediately
	void Invalidate()
	{
		pComponent.reset();
		// Clear the event mask, to avoid any events being sent to the component
		registeredEventsMask = 0;
	}

	// Check whether the component has been removed
	bool IsValid() const { return pComponent != nullptr; }
};

//! Main listener collection class used in conjunction with CEntityComponentsIterator.
class CEntityComponentsVector
{
private:
	std::vector<SEntityComponentRecord> m_vector; //!< Collection of unique elements.
	int m_activeScopes    : 29;                   //!< Counts current iteration scopes (cleanup cannot occur unless this is 0).
	int m_cleanupRequired : 1;                    //!< Indicates NULL elements in listener.
	int m_sortingValid  : 1;                      //!< Indicates that sorting order of the elements maybe not be valid.
	CryCriticalSection m_lock;

public:
	//! \note No default constructor in favor of forcing users to provide an expectedCapacity.
	CEntityComponentsVector() : m_activeScopes(0), m_cleanupRequired(false), m_sortingValid(false) {}
	~CEntityComponentsVector()
	{
		CRY_ASSERT(m_activeScopes == 0);
		CRY_ASSERT(!m_cleanupRequired);
	};

	const std::vector<SEntityComponentRecord>& GetVector() const { return m_vector; }

	//! Appends a component to the end of the collection.
	void Add(const SEntityComponentRecord& rec)
	{
		CRY_ASSERT(gEnv->mMainThreadId == CryGetCurrentThreadId());
		CryAutoCriticalSection lock(m_lock);

		m_vector.push_back(rec);
		m_sortingValid = false;
	}

	//! Removes a component from the collection.
	void Remove(IEntityComponent* pComponent)
	{
		CRY_ASSERT(gEnv->mMainThreadId == CryGetCurrentThreadId());
		CryAutoCriticalSection lock(m_lock);

		std::shared_ptr<IEntityComponent> pTempComponent;

		auto endIter = m_vector.end();
		auto iter = m_vector.end();
		for (iter = m_vector.begin(); iter != m_vector.end(); ++iter)
		{
			if (iter->pComponent.get() == pComponent)
			{
				pTempComponent = iter->pComponent;

				// If no iteration in progress
				if (m_activeScopes == 0)
				{
					// Just delete the entry immediately
					m_sortingValid = false;
					m_vector.erase(iter);
				}
				else  // Iteration(s) in progress, cannot re-order vector
				{
					m_sortingValid = false;
					// Mark for cleanup
					iter->Invalidate();
					m_cleanupRequired = true;
				}

				break;
			}
		}

		if (pTempComponent != nullptr)
		{
			ShutDownComponent(pTempComponent.get());
			// Force release of the component
			pTempComponent.reset();
		}
	}

	//! Removes all components from the collection
	void Clear()
	{
		CRY_ASSERT(gEnv->mMainThreadId == CryGetCurrentThreadId());
		CRY_ASSERT(m_activeScopes == 0);
		CRY_ASSERT(!m_cleanupRequired);
		CryAutoCriticalSection lock(m_lock);

		//////////////////////////////////////////////////////////////////////////
		// ShutDown all components.
		std::shared_ptr<IEntityComponent> pUserComponent;

		ForEach([this, &pUserComponent](const SEntityComponentRecord& rec)
		{
			if (rec.proxyType == ENTITY_PROXY_USER)
			{
				// User component must be deleted last after all other components are destroyed (Required by CGameObject)
				pUserComponent = rec.pComponent;
			}

			ShutDownComponent(rec.pComponent.get());
		});

		// Remove all entity components in a temporary vector, in case any components try to modify components in their destructor.
		auto tempComponents = std::move(m_vector);
		tempComponents.clear();

		if (pUserComponent != nullptr)
		{
			ShutDownComponent(pUserComponent.get());

			// User component must be the last of the components to be destroyed.
			pUserComponent.reset();
		}
		//////////////////////////////////////////////////////////////////////////
	}

	//! Reserves space to help avoid runtime reallocation.
	void Reserve(size_t capacity)
	{
		CRY_ASSERT(gEnv->mMainThreadId == CryGetCurrentThreadId());
		CryAutoCriticalSection lock(m_lock);
		m_vector.reserve(capacity);
	}

	size_t Size() const
	{
		return m_vector.size();
	}

	//! Invokes the provided function f once with each component.
	template<typename LambdaFunction>
	void ForEach(LambdaFunction f)
	{
		CryAutoCriticalSection lock(m_lock);
		++m_activeScopes;

		// Iterate only until current count, ignore new elements added during the loop
		for (size_t index = 0; index < m_vector.size(); ++index)
		{
			SEntityComponentRecord& rec = m_vector[index];
			if (rec.pComponent)
			{
				f(rec);
			}
		}

		// End the scope
		// Ensure at least one notification scope was started
		CRY_ASSERT(m_activeScopes > 0);

		// If this is the last notification, remove null elements
		if (--m_activeScopes == 0 && m_cleanupRequired)
		{
			// Shuffles all elements != value to the front and returns the start of the removed elements.
			auto newEndIter = std::remove_if(m_vector.begin(), m_vector.end(),
				[](const SEntityComponentRecord& rec) { return rec.pComponent.get() == nullptr; }
			);
			// Delete the removed range at the back of the container (low-cost for vector).
			m_vector.erase(newEndIter, m_vector.end());

			m_sortingValid = false;
			m_cleanupRequired = false;
		}
	}

	//! Invokes the provided function f once with each component after sorting.
	template<typename LambdaFunction>
	void ForEachSorted(LambdaFunction f)
	{
		if (!m_sortingValid && m_activeScopes == 0)
		{
			CryAutoCriticalSection lock(m_lock);

			// Requires resorting of the components
			// Elements are sorted by the event priority
			std::sort(m_vector.begin(), m_vector.end(), [](const SEntityComponentRecord& a, const SEntityComponentRecord& b) -> bool { return a.eventPriority < b.eventPriority; });
			m_sortingValid = true;
		}
		ForEach(f);
	}

private:
	static void ShutDownComponent(IEntityComponent *pComponent);
};
