// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
////////////////////////////////////////////////////////////////////////////
//
//  CryEngine Source File.
//  Copyright (C), Crytek, 1999-2010.
// -------------------------------------------------------------------------
//  File name:	Observable.h
//  Version:	v1.00
//  Created:	08.07.2010 by Nicusor Nedelcu
//  Description:  This file declares templates to be used when a class can
//								have a list of observers to feed
////////////////////////////////////////////////////////////////////////////

#include <vector>

//! Observable template class, holds a list of observers which can be called at once using the helper defines
template<class T>
class CObservable
{
public:
	// Description:
	//		Register a new observer for the class, will check if not already added
	// Return:
	//		true - if the observer was successfully added to the list
	//		false - if the observer is already in the list
	bool RegisterObserver(T* pObserver)
	{
		if (m_observers.end() != std::find(m_observers.begin(), m_observers.end(), pObserver))
			return false;

		m_observers.push_back(pObserver);

		return true;
	}

	// Description:
	//		Unregister an observer from the list
	// Return:
	//		true - if the observer was successfuly removed
	//		false - if the observer is not in the list
	bool UnregisterObserver(T* pObserver)
	{
		std::vector<T*>::iterator iter;

		if (m_observers.end() == (iter = std::find(m_observers.begin(), m_observers.end(), pObserver)))
			return false;

		m_observers.erase(iter);

		return true;
	}

	// Description:
	//		Uregister all the observers
	void UnregisterAllObservers()
	{
		m_observers.clear();
	}

protected:
	std::vector<T*> m_observers;
};

//
// Helper defines to ease the process of calling the methods of all observers in the list
//

// Description:
//		Call the method of the observers, this must be used inside the subject class
// Example: CALL_OBSERVERS_METHOD(OnStuffHappened(120, "NO!"))
#define CALL_OBSERVERS_METHOD(methodCall)                                                                              \
  { for (size_t iObs = 0, iObsCount = m_observers.size(); iObs < iObsCount; ++iObs) { m_observers[iObs]->methodCall; } \
  }

// Description:
//		Call the method of the observers, this can be used outside the subject class
// Example: CALL_OBSERVERS_METHOD_OF(pSomeObservableSubject, OnStuffHappened(120, "NO!"))
#define CALL_OBSERVERS_METHOD_OF(pObservable, methodCall)                                                                                        \
  { for (size_t iObs = 0, iObsCount = pObservable->m_observers.size(); iObs < iObsCount; ++iObs) { pObservable->m_observers[iObs]->methodCall; } \
  }

// Description:
//		Call the method of the observers, this can be called using a custom vector of observers
// Example: CALL_SPECIFIED_OBSERVERS_LIST_METHOD(vMyPreciousSpecialObservers, OnStuffHappened(120, "NO!"))
#define CALL_SPECIFIED_OBSERVERS_LIST_METHOD(vObservers, methodCall)                                                 \
  { for (size_t iObs = 0, iObsCount = vObservers.size(); iObs < iObsCount; ++iObs) { vObservers[iObs]->methodCall; } \
  }

// Description:
//		Implement the observable methods when the user class is inheriting from a virtual interface using the observable methods
#define IMPLEMENT_OBSERVABLE_METHODS(observerClassName)                                                                                     \
  virtual bool RegisterObserver(observerClassName * pObserver) { return CObservable<observerClassName>::RegisterObserver(pObserver); };     \
  virtual bool UnregisterObserver(observerClassName * pObserver) { return CObservable<observerClassName>::UnregisterObserver(pObserver); }; \
  virtual void UnregisterAllObservers() { CObservable<observerClassName>::UnregisterAllObservers(); };

