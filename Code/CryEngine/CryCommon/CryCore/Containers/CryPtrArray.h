// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _CRY_PTR_ARRAY_H_
#define _CRY_PTR_ARRAY_H_
#pragma once

#include "CryArray.h"

//---------------------------------------------------------------------------
template<class T, class P = T*>
struct PtrArray : DynArray<P>
{
	typedef DynArray<P> super;

	// Overrides.
	typedef T value_type;

	inline T& operator[](int i) const
	{ return *super::operator[](i); }

	//! Iterators, performing automatic indirection.
	template<class IP>
	struct cv_iterator
	{
		typedef std::random_access_iterator_tag iterator_category;
		typedef ptrdiff_t                       difference_type;
		typedef ptrdiff_t                       distance_type;
		typedef T                               value_type;
		typedef T*                              pointer;
		typedef T&                              reference;

		cv_iterator(IP* p)
			: _ptr(p) {}

		operator IP*() const
		{ return _ptr; }
		void operator++()
		{ _ptr++; }
		void operator--()
		{ _ptr--; }
		T&   operator*() const
		{ assert(_ptr); return **_ptr; }
		T*   operator->() const
		{ assert(_ptr); return *_ptr; }

	protected:
		IP* _ptr;
	};

	typedef cv_iterator<P>       iterator;
	typedef cv_iterator<const P> const_iterator;

	iterator begin()
	{ return iterator(super::begin()); }
	iterator end()
	{ return iterator(super::end()); }

	const_iterator begin() const
	{ return const_iterator(super::begin()); }
	const_iterator end() const
	{ return const_iterator(super::end()); }

	void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(this->begin(), this->get_alloc_size());
		for (int i = 0; i < this->size(); ++i)
			pSizer->AddObject(this->super::operator[](i));
	}
};

//---------------------------------------------------------------------------
template<class T>
struct SmartPtrArray : PtrArray<T, _smart_ptr<T>>
{
};

#endif
