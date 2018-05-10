// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
// Helper to enable inplace construction and destruction of objects
//
// History:
// Thu Apr 22 11:10:12 2010: Created by Chris Raine
//
/////////////////////////////////////////////////////////////////////////////
#ifndef STACKCONTAINER_H
#define STACKCONTAINER_H
#include <CrySystem/InplaceFactory.h>

//! Class that contains a non-pod data type allocated on the stack via placement new.
//! Constructor parameters up to an arity of 5 are forwarded over an in-place factory expression.
template<typename T>
class CStackContainer
{
	//! The backing storage.
	uint8 m_Storage[sizeof(T)];

	//! Constructs the object via an inplace factory.
	template<class Factory>
	void construct(const Factory& factory)
	{
		factory.template apply<T>(m_Storage);
	}

	//! Destructs the object. The destructor of the contained object is called.
	void destruct()
	{
		reinterpret_cast<T*>(m_Storage)->~T();
	}

	//! Prevent the object to be placed on the heap (it simply wouldn't make much sense).
	void* operator new(size_t);
	void  operator delete(void*);

public:

	//! Constructs inside the object.
	template<class Expr>
	CStackContainer(const Expr& expr)
	{ construct(expr); }

	//! Destroys the object contained.
	~CStackContainer() { destruct(); }

	// Accessor methods.
	T*       get()       { return reinterpret_cast<T*>(m_Storage); }
	const T* get() const { return reinterpret_cast<const T*>(m_Storage); }

};

#endif
