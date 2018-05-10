// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------

   $Id$
   $DateTime$

   Description: A simple, intelligent and efficient container for listeners.

   This is designed to provide a simple & consistent interface and behavior
   for adding, removing and iterating listeners - hopefully avoiding the
   common pitfalls such as duplicated elements, invalid iterators and
   dangling pointers.

   -------------------------------------------------------------------------
   History:
   - 07:12:2009: Created by Will Wilson

*************************************************************************/

#ifndef __CRY_LISTENERSET_H__
#define __CRY_LISTENERSET_H__

#include <CryMemory/CrySizer.h>
/************************************************************************

   Core elements:

 * CListenerSet<T> - The collection of listeners.
 * CListenerSet<T>::Notifier - The iterator for safely calling listeners in sequence.

   [Where T is a pointer or something with pointer traits]

   Advantages:

 * Greatly reduces the complexity of managing listener collections.
 * Can safely add and remove listeners during listener iteration.
 * Automatically and safely removes NULL elements.
 * Simple interface (but different from an STL collection to avoid confusion).
 * Checks to see all listeners have been removed at destruction.
 * Low overhead implementation (cheap use of std::vector and minimal heap allocation).
 * Can add debug checks as needed - including stack tracing of Add() calls.
 * Safe for recursive notification chains.
 * Works with vanilla pointers and smart pointers.
 * Provides full support for named listeners to aid debugging.
 * Listener names tracked during notification to help resolve crashes.
 * Designed to ensure names are recorded correctly in crash dump files.

   Named listener support:

   Supplying a name for a listener provides valuable debug information in the following cases:

 * Resolving crashes during listener notification - the name will help trace what listener caused the crash.
 * Resolving which listeners are present in the CListenerSet during runtime.

   IMPORTANT: Please ensure heap allocated strings passed in as names are marked as such ie.

    m_listeners.Add(pListener, "MyListener");                   //!< OK: Static string passed.
    m_listeners.Add(pListener2, m_myName.c_str(), false);       //!< OK: Heap string passed and marked as non-static.
    m_listeners.Add(pListener3, m_myName.c_str(), true);        //!< BAD: Heap string passed and marked as static (potential CRASH).

   Why store names like this?

 * 99% of use cases use static strings - so why allocate memory to make copies of static data?
 * Pointers to static strings will *always* survive crash dumps - great for debugging crashes.
 * We can enable debug support in QA builds - key for catching rare listener related crashes.

   Example:

   class CMyWorld
   {
   public:
    void AddListener(IMyWorldListener* pListener, const char* szName) { m_listeners.Add(pListener, szName); }
    void RemoveListener(IMyWorldListener* pListener) { m_listeners.Remove(pListener); }

    void NotifyListeners(CSomeEvent& event)
    {
      for (TWorldListeners::Notifier notifier(m_listeners); notifier.IsValid(); notifier.Next())
      {
        notifier->OnWorldEvent(event);
      }
    }

   private:
    typedef CListenerSet<IMyWorldListener*> TWorldListeners;
    TWorldListeners m_listeners;
   };


   //! Implements IMyWorldListener.
   CMyWorldUser::OnWorldEvent(CSomeEvent& event)
   {
    //! OK: Removing a listeners within an event handler.
    m_pWorld->RemoveListener(this);

    //! OK: Notifying listeners within an event handler.
    m_pWorld->NotifyListeners(CSomeEvent newEvent(USER_REMOVED, this));
   }


 *************************************************************************/

#ifndef _RELEASE
	#define CRY_LISTENERSET_DEBUG
	//#define CRY_LISTENERSET_DEBUG_PRINT
#endif

// Forward decl.
template<typename T>
class CListenerNotifier;

