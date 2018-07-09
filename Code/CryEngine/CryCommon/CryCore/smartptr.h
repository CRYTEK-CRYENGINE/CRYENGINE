// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _SMART_PTR_H_
#define _SMART_PTR_H_

void CryFatalError(const char*, ...) PRINTF_PARAMS(1, 2);
#if CRY_PLATFORM_APPLE
#include <cstddef>
#endif

//! Smart Pointer.
template<class _I> class _smart_ptr
{
private:
	_I* p;
public:
	_smart_ptr() : p(NULL) {}
	_smart_ptr(_I* p_)
	{
		p = p_;
		if (p)
			p->AddRef();
	}

	_smart_ptr(const _smart_ptr& p_)
	{
		p = p_.p;
		if (p)
			p->AddRef();
	}

	_smart_ptr(_smart_ptr&& p_) noexcept
	{
		p = p_.p;
		p_.p = nullptr;
	}

	template<typename _Y>
	_smart_ptr(const _smart_ptr<_Y>& p_)
	{
		p = p_.get();
		if (p)
			p->AddRef();
	}

	~_smart_ptr()
	{
		if (p)
			p->Release();
	}
	operator _I*() const { return p; }

	_I&         operator*() const { return *p; }
	_I*         operator->(void) const { return p; }
	_I*         get() const { return p; }
	_smart_ptr& operator=(_I* newp)
	{
		_I* oldp = p;
		p = newp;
		if (p)
			p->AddRef();
		if (oldp)
			oldp->Release();
		return *this;
	}

	void reset()
	{
		_smart_ptr<_I>().swap(*this);
	}

	void reset(_I* p)
	{
		if (p != this->p)
		{
			_smart_ptr<_I>(p).swap(*this);
		}
	}

	_smart_ptr& operator=(const _smart_ptr& newp)
	{
		if (newp.p)
			newp.p->AddRef();
		if (p)
			p->Release();
		p = newp.p;
		return *this;
	}

	_smart_ptr& operator=(_smart_ptr&& p_)
	{
		if (this != &p_)
		{
			if (p)
				p->Release();
			p = p_.p;
			p_.p = nullptr;
		}
		return *this;
	}

	template<typename _Y>
	_smart_ptr& operator=(const _smart_ptr<_Y>& newp)
	{
		_I* const p2 = newp.get();
		if (p2)
			p2->AddRef();
		if (p)
			p->Release();
		p = p2;
		return *this;
	}

	void swap(_smart_ptr<_I>& other)
	{
		std::swap(p, other.p);
	}

	//! Assigns a pointer without increasing ref count.
	void Assign_NoAddRef(_I* ptr)
	{
		CRY_ASSERT_MESSAGE(!p, "Assign_NoAddRef should only be used on a default-constructed, not-yet-assigned smart_ptr instance");
		p = ptr;
	}

	//! Set our contained pointer to null, but don't call Release().
	//! Useful for when we want to do that ourselves, or when we know that
	//! the contained pointer is invalid.
	_I* ReleaseOwnership()
	{
		_I* ret = p;
		p = 0;
		return ret;
	}

	AUTO_STRUCT_INFO;
};

template<typename T>
ILINE void swap(_smart_ptr<T>& a, _smart_ptr<T>& b)
{
	a.swap(b);
}

//! Reference target without vtable for smart pointer.
//! Implements AddRef() and Release() strategy using reference counter of the specified type.
template<typename TDerived, typename Counter = int> class _reference_target_no_vtable
{
public:
	_reference_target_no_vtable() :
		m_nRefCounter(0)
	{
	}

	_reference_target_no_vtable(const _reference_target_no_vtable&) :
		m_nRefCounter(0)
	{
	}

	~_reference_target_no_vtable()
	{
		//assert (!m_nRefCounter);
	}

	_reference_target_no_vtable& operator=(const _reference_target_no_vtable&) { return *this; }

	void AddRef() const
	{
		CHECK_REFCOUNT_CRASH(m_nRefCounter >= 0);
		++m_nRefCounter;
	}

	void Release() const
	{
		CHECK_REFCOUNT_CRASH(m_nRefCounter > 0);
		if (--m_nRefCounter == 0)
		{
			delete static_cast<TDerived*>(this);
		}
		else if (m_nRefCounter < 0)
		{
			assert(0);
			CryFatalError("Deleting Reference Counted Object Twice");
		}
	}

	Counter UseCount() const
	{
		return m_nRefCounter;
	}

	bool Unique() const
	{
		return (m_nRefCounter == 1);
	}
protected:
	mutable Counter m_nRefCounter;
};

