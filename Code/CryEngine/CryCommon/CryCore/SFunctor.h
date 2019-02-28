// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   IFunctor.h
//  Version:     v1.00
//  Created:     05/30/2013 by Paulo Zaffari.
//  Description: User header for multi DLL functors.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef  _IFUNCTOR_H_
#define  _IFUNCTOR_H_

#pragma once

#include <functional>

struct SFunctor
{
public:
	SFunctor() {}
	SFunctor(const SFunctor &f)
	{
		m_callback = f.m_callback;
	}
	explicit SFunctor( const std::function<void(void)> &callback )
	{
		m_callback = callback;
	}

	//! Calls the functor method.
	//! \return true if a functor is registered, false otherwise.
	bool Call()
	{
		if (!m_callback)
		{
			return false;
		}

		m_callback();

		return true;
	}

	//! Sets a new functor, common function, void return type, no arguments.
	//! \param pCallback The function pointer.
	template<typename tCallback>
	void Set(tCallback pCallback)
	{
		m_callback = [=]{ pCallback(); };
	}

	//! Sets a new functor, common function, void return type, 1 argument.
	//! \param pCallback The function pointer.
	//! \param Argument1 The argument to be passed to the function.
	template<typename tCallback, typename tArgument1>
	void Set(tCallback pCallback, const tArgument1& Argument1)
	{
		m_callback = [=]{ pCallback( Argument1 ); };
	}

	//! Sets a new functor, common function, void return type, 2 arguments.
	//! \param tCallback The function pointer.
	//! \param tArgument1 The first argument to be passed to the function.
	//! \param tArgument2 The second argument to be passed to the function.
	template<typename tCallback, typename tArgument1, typename tArgument2>
	void Set(tCallback pCallback, const tArgument1& Argument1, const tArgument2& Argument2)
	{
		m_callback = [=]{ pCallback( Argument1,Argument2); };
	}

	//! Sets a new functor, common function, void return type, no arguments.
	//! \param pCallback The function pointer.
	template<typename tCallee>
	void Set(tCallee* pCallee, void (tCallee::* pCallback)())
	{
		m_callback = [=]{ (pCallee->*pCallback)(); };
	}

	//! Sets a new functor, common function, void return type, 1 argument.
	//! Parameters: the function pointer, the 2 arguments to be passed to the function, the argument.
	template<typename tCallee, typename tArgument1>
	void Set(tCallee* pCallee, void (tCallee::* pCallback)(tArgument1), const tArgument1& Argument1)
	{
		m_callback = [=]{ (pCallee->*pCallback)(Argument1); };
	}

	//! Sets a new functor, common function, void return type, 2 arguments.
	//! Parameters: the function pointer, the 2 arguments to be passed to the function, the 2 arguments.
	template<typename tCallee, typename tArgument1, typename tArgument2>
	void Set(tCallee* pCallee, void (tCallee::* pCallback)(tArgument1, tArgument2), const tArgument1& Argument1, const tArgument2& Argument2)
	{
		m_callback = [=]{ (pCallee->*pCallback)(Argument1,Argument2); };
	}

protected:
	std::function<void(void)> m_callback;
};

#endif //_IFUNCTOR_H_
