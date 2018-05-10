#pragma once

#include <CryExtension/ICryFactoryRegistry.h>
#include <bitset>

// Implemented in a similar way to CListenerSet

// Forward decl.
template<typename T>
class CEntityComponentsIterator;

struct SEntityComponentRecord
{
	int                               proxyType;            //!< Proxy id associated with this component (Only when component correspond to know proxy, -1 overwise)
	int                               eventPriority;        //!< Event dispatch priority
	EntityEventMask                   registeredEventsMask; //!< Bitmask of the EEntityEvent values
	CryInterfaceID                    typeId;               //!< Interface IDD for the registered component.
	std::shared_ptr<IEntityComponent> pComponent;           //!< Pointer to the owned component, Only the entity owns the component life time

	// Mark the component was removed
	// This is done since we are not always able to remove the record from storage immediately
	void Invalidate()
	{
		pComponent.reset();
	}

	// Check whether the component has been removed
	bool IsValid() const { return pComponent != nullptr; }
};

//! Main listener collection class used in conjunction with CEntityComponentsIterator.
class CEntityComponentsVector
{
private:
	std::vector<SEntityComponentRecord> m_vector; //!< Collection of unique elements.
	int                m_activeScopes    : 29;    //!< Counts current iteration scopes (cleanup cannot occur unless this is 0).
	int                m_cleanupRequired : 1;     //!< Indicates NULL elements in listener.
	int                m_sortingValid    : 1;     //!< Indicates that sorting order of the elements maybe not be valid.
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
	SEntityComponentRecord& Add()
	{
		CryAutoCriticalSection lock(m_lock);

		m_vector.emplace_back();
		m_sortingValid = false;

		return m_vector.back();
	}

	//! Removes a component from the collection.
	void Remove(IEntityComponent* pComponent)
	{
		CryAutoCriticalSection lock(m_lock);

		SEntityComponentRecord tempComponentRecord;

		auto endIter = m_vector.end();
		auto iter = m_vector.end();
		for (iter = m_vector.begin(); iter != m_vector.end(); ++iter)
		{
			if (iter->pComponent.get() == pComponent)
			{
				tempComponentRecord = *iter;

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

		if (tempComponentRecord.IsValid())
		{
			ShutDownComponent(tempComponentRecord);
		}
	}

	//! Removes all components from the collection
	void Clear()
	{
		CRY_ASSERT(m_activeScopes == 0);
		CRY_ASSERT(!m_cleanupRequired);
		CryAutoCriticalSection lock(m_lock);

		//////////////////////////////////////////////////////////////////////////
		// ShutDown all components.
		SEntityComponentRecord legacyGameObjectComponentRecord;

		ForEach([this, &legacyGameObjectComponentRecord](SEntityComponentRecord& rec) -> bool
		{
			if (rec.IsValid())
			{
			  if (rec.proxyType == ENTITY_PROXY_USER)
			  {
			    // User component must be deleted last after all other components are destroyed (Required by CGameObject)
			    legacyGameObjectComponentRecord = rec;
			  }

			  ShutDownComponent(rec);
			}

			return true;
		});

		// Remove all entity components in a temporary vector, in case any components try to modify components in their destructor.
		auto tempComponents = std::move(m_vector);
		tempComponents.clear();
	}

	//! Reserves space to help avoid runtime reallocation.
	void Reserve(size_t capacity)
	{
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
				if (!f(rec))
				{
					break;
				}
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

	// Iterate through all components, assumes that m_vector will not be changed during iteration!
	template<typename LambdaFunction>
	void ForEachUnchecked(LambdaFunction f) const
	{
		for (size_t index = 0; index < m_vector.size(); ++index)
		{
			const SEntityComponentRecord& rec = m_vector[index];
			if (rec.pComponent)
			{
				f(rec);
			}
		}
	}

private:
	static void ShutDownComponent(SEntityComponentRecord& componentRecord);
};