//! Reference target with vtable for smart pointer.
//! Implements AddRef() and Release() strategy using reference counter of the specified type.
template<typename Counter> class _reference_target
{
public:
	_reference_target() :
		m_nRefCounter(0)
	{
	}

	_reference_target(const _reference_target&) :
		m_nRefCounter(0)
	{
	}

	virtual ~_reference_target()
	{
		//assert (!m_nRefCounter);
	}

	_reference_target& operator=(const _reference_target&) { return *this; }

	void AddRef() const
	{
		CHECK_REFCOUNT_CRASH(m_nRefCounter >= 0);
		++m_nRefCounter;
	}

	void Release() const
	{
		CHECK_REFCOUNT_CRASH(m_nRefCounter > 0);
		if (--m_nRefCounter == 0)
		{
			delete this;
		}
		else if (m_nRefCounter < 0)
		{
			assert(0);
			CryFatalError("Deleting Reference Counted Object Twice");
		}
	}

	Counter UseCount() const
	{
		return m_nRefCounter;
	}

	bool Unique() const
	{
		return m_nRefCounter == 1;
	}
protected:
	mutable Counter m_nRefCounter;
};

//! Default implementation is int counter - for better alignment.
typedef _reference_target<int> _reference_target_t;

//! Reference target for smart pointer with configurable destruct function.
//! Implements AddRef() and Release() strategy using reference counter of the specified type.
template<typename T, typename Counter = int> class _cfg_reference_target
{
public:
	using DeleteFncPtr = void(*)(void*);

	_cfg_reference_target() :
		m_nRefCounter(0),
		m_pDeleteFnc(operator delete)
	{
	}

	explicit _cfg_reference_target(DeleteFncPtr pDeleteFnc) :
		m_nRefCounter(0),
		m_pDeleteFnc(pDeleteFnc)
	{
	}

	_cfg_reference_target(const _cfg_reference_target<T, Counter>& rhs) :
		m_nRefCounter(0),
		m_pDeleteFnc(rhs.m_pDeleteFnc)
	{
	}

	virtual ~_cfg_reference_target()
	{
	}

	_cfg_reference_target& operator=(const _cfg_reference_target&) { return this; }

	void AddRef() const
	{
		CHECK_REFCOUNT_CRASH(m_nRefCounter >= 0);
		++m_nRefCounter;
	}

	void Release() const
	{
		CHECK_REFCOUNT_CRASH(m_nRefCounter > 0);
		if (--m_nRefCounter == 0)
		{
			assert(m_pDeleteFnc);
			static_cast<const T*>(this)->~T();
			m_pDeleteFnc(const_cast<_cfg_reference_target*>(this));
		}
		else if (m_nRefCounter < 0)
		{
			assert(0);
			CryFatalError("Deleting Reference Counted Object Twice");
		}
	}

	//! Sets the delete function with which this object is supposed to be deleted.
	void SetDeleteFnc(DeleteFncPtr pDeleteFnc) { m_pDeleteFnc = pDeleteFnc; }

	Counter UseCount() const
	{
		return m_nRefCounter;
	}

	bool Unique() const
	{
		return m_nRefCounter == 1;
	}

protected:
	mutable Counter  m_nRefCounter;
	DeleteFncPtr     m_pDeleteFnc;
};

