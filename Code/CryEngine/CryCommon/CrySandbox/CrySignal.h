// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <vector>

#include "CryFunction.h"

namespace CrySignalPrivate
{
template<typename Signature>
struct Observer
{
	std::function<Signature> m_function;
	uintptr_t                m_id;

	template<typename Object, typename MemberFunction>
	void SetMemberFunction(Object* pObject, MemberFunction function)
	{
		m_function = function_cast<Signature>(WrapMemberFunction(pObject, function));
	}

	template<typename Function>
	void SetFunction(const Function& function)
	{
		m_function = function_cast<Signature>(function);
	}
};

template<typename Signature> class CrySignalBase
{
};

template<typename... Args>
class CrySignalBase<void(Args...)>
{
public:
	void operator()(Args... args) const
	{
		//Iterating is done as such on purpose to avoid problems if the signal is resized during iteration
		const int count = m_observers.size();
		for (int i = count - 1; i >= 0; i--)
		{
			m_observers[i].m_function(args...);
		}
	}

protected:
	typedef Observer<void(Args...)> TObserver;
	std::vector<TObserver> m_observers;
};

}

//////////////////////////////////////////////////////////////////////////

template<typename Signature>
class CCrySignal : public CrySignalPrivate::CrySignalBase<Signature>
{
public:

	CCrySignal();
	~CCrySignal();

	template<typename Object, typename MemberFunction, typename std::enable_if<CryMemFunTraits<MemberFunction>::isMemberFunction, int>::type* = 0>
	void Connect(Object* pObject, MemberFunction function, uintptr_t forceId = 0);

	template<typename Function, typename std::enable_if<IsCallable<Function>::value, int>::type* = 0>
	void Connect(const Function& function, uintptr_t id = 0);

	void DisconnectObject(void* pObject);
	void DisconnectById(uintptr_t id);
	void DisconnectAll();
};

//////////////////////////////////////////////////////////////////////////

template<typename Signature>
CCrySignal<Signature>::CCrySignal()
{

}

template<typename Signature>
CCrySignal<Signature>::~CCrySignal()
{
	DisconnectAll();
}

template<typename Signature>
template<typename Object, typename MemberFunction, typename std::enable_if<CryMemFunTraits<MemberFunction>::isMemberFunction, int>::type*>
void CCrySignal<Signature >::Connect(Object* pObject, MemberFunction function, uintptr_t forceId /*= 0*/)
{
	TObserver observer;
	observer.m_id = forceId == 0 ? reinterpret_cast<uintptr_t>(pObject) : forceId;
	observer.SetMemberFunction(pObject, function);
	m_observers.push_back(observer);
}

template<typename Signature>
template<typename Function, typename std::enable_if<IsCallable<Function>::value, int>::type*>
void CCrySignal<Signature >::Connect(const Function& function, uintptr_t id /*= 0*/)
{
	TObserver observer;
	observer.m_id = id;
	observer.SetFunction(function);
	m_observers.push_back(observer);
}

template<typename Signature>
void CCrySignal<Signature >::DisconnectObject(void* pObject)
{
	DisconnectById(reinterpret_cast<uintptr_t>(pObject));
}

template<typename Signature>
void CCrySignal<Signature >::DisconnectById(uintptr_t id)
{
	for (auto it = m_observers.begin(); it != m_observers.end(); )
	{
		if (it->m_id == id)
			it = m_observers.erase(it);
		else
			++it;
	}
}

template<typename Signature>
void CCrySignal<Signature >::DisconnectAll()
{
	m_observers.clear();
}
