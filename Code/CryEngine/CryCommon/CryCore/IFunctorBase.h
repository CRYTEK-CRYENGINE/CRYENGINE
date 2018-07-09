// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   IFunctorBase.h
//  Version:     v1.00
//  Created:     05/30/2013 by Paulo Zaffari.
//  Description: Base header for multi DLL functors.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __IFUNCTORBASE_H_
#define __IFUNCTORBASE_H_

#pragma once

//! Base class for functor storage. Not intended for direct usage.
class IFunctorBase
{
public:
	IFunctorBase() : m_nReferences(0){}
	virtual ~IFunctorBase(){};
	virtual void Call() = 0;

	void         AddRef()
	{
		CryInterlockedIncrement(&m_nReferences);
	}

	void Release()
	{
		if (CryInterlockedDecrement(&m_nReferences) <= 0)
		{
			delete this;
		}
	}

protected:
	volatile int m_nReferences;
};

//! Base Template for specialization. Not intended for direct usage.
template<typename tType> class TFunctor : public IFunctorBase
{
};

#endif //__IFUNCTORBASE_H_