//! Main listener collection class used in conjunction with CListenerNotifier.
template<typename T>
class CListenerSet
{
public:
	//! \note No default constructor in favor of forcing users to provide an expectedCapacity.
	inline CListenerSet(size_t expectedCapacity);
	inline /*non-virtual*/ ~CListenerSet();
	
#if defined(CRY_LISTENERSET_DEBUG_PRINT)
	//! Appends a listener to the end of the collection. Name is optional but recommended.
	inline bool Add(T pListener, const char* name = NULL, bool staticName = false);
#else
	//! Appends a listener to the end of the collection. Name is optional but recommended.
	inline bool Add(T pListener, const char* name = NULL, bool staticName = true);
#endif

	//! Removes a listener from the collection.
	inline void Remove(T pListener);

	//! Removes all listeners from the collection (NOTE: prefer informing listeners to remove themselves).
	inline void Clear(bool bFreeMemory = false);

	//! \return True if this contains pListener.
	inline bool Contains(T pListener) const;

	//! \return Number of valid listeners.
	inline size_t ValidListenerCount() const;

	//! \return True if no valid listeners exist.
	inline bool Empty() const;

	//! Reserves space to help avoid runtime reallocation.
	inline void Reserve(size_t capacity);

	//! \return The memory size of this object (to support CrySizer).
	inline size_t MemSize() const;

	//! \return True if currently in the process of notifying listeners.
	inline bool IsNotifying() const;

	//! Invokes the provided functor f once witho each listener in the set.
	template<typename TFunctor>
	inline void ForEachListener(const TFunctor& f);

	template<typename LambdaFunction>
	inline void ForEach(LambdaFunction f);

	//! Allow access for Notifier for iteration.
	friend class CListenerNotifier<T>;

	//! Allow TListeners::Notifier style usage.
	typedef class CListenerNotifier<T> Notifier;

	void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddContainer(m_listeners);
#if defined(CRY_LISTENERSET_DEBUG)
		pSizer->AddContainer(m_allocatedNames);
#endif
	}
private:  // DO NOT REMOVE - following methods only to be accessed only via CNotifier

	struct ListenerRecord
	{
		ListenerRecord() : m_pListener() {}
		ListenerRecord(T pListener, const char* szName = NULL)
			: m_pListener(pListener)
#ifdef CRY_LISTENERSET_DEBUG
			, m_szName(szName)
#endif
		{}

		bool operator==(const ListenerRecord& other) const { return m_pListener == other.m_pListener; }
		bool operator==(const T& other) const              { return m_pListener == other; }

		T m_pListener;                  //!< The listener reference.

#ifdef CRY_LISTENERSET_DEBUG
		const char* m_szName;           //!< Name of tracked listener (owned if pointing to data in m_allocatedNames).
#endif
	};

	typedef std::vector<ListenerRecord> TListenerVec;
	typedef std::vector<string>         TAllocatedNameVec;

	inline void StartNotificationScope();
	inline void EndNotificationScope();

	inline void EraseNullElements();

private:
	TListenerVec m_listeners;               //!< Collection of unique listeners.
	size_t       m_activeNotifications;     //!< Counts current notifications in progress (cleanup cannot occur unless this is 0).
	bool         m_cleanupRequired;         //!< Indicates NULL elements in listener.
	bool         m_freeMemOnCleanup;        //!< Indicates how to clean up.

#ifdef CRY_LISTENERSET_DEBUG
	//! Used to delete heap allocated names.
	inline void DeleteName(const char* name);

	TAllocatedNameVec m_allocatedNames;         //!< Collection of strings pointing at heap allocated (i.e. copied) names (typically empty).
#endif
};

//! Helper class used to iterate listeners during listener notification.
template<typename T>
class CListenerNotifier
{
public:
	ILINE CListenerNotifier(CListenerSet<T>& listeners);
	ILINE /*non-virtual*/ ~CListenerNotifier();

	//! True if the current element is ready for iteration.
	ILINE bool IsValid();

	//! Dereference current listener, MUST only be done after a call to IsValid().
	ILINE T operator->();

