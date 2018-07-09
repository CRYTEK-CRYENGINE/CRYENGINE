// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _CRY_DBG_ALLOCATOR_HDR_
#define _CRY_DBG_ALLOCATOR_HDR_

#include <iterator>
#if CRY_PLATFORM_WINDOWS
	#include <xmemory>
#endif

#include "TAlloc.h"
#include <CryCore/StlUtils.h>

namespace cry
{
#if defined(_DEBUG) && defined(_INC_CRTDBG) && CRY_PLATFORM_WINDOWS
//! This is just a debug mode build allocator. In release mode build it just defaults to the std::allocator
//! The szSource is the actual reason for this class - it will call operator new using this as the source file allocating the block
//! (of course this can be any name). THe number is just because the std lib interface already accepts line number
//! This allocator has static fields, meaning that copying it won't copy the szSource and nSource properties

//! Generic allocator for objects of class T.
template<class T>
class dbg_allocator : public TSimpleAllocator<T>, public stl::SAllocatorConstruct
{
public:
	typedef _SIZT          size_type;
	typedef _PDFT          difference_type;
	typedef T _FARQ*       pointer;
	typedef const T _FARQ* const_pointer;
	typedef T _FARQ&       reference;
	typedef const T _FARQ& const_reference;
	typedef T              value_type;

	//! Convert an allocator<T> to an allocator <_Other>.
	template<class _Other>
	struct rebind
	{
		typedef dbg_allocator<_Other> other;
	};

	//! \return address of mutable _Val.
	pointer address(reference _Val) const
	{
		return (&_Val);
	}

	//! \return address of nonmutable _Val.
	const_pointer address(const_reference _Val) const
	{
		return (&_Val);
	}

	//! Construct default allocator (do nothing).
	dbg_allocator(const char* szParentObject = "cry_dbg_alloc", int nParentIndex = 0) :
		m_szParentObject(szParentObject),
		m_nParentIndex(nParentIndex)
	{
	}

	//! Construct by copying (do nothing).
	dbg_allocator(const dbg_allocator<T>& rThat)
	{
		m_szParentObject = rThat.m_szParentObject;
		m_nParentIndex = rThat.m_nParentIndex;
	}

	//! Construct from a related allocator (do nothing).
	template<class _Other>
	dbg_allocator(const dbg_allocator<_Other>& rThat)
	{
		m_szParentObject = rThat.m_szParentObject;
		m_nParentIndex = rThat.m_nParentIndex;
	}

	//! Assign from a related allocator (do nothing).
	template<class _Other>
	dbg_allocator<T>& operator=(const dbg_allocator<_Other>& rThat)
	{
		m_szParentObject = rThat.m_szParentObject;
		m_nParentIndex = rThat.m_nParentIndex;
		return (*this);
	}

	//! Allocate array of _Count elements, ignore hint.
	pointer allocate(size_type _Count, const void*)
	{
		return allocate(_Count);
	}

	//!< Allocate array of _Count elements.
	pointer allocate(size_type _Count)
	{
		return (pointer)_malloc_dbg(_Count * sizeof(T), _NORMAL_BLOCK, m_szParentObject, m_nParentIndex);
	}

	//! Deallocate object at _Ptr, ignore size.
	void deallocate(pointer _Ptr, size_type)
	{
		_free_dbg(_Ptr, _NORMAL_BLOCK);
	}

	//! Destroy object at _Ptr.
	void destroy(pointer _Ptr)
	{
		_Ptr->~T();
	}

	//! Estimate maximum array size.
	_SIZT max_size() const
	{
		_SIZT _Count = (_SIZT)(-1) / sizeof(T);
		return (0 < _Count ? _Count : 1);

	}

