// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// Created by: Michael Kopietz
// Modified: -
//
//---------------------------------------------------------------------------

#ifndef __CCRYPOOLSTLWRAPPER__
#define __CCRYPOOLSTLWRAPPER__

#include <CryCore/StlUtils.h>

namespace NCryPoolAlloc
{

//namespace CSTLPoolAllocWrapperHelper
//{
//	inline void destruct(char *) {}
//	inline void destruct(wchar_t*) {}
//	template <typename T>
//		inline void destruct(T *t) {t->~T();}
//}

//template <size_t S, class L, size_t A, typename T>
//struct CSTLPoolAllocWrapperStatic
//{
//	static PoolAllocator<S, L, A> * allocator;
//};

//template <class T, class L, size_t A>
//struct CSTLPoolAllocWrapperKungFu : public CSTLPoolAllocWrapperStatic<sizeof(T),L,A,T>
//{
//};

template<class T, class TCont>
class CSTLPoolAllocWrapper : public stl::SAllocatorConstruct
{
private:
	static TCont* m_pContainer;
public:
	typedef size_t    size_type;
	typedef ptrdiff_t difference_type;
	typedef T*        pointer;
	typedef const T*  const_pointer;
	typedef T&        reference;
	typedef const T&  const_reference;
	typedef T         value_type;

	static TCont* Container()                  { return m_pContainer; }
	static void   Container(TCont* pContainer) { m_pContainer = pContainer; }

	template<class U> struct rebind
	{
		typedef CSTLPoolAllocWrapper<T, TCont> other;
	};

	CSTLPoolAllocWrapper() throw()
	{
	}

	CSTLPoolAllocWrapper(const CSTLPoolAllocWrapper&) throw()
	{
	}

	template<class TTemp, class TTempCont>
	CSTLPoolAllocWrapper(const CSTLPoolAllocWrapper<TTemp, TTempCont>&) throw()
	{
	}

	~CSTLPoolAllocWrapper() throw()
	{
	}

	pointer address(reference x) const
	{
		return &x;
	}

	const_pointer address(const_reference x) const
	{
		return &x;
	}

	pointer allocate(size_type n = 1, const_pointer hint = 0)
	{
		TCont* pContainer = Container();
		uint8* pData = pContainer->TCont::template Allocate<uint8*>(n * sizeof(T), sizeof(T));
		return pContainer->TCont::template Resolve<pointer>(pData);
		//	return Container()?Container()->Allocate<void*>(n*sizeof(T),sizeof(T)):0
	}

	void deallocate(pointer p, size_type n = 1)
	{
		if (Container())
			Container()->Free(p);
	}

	size_type max_size() const throw()
	{
		return Container() ? Container()->MemSize() : 0;
	}

	void destroy(pointer p)
	{
		p->~T();
	}

	pointer new_pointer()
	{
		return new(allocate())T();
	}

	pointer new_pointer(const T& val)
	{
		return new(allocate())T(val);
	}

	void delete_pointer(pointer p)
	{
		p->~T();
		deallocate(p);
	}

	bool operator==(const CSTLPoolAllocWrapper&) { return true; }
	bool operator!=(const CSTLPoolAllocWrapper&) { return false; }

};

}

#endif