	//! Dereference current listener, MUST only be done after a call to IsValid().
	ILINE T operator*();

	//! Move to next valid listener (skipping NULL elements).
	ILINE void Next();

	//! Returns the name of the listener (if available).
	inline const char* Name() const;

private:
	CListenerSet<T>& m_listenerSet;     //!< ListenerSet being notified.
	T                m_pListener;       //!< Current listener at index (resolved by IsValid(), cleared after each dereference).
	size_t           m_index;           //!< Current index of element (incremented by next).

#ifdef CRY_LISTENERSET_DEBUG
	const char* m_szName;               //!< Name of the listener (if provided) to aid debugging.
#endif
};

/******************************************************************************************/

template<typename T>
inline CListenerSet<T>::CListenerSet(size_t expectedCapacity)
	: m_activeNotifications(0),
	m_cleanupRequired(false),
	m_freeMemOnCleanup(false)
{
	// Reserve the expected capacity to avoid reallocations
	m_listeners.reserve(expectedCapacity);
}

template<typename T>
inline CListenerSet<T>::~CListenerSet()
{
	// Ensure no notifications are in progress
	CRY_ASSERT(m_activeNotifications == 0);

	// Ensure NULL elements were removed at end of last notification
	CRY_ASSERT(!m_cleanupRequired);
}

//! Appends a listener to the end of the collection. Name is optional but recommended.
template<typename T>
inline bool CListenerSet<T >::Add(T pListener, const char* name, bool staticName)
{
	bool success = false;

	// Ensure the listener exists
	CRY_ASSERT(pListener);

	if (pListener)
	{
		// Ensure the listener is only added once
		if (!Contains(pListener))
		{
			// Resolve name buffer safe for usage outside of this scope
			const char* safeName = name;

#ifdef CRY_LISTENERSET_DEBUG
			// If a name was provided but it's not static data
			if (name && !staticName)
			{
				// Add it to the list of heap allocated names (that we need to later delete)
				m_allocatedNames.push_back(name);
				safeName = m_allocatedNames.back().c_str();

	#if defined(CRY_LISTENERSET_DEBUG_PRINT) && !defined(NOT_USE_CRY_STRING)
				CryLogAlways("%s => %s", __FUNCTION__, safeName);
	#endif

			}
#endif

			m_listeners.push_back(ListenerRecord(pListener, safeName));
			success = true;
		}
	}

	return success;
}

//! Removes a listener from the collection.
template<typename T>
inline void CListenerSet<T >::Remove(T pListener)
{
	typename TListenerVec::iterator endIter(m_listeners.end());
	typename TListenerVec::iterator iter(std::find(m_listeners.begin(), endIter, pListener));
	if (iter != endIter)
	{
#ifdef CRY_LISTENERSET_DEBUG
		// Delete name if it was heap allocated
		if (const char* name = iter->m_szName)
		{

	#if defined(CRY_LISTENERSET_DEBUG_PRINT ) && !defined(NOT_USE_CRY_STRING)
			CryLogAlways("%s => %s", __FUNCTION__, name);
	#endif

			DeleteName(name);
		}
#endif

		// If no notifications in progress
		if (m_activeNotifications == 0)
		{
			// Just delete the listener entry immediately
			m_listeners.erase(iter);
		}
		else  // Notification(s) in progress, cannot re-order listeners
		{
			// Mark for cleanup
			iter->m_pListener = NULL;

			m_cleanupRequired = true;
			m_freeMemOnCleanup = false;
		}
	}
	else  // The listener is not in the set
	{
		// TODO: Warn about redundant Remove()
	}
}