	// The parent object (on behalf of which to call the new operator).
	const char* m_szParentObject;      //!< The file name.
	int         m_nParentIndex;        //!< The file line number.
};

//! Test for allocator equality (always true).
template<class _Ty,
         class _Other> inline
bool operator==(const dbg_allocator<_Ty>& rLeft, const dbg_allocator<_Other>& rRight)
{
	return rLeft.m_szParentObject == rRight.m_szParentObject && rLeft.m_nParentIndex == rRight.m_nParentIndex;
}

//! Test for allocator inequality (always false).
template<class _Ty,
         class _Other> inline
bool operator!=(const dbg_allocator<_Ty>&, const dbg_allocator<_Other>&)
{
	return rLeft.m_szParentObject != rRight.m_szParentObject && rLeft.m_nParentIndex != rRight.m_nParentIndex;
}

/*
   //! Generic allocator for objects of class T.
   template<class T>
   class dbg_allocator: public TSimpleAllocator<T>
   {
   public:
   dbg_allocator<T>()
   {
   }

   //! Construct default allocator (do nothing).
   dbg_allocator<T>(const char* szParentObject, int nParentIndex = 0):
    TSimpleAllocator<T>(szParentObject, nParentIndex)
   {
   }

   //! Construct by copying (do nothing).
   dbg_allocator<T>(const dbg_allocator<T>& rThat):
    TSimpleAllocator<T>(rThat)
   {
   }

   //! Construct from a related allocator (do nothing).
   template<class _Other>
    dbg_allocator(const dbg_allocator<_Other>&rThat):
      TSimpleAllocator<T>(rThat)
    {
    }

   //! Assign from a related allocator (do nothing).
   template<class _Other>
    dbg_allocator<T>& operator=(const dbg_allocator<_Other>&rThat)
    {
 * static_cast<TSimpleAllocator<T>*>(this) = rThat;
    return (*this);
    }
   };
 */

//! Generic allocator for objects of class T.
template<class T>
class dbg_struct_allocator : public TSimpleStructAllocator<T>
{
public:
	dbg_struct_allocator<T>()
	{
	}

	//! Construct default allocator (do nothing).
	dbg_struct_allocator(const char* szParentObject, int nParentIndex = 0) :
		TSimpleStructAllocator<T>(szParentObject, nParentIndex)
	{
	}

	//! Construct by copying (do nothing).
	dbg_struct_allocator(const dbg_struct_allocator<T>& rThat) :
		TSimpleStructAllocator<T>(rThat)
	{
	}

	//! Construct from a related allocator (do nothing).
	template<class _Other>
	dbg_struct_allocator(const dbg_struct_allocator<_Other>& rThat) :
		TSimpleStructAllocator<T>(rThat)
	{
	}

	//!< Assign from a related allocator (do nothing).
	template<class _Other>
	dbg_struct_allocator<T>& operator=(const dbg_struct_allocator<_Other>& rThat)
	{
		*static_cast<TSimpleStructAllocator<T>*>(this) = rThat;
		return (*this);
	}
};
#else
template<class T>
class dbg_allocator : public std::allocator<T>
{
public:
	//! Construct default allocator (do nothing).
	dbg_allocator(const char* szSource = "CryDbgAlloc", int nSource = 0)
	{
	}

	//! Construct by copying (do nothing).
	dbg_allocator(const dbg_allocator<T>& rThat) :
		std::allocator<T>(rThat)
	{
	}
	/*
	   //! Construct from a related allocator (do nothing).
	   template<class _Other>
	   dbg_allocator(const dbg_allocator<_Other>&rThat):
	   std::allocator<T>(rThat)
	   {
	   }

	   //! Assign from a related allocator (do nothing).
	   template<class _Other>
	   dbg_allocator<T>& operator=(const dbg_allocator<_Other>&rThat):
	   std::allocator<T>(rThat)
	   {
	   return (*this);
	   }

	   //! Construct by copying (do nothing).
	   dbg_allocator(const std::allocator<T>& rThat):
	   std::allocator<T> (rThat)
	   {
	   }
	 */

	//! Construct from a related allocator (do nothing).
	template<class _Other>
	dbg_allocator(const std::allocator<_Other>& rThat) :
		std::allocator<T>(rThat)
	{
	}

	//! Assign from a related allocator (do nothing).
	template<class _Other>
	dbg_allocator<T>& operator=(const std::allocator<_Other>& rThat) :
		std::allocator<T>(rThat)
	{
		return (*this);
	}
};
#endif

#if defined(_DEBUG) && CRY_PLATFORM_WINDOWS
	#if _MSC_VER >= 1300 && defined(_MAP_)
template<class Key, class Type, class Traits = std::less<Key>>
class map : public std::map<Key, Type, Traits, dbg_allocator<std::pair<Key, Type>>>
{
public:
	typedef dbg_allocator<std::pair<Key, Type>>     _Allocator;
	typedef std::map<Key, Type, Traits, _Allocator> _Base;
	map(const char* szParentObject = "STL map", int nParentIndex = 0) :
		_Base(_Base::key_compare(), _Base::allocator_type(szParentObject, nParentIndex))
	{
	}
};
	#endif