//! Base class for interfaces implementing reference counting.
//! Derive your interface from this class and the descendants won't have to implement
//! the reference counting logic.
template<typename Counter> class _i_reference_target
{
public:
	_i_reference_target() :
		m_nRefCounter(0)
	{
	}

	_i_reference_target(const _i_reference_target&) :
		m_nRefCounter(0)
	{
	}

	virtual ~_i_reference_target()
	{
	}

	_i_reference_target& operator=(const _i_reference_target&) { return *this; }

	virtual void AddRef() const
	{
		++m_nRefCounter;
	}

	virtual void Release() const
	{
		if (--m_nRefCounter == 0)
		{
			delete this;
		}
		else if (m_nRefCounter < 0)
		{
			assert(0);
			CryFatalError("Deleting Reference Counted Object Twice");
		}
	}

	virtual Counter UseCount() const
	{
		return m_nRefCounter;
	}

	virtual bool Unique() const
	{
		return m_nRefCounter == 1;
	}

protected:
	mutable Counter m_nRefCounter;
};

class CMultiThreadRefCount
{
public:
	CMultiThreadRefCount() : m_cnt(0) {}
	CMultiThreadRefCount(const CMultiThreadRefCount&) : m_cnt(0) {}
	virtual ~CMultiThreadRefCount() {}

	CMultiThreadRefCount& operator=(const CMultiThreadRefCount&) { return *this; }

	void AddRef() const
	{
		CryInterlockedIncrement(&m_cnt);
	}
	void Release() const
	{
		const int nCount = CryInterlockedDecrement(&m_cnt);
		assert(nCount >= 0);
		if (nCount == 0)
		{
			DeleteThis();
		}
		else if (nCount < 0)
		{
			assert(0);
			CryFatalError("Deleting Reference Counted Object Twice");
		}
	}

	int UseCount() const
	{
		CryFatalError("Do not use this function as it is not thread-safe.The return value of UseCount() may be wrong on return already.");
		return -1;
	}

	bool Unique() const
	{
		return m_cnt == 1;
	}
protected:
	// Allows the memory for the object to be deallocated in the dynamic module where it was originally constructed, as it may use different memory manager (Debug/Release configurations)
	virtual void DeleteThis() const { delete this; }

private:
	mutable volatile int m_cnt; // Private: Discourage inheriting classes to observe m_nRefCounter and act on its value which is not thread safe.
};

//! Base class for interfaces implementing reference counting that needs to be thread-safe.
//! Derive your interface from this class and the descendants won't have to implement
//! the reference counting logic.
template<typename Counter>
class _i_multithread_reference_target
{
public:
	_i_multithread_reference_target()
		: m_nRefCounter(0)
	{
	}

	_i_multithread_reference_target(const _i_multithread_reference_target&)
		: m_nRefCounter(0)
	{
	}

	virtual ~_i_multithread_reference_target()
	{
	}

	_i_multithread_reference_target& operator=(const _i_multithread_reference_target&) { return *this; }

	virtual void AddRef() const
	{
		CryInterlockedIncrement(&m_nRefCounter);
	}

	virtual void Release() const
	{
		const int nCount = CryInterlockedDecrement(&m_nRefCounter);
		assert(nCount >= 0);
		if (nCount == 0)
		{
			delete this;
		}
		else if (nCount < 0)
		{
			assert(0);
			CryFatalError("Deleting Reference Counted Object Twice");
		}
	}

	Counter UseCount() const
	{
		CryFatalError("Do not use this function as it is not thread-safe.The return value of UseCount() may be wrong on return already.");
		return Counter(-1);
	}

	virtual bool Unique() const
	{
		return m_nRefCounter == 1;
	}

protected:
	mutable volatile Counter m_nRefCounter;
};

typedef _i_reference_target<int>             _i_reference_target_t;
typedef _i_multithread_reference_target<int> _i_multithread_reference_target_t;

// TYPEDEF_AUTOPTR macro, declares Class_AutoPtr, which is the smart pointer to the given class,
// and Class_AutoArray, which is the array(STL vector) of autopointers
#ifdef ENABLE_NAIIVE_AUTOPTR
// naiive autopointer makes it easier for Visual Assist to parse the declaration and sometimes is easier for debug
#define TYPEDEF_AUTOPTR(T) typedef T* T ##                                                              _AutoPtr; typedef std::vector<T ## _AutoPtr> T ##_AutoArray;
#else
#define TYPEDEF_AUTOPTR(T) typedef _smart_ptr<T> T ##                                                   _AutoPtr; typedef std::vector<T ## _AutoPtr> T ##_AutoArray;
#endif

#endif //_SMART_PTR_H_