//! Removes all listeners from the collection (NOTE: prefer informing listeners to remove themselves).
template<typename T>
inline void CListenerSet<T >::Clear(bool bFreeMemory)
{
	// If no notifications in progress
	if (m_activeNotifications == 0)
	{
		// Simply clear the listeners immediately
		if (bFreeMemory)
			stl::free_container(m_listeners);
		else
			m_listeners.clear();
	}
	else
	{
		// Mark all listeners for cleanup
		std::fill(m_listeners.begin(), m_listeners.end(), ListenerRecord());

		m_cleanupRequired = true;
		m_freeMemOnCleanup = true;
	}

#ifdef CRY_LISTENERSET_DEBUG
	// Safe to clear allocated names immediately (no references exist any more)
	if (bFreeMemory)
		stl::free_container(m_allocatedNames);
	else
		m_allocatedNames.clear();
#endif
}

//! Returns true if this contains pListener.
template<typename T>
inline bool CListenerSet<T >::Contains(T pListener) const
{
	return stl::find(m_listeners, pListener);
}

//! \return Number of valid listeners.
template<typename T>
inline size_t CListenerSet<T >::ValidListenerCount() const
{
	size_t validCount = m_listeners.size();

	if (m_cleanupRequired)
	{
		//! Remove the count of NULL elements from the result.
		validCount = validCount - std::count(m_listeners.begin(), m_listeners.end(), T());
	}

	return validCount;
}

//! \return True if no valid listeners exist.
template<typename T>
inline bool CListenerSet<T >::Empty() const
{
	return ValidListenerCount() == 0;
}

//! Reserves space to help avoid runtime reallocation.
template<typename T>
inline void CListenerSet<T >::Reserve(size_t capacity)
{
	m_listeners.reserve(capacity);
}

//! \return The memory size of this object (to support CrySizer).
template<typename T>
inline size_t CListenerSet<T >::MemSize() const
{
	size_t size = sizeof(CListenerSet<T> ) + sizeof(ListenerRecord) * m_listeners.size();

#ifdef CRY_LISTENERSET_DEBUG
	size += sizeof(typename TAllocatedNameVec::value_type);
	for (typename TAllocatedNameVec::const_iterator iter(m_allocatedNames.begin()); iter != m_allocatedNames.end(); ++iter)
		size += iter->GetAllocatedMemory();
#endif

	return size;
}

template<typename T>
inline bool CListenerSet<T >::IsNotifying() const
{
	return m_activeNotifications > 0;
}

template<typename T>
template<typename TFunctor>
void CListenerSet<T >::ForEachListener(const TFunctor& f)
{
	StartNotificationScope();
	size_t count = m_listeners.size();
	// Iterate only until current count, ignore new elements added during the loop
	for (size_t index = 0; index < count; ++index)
	{
		if (m_listeners[index].m_pListener)
		{
			f(m_listeners[index].m_pListener);
		}
	}
	EndNotificationScope();
}

template<typename T>
template<typename LambdaFunction>
void CListenerSet<T >::ForEach(LambdaFunction f)
{
	StartNotificationScope();
	size_t count = m_listeners.size();
	// Iterate only until current count, ignore new elements added during the loop
	for (size_t index = 0; index < count; ++index)
	{
		if (m_listeners[index].m_pListener)
		{
			f(m_listeners[index].m_pListener);
		}
	}
	EndNotificationScope();
}


template<typename T>
inline void CListenerSet<T >::StartNotificationScope()
{
	++m_activeNotifications;
}

template<typename T>
inline void CListenerSet<T >::EndNotificationScope()
{
	// Ensure at least one notification scope was started
	CRY_ASSERT(m_activeNotifications > 0);

	// If this is the last notification
	if (--m_activeNotifications == 0)
	{
		EraseNullElements();
	}
}

template<typename T>
inline void CListenerSet<T >::EraseNullElements()
{
	// Ensure no modification while notification(s) are ongoing
	CRY_ASSERT(m_activeNotifications == 0);

	if (m_cleanupRequired && m_activeNotifications == 0)
	{
		stl::find_and_erase_all(m_listeners, T());
		if (m_freeMemOnCleanup && m_listeners.empty())
			stl::free_container(m_listeners);
		m_cleanupRequired = false;
		m_freeMemOnCleanup = false;
	}
}