	#if (_MSC_VER >= 1300) && defined(_SET_)
template<class Key, class Traits = std::less<Key>>
class set : public std::set<Key, Traits, dbg_allocator<Key>>
{
public:
	typedef dbg_allocator<Key>                _Allocator;
	typedef std::set<Key, Traits, _Allocator> _Base;
	set(const char* szParentObject = "STL set", int nParentIndex = 0) :
		_Base(_Base::key_compare(), _Allocator(szParentObject, nParentIndex))
	{
	}
};
	#endif

	#if (_MSC_VER >= 1300) && defined(_VECTOR_)
template<class Type>
class vector : public std::vector<Type, dbg_allocator<Type>>
{
public:
	typedef dbg_allocator<Type>           _Allocator;
	typedef std::vector<Type, _Allocator> _Base;
	typedef vector<Type>                  _Myt;
	vector(const char* szParentObject = "STL vector", int nParentIndex = 0) :
		_Base(_Allocator(szParentObject, nParentIndex))
	{
	}
	vector(const _Myt& that) : _Base(that)
	{
	}
};
	#endif

	#if (_MSC_VER >= 1300) && defined(_STRING_)
class string : public std::basic_string<char, std::char_traits<char>, dbg_allocator<char>>
{
public:
	typedef dbg_allocator<char>                                         _Allocator;
	typedef std::basic_string<char, std::char_traits<char>, _Allocator> _Base;
	explicit string(const _Allocator allocator = _Allocator("STL string", 0)) :
		_Base(allocator)
	{
	}
	string(const char* szThat, const _Allocator allocator = _Allocator("STL string", 0)) :
		_Base(szThat, 0, _Base::npos, allocator)
	{
	}
	string(const char* szBegin, const char* szEnd, const _Allocator allocator = _Allocator("STL string", 0)) :
		_Base(szBegin, szEnd, allocator)
	{
	}
	string(const _Base& strRight, const _Allocator allocator = _Allocator("STL string", 0)) :
		_Base(strRight, 0, npos, allocator)
	{
	}
};
	#endif
#else
//#if _MSC_VER >= 1300 && defined(_MAP_)
template<class Key, class Type, class Traits = std::less<Key>>
class map : public std::map<Key, Type, Traits>
{
public:
	typedef std::map<Key, Type, Traits> _Base;
	map()
	{
	}
	map(const char* szParentObject, int nParentIndex = 0)
	{
	}
};
//#endif

//#if (_MSC_VER >= 1300) && defined(_SET_)
template<class Key, class Traits = std::less<Key>>
class set : public std::set<Key, Traits>
{
public:
	typedef std::set<Key, Traits> _Base;
	set()
	{
	}
	set(const char* szParentObject, int nParentIndex = 0)
	{
	}
};
//#endif

//#if (_MSC_VER >= 1300) && defined(_VECTOR_)
template<class Type>
class vector : public std::vector<Type>
{
public:
	typedef std::vector<Type> _Base;
	vector()
	{
	}
	vector(const char* szParentObject, int nParentIndex = 0)
	{
	}
};
//#endif

//#if (_MSC_VER >= 1300) && defined(_STRING_)
class string : public string
{
public:
	typedef dbg_allocator<char> _Allocator;
	typedef string              _Base;
	string()
	{
	}
	explicit string(const _Allocator allocator)
	{
	}
	string(const char* szThat) :
		_Base(szThat)
	{
	}
	string(const char* szThat, const _Allocator allocator) :
		_Base(szThat)
	{
	}
	string(const char* szBegin, const char* szEnd) :
		_Base(szBegin, szEnd)
	{
	}
	string(const char* szBegin, const char* szEnd, const _Allocator allocator) :
		_Base(szBegin, szEnd)
	{
	}
	string(const _Base& strRight) :
		_Base(strRight)
	{
	}
	string(const _Base& strRight, const _Allocator allocator) :
		_Base(strRight)
	{
	}
};
//#endif
#endif
}
#endif