#ifdef CRY_LISTENERSET_DEBUG

//! Used to delete heap allocated names.
template<typename T>
inline void CListenerSet<T >::DeleteName(const char* name)
{
	if (!m_allocatedNames.empty())
	{
		typename TAllocatedNameVec::iterator endIter(m_allocatedNames.end());
		for (typename TAllocatedNameVec::iterator iter(m_allocatedNames.begin()); iter != endIter; ++iter)
		{
			// Is this the source string?
			if (iter->c_str() == name)
			{
				// Delete it
				m_allocatedNames.erase(iter);
				break;
			}
		}
	}
}

#endif  // defined CRY_LISTENERSET_DEBUG

/******************************************************************************************/

template<typename T>
ILINE CListenerNotifier<T>::CListenerNotifier(CListenerSet<T>& listeners)
	: m_listenerSet(listeners),
	m_pListener(),
	m_index(0)
#ifdef CRY_LISTENERSET_DEBUG
	, m_szName()
#endif
{
	// Flag iteration to listener set to ensure no erase is attempted during iteration
	m_listenerSet.StartNotificationScope();

	// If first element is NULL, move to next valid element
	if (!IsValid())
		Next();
}

template<typename T>
ILINE CListenerNotifier<T>::~CListenerNotifier()
{
	// Erases any NULL elements from listeners
	m_listenerSet.EndNotificationScope();
}

//! \return True if the current element is ready for iteration.
template<typename T>
ILINE bool CListenerNotifier<T >::IsValid()
{
	if (!m_pListener)
	{
		// Always check with original collection
		if (m_index < m_listenerSet.m_listeners.size())
		{
			const typename CListenerSet<T>::ListenerRecord & record(m_listenerSet.m_listeners[m_index]);
			m_pListener = record.m_pListener;

#ifdef CRY_LISTENERSET_DEBUG
			m_szName = record.m_szName;
#endif
		}
	}

	return m_pListener != NULL;
}

//! Dereference current listener, must ONLY be done after a call to IsReady().
template<typename T>
ILINE T CListenerNotifier<T >::operator->()
{
	return operator*();
}

//! Dereference current listener, MUST only be done after a call to IsReady().
template<typename T>
ILINE T CListenerNotifier<T >::operator*()
{
	// Ensure IsReady() was called and its return value checked
	CRY_ASSERT(m_pListener);

	// Clear cached listener pointer to force a IsReady() call before this can be called again.
	// This is done as the listener could be removed during any call to its own event handlers
	// resulting in m_pListener becoming a dangling pointer.
	T pListener(m_pListener);
	m_pListener = T();
	return pListener;
}

//! Move to next valid listener.
template<typename T>
ILINE void CListenerNotifier<T >::Next()
{
	size_t index = m_index;
	typename CListenerSet<T>::ListenerRecord * pNextRecord = NULL;
	m_pListener = NULL; // Always assume there's no next, let the code below prove otherwise!

	const size_t listenerCount = m_listenerSet.m_listeners.size();
	while (++index < listenerCount)
	{
		typename CListenerSet<T>::ListenerRecord & record(m_listenerSet.m_listeners[index]);

		// Is this element valid?
		if (record.m_pListener)
		{
			pNextRecord = &record;
			break;
		}
		// Else move to next element
	}

	if (pNextRecord)
	{
		m_pListener = pNextRecord->m_pListener;
#ifdef CRY_LISTENERSET_DEBUG
		m_szName = pNextRecord->m_szName;
#endif
	}

	m_index = index;
}

//! \return Name of the listener (if available).
template<typename T>
inline const char* CListenerNotifier<T >::Name() const
{
#ifdef CRY_LISTENERSET_DEBUG
	return m_szName;
#else
	return NULL;
#endif
}

#endif  // END guard
